// MOU 로비 - 방 생성 UI 구현.
//
// 이 파일은 소켓/패킷을 전혀 모른다.
//   보낼 때/받을 때: ULobbyFlowCoordinator
//   상태 조회와 도달성 프로브: UServerSubsystem

#include "Server/Lobby/RoomCreateWidgetBase.h"

// MOU::kMaxRoomTitleLen 과 MOUChat::GetUtf8Length 를 쓰기 위해 포함한다.
// ChatProtocol.h 를 직접 넣지 않고 ChatFraming.h 를 거치는 이유는
// 그쪽이 THIRD_PARTY_INCLUDES_START 로 감싸주기 때문이다.
#include "Server/Net/ChatFraming.h"
#include "Server/Lobby/LobbyFlowCoordinator.h"
#include "Server/ServerSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

URoomCreateWidgetBase::URoomCreateWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// 수명 주기
// ---------------------------------------------------------------------------

void URoomCreateWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WidgetTree->RootWidget 이 이미 있으면 WBP 가 만든 레이아웃이다. 손대지 않는다.
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

// [RCUI-010] 페이지가 열릴 때 입력 이벤트를 연결하고 비밀번호를 가린다.
void URoomCreateWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (CreateButton != nullptr)
	{
		CreateButton->OnClicked.AddUniqueDynamic(this, &URoomCreateWidgetBase::HandleCreateClicked);
	}
	if (CancelButton != nullptr)
	{
		CancelButton->OnClicked.AddUniqueDynamic(this, &URoomCreateWidgetBase::HandleCancelClicked);
	}

	// NativeConstruct 는 뷰포트에 다시 붙을 때마다 불릴 수 있어 중복 구독을 막는다.
	if (!bSubscribed)
	{
		if (ULobbyFlowCoordinator* Flow = GetFlowCoordinator())
		{
			Flow->OnRoomCreateCompleted.AddUObject(this, &URoomCreateWidgetBase::HandleRoomCreated);
			Flow->OnReachabilityChecked.AddUObject(this, &URoomCreateWidgetBase::HandleReachabilityChecked);
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

	bPasswordVisible = false;
	if (PasswordRoomCheckBox)
	{
		PasswordRoomCheckBox->OnCheckStateChanged.AddUniqueDynamic(
			this, &URoomCreateWidgetBase::HandlePasswordRoomChanged);
	}
	if (TogglePasswordVisibilityButton)
	{
		TogglePasswordVisibilityButton->OnClicked.AddUniqueDynamic(
			this, &URoomCreateWidgetBase::HandlePasswordVisibilityClicked);
	}
	if (RoomPasswordBox)
	{
		RoomPasswordBox->OnTextChanged.AddUniqueDynamic(
			this, &URoomCreateWidgetBase::HandlePasswordTextChanged);
		// 재구성 시 기존 유효 입력을 기준으로 복원 상태를 초기화한다.
		LastValidPassword.Empty();
		HandlePasswordTextChanged(RoomPasswordBox->GetText());
	}
	if (PasswordRoomCheckBox && !PasswordRoomCheckBox->IsChecked())
	{
		HandlePasswordRoomChanged(false);
	}
	RefreshPasswordControls();
	SetMessage(TEXT("방 제목을 입력하세요. 비밀번호 방은 체크 후 숫자 4자리를 입력하세요."), false);

	if (RoomTitleBox != nullptr)
	{
		RoomTitleBox->SetKeyboardFocus();
	}

	// ★ 창이 열리는 지금 포트 열기를 시작한다. 사용자가 제목을 입력하는 몇 초가
	//   SSDP 탐색 + SOAP 왕복 시간과 겹쳐서, "방 만들기" 를 누를 때쯤이면 대개 끝나 있다.
	//   실패해도 방 만들기는 그대로 진행된다 (헤더의 bOpenPortOnShow 주석 참고).
	// UPnP 를 이제부터 돌릴 것인가. 돌린다면 도달성 프로브는 그것이 끝난 뒤에 해야 한다 —
	// 매핑이 생기기 전에 확인하면 당연히 실패하고, 그 거짓 음성이 방을 LAN 전용으로 막는다.
	bool bWillRunUpnp = false;

	if (bOpenPortOnShow)
	{
		if (UNatPortMappingSubsystem* Nat = GetNatSubsystem())
		{
			if (!bNatSubscribed)
			{
				Nat->OnNatMappingFinished.AddDynamic(this, &URoomCreateWidgetBase::HandleNatMappingFinished);
				bNatSubscribed = true;
			}

			// 이미 열려 있으면(창을 닫았다 다시 연 경우) 다시 열 필요가 없다.
			if (Nat->GetMappedExternalPort() == 0 && !Nat->IsMappingInProgress())
			{
				Nat->BeginPortMapping(HostPort);
				// 설정에서 UPnP를 꺼 둔 경우 BeginPortMapping은 즉시 돌아온다.
				// 그때도 true로 두면 수동 포트포워딩 사용자가 도달성 프로브를 영영
				// 시작하지 못하므로, 실제로 워커가 시작됐는지만 다시 읽는다.
				bWillRunUpnp = Nat->IsMappingInProgress();
			}
			else if (Nat->IsMappingInProgress())
			{
				bWillRunUpnp = true;
			}
		}
	}

	// ★ UPnP 를 안 돌리는 경로에서도 도달성은 확인해야 한다. (v9)
	//
	//   프로브를 HandleNatMappingFinished 에만 걸어두면, 설정으로 UPnP 를 껐거나
	//   (bUseUpnpPortMapping=False) 매핑이 이미 있는 경우에는 **한 번도 돌지 않는다.**
	//   수동 포워딩으로 운영하는 팀이 정확히 그 경로를 탄다 — 확인이 가장 필요한
	//   사람들이 확인을 못 받는 셈이다.
	if (!bWillRunUpnp)
	{
		if (UServerSubsystem* Server = GetServerSubsystem())
		{
			if (!Server->IsProbingReachability())
			{
				Server->BeginReachabilityProbe(HostPort);
			}
		}
	}
}

// [RCUI-011] 페이지 구독을 해제하고 비밀번호를 다시 가린다.
void URoomCreateWidgetBase::NativeDestruct()
{
	bPasswordVisible = false;
	RefreshPasswordControls();
	// 구독 해제를 여기서 반드시 해야 파괴된 위젯으로 델리게이트가 날아오지 않는다.
	if (bSubscribed)
	{
		if (ULobbyFlowCoordinator* Flow = GetFlowCoordinator())
		{
			Flow->OnRoomCreateCompleted.RemoveAll(this);
			Flow->OnReachabilityChecked.RemoveAll(this);
		}
		bSubscribed = false;
	}

	if (bNatSubscribed)
	{
		if (UNatPortMappingSubsystem* Nat = GetNatSubsystem())
		{
			Nat->OnNatMappingFinished.RemoveDynamic(this, &URoomCreateWidgetBase::HandleNatMappingFinished);
		}
		bNatSubscribed = false;
	}

	// ★ 여기서 ReleasePortMapping 을 부르면 안 된다.
	//   이 함수는 취소할 때만이 아니라 방 생성에 성공해서 창이 닫힐 때도 불린다.
	//   성공 시 포트를 닫아버리면 정작 참가자가 못 들어온다.
	//   취소 경로의 해제는 CancelCreate 가 담당한다.

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// 기본 레이아웃 조립 (WBP 가 없을 때만)
//
//   CanvasPanel (화면 전체)
//     └ Border (화면 정중앙 420x260, 반투명 검정)
//         └ VerticalBox
//             ├ TitleText          "방 만들기"
//             ├ RoomTitleBox       방 제목
//             ├ RoomPasswordBox    비밀번호 4자리 (선택)
//             ├ HorizontalBox
//             │   ├ CreateButton
//             │   └ CancelButton
//             └ MessageText        안내 / 실패 사유
// ---------------------------------------------------------------------------

// [RCUI-012] WBP가 없을 때 체크·보기 버튼을 포함한 기본 폼을 만든다.
void URoomCreateWidgetBase::BuildDefaultLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RoomCreateRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RoomCreatePanel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.94f));
	Panel->SetPadding(FMargin(20.f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	// 화면 정중앙에 고정한다. 해상도가 바뀌어도 가운데를 유지한다.
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(false);
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(420.f, 340.f));

	UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomCreateMainBox"));
	Panel->AddChild(MainBox);

	auto AddRow = [&](UWidget* Widget, float BottomPadding)
	{
		if (UVerticalBoxSlot* Slot = MainBox->AddChildToVerticalBox(Widget))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
	};

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("방 만들기")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AddRow(TitleText, 12.f);

	RoomTitleBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("RoomTitleBox"));
	RoomTitleBox->SetHintText(FText::FromString(TEXT("방 제목")));
	AddRow(RoomTitleBox, 6.f);

	PasswordRoomCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("PasswordRoomCheckBox"));
	UTextBlock* CheckLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PasswordRoomLabel"));
	CheckLabel->SetText(FText::FromString(TEXT("비밀번호 방")));
	PasswordRoomCheckBox->AddChild(CheckLabel);
	PasswordRoomCheckBox->SetIsChecked(false);
	AddRow(PasswordRoomCheckBox, 6.f);

	RoomPasswordBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("RoomPasswordBox"));
	RoomPasswordBox->SetHintText(FText::FromString(TEXT("비밀번호 숫자 4자리")));
	RoomPasswordBox->SetIsPassword(true);
	AddRow(RoomPasswordBox, 6.f);
	TogglePasswordVisibilityButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TogglePasswordVisibilityButton"));
	UTextBlock* EyeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PasswordVisibilityLabel"));
	EyeLabel->SetText(FText::FromString(TEXT("비밀번호 보기 / 숨기기")));
	TogglePasswordVisibilityButton->AddChild(EyeLabel);
	AddRow(TogglePasswordVisibilityButton, 12.f);

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RoomCreateButtonRow"));
	AddRow(ButtonRow, 10.f);

	auto MakeButton = [&](const TCHAR* Name, const FString& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(Name) + TEXT("Label")));
		ButtonLabel->SetText(FText::FromString(Label));
		Button->AddChild(ButtonLabel);

		if (UHorizontalBoxSlot* Slot = ButtonRow->AddChildToHorizontalBox(Button))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		return Button;
	};

	CreateButton = MakeButton(TEXT("CreateButton"), TEXT("방 만들기"));
	CancelButton = MakeButton(TEXT("CancelButton"), TEXT("취소"));

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	MessageText->SetAutoWrapText(true);
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)));
	AddRow(MessageText, 0.f);
}

// ---------------------------------------------------------------------------
// 동작
// ---------------------------------------------------------------------------

UServerSubsystem* URoomCreateWidgetBase::GetServerSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UServerSubsystem>();
	}
	return nullptr;
}

ULobbyFlowCoordinator* URoomCreateWidgetBase::GetFlowCoordinator() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<ULobbyFlowCoordinator>();
	}
	return nullptr;
}

// [RCUI-013] 제목과 공개·비밀번호방 입력을 검증한 뒤 기존 생성 흐름으로 전달한다.
void URoomCreateWidgetBase::TryCreateRoom()
{
	if (bBusy)
	{
		return;
	}

	UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat == nullptr)
	{
		SetMessage(TEXT("채팅 시스템을 찾을 수 없습니다."), true);
		return;
	}

	// 서버도 같은 규칙을 검사하지만, 여기서 먼저 걸러주면 왕복 없이 즉시 알려줄 수 있다.
	if (Chat->GetConnectionState() != EChatConnectionState::LoggedIn)
	{
		SetMessage(UServerSubsystem::GetRoomResultText(EMOURoomResultBP::NotAuthed), true);
		return;
	}
	if (Chat->GetMyRoomId() != 0)
	{
		SetMessage(UServerSubsystem::GetRoomResultText(EMOURoomResultBP::AlreadyHosting), true);
		return;
	}

	FString Title = RoomTitleBox ? RoomTitleBox->GetText().ToString() : FString();
	Title.TrimStartAndEndInline();
	if (Title.IsEmpty())
	{
		SetMessage(TEXT("방 제목을 입력하세요."), true);
		return;
	}

	// 제목 상한은 글자 수가 아니라 UTF-8 바이트 수다(널 종료 포함 48바이트).
	// CopyFixedString 이 알아서 자르지만, 잘린 이름으로 방이 열리면
	// 사용자는 왜 그런지 모른다. 보내기 전에 알려주고 멈춘다.
	const int32 TitleBytes = MOUChat::GetUtf8Length(Title);
	if (TitleBytes > static_cast<int32>(MOU::kMaxRoomTitleLen) - 1)
	{
		SetMessage(FString::Printf(TEXT("방 제목이 너무 깁니다. (한글 %d자 정도까지)"),
			(static_cast<int32>(MOU::kMaxRoomTitleLen) - 1) / 3), true);
		return;
	}

	const FString RawPassword = RoomPasswordBox ? RoomPasswordBox->GetText().ToString() : FString();
	// 체크가 없는 구형 WBP는 기존 '빈 값=공개방' 규칙을 유지한다.
	const bool bUsePassword = PasswordRoomCheckBox
		? PasswordRoomCheckBox->IsChecked() : !RawPassword.IsEmpty();
	const FString RoomPassword = bUsePassword ? RawPassword : FString();
	if (bUsePassword && !UServerSubsystem::IsValidRoomPassword(RoomPassword))
	{
		SetMessage(TEXT("비밀번호 방은 숫자 4자리를 입력해야 합니다."), true);
		return;
	}
	SubmittedTitle = Title;
	SubmittedPassword = RoomPassword;

	SetBusy(true);

	// ★ 포트 열기가 아직 진행 중이면 여기서 보내지 않는다.
	//   지금 보내면 HostPort 로 방이 등록되는데, 잠시 뒤 공유기가 다른 외부 포트를
	//   열어주면 방 정보에 이미 틀린 포트가 나가 있게 된다. 매핑이 끝나면
	//   HandleNatMappingFinished 가 이어서 보낸다.
	if (UNatPortMappingSubsystem* Nat = GetNatSubsystem())
	{
		if (Nat->IsMappingInProgress())
		{
			bCreateWaitingForNat = true;
			SetMessage(TEXT("공유기에 포트를 여는 중입니다..."), false);
			return;
		}
	}

	// 포트 매핑이 끝났더라도 실제 외부 패킷 확인이 진행 중이면 그 결과까지 기다린다.
	// ReportReachability가 CreateRoom보다 먼저 같은 TCP 큐에 들어가야 서버가 방 생성
	// 로그부터 정확한 외부 접속 상태를 표시할 수 있다.
	if (Chat->IsProbingReachability())
	{
		bCreateWaitingForProbe = true;
		SetMessage(TEXT("외부 접속 가능 여부를 확인하는 중입니다..."), false);
		return;
	}

	SubmitCreateRoom();
}

void URoomCreateWidgetBase::SubmitCreateRoom()
{
	ULobbyFlowCoordinator* Flow = GetFlowCoordinator();
	if (Flow == nullptr)
	{
		SetBusy(false);
		SetMessage(TEXT("로비 흐름 관리자를 찾을 수 없습니다."), true);
		return;
	}

	SetMessage(TEXT("방을 만드는 중..."), false);
	if (!Flow->CreateRoom(SubmittedTitle, SubmittedPassword, ResolveAdvertisedPort()))
	{
		SetBusy(false);
		SetMessage(TEXT("다른 방 요청이 처리 중입니다."), true);
	}
}

int32 URoomCreateWidgetBase::ResolveAdvertisedPort() const
{
	// 공유기가 열어준 외부 포트가 있으면 그것을 신고한다.
	// 리슨서버는 HostPort 그대로 열린다 — 공유기가 외부 포트를 내부 포트로 넘겨주므로
	// 바뀌는 것은 "밖에서 부를 주소" 뿐이고, 프로토콜도 Server.exe 도 그대로다.
	if (const UNatPortMappingSubsystem* Nat = GetNatSubsystem())
	{
		const int32 External = Nat->GetMappedExternalPort();
		if (External > 0)
		{
			return External;
		}
	}
	return HostPort;
}

UNatPortMappingSubsystem* URoomCreateWidgetBase::GetNatSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UNatPortMappingSubsystem>();
	}
	return nullptr;
}

void URoomCreateWidgetBase::HandleNatMappingFinished(EMOUNatResultBP Result, int32 ExternalPort, const FString& ExternalIp)
{
	// ★ 매핑이 끝났으니 이제 **실제로 들어오는지** 확인한다. (v9)
	//
	//   UPnP 가 성공을 보고해도 패킷이 들어온다는 보장이 없다 — 규칙을 기록만 하고
	//   NAT 테이블에 반영하지 않는 공유기가 있다. 실측으로 확인한 문제다.
	//   여기가 확인하기 가장 좋은 시점이다: 로비 맵이라 게임 포트가 비어 있고,
	//   사용자는 아직 방 제목을 입력하는 중이라 몇 초를 써도 티가 나지 않는다.
	if (UServerSubsystem* Server = GetServerSubsystem())
	{
		if (!Server->IsProbingReachability())
		{
			Server->BeginReachabilityProbe(HostPort);
		}
	}

	// 사용자가 이미 "방 만들기" 를 눌러 기다리고 있었다면, 지금이 보낼 때다.
	if (bCreateWaitingForNat)
	{
		bCreateWaitingForNat = false;
		if (UServerSubsystem* Server = GetServerSubsystem())
		{
			if (Server->IsProbingReachability())
			{
				bCreateWaitingForProbe = true;
				SetMessage(TEXT("포트 매핑 완료. 실제 외부 접속을 확인하는 중입니다..."), false);
				return;
			}
		}
		SubmitCreateRoom();
		return;
	}

	// 아직 입력 중이다. 결과만 알려주고 흐름은 막지 않는다.
	if (Result == EMOUNatResultBP::Success)
	{
		SetMessage(FString::Printf(
			TEXT("공유기에 포트를 열었습니다. 다른 네트워크에서도 접속할 수 있습니다. (%s:%d)"),
			ExternalIp.IsEmpty() ? TEXT("외부IP") : *ExternalIp, ExternalPort), false);
	}
	else
	{
		// ★ 실패해도 오류가 아니다. 방 만들기를 막지 않는다.
		//   "밖에서는 못 들어온다" 고 단정하지도 않는다 — 공유기에 포트포워딩이
		//   수동으로 걸려 있으면 UPnP 가 실패해도 외부에서 들어온다. 문구는
		//   GetNatResultText 가 그 사실에 맞게 들고 있다.
		SetMessage(UNatPortMappingSubsystem::GetNatResultText(Result).ToString(), false);
	}
}

void URoomCreateWidgetBase::HandleReachabilityChecked(bool bReachable, const FString& Detail)
{
	if (!bCreateWaitingForProbe)
	{
		return;
	}

	bCreateWaitingForProbe = false;
	SetMessage(bReachable
		? TEXT("외부 접속 확인 완료. 방을 만듭니다...")
		: FString::Printf(TEXT("직접 연결 확인 실패(%s). 릴레이 폴백을 포함해 방을 만듭니다..."), *Detail),
		false);
	SubmitCreateRoom();
}

void URoomCreateWidgetBase::CancelCreate()
{
	if (const ULobbyFlowCoordinator* Flow = GetFlowCoordinator())
	{
		if (Flow->GetOperation() == EMOULobbyFlowOperation::CreatingRoom)
		{
			SetMessage(TEXT("방 생성 응답을 기다리는 중에는 닫을 수 없습니다."), false);
			return;
		}
	}

	// ★ 호스트가 되기를 그만뒀으므로 열어둔 포트를 닫는다.
	//   성공 시에는 닫지 않는다 — 그때는 매핑이 계속 살아 있어야 참가자가 들어온다.
	if (UNatPortMappingSubsystem* Nat = GetNatSubsystem())
	{
		Nat->ReleasePortMapping();
	}

	bCreateWaitingForNat = false;
	bCreateWaitingForProbe = false;

	OnRoomCreateCancelled.ExecuteIfBound();
	RemoveFromParent();
}

// [RCUI-001] 체크 해제 시 비밀번호를 지우고 입력 상태를 갱신한다.
void URoomCreateWidgetBase::HandlePasswordRoomChanged(bool bChecked)
{
	bPasswordVisible = false;
	if (!bChecked)
	{
		LastValidPassword.Empty();
		if (RoomPasswordBox) RoomPasswordBox->SetText(FText::GetEmpty());
	}
	RefreshPasswordControls();
}

// [RCUI-002] 비밀번호를 가림 또는 숫자 표시로 전환한다.
void URoomCreateWidgetBase::HandlePasswordVisibilityClicked()
{
	if (bBusy || !RoomPasswordBox ||
		(PasswordRoomCheckBox && !PasswordRoomCheckBox->IsChecked())) return;
	bPasswordVisible = !bPasswordVisible;
	RefreshPasswordControls();
}

// [RCUI-003] 숫자 0~4자리 편집만 허용하고 잘못된 편집은 되돌린다.
void URoomCreateWidgetBase::HandlePasswordTextChanged(const FText& Text)
{
	if (bRestoringPassword || !RoomPasswordBox) return;
	const FString Value = Text.ToString();
	bool bValid = Value.Len() <= 4;
	for (TCHAR Ch : Value)
		bValid = bValid && Ch >= TEXT('0') && Ch <= TEXT('9');
	if (bValid)
	{
		LastValidPassword = Value;
		return;
	}
	bRestoringPassword = true;
	RoomPasswordBox->SetText(FText::FromString(LastValidPassword));
	bRestoringPassword = false;
	SetMessage(TEXT("비밀번호는 숫자 4자리까지 입력할 수 있습니다."), true);
}

// [RCUI-004] 체크 상태와 요청 대기에 맞춰 입력·보기 상태 및 눈 아이콘을 적용한다.
void URoomCreateWidgetBase::RefreshPasswordControls()
{
	const bool bUsePassword = !PasswordRoomCheckBox || PasswordRoomCheckBox->IsChecked();
	const bool bEditable = bUsePassword && !bBusy;
	if (RoomPasswordBox)
	{
		RoomPasswordBox->SetIsEnabled(bEditable);
		RoomPasswordBox->SetIsPassword(!bPasswordVisible);
	}
	if (TogglePasswordVisibilityButton)
	{
		TogglePasswordVisibilityButton->SetIsEnabled(bEditable);
		TogglePasswordVisibilityButton->SetToolTipText(FText::FromString(
			bPasswordVisible ? TEXT("비밀번호 숨기기") : TEXT("비밀번호 보기")));
	}
	if (PasswordHiddenMark)
	{
		PasswordHiddenMark->SetVisibility(bPasswordVisible
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (PasswordRoomCheckBox) PasswordRoomCheckBox->SetIsEnabled(!bBusy);
}

void URoomCreateWidgetBase::HandleCreateClicked() { TryCreateRoom(); }
void URoomCreateWidgetBase::HandleCancelClicked() { CancelCreate(); }

void URoomCreateWidgetBase::HandleRoomCreated(
	bool bSuccess,
	int32 RoomId,
	EMOURoomResultBP Result,
	const FString& RoomPassword)
{
	SetBusy(false);

	if (!bSuccess)
	{
		SubmittedPassword.Empty();
		SetMessage(UServerSubsystem::GetRoomResultText(Result), true);
		return;
	}

	SetMessage(FString::Printf(TEXT("방 #%d 을(를) 만들었습니다."), RoomId), false);

	// 소유자(로비)와 블루프린트에 같은 정보를 넘긴다.
	// 여기서 리슨서버를 여는 것이 다음 차례지만, 맵 이름은 게임 쪽 사정이라
	// 이 위젯이 결정하지 않는다.
	SubmittedPassword.Empty();

	OnRoomCreateSucceeded(RoomId, RoomPassword);
	OnRoomCreateFinished.ExecuteIfBound(RoomId, RoomPassword);

	if (bRemoveOnSuccess)
	{
		RemoveFromParent();
	}
}

// [RCUI-014] 요청 대기에 맞춰 폼 입력과 버튼을 함께 잠그거나 해제한다.
void URoomCreateWidgetBase::SetBusy(bool bInBusy)
{
	bBusy = bInBusy;
	if (RoomTitleBox) RoomTitleBox->SetIsEnabled(!bInBusy);
	RefreshPasswordControls();
	if (CreateButton != nullptr)
	{
		CreateButton->SetIsEnabled(!bInBusy);
	}
	if (CancelButton != nullptr)
	{
		CancelButton->SetIsEnabled(!bInBusy);
	}
}

void URoomCreateWidgetBase::SetMessage(const FString& Text, bool bIsError)
{
	if (MessageText == nullptr)
	{
		return;
	}
	MessageText->SetText(FText::FromString(Text));
	MessageText->SetColorAndOpacity(FSlateColor(bIsError
		? FLinearColor(1.f, 0.45f, 0.45f)
		: FLinearColor(0.75f, 0.75f, 0.75f)));
}

#if WITH_DEV_AUTOMATION_TESTS
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Server/Lobby/RoomListWidgetBase.h"
#include "Server/Lobby/LobbyWidgetBase.h"
#include "Server/Lobby/LobbyPageWidgetBase.h"
#include "Server/Chat/MessengerWidgetBase.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLobbyUIRegressionTest, "MOU.LobbyUI.WidgetRegression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// [RCUI-090] 외부 접속 없이 실제 로비 페이지 전환·비밀번호 입력·입장 보류 및 WBP 렌더링을 검증한다.
bool FLobbyUIRegressionTest::RunTest(const FString& Parameters)
{
	// 백엔드 없는 fixture의 목록 요청/생성 시 발생하는 예상 경고만 허용한다.
	AddExpectedError(TEXT("로그인 후에 방 목록을 볼 수 있다."), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("로그인 후에 방을 만들 수 있다."), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("잘못된 포트 번호: 0"), EAutomationExpectedErrorFlags::Contains, 0);
	UGameInstance* GI = NewObject<UGameInstance>(GEngine); GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	APlayerController* PC = World->SpawnActor<APlayerController>();
	ULocalPlayer* LP = NewObject<ULocalPlayer>(GEngine); LP->PlayerController = PC; PC->Player = LP;
	PC->SetAsLocalPlayerController(); World->AddController(PC);
	UServerSubsystem* Server = GI->GetSubsystem<UServerSubsystem>();
	ULobbyFlowCoordinator* Flow = GI->GetSubsystem<ULobbyFlowCoordinator>();
	TestEqual(TEXT("Fixture has no network backend"), Server->GetBackendName(), FString(TEXT("(없음)")));
	FEnumProperty* State = FindFProperty<FEnumProperty>(Server->GetClass(), TEXT("ConnectionState"));
	State->GetUnderlyingProperty()->SetIntPropertyValue(State->ContainerPtrToValuePtr<void>(Server), static_cast<int64>(EChatConnectionState::LoggedIn));
	FStructProperty* LoginProperty = FindFProperty<FStructProperty>(Server->GetClass(), TEXT("LoginResult"));
	FChatLoginResult& Login = *LoginProperty->ContainerPtrToValuePtr<FChatLoginResult>(Server);
	Login.bSuccess = true; Login.Name = TEXT("player1");

	UClass* LobbyClass = LoadClass<ULobbyWidgetBase>(nullptr, TEXT("/Game/02_JSY/MainLobby/WBP_LobbyWidget.WBP_LobbyWidget_C"));
	UClass* CreateClass = LoadClass<URoomCreateWidgetBase>(nullptr, TEXT("/Game/02_JSY/MainLobby/WBP_RoomCreateWidget.WBP_RoomCreateWidget_C"));
	UClass* ListClass = LoadClass<URoomListWidgetBase>(nullptr, TEXT("/Game/02_JSY/MainLobby/WBP_RoomListWidget.WBP_RoomListWidget_C"));
	if (!TestNotNull(TEXT("Lobby asset"),LobbyClass) || !TestNotNull(TEXT("Create asset"),CreateClass) || !TestNotNull(TEXT("List asset"),ListClass)) { GI->Shutdown(); return false; }
	URoomCreateWidgetBase* CreateCDO = CreateClass->GetDefaultObject<URoomCreateWidgetBase>();
	// 이 메모리 기본값은 테스트 종료 시 복원하며 에셋으로 저장하지 않는다.
	TGuardValue<bool> DisableUpnp(CreateCDO->bOpenPortOnShow, false);
	ULobbyWidgetBase* Lobby = CreateWidget<ULobbyWidgetBase>(PC,LobbyClass); Lobby->bManageMouseCursor=false; Lobby->HostPort=0;
	TSharedRef<SWidget> SlateLobby=Lobby->TakeWidget();
	UWidgetSwitcher* Stack=Cast<UWidgetSwitcher>(Lobby->GetWidgetFromName(TEXT("LobbyScreenStack")));
	if (!TestNotNull(TEXT("Actual lobby stack"),Stack)) { GI->Shutdown(); return false; }
	TestEqual(TEXT("One initial main page"),Stack->GetChildrenCount(),1);
	ULobbyMainWidgetBase* Main=Cast<ULobbyMainWidgetBase>(Stack->GetActiveWidget());
	TestTrue(TEXT("Main page uses configured WBP"), Main && Main->GetClass()==Lobby->MainLobbyWidgetClass);
	TestNotNull(TEXT("Main title PNG"),Main->GetWidgetFromName(TEXT("GameTitleImage")));
	int32 MessengerCount=0,MainCount=0;
	for(TObjectIterator<UMessengerWidgetBase> It;It;++It) if(!It->IsTemplate() && It->GetGameInstance()==GI) ++MessengerCount;
	for(TObjectIterator<ULobbyMainWidgetBase> It;It;++It) if(!It->IsTemplate() && It->GetGameInstance()==GI) ++MainCount;
	TestEqual(TEXT("One messenger after lobby construct"),MessengerCount,1);
	TestEqual(TEXT("No duplicate native main page"),MainCount,1);

	FString PreviewDir; FParse::Value(FCommandLine::Get(),TEXT("LobbyUIPreviewDir="),PreviewDir);
	auto Render=[&](const TCHAR* Filename,int32 Width=1280,int32 Height=800)
	{
		if(PreviewDir.IsEmpty()) return;
#if WITH_EDITOR
		FAssetCompilingManager::Get().FinishAllCompilation();
#endif
		FWidgetRenderer Renderer(false); Lobby->ForceLayoutPrepass();
		for(int32 I=0;I<3;++I) Renderer.DrawWidget(SlateLobby,FVector2D(Width,Height));
		UTextureRenderTarget2D* Target=Renderer.DrawWidget(SlateLobby,FVector2D(Width,Height));
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		if(Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))
		{
			for(FColor& Pixel:Pixels) Pixel=Pixel.ReinterpretAsLinear().ToFColorSRGB();
			TArray64<uint8> Bytes; FImageUtils::PNGCompressImageArray(Width,Height,Pixels,Bytes);
			TestTrue(TEXT("Preview saved"),FFileHelper::SaveArrayToFile(Bytes,*(PreviewDir/Filename)));
		}
		else AddError(TEXT("Lobby render failed"));
	};
	Render(TEXT("Lobby_Main.png"));
	Cast<UButton>(Main->GetWidgetFromName(TEXT("CreateRoomButton")))->OnClicked.Broadcast();
	URoomCreateWidgetBase* Create=Cast<URoomCreateWidgetBase>(Stack->GetActiveWidget());
	if(!TestTrue(TEXT("Create button opens configured page"),Create && Create->GetClass()==CreateClass)) {GI->Shutdown();return false;}
	TestEqual(TEXT("Main and create only in stack"),Stack->GetChildrenCount(),2);
	TestFalse(TEXT("Default public room"),Create->PasswordRoomCheckBox->IsChecked());
	TestFalse(TEXT("Public password disabled"),Create->RoomPasswordBox->GetIsEnabled());
	TestFalse(TEXT("Public eye disabled"),Create->TogglePasswordVisibilityButton->GetIsEnabled());
	TestTrue(TEXT("Initial masking"),Create->RoomPasswordBox->GetIsPassword());
	if (!TestNotNull(TEXT("Hidden eye mark binding"),Create->PasswordHiddenMark.Get())) { GI->Shutdown(); return false; }
	TestTrue(TEXT("Hidden password shows closed eye mark"),Create->PasswordHiddenMark->IsVisible());
	const UCanvasPanelSlot* CreateSlot=Cast<UCanvasPanelSlot>(Create->CreateButton->Slot);
	const UCanvasPanelSlot* CancelSlot=Cast<UCanvasPanelSlot>(Create->CancelButton->Slot);
	TestTrue(TEXT("Create button is left of back button"),CreateSlot && CancelSlot && CreateSlot->GetPosition().X < CancelSlot->GetPosition().X);
	Create->RoomTitleBox->SetText(FText::FromString(TEXT("함께 탐험할 사람!")));
	// SetText는 프로그램 변경이며 OnTextChanged를 보장하지 않는다. 실제 편집 이벤트까지 전달한다.
	auto EditPassword=[&](const TCHAR* Value)
	{
		const FText Input=FText::FromString(Value);
		Create->RoomPasswordBox->SetText(Input);
		Create->RoomPasswordBox->OnTextChanged.Broadcast(Input);
	};
	int32 CreateAttempts=0; FString UsedPassword;
	FDelegateHandle CreateResult=Flow->OnRoomCreateCompleted.AddLambda([&](bool,int32,EMOURoomResultBP,const FString& Password){++CreateAttempts;UsedPassword=Password;});
	Create->PasswordRoomCheckBox->SetIsChecked(true);Create->PasswordRoomCheckBox->OnCheckStateChanged.Broadcast(true);
	for(const TCHAR* Value:{TEXT(""),TEXT("1"),TEXT("12"),TEXT("123")})
	{
		EditPassword(Value);Create->TryCreateRoom();
	}
	TestEqual(TEXT("Empty/short password never reaches flow"),CreateAttempts,0);
	EditPassword(TEXT("0007"));
	for(const TCHAR* Value:{TEXT("12a3"),TEXT(" 123"),TEXT("12345"),TEXT("１２３４"),TEXT("12\n3")})
	{
		EditPassword(Value);
		TestEqual(TEXT("Invalid paste restores last valid input"),Create->RoomPasswordBox->GetText().ToString(),FString(TEXT("0007")));
	}
	Create->TogglePasswordVisibilityButton->OnClicked.Broadcast();
	TestFalse(TEXT("Eye reveals password"),Create->RoomPasswordBox->GetIsPassword());
	TestFalse(TEXT("Visible password shows open eye"),Create->PasswordHiddenMark->IsVisible());
	Create->TryCreateRoom();
	TestEqual(TEXT("Valid creation reaches existing flow once"),CreateAttempts,1);
	TestEqual(TEXT("Leading zeros reach flow unchanged"),UsedPassword,FString(TEXT("0007")));
	TestFalse(TEXT("Offline failure unlocks form"),Create->bBusy);
	TestEqual(TEXT("Failure preserves editable password"),Create->RoomPasswordBox->GetText().ToString(),FString(TEXT("0007")));
	Create->SetBusy(true);
	TestFalse(TEXT("Busy title disabled"),Create->RoomTitleBox->GetIsEnabled());
	TestFalse(TEXT("Busy checkbox disabled"),Create->PasswordRoomCheckBox->GetIsEnabled());
	TestFalse(TEXT("Busy password disabled"),Create->RoomPasswordBox->GetIsEnabled());
	TestFalse(TEXT("Busy eye disabled"),Create->TogglePasswordVisibilityButton->GetIsEnabled());
	Create->TogglePasswordVisibilityButton->OnClicked.Broadcast();TestTrue(TEXT("Busy eye cannot toggle"),Create->bPasswordVisible);
	Create->SetBusy(false);Create->SetMessage(TEXT("비밀번호 방은 숫자 4자리를 입력하세요."),false);
	Render(TEXT("Lobby_Create_Revealed.png"));
	Create->TogglePasswordVisibilityButton->OnClicked.Broadcast();
	TestTrue(TEXT("Hide restores closed eye mark"),Create->PasswordHiddenMark->IsVisible());
	Render(TEXT("Lobby_Create.png"));
	Render(TEXT("Lobby_Create_960x540.png"),960,540);
	Create->PasswordRoomCheckBox->SetIsChecked(false);Create->PasswordRoomCheckBox->OnCheckStateChanged.Broadcast(false);
	TestTrue(TEXT("Unchecked clears password"),Create->RoomPasswordBox->GetText().IsEmpty());
	TestTrue(TEXT("Unchecked restores masking"),Create->RoomPasswordBox->GetIsPassword());
	Create->TryCreateRoom();TestEqual(TEXT("Public submission is empty"),UsedPassword,FString());
	TestEqual(TEXT("Two valid submissions only"),CreateAttempts,2);
	Flow->OnRoomCreateCompleted.Remove(CreateResult);
	Create->CancelButton->OnClicked.Broadcast();TestEqual(TEXT("Cancel returns to main only"),Stack->GetChildrenCount(),1);

	Cast<UButton>(Main->GetWidgetFromName(TEXT("JoinRoomButton")))->OnClicked.Broadcast();
	URoomListWidgetBase* List=Cast<URoomListWidgetBase>(Stack->GetActiveWidget());
	if(!TestTrue(TEXT("Join button opens configured list"),List && List->GetClass()==ListClass)) {GI->Shutdown();return false;}
	TestTrue(TEXT("WBP private join enabled"),List->bAllowPasswordRoomJoin);
	TestEqual(TEXT("Existing auto refresh interval"),List->AutoRefreshInterval,3.f);
	if (!TestNotNull(TEXT("Password popup bound"),List->PasswordPromptPanel.Get())) {GI->Shutdown();return false;}
	TestFalse(TEXT("Popup initially hidden"),List->bPasswordPromptOpen);
	TArray<FMOURoomInfo> Rooms;
	for(int32 I=1;I<=7;++I)
	{
		FMOURoomInfo Room;Room.RoomId=I;Room.Title=I==1?TEXT("처음 오신 분도 환영합니다"):I==2?TEXT("친구끼리 비밀번호 방"):I==3?TEXT("아주 긴 방 제목에서도 참여 버튼과 인원은 유지됩니다"):FString::Printf(TEXT("함께 모험할 팀원 모집 %d"),I);
		Room.CurrentPlayers=I%3+1;Room.MaxPlayers=4;Room.bHasPassword=I==2;Rooms.Add(Room);
	}
	List->bAllowPasswordRoomJoin=false;List->HandleRoomListReceived(Rooms);
	TestEqual(TEXT("All rows created"),List->EntryWidgets.Num(),7);
	URoomListEntryWidget* Locked=List->EntryWidgets[1];
	TestFalse(TEXT("Opt-out private button disabled"),Locked->EntryJoinButton->GetIsEnabled());
	TestTrue(TEXT("Lock PNG shown"),Locked->EntryLockImage->IsVisible());
	int32 RowRequests=0;Locked->OnJoinClicked.BindLambda([&](int32){++RowRequests;});Locked->RequestJoin();TestEqual(TEXT("Opt-out row blocks delegate"),RowRequests,0);
	int32 JoinAttempts=0;FString JoinedPassword;
	FDelegateHandle JoinResult=Flow->OnRoomJoinCompleted.AddLambda([&](const FMOURoomJoinResult&,const FString& Password){++JoinAttempts;JoinedPassword=Password;});
	List->BeginJoin(2);List->ConfirmJoinWithPassword();List->SendJoinRequest(2,TEXT("1234"));List->SendJoinRequest(1,TEXT("1234"));List->SendJoinRequest(999,FString());
	TestEqual(TEXT("Opt-out and bypass attempts never reach flow"),JoinAttempts,0);
	List->BeginJoin(999);TestEqual(TEXT("Unknown room cannot bypass policy"),JoinAttempts,0);
	List->bAllowPasswordRoomJoin=true;List->HandleRoomListReceived(Rooms);
	List->BeginJoin(1);TestEqual(TEXT("Public join reaches existing flow"),JoinAttempts,1);
	TestFalse(TEXT("Offline join failure clears busy"),List->bBusy);
	List->SetBusy(true);List->HandleRoomListReceived(Rooms);
	for(const auto& Entry:List->EntryWidgets)TestFalse(TEXT("Refreshed row remains busy"),Entry->GetIsEnabled());
	List->SetBusy(false);TestTrue(TEXT("Private button enabled after busy"),List->EntryWidgets[1]->EntryJoinButton->GetIsEnabled());
	List->SetStatus(TEXT("방 7개 · 잠금방은 비밀번호를 입력하여 참여하세요."),false);
	Render(TEXT("Lobby_RoomList.png"));Render(TEXT("Lobby_RoomList_960x540.png"),960,540);

	UWidget* Messenger=Lobby->GetWidgetFromName(TEXT("LobbyMessenger"));
	const ESlateVisibility InitialMessengerVisibility=Messenger->GetVisibility();
	auto EditJoinPassword=[&](const TCHAR* Value)
	{
		const FText Input=FText::FromString(Value);
		List->JoinPasswordBox->SetText(Input);List->JoinPasswordBox->OnTextChanged.Broadcast(Input);
	};
	List->EntryWidgets[1]->RequestJoin();
	TestTrue(TEXT("Private row opens popup"),List->bPasswordPromptOpen);
	TestEqual(TEXT("Selected room ID"),List->PendingJoinRoomId,2);
	TestEqual(TEXT("Selected room title"),List->PasswordRoomNameText->GetText().ToString(),Rooms[1].Title);
	TestEqual(TEXT("Popup shares existing page"),Stack->GetChildrenCount(),2);
	TestTrue(TEXT("Messenger hidden behind modal"),Messenger->GetVisibility()==ESlateVisibility::Collapsed);
	TestFalse(TEXT("List controls blocked behind modal"),List->CloseButton->GetIsEnabled());
	TestTrue(TEXT("Password initially masked"),List->JoinPasswordBox->GetIsPassword());
	TestTrue(TEXT("Hidden eye mark shown"),List->JoinPasswordHiddenMark->IsVisible());
	TestFalse(TEXT("Empty password confirm disabled"),List->JoinConfirmButton->GetIsEnabled());
	List->BeginJoin(3);TestEqual(TEXT("Modal keeps selected room"),List->PendingJoinRoomId,2);
	Render(TEXT("PasswordJoin_Initial.png"));
	for(const TCHAR* Value:{TEXT(""),TEXT("1"),TEXT("12"),TEXT("123")})
	{
		EditJoinPassword(Value);List->ConfirmJoinWithPassword();
	}
	TestEqual(TEXT("Short input cannot submit"),JoinAttempts,1);
	EditJoinPassword(TEXT("0007"));
	for(const TCHAR* Value:{TEXT("12a3"),TEXT(" 123"),TEXT("12345"),TEXT("１２３４"),TEXT("12\n3")})
	{
		EditJoinPassword(Value);TestEqual(TEXT("Bad paste restores last valid password"),List->JoinPasswordBox->GetText().ToString(),FString(TEXT("0007")));
	}
	List->JoinPasswordVisibilityButton->OnClicked.Broadcast();
	TestFalse(TEXT("Show password unmasks input"),List->JoinPasswordBox->GetIsPassword());
	TestFalse(TEXT("Show password opens eye"),List->JoinPasswordHiddenMark->IsVisible());
	List->SetPasswordPromptStatus(FString(),false);Render(TEXT("PasswordJoin_Visible.png"));
	List->JoinPasswordVisibilityButton->OnClicked.Broadcast();Render(TEXT("PasswordJoin_Masked.png"));Render(TEXT("PasswordJoin_960x540.png"),960,540);
	List->HandleRoomListReceived(Rooms);
	TestEqual(TEXT("Refresh preserves password"),List->JoinPasswordBox->GetText().ToString(),FString(TEXT("0007")));
	for(const auto& Entry:List->EntryWidgets)TestFalse(TEXT("New rows remain blocked behind modal"),Entry->GetIsEnabled());
	List->ActiveJoinRoomId=2;List->SetBusy(true);
	List->CancelPasswordPrompt();List->JoinCloseButton->OnClicked.Broadcast();List->CloseList();List->ConfirmJoinWithPassword();
	const FKeyEvent EscapeKey(EKeys::Escape,FModifierKeysState(),0,false,0,0);
	TestTrue(TEXT("Busy Escape consumed"),List->NativeOnPreviewKeyDown(FGeometry(),EscapeKey).IsEventHandled());
	TestTrue(TEXT("Busy cannot close popup"),List->bPasswordPromptOpen);
	TestFalse(TEXT("Busy input disabled"),List->JoinPasswordBox->GetIsEnabled());
	TestFalse(TEXT("Busy eye disabled"),List->JoinPasswordVisibilityButton->GetIsEnabled());
	TestFalse(TEXT("Busy close disabled"),List->JoinCloseButton->GetIsEnabled());
	List->JoinPasswordVisibilityButton->OnClicked.Broadcast();TestFalse(TEXT("Busy eye does not toggle"),List->bJoinPasswordVisible);
	List->HandleRoomListReceived({});TestTrue(TEXT("Late empty list does not close in-flight popup"),List->bPasswordPromptOpen);
	List->HandleRoomListReceived(Rooms);
	FMOURoomJoinResult Failed;Failed.RoomId=2;Failed.Result=EMOURoomResultBP::WrongPassword;
	List->HandleRoomJoinCompleted(Failed,TEXT("0007"));
	TestTrue(TEXT("Wrong password keeps popup"),List->bPasswordPromptOpen);
	TestFalse(TEXT("Wrong password releases busy"),List->bBusy);
	TestTrue(TEXT("Wrong password clears input"),List->JoinPasswordBox->GetText().IsEmpty());
	TestFalse(TEXT("Wrong password displays error"),List->PasswordPromptStatusText->GetText().IsEmpty());
	Render(TEXT("PasswordJoin_Error.png"));
	EditJoinPassword(TEXT("0007"));List->HandleJoinPasswordCommitted(List->JoinPasswordBox->GetText(),ETextCommit::OnEnter);
	TestEqual(TEXT("Enter submits private join once"),JoinAttempts,2);
	TestEqual(TEXT("Leading zeros reach existing join flow"),JoinedPassword,FString(TEXT("0007")));
	TestFalse(TEXT("Offline failure closes popup"),List->bPasswordPromptOpen);
	TestTrue(TEXT("Messenger visibility restored"),Messenger->GetVisibility()==InitialMessengerVisibility);
	Flow->OnRoomJoinCompleted.Remove(JoinResult);

	List->BeginJoin(2);EditJoinPassword(TEXT("1234"));List->JoinCloseButton->OnClicked.Broadcast();
	TestFalse(TEXT("X closes popup only"),List->bPasswordPromptOpen);TestEqual(TEXT("List page retained after X"),Stack->GetChildrenCount(),2);
	List->BeginJoin(2);TestTrue(TEXT("Reopen clears previous password"),List->JoinPasswordBox->GetText().IsEmpty());
	List->JoinCancelButton->OnClicked.Broadcast();TestFalse(TEXT("Back closes popup only"),List->bPasswordPromptOpen);
	List->BeginJoin(2);List->NativeOnPreviewKeyDown(FGeometry(),EscapeKey);
	TestFalse(TEXT("Escape closes idle popup"),List->bPasswordPromptOpen);
	for (EMOURoomResultBP Result : {EMOURoomResultBP::Full,EMOURoomResultBP::AlreadyStarted})
	{
		List->BeginJoin(2);List->ActiveJoinRoomId=2;List->SetBusy(true);Failed.RoomId=2;Failed.Result=Result;
		List->HandleRoomJoinCompleted(Failed,FString());TestFalse(TEXT("Terminal room failure closes popup"),List->bPasswordPromptOpen);
	}
	List->BeginJoin(2);List->HandleRoomListReceived({});TestFalse(TEXT("Removed selected room closes popup"),List->bPasswordPromptOpen);
	List->HandleRoomListReceived(Rooms);List->BeginJoin(2);List->ActiveJoinRoomId=2;List->SetBusy(true);
	Failed.RoomId=0;Failed.Result=EMOURoomResultBP::NotAuthed;List->HandleRoomJoinCompleted(Failed,FString());
	TestFalse(TEXT("Disconnect result without room ID unlocks"),List->bBusy);TestFalse(TEXT("Disconnect closes popup"),List->bPasswordPromptOpen);
	// 성공 콜백 전달을 확인한다. 실제 네트워크 왕복은 수행하지 않는다.
	List->BeginJoin(2);List->ActiveJoinRoomId=2;List->SetBusy(true);int32 Approved=0;
	List->OnRoomJoinApprovedNative.BindLambda([&](const FMOURoomJoinResult&,const FString& Password){++Approved;TestEqual(TEXT("Approved callback preserves password"),Password,FString(TEXT("0007")));});
	FMOURoomJoinResult Success;Success.RoomId=2;Success.bSuccess=true;List->HandleRoomJoinCompleted(Success,TEXT("0007"));
	TestEqual(TEXT("Success forwarded once"),Approved,1);TestFalse(TEXT("Success closes modal"),List->bPasswordPromptOpen);List->OnRoomJoinApprovedNative.Unbind();
	TestTrue(TEXT("Success restores messenger"),Messenger->GetVisibility()==InitialMessengerVisibility);
	Messenger->SetVisibility(ESlateVisibility::Hidden);List->BeginJoin(2);List->CancelPasswordPrompt();
	TestTrue(TEXT("Previously hidden messenger remains hidden"),Messenger->GetVisibility()==ESlateVisibility::Hidden);Messenger->SetVisibility(InitialMessengerVisibility);
	List->HandleRoomListReceived({});Render(TEXT("Lobby_RoomList_Empty.png"));TestEqual(TEXT("Empty list clears old rows"),List->RoomListBox->GetChildrenCount(),0);
	List->CloseButton->OnClicked.Broadcast();TestEqual(TEXT("List closes to main"),Stack->GetChildrenCount(),1);
	for(int32 I=0;I<3;++I)
	{
		Lobby->OpenRoomCreate();Create=Cast<URoomCreateWidgetBase>(Stack->GetActiveWidget());
		TestEqual(TEXT("Repeated create stack count"),Stack->GetChildrenCount(),2);Create->CancelCreate();
		Lobby->OpenRoomList();List=Cast<URoomListWidgetBase>(Stack->GetActiveWidget());
		TestEqual(TEXT("Repeated list stack count"),Stack->GetChildrenCount(),2);List->CloseList();
	}
	TestEqual(TEXT("Repeated navigation leaves one main"),Stack->GetChildrenCount(),1);
	URoomCreateWidgetBase* Native=CreateWidget<URoomCreateWidgetBase>(PC);Native->bOpenPortOnShow=false;Native->HostPort=0;
	TSharedRef<SWidget> NativeSlate=Native->TakeWidget();
	TestNotNull(TEXT("Native fallback checkbox"),Native->PasswordRoomCheckBox.Get());TestNotNull(TEXT("Native fallback eye"),Native->TogglePasswordVisibilityButton.Get());
	TestTrue(TEXT("Native fallback password masked"),Native->RoomPasswordBox->GetIsPassword());
	Native->NativeDestruct();Native->ReleaseSlateResources(true);
	Lobby->NativeDestruct();Lobby->ReleaseSlateResources(true);
	GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
	return true;
}
#endif
