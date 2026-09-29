// MOU 로비 - 방 목록 / 참여 UI 구현.
//
// 이 파일은 소켓/패킷을 전혀 모른다.
//   보낼 때/받을 때: ULobbyFlowCoordinator
//   로그인 상태 조회: UServerSubsystem

#include "Server/Lobby/RoomListWidgetBase.h"

#include "Server/Lobby/LobbyFlowCoordinator.h"
#include "Server/ServerSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"

// ===========================================================================
// URoomListEntryWidget - 목록의 한 줄
// ===========================================================================

void URoomListEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void URoomListEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EntryJoinButton != nullptr)
	{
		EntryJoinButton->OnClicked.AddUniqueDynamic(this, &URoomListEntryWidget::HandleJoinClicked);
	}
}

// 한 줄의 기본 모양:
//   HorizontalBox
//     ├ EntryLockText     [비번]  (공개방이면 빈칸)
//     ├ EntryTitleText    방 제목 (Fill)
//     ├ EntryHostText     방장 닉네임
//     ├ EntryPlayersText  2 / 4
//     └ EntryJoinButton   참여
void URoomListEntryWidget::BuildDefaultLayout()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RoomEntryRow"));
	WidgetTree->RootWidget = Row;

	auto AddCell = [&](UWidget* Widget, ESlateSizeRule::Type Rule, float RightPadding)
	{
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget))
		{
			Slot->SetSize(FSlateChildSize(Rule));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, RightPadding, 0.f));
		}
	};

	EntryLockText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EntryLockText"));
	EntryLockText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)));
	AddCell(EntryLockText, ESlateSizeRule::Automatic, 6.f);

	// 제목만 Fill 이다. 방 이름이 길어져도 오른쪽의 인원수와 참여 버튼은 제자리를 지킨다.
	EntryTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EntryTitleText"));
	EntryTitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AddCell(EntryTitleText, ESlateSizeRule::Fill, 8.f);

	EntryHostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EntryHostText"));
	EntryHostText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	AddCell(EntryHostText, ESlateSizeRule::Automatic, 8.f);

	EntryPlayersText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EntryPlayersText"));
	EntryPlayersText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.85f, 1.f)));
	AddCell(EntryPlayersText, ESlateSizeRule::Automatic, 8.f);

	EntryJoinButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EntryJoinButton"));
	UTextBlock* JoinLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EntryJoinButtonLabel"));
	JoinLabel->SetText(FText::FromString(TEXT("참여")));
	EntryJoinButton->AddChild(JoinLabel);
	AddCell(EntryJoinButton, ESlateSizeRule::Automatic, 0.f);
}

void URoomListEntryWidget::SetRoomInfo(const FMOURoomInfo& InRoomInfo)
{
	RoomInfo = InRoomInfo;
	RefreshTexts();
	OnRoomInfoSet(RoomInfo);
}

// [RLUI-001] 목록의 입장 정책을 해당 행에 전달한다.
void URoomListEntryWidget::SetPasswordJoinAllowed(bool bAllowed)
{
	bPasswordJoinAllowed = bAllowed;
	RefreshTexts();
}


// [RLUI-010] 방 정보와 잠금 아이콘을 표시하고 참여 정책을 버튼에 적용한다.
void URoomListEntryWidget::RefreshTexts()
{
	if (EntryTitleText != nullptr)
	{
		EntryTitleText->SetText(FText::FromString(FString::Printf(TEXT("#%d  %s"), RoomInfo.RoomId, *RoomInfo.Title)));
	}
	if (EntryHostText != nullptr)
	{
		EntryHostText->SetText(FText::FromString(RoomInfo.HostName));
	}
	if (EntryPlayersText != nullptr)
	{
		EntryPlayersText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), RoomInfo.CurrentPlayers, RoomInfo.MaxPlayers)));
	}
	if (EntryLockText != nullptr)
	{
		EntryLockText->SetText(FText::FromString(RoomInfo.bHasPassword ? TEXT("[비번]") : TEXT("")));
	}
	if (EntryLockImage)
	{
		EntryLockImage->SetVisibility(RoomInfo.bHasPassword
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EntryLockText)
	{
		EntryLockText->SetVisibility(EntryLockImage
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (EntryJoinButton)
	{
		const bool bBlocked = RoomInfo.bHasPassword && !bPasswordJoinAllowed;
		EntryJoinButton->SetIsEnabled(!bBlocked);
		EntryJoinButton->SetToolTipText(FText::FromString(bBlocked
			? TEXT("비밀번호 방 참여는 추후 지원합니다.") : TEXT("방 참여")));
	}
}

// [RLUI-011] 보류된 비밀번호방을 차단하고 공개방 선택을 목록에 전달한다.
void URoomListEntryWidget::RequestJoin()
{
	if (RoomInfo.bHasPassword && !bPasswordJoinAllowed) return;
	OnJoinClicked.ExecuteIfBound(RoomInfo.RoomId);
}

void URoomListEntryWidget::HandleJoinClicked() { RequestJoin(); }

// ===========================================================================
// URoomListWidgetBase - 목록 전체
// ===========================================================================

URoomListWidgetBase::URoomListWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// 수명 주기
// ---------------------------------------------------------------------------

void URoomListWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

// [PJOIN-010] 비밀번호 팝업의 NativeConstruct 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (RefreshButton != nullptr)
	{
		RefreshButton->OnClicked.AddUniqueDynamic(this, &URoomListWidgetBase::HandleRefreshClicked);
	}
	if (CloseButton != nullptr)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &URoomListWidgetBase::HandleCloseClicked);
	}
	if (JoinConfirmButton != nullptr)
	{
		JoinConfirmButton->OnClicked.AddUniqueDynamic(this, &URoomListWidgetBase::HandleJoinConfirmClicked);
	}
	if (JoinCancelButton != nullptr)
	{
		JoinCancelButton->OnClicked.AddUniqueDynamic(this, &URoomListWidgetBase::HandleJoinCancelClicked);
	}

	// NativeConstruct 는 뷰포트에 다시 붙을 때마다 불릴 수 있어 중복 구독을 막는다.
	if (!bSubscribed)
	{
		if (ULobbyFlowCoordinator* Flow = GetFlowCoordinator())
		{
			Flow->OnRoomListReceived.AddUObject(this, &URoomListWidgetBase::HandleRoomListReceived);
			Flow->OnRoomJoinCompleted.AddUObject(this, &URoomListWidgetBase::HandleRoomJoinCompleted);
			bSubscribed = true;
		}
	}

	if (bManageMouseCursor)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->SetShowMouseCursor(true);
		}
	}

	if (JoinCloseButton)
		JoinCloseButton->OnClicked.AddUniqueDynamic(this, &URoomListWidgetBase::HandleJoinCancelClicked);
	if (JoinPasswordVisibilityButton)
		JoinPasswordVisibilityButton->OnClicked.AddUniqueDynamic(this, &URoomListWidgetBase::HandleJoinPasswordVisibilityClicked);
	if (JoinPasswordBox)
	{
		JoinPasswordBox->OnTextChanged.AddUniqueDynamic(this, &URoomListWidgetBase::HandleJoinPasswordTextChanged);
		JoinPasswordBox->OnTextCommitted.AddUniqueDynamic(this, &URoomListWidgetBase::HandleJoinPasswordCommitted);
	}
	ShowPasswordPrompt(false);
	RefreshRoomList();

	// 방은 서버 메모리에만 있고 호스트가 끊기면 사라진다.
	// 주기적으로 다시 받아오지 않으면 이미 없는 방을 계속 보여주게 된다.
	if (AutoRefreshInterval > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RefreshTimerHandle, this, &URoomListWidgetBase::RefreshRoomList, AutoRefreshInterval, /*bLoop=*/true);
		}
	}
}

// [PJOIN-011] 비밀번호 팝업의 NativeDestruct 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::NativeDestruct()
{
	ShowPasswordPrompt(false);
	bBusy = false;
	ActiveJoinRoomId = 0;
	// 타이머를 먼저 끈다. 위젯이 사라진 뒤에 타이머가 돌면 죽은 객체를 호출한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if (bSubscribed)
	{
		if (ULobbyFlowCoordinator* Flow = GetFlowCoordinator())
		{
			Flow->OnRoomListReceived.RemoveAll(this);
			Flow->OnRoomJoinCompleted.RemoveAll(this);
		}
		bSubscribed = false;
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// 기본 레이아웃 조립 (WBP 가 없을 때만)
//
//   CanvasPanel (화면 전체)
//     └ Border (화면 정중앙 640x420, 반투명 검정)
//         └ VerticalBox
//             ├ HorizontalBox
//             │   ├ TitleText        "방 목록"
//             │   ├ RefreshButton
//             │   └ CloseButton
//             ├ RoomListScrollBox    (Fill)
//             │   └ RoomListBox      여기에 줄이 쌓인다
//             ├ PasswordPromptPanel  비밀번호 방을 고를 때만 보인다
//             │   ├ JoinPasswordBox
//             │   ├ JoinConfirmButton
//             │   └ JoinCancelButton
//             └ StatusText
// ---------------------------------------------------------------------------

// [PJOIN-012] 비밀번호 팝업의 BuildDefaultLayout 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::BuildDefaultLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RoomListRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RoomListPanel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.94f));
	Panel->SetPadding(FMargin(20.f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(false);
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(640.f, 420.f));

	UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomListMainBox"));
	Panel->AddChild(MainBox);

	auto AddRow = [&](UWidget* Widget, ESlateSizeRule::Type Rule, float BottomPadding)
	{
		if (UVerticalBoxSlot* Slot = MainBox->AddChildToVerticalBox(Widget))
		{
			Slot->SetSize(FSlateChildSize(Rule));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
	};

	// --- 제목줄 ---
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RoomListHeaderRow"));
	AddRow(HeaderRow, ESlateSizeRule::Automatic, 10.f);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("방 목록")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	// 지역 변수 이름을 HeaderSlot 으로 두는 이유: UWidget 에 Slot 멤버가 있어서
	// 람다 밖에서 Slot 이라는 이름을 쓰면 C4458(멤버를 가림) 경고가 에러로 승격된다.
	if (UHorizontalBoxSlot* HeaderSlot = HeaderRow->AddChildToHorizontalBox(TitleText))
	{
		HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HeaderSlot->SetVerticalAlignment(VAlign_Center);
	}

	auto MakeHeaderButton = [&](const TCHAR* Name, const FString& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(Name) + TEXT("Label")));
		ButtonLabel->SetText(FText::FromString(Label));
		Button->AddChild(ButtonLabel);

		if (UHorizontalBoxSlot* Slot = HeaderRow->AddChildToHorizontalBox(Button))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		}
		return Button;
	};

	RefreshButton = MakeHeaderButton(TEXT("RefreshButton"), TEXT("새로고침"));
	CloseButton   = MakeHeaderButton(TEXT("CloseButton"),   TEXT("닫기"));

	// --- 목록 영역 ---
	RoomListScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RoomListScrollBox"));
	AddRow(RoomListScrollBox, ESlateSizeRule::Fill, 8.f);

	RoomListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomListBox"));
	RoomListScrollBox->AddChild(RoomListBox);

	// --- 비밀번호 입력줄 ---
	UHorizontalBox* PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PasswordPromptPanel"));
	PasswordPromptPanel = PromptRow;
	AddRow(PromptRow, ESlateSizeRule::Automatic, 8.f);

	JoinPasswordBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinPasswordBox"));
	JoinPasswordBox->SetHintText(FText::FromString(TEXT("방 비밀번호 숫자 4자리")));
	JoinPasswordBox->SetIsPassword(true);
	if (UHorizontalBoxSlot* PasswordSlot = PromptRow->AddChildToHorizontalBox(JoinPasswordBox))
	{
		PasswordSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PasswordSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}

	auto MakePromptButton = [&](const TCHAR* Name, const FString& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(Name) + TEXT("Label")));
		ButtonLabel->SetText(FText::FromString(Label));
		Button->AddChild(ButtonLabel);

		if (UHorizontalBoxSlot* Slot = PromptRow->AddChildToHorizontalBox(Button))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		return Button;
	};

	JoinConfirmButton = MakePromptButton(TEXT("JoinConfirmButton"), TEXT("확인"));
	JoinCancelButton  = MakePromptButton(TEXT("JoinCancelButton"),  TEXT("취소"));

	// --- 상태줄 ---
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetAutoWrapText(true);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)));
	AddRow(StatusText, ESlateSizeRule::Automatic, 0.f);
}

// ---------------------------------------------------------------------------
// 목록
// ---------------------------------------------------------------------------

UServerSubsystem* URoomListWidgetBase::GetServerSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UServerSubsystem>();
	}
	return nullptr;
}

ULobbyFlowCoordinator* URoomListWidgetBase::GetFlowCoordinator() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<ULobbyFlowCoordinator>();
	}
	return nullptr;
}

// [PJOIN-013] 비밀번호 팝업의 RefreshRoomList 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::RefreshRoomList()
{
	if (bBusy) return;
	UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat == nullptr)
	{
		SetStatus(TEXT("채팅 시스템을 찾을 수 없습니다."), true);
		return;
	}

	// 로그인 전이면 서버가 빈 목록만 돌려준다. 사유를 미리 알려준다.
	if (Chat->GetConnectionState() != EChatConnectionState::LoggedIn)
	{
		SetStatus(UServerSubsystem::GetRoomResultText(EMOURoomResultBP::NotAuthed), true);
		return;
	}

	ULobbyFlowCoordinator* Flow = GetFlowCoordinator();
	if (Flow == nullptr || !Flow->RequestRoomList())
	{
		SetStatus(TEXT("로비 흐름 관리자를 찾을 수 없습니다."), true);
	}
}

// [PJOIN-014] 비밀번호 팝업의 HandleRoomListReceived 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::HandleRoomListReceived(const TArray<FMOURoomInfo>& Rooms)
{
	CachedRooms = Rooms;
	RebuildEntries(Rooms);
	// 입장 요청 이후의 목록은 참고용이다. 입장 응답이 팝업 수명을 결정한다.
	if (bBusy) return;
	if (bPasswordPromptOpen)
	{
		const FMOURoomInfo* Selected = CachedRooms.FindByPredicate(
			[this](const FMOURoomInfo& Room) { return Room.RoomId == PendingJoinRoomId; });
		if (!Selected || !Selected->bHasPassword)
		{
			ShowPasswordPrompt(false);
			SetStatus(TEXT("고른 방이 사라졌거나 변경되었습니다. 목록에서 다시 선택하세요."), true);
			return;
		}
		if (PasswordRoomNameText) PasswordRoomNameText->SetText(FText::FromString(Selected->Title));
		return;
	}
	SetStatus(Rooms.IsEmpty() ? FString(TEXT("대기 중인 방이 없습니다. 방을 만들어보세요."))
		: FString::Printf(TEXT("방 %d개"), Rooms.Num()), false);
}

// [RLUI-020] 목록 정책과 요청 대기 상태를 적용하여 방 행들을 다시 만든다.
void URoomListWidgetBase::RebuildEntries(const TArray<FMOURoomInfo>& Rooms)
{
	if (RoomListBox == nullptr)
	{
		return;
	}

	// 줄을 재사용하지 않고 통째로 다시 만든다.
	// 방 개수 상한이 20 이라(kMaxRoomsInList) 매 초 다시 만들어도 부담이 없고,
	// 재사용 로직을 두면 "사라진 방의 버튼이 남아있는" 종류의 버그가 생긴다.
	RoomListBox->ClearChildren();
	EntryWidgets.Reset();

	UClass* EntryClass = EntryWidgetClass ? EntryWidgetClass.Get() : URoomListEntryWidget::StaticClass();

	for (const FMOURoomInfo& Room : Rooms)
	{
		URoomListEntryWidget* Entry = CreateWidget<URoomListEntryWidget>(this, EntryClass);
		if (Entry == nullptr)
		{
			continue;
		}

		// 줄이 자기 RoomId 를 들고 있으므로, 클릭이 오면 어느 방인지 알 수 있다.
		Entry->OnJoinClicked.BindUObject(this, &URoomListWidgetBase::HandleEntryJoinClicked);
		Entry->SetPasswordJoinAllowed(bAllowPasswordRoomJoin);
		Entry->SetRoomInfo(Room);
		Entry->SetIsEnabled(!bBusy && !bPasswordPromptOpen);

		if (UVerticalBoxSlot* EntrySlot = RoomListBox->AddChildToVerticalBox(Entry))
		{
			EntrySlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
		EntryWidgets.Add(Entry);
	}
}

// ---------------------------------------------------------------------------
// 참여
// ---------------------------------------------------------------------------

void URoomListWidgetBase::HandleEntryJoinClicked(int32 RoomId)
{
	BeginJoin(RoomId);
}

// [RLUI-021] 목록의 잠금방 정책을 검사하고 허용된 방의 참여를 시작한다.
void URoomListWidgetBase::BeginJoin(int32 RoomId)
{
	if (bBusy || bPasswordPromptOpen || RoomId <= 0) return;
	const FMOURoomInfo* Room = CachedRooms.FindByPredicate(
		[RoomId](const FMOURoomInfo& R) { return R.RoomId == RoomId; });
	if (!Room)
	{
		RefreshRoomList();
		SetStatus(TEXT("방 목록을 갱신한 뒤 다시 선택하세요."), true);
		return;
	}
	if (Room->bHasPassword && !bAllowPasswordRoomJoin)
	{
		SetStatus(TEXT("현재 화면에서는 비밀번호 방에 참여할 수 없습니다."), false);
		return;
	}
	if (!Room->bHasPassword)
	{
		SendJoinRequest(RoomId, FString());
		return;
	}
	if (!PasswordPromptPanel || !JoinPasswordBox || !JoinConfirmButton)
	{
		SetStatus(TEXT("비밀번호 입력 UI가 연결되지 않았습니다."), true);
		return;
	}
	PendingJoinRoomId = RoomId;
	if (PasswordRoomNameText) PasswordRoomNameText->SetText(FText::FromString(Room->Title));
	ShowPasswordPrompt(true);
}

// [RLUI-022] 잠금방 참여가 허용된 경우에만 비밀번호 확인 요청을 진행한다.
void URoomListWidgetBase::ConfirmJoinWithPassword()
{
	if (bBusy || !bPasswordPromptOpen || PendingJoinRoomId == 0) return;
	if (!bAllowPasswordRoomJoin)
	{
		ShowPasswordPrompt(false);
		SetStatus(TEXT("현재 화면에서는 비밀번호 방에 참여할 수 없습니다."), false);
		return;
	}
	const FString RoomPassword = JoinPasswordBox ? JoinPasswordBox->GetText().ToString() : FString();
	if (!UServerSubsystem::IsValidRoomPassword(RoomPassword))
	{
		SetPasswordPromptStatus(TEXT("비밀번호는 숫자 4자리를 입력하세요."), true);
		return;
	}
	SendJoinRequest(PendingJoinRoomId, RoomPassword);
}

// [PJOIN-016] 비밀번호 팝업의 CancelPasswordPrompt 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::CancelPasswordPrompt()
{
	if (bBusy)
	{
		SetPasswordPromptStatus(TEXT("참여 응답을 기다리는 중입니다."), false);
		return;
	}
	ShowPasswordPrompt(false);
	SetStatus(TEXT("참여를 취소했습니다."), false);
}

// [RLUI-023] 최종 입장 정책을 검사한 뒤 기존 흐름 관리자에 참여를 요청한다.
void URoomListWidgetBase::SendJoinRequest(int32 RoomId, const FString& RoomPassword)
{
	if (bBusy) return;
	const FMOURoomInfo* Room = CachedRooms.FindByPredicate(
		[RoomId](const FMOURoomInfo& R) { return R.RoomId == RoomId; });
	if (!Room)
	{
		ShowPasswordPrompt(false);
		SetStatus(TEXT("고른 방이 사라졌습니다. 목록에서 다시 선택하세요."), true);
		return;
	}
	if (Room->bHasPassword)
	{
		if (!bAllowPasswordRoomJoin || !bPasswordPromptOpen || PendingJoinRoomId != RoomId ||
			!UServerSubsystem::IsValidRoomPassword(RoomPassword))
		{
			SetPasswordPromptStatus(TEXT("선택한 방의 비밀번호 숫자 4자리를 입력하세요."), true);
			return;
		}
	}
	else if (!RoomPassword.IsEmpty())
	{
		ShowPasswordPrompt(false);
		SetStatus(TEXT("방 정보가 변경되었습니다. 목록에서 다시 선택하세요."), true);
		return;
	}
	if (bPasswordPromptOpen && PendingJoinRoomId != RoomId) return;
	ULobbyFlowCoordinator* Flow = GetFlowCoordinator();
	if (!Flow)
	{
		if (bPasswordPromptOpen) SetPasswordPromptStatus(TEXT("로비 흐름 관리자를 찾을 수 없습니다."), true);
		else SetStatus(TEXT("로비 흐름 관리자를 찾을 수 없습니다."), true);
		return;
	}
	ActiveJoinRoomId = RoomId;
	SetBusy(true);
	if (bPasswordPromptOpen) SetPasswordPromptStatus(TEXT("방에 참여하는 중입니다..."), false);
	else SetStatus(FString::Printf(TEXT("방 #%d 에 참여하는 중..."), RoomId), false);
	if (!Flow->JoinRoom(RoomId, RoomPassword))
	{
		ActiveJoinRoomId = 0;
		SetBusy(false);
		if (bPasswordPromptOpen) SetPasswordPromptStatus(TEXT("다른 방 요청이 처리 중입니다."), true);
		else SetStatus(TEXT("다른 방 요청이 처리 중입니다."), true);
	}
}

// [PJOIN-015] 비밀번호 팝업의 HandleRoomJoinCompleted 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::HandleRoomJoinCompleted(const FMOURoomJoinResult& Result, const FString& RoomPassword)
{
	if (!bBusy || (Result.RoomId != 0 && Result.RoomId != ActiveJoinRoomId)) return;
	ActiveJoinRoomId = 0;
	SetBusy(false);
	if (!Result.bSuccess)
	{
		const FMOURoomInfo* Selected = CachedRooms.FindByPredicate(
			[this](const FMOURoomInfo& Room) { return Room.RoomId == PendingJoinRoomId; });
		if (Result.Result == EMOURoomResultBP::WrongPassword && bPasswordPromptOpen && Selected && Selected->bHasPassword)
		{
			LastValidJoinPassword.Empty();
			bJoinPasswordVisible = false;
			if (JoinPasswordBox) JoinPasswordBox->SetText(FText::GetEmpty());
			RefreshPasswordPromptControls();
			SetPasswordPromptStatus(TEXT("비밀번호가 올바르지 않습니다. 다시 입력하세요."), true);
			if (JoinPasswordBox) JoinPasswordBox->SetKeyboardFocus();
		}
		else
		{
			ShowPasswordPrompt(false);
			SetStatus(UServerSubsystem::GetRoomResultText(Result.Result), true);
		}
		return;
	}
	ShowPasswordPrompt(false);
	SetStatus(TEXT("참여가 승인되었습니다. 대기실로 이동합니다."), false);
	OnRoomJoinApproved(Result, RoomPassword);
	OnRoomJoinApprovedNative.ExecuteIfBound(Result, RoomPassword);
	if (bRemoveOnSuccess) RemoveFromParent();
}

// [PJOIN-017] 비밀번호 팝업의 CloseList 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::CloseList()
{
	if (bPasswordPromptOpen) { CancelPasswordPrompt(); return; }
	if (const ULobbyFlowCoordinator* Flow = GetFlowCoordinator())
	{
		if (Flow->GetOperation() == EMOULobbyFlowOperation::JoiningRoom)
		{
			SetStatus(TEXT("방 참여 응답을 기다리는 중에는 닫을 수 없습니다."), false);
			return;
		}
	}

	OnRoomListClosed.ExecuteIfBound();
	RemoveFromParent();
}

void URoomListWidgetBase::HandleRefreshClicked()     { RefreshRoomList(); }
void URoomListWidgetBase::HandleCloseClicked()       { CloseList(); }
void URoomListWidgetBase::HandleJoinConfirmClicked() { ConfirmJoinWithPassword(); }
void URoomListWidgetBase::HandleJoinCancelClicked()  { CancelPasswordPrompt(); }

// ---------------------------------------------------------------------------
// 표시
// ---------------------------------------------------------------------------

// [PJOIN-018] 비밀번호 팝업의 ShowPasswordPrompt 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::ShowPasswordPrompt(bool bShow)
{
	const bool bChanged = bPasswordPromptOpen != bShow;
	bPasswordPromptOpen = bShow;
	bJoinPasswordVisible = false;
	LastValidJoinPassword.Empty();
	if (!bShow) PendingJoinRoomId = 0;
	if (PasswordPromptPanel)
		PasswordPromptPanel->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (JoinPasswordBox)
	{
		JoinPasswordBox->SetText(FText::GetEmpty());
		JoinPasswordBox->SetIsPassword(true);
	}
	SetPasswordPromptStatus(FString(), false);
	SetBusy(bBusy); // 새 팝업 상태에 맞춰 목록과 팝업 제어를 함께 갱신
	if (bChanged) OnPasswordPromptChanged.ExecuteIfBound(bShow);
	if (bShow && JoinPasswordBox) JoinPasswordBox->SetKeyboardFocus();
}

// [PJOIN-019] 비밀번호 팝업의 SetBusy 처리와 표시 상태를 동기화한다.
void URoomListWidgetBase::SetBusy(bool bInBusy)
{
	bBusy = bInBusy;
	const bool bListEnabled = !bBusy && !bPasswordPromptOpen;
	if (RefreshButton) RefreshButton->SetIsEnabled(bListEnabled);
	if (CloseButton) CloseButton->SetIsEnabled(bListEnabled);
	if (RoomListScrollBox) RoomListScrollBox->SetIsEnabled(bListEnabled);
	for (const TObjectPtr<URoomListEntryWidget>& Entry : EntryWidgets)
		if (Entry) Entry->SetIsEnabled(bListEnabled);
	RefreshPasswordPromptControls();
}

void URoomListWidgetBase::SetStatus(const FString& Text, bool bIsError)
{
	if (StatusText == nullptr)
	{
		return;
	}
	StatusText->SetText(FText::FromString(Text));
	StatusText->SetColorAndOpacity(FSlateColor(bIsError
		? FLinearColor(1.f, 0.45f, 0.45f)
		: FLinearColor(0.75f, 0.75f, 0.75f)));
}


// [PJOIN-001] 비밀번호를 가리거나 표시하고 눈 아이콘을 갱신한다.
void URoomListWidgetBase::HandleJoinPasswordVisibilityClicked()
{
	if (bBusy || !bPasswordPromptOpen || !JoinPasswordBox) return;
	bJoinPasswordVisible = !bJoinPasswordVisible;
	RefreshPasswordPromptControls();
}

// [PJOIN-002] 숫자 0~4자리만 편집하고 잘못된 편집을 되돌린다.
void URoomListWidgetBase::HandleJoinPasswordTextChanged(const FText& Text)
{
	if (bRestoringJoinPassword || !JoinPasswordBox) return;
	const FString Value = Text.ToString();
	bool bValid = Value.Len() <= 4;
	for (TCHAR Ch : Value) bValid = bValid && Ch >= TEXT('0') && Ch <= TEXT('9');
	if (bValid)
	{
		LastValidJoinPassword = Value;
		SetPasswordPromptStatus(FString(), false);
	}
	else
	{
		TGuardValue<bool> Guard(bRestoringJoinPassword, true);
		JoinPasswordBox->SetText(FText::FromString(LastValidJoinPassword));
		SetPasswordPromptStatus(TEXT("숫자 4자리까지 입력할 수 있습니다."), true);
	}
	RefreshPasswordPromptControls();
}

// [PJOIN-003] Enter 입력을 기존 비밀번호 참여 확인으로 전달한다.
void URoomListWidgetBase::HandleJoinPasswordCommitted(const FText& Text, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter) ConfirmJoinWithPassword();
}


// [PJOIN-004] 팝업 입력·버튼·눈 아이콘의 상태를 동기화한다.
void URoomListWidgetBase::RefreshPasswordPromptControls()
{
	const bool bEditable = bPasswordPromptOpen && !bBusy;
	if (JoinPasswordBox)
	{
		JoinPasswordBox->SetIsEnabled(bEditable);
		JoinPasswordBox->SetIsPassword(!bJoinPasswordVisible);
	}
	if (JoinPasswordVisibilityButton)
	{
		JoinPasswordVisibilityButton->SetIsEnabled(bEditable);
		JoinPasswordVisibilityButton->SetToolTipText(FText::FromString(
			bJoinPasswordVisible ? TEXT("비밀번호 숨기기") : TEXT("비밀번호 보기")));
	}
	if (JoinPasswordHiddenMark)
		JoinPasswordHiddenMark->SetVisibility(bJoinPasswordVisible
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (JoinConfirmButton)
		JoinConfirmButton->SetIsEnabled(bEditable && JoinPasswordBox &&
			UServerSubsystem::IsValidRoomPassword(JoinPasswordBox->GetText().ToString()));
	if (JoinCancelButton) JoinCancelButton->SetIsEnabled(bEditable);
	if (JoinCloseButton) JoinCloseButton->SetIsEnabled(bEditable);
}


// [PJOIN-005] 팝업 내부에 오류 또는 진행 안내를 표시한다.
void URoomListWidgetBase::SetPasswordPromptStatus(const FString& Text, bool bIsError)
{
	if (!PasswordPromptStatusText)
	{
		SetStatus(Text, bIsError); // 기본 레이아웃 호환
		return;
	}
	PasswordPromptStatusText->SetText(FText::FromString(Text));
	PasswordPromptStatusText->SetColorAndOpacity(FSlateColor(bIsError
		? FLinearColor(1.f, 0.45f, 0.45f) : FLinearColor(0.75f, 0.85f, 1.f)));
}


// [PJOIN-006] 팝업에서 Escape를 처리하고 다른 키는 기본 입력으로 전달한다.
FReply URoomListWidgetBase::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (bPasswordPromptOpen && KeyEvent.GetKey() == EKeys::Escape)
	{
		CancelPasswordPrompt(); // 요청 중에는 이 함수의 busy 가드가 닫힘을 막는다.
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}
