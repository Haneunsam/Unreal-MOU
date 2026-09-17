#include "Server/Lobby/LobbyPageWidgetBase.h"
#include "Server/Lobby/RoomPlayerSlotWidgetBase.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Server/ServerSubsystem.h"

namespace
{
	UVerticalBox* BuildPagePanel(UWidgetTree* Tree, const TCHAR* RootName, const FVector2D& Size)
	{
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), RootName);
		Tree->RootWidget = Canvas;

		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.94f));
		Border->SetPadding(FMargin(20.f));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Border);
		Slot->SetAnchors(FAnchors(0.5f));
		Slot->SetAlignment(FVector2D(0.5f));
		Slot->SetSize(Size);

		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Border->AddChild(Box);
		return Box;
	}

	void AddRow(UVerticalBox* Box, UWidget* Widget, float BottomPadding = 8.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
	}

	UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const TCHAR* Text)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		AddRow(Box, Label);
		return Label;
	}

	UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const TCHAR* Text, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(Name) + TEXT("Label")));
		Label->SetText(FText::FromString(Text));
		Button->AddChild(Label);
		AddRow(Box, Button);
		if (OutLabel != nullptr)
		{
			*OutLabel = Label;
		}
		return Button;
	}

	void SetMessageText(UTextBlock* Target, const FString& Text, bool bIsError)
	{
		if (Target == nullptr)
		{
			return;
		}
		Target->SetText(FText::FromString(Text));
		Target->SetColorAndOpacity(FSlateColor(bIsError
			? FLinearColor(1.f, 0.45f, 0.45f)
			: FLinearColor(0.75f, 0.75f, 0.75f)));
	}
}

void ULobbyMainWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbyMainWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (CreateRoomButton) { CreateRoomButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleCreateRoomClicked); }
	if (JoinRoomButton) { JoinRoomButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleJoinRoomClicked); }
	if (SettingsButton) { SettingsButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleSettingsClicked); }
	if (QuitGameButton) { QuitGameButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleQuitGameClicked); }
}

void ULobbyMainWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbyMainRoot"), FVector2D(380.f, 440.f));
	TitleText = AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("MOU 로비"));
	StatusText = AddText(WidgetTree, Box, TEXT("StatusText"), TEXT("서버 상태 확인 중..."));
	CreateRoomButton = AddButton(WidgetTree, Box, TEXT("CreateRoomButton"), TEXT("방 만들기"));
	JoinRoomButton = AddButton(WidgetTree, Box, TEXT("JoinRoomButton"), TEXT("방 참여하기"));
	SettingsButton = AddButton(WidgetTree, Box, TEXT("SettingsButton"), TEXT("환경설정"));
	QuitGameButton = AddButton(WidgetTree, Box, TEXT("QuitGameButton"), TEXT("게임 종료"));
	MessageText = AddText(WidgetTree, Box, TEXT("MessageText"), TEXT(""));
}

void ULobbyMainWidgetBase::Refresh(const UServerSubsystem* Server)
{
	const bool bLoggedIn = Server != nullptr && Server->GetConnectionState() == EChatConnectionState::LoggedIn;
	if (CreateRoomButton) { CreateRoomButton->SetIsEnabled(bLoggedIn); }
	if (JoinRoomButton) { JoinRoomButton->SetIsEnabled(bLoggedIn); }
	if (StatusText)
	{
		FString Status = TEXT("서버에 연결되어 있지 않습니다.");
		if (Server != nullptr)
		{
			switch (Server->GetConnectionState())
			{
			case EChatConnectionState::LoggedIn: Status = FString::Printf(TEXT("%s 님으로 접속 중"), *Server->GetLoginResult().Name); break;
			case EChatConnectionState::Connected: Status = TEXT("서버에 연결됨. 로그인 대기 중..."); break;
			case EChatConnectionState::Connecting: Status = TEXT("서버에 연결하는 중..."); break;
			default: break;
			}
		}
		StatusText->SetText(FText::FromString(Status));
	}
}

void ULobbyMainWidgetBase::SetMessage(const FString& Text, bool bIsError) { SetMessageText(MessageText, Text, bIsError); }
void ULobbyMainWidgetBase::HandleCreateRoomClicked() { OnCreateRoom.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleJoinRoomClicked() { OnJoinRoom.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleSettingsClicked() { OnOpenSettings.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleQuitGameClicked() { OnQuitGame.ExecuteIfBound(); }

void URoomLobbyWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void URoomLobbyWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	EnsurePlayerSlots();
	if (ReadyButton) { ReadyButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleReadyClicked); }
	if (StartGameButton) { StartGameButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleStartClicked); }
	if (CustomizeButton) { CustomizeButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleCustomizeClicked); }
	if (LeaveRoomButton) { LeaveRoomButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleLeaveClicked); }
}

void URoomLobbyWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("RoomLobbyRoot"), FVector2D(560.f, 660.f));
	TitleText = AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("방 대기실"));
	StatusText = AddText(WidgetTree, Box, TEXT("StatusText"), TEXT(""));
	PlayerSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PlayerSlotGrid"));
	AddRow(Box, PlayerSlotGrid, 12.f);
	CastChecked<UVerticalBoxSlot>(PlayerSlotGrid->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* ReadyLabel = nullptr;
	UTextBlock* StartLabel = nullptr;
	ReadyButton = AddButton(WidgetTree, Box, TEXT("ReadyButton"), TEXT("준비하기"), &ReadyLabel);
	StartGameButton = AddButton(WidgetTree, Box, TEXT("StartGameButton"), TEXT("게임 시작"), &StartLabel);
	ReadyButtonLabel = ReadyLabel;
	StartGameButtonLabel = StartLabel;
	CustomizeButton = AddButton(WidgetTree, Box, TEXT("CustomizeButton"), TEXT("커스터마이징"));
	LeaveRoomButton = AddButton(WidgetTree, Box, TEXT("LeaveRoomButton"), TEXT("방 나가기"));
	MessageText = AddText(WidgetTree, Box, TEXT("MessageText"), TEXT(""));
}

void URoomLobbyWidgetBase::Refresh(const UServerSubsystem* Server)
{
	if (Server == nullptr)
	{
		return;
	}
	const bool bIsHost = Server->IsRoomHost();
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("방 #%d — %s"),
			Server->GetCurrentRoomId(), bIsHost ? TEXT("방장") : TEXT("참여자"))));
	}
	if (ReadyButton)
	{
		ReadyButton->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ReadyButton->SetIsEnabled(!bIsHost);
	}
	if (ReadyButtonLabel)
	{
		ReadyButtonLabel->SetText(FText::FromString(Server->IsSelfReady() ? TEXT("준비 해제") : TEXT("준비하기")));
	}
	if (StartGameButton)
	{
		StartGameButton->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StartGameButton->SetIsEnabled(bIsHost && Server->AreAllMembersReady());
	}
	if (StartGameButtonLabel)
	{
		StartGameButtonLabel->SetText(FText::FromString(Server->AreAllMembersReady()
			? TEXT("게임 시작") : TEXT("게임 시작 (준비 대기 중)")));
	}
	RebuildMemberList(Server);
}

void URoomLobbyWidgetBase::EnsurePlayerSlots()
{
	if (!PlayerSlotGrid && MemberListBox)
	{
		PlayerSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
		MemberListBox->ClearChildren();
		UVerticalBoxSlot* GridSlot = MemberListBox->AddChildToVerticalBox(PlayerSlotGrid);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (!PlayerSlotGrid) { return; }
	if (PlayerSlots.Num() < 4)
	{
		PlayerSlotGrid->SetSlotPadding(FMargin(6.f));
		UClass* SlotClass = PlayerSlotWidgetClass ? PlayerSlotWidgetClass.Get() : URoomPlayerSlotWidgetBase::StaticClass();
		for (int32 Index = PlayerSlots.Num(); Index < 4; ++Index)
		{
			auto* PlayerSlot = CreateWidget<URoomPlayerSlotWidgetBase>(GetOwningPlayer(), SlotClass);
			if (!PlayerSlot) { return; }
			PlayerSlots.Add(PlayerSlot);
			UUniformGridSlot* GridSlot = PlayerSlotGrid->AddChildToUniformGrid(
				PlayerSlot, Index / FMath::Clamp(SlotColumns, 1, 4), Index % FMath::Clamp(SlotColumns, 1, 4));
			// Canvas-based WBP cards must receive the whole cell, not only their desired size.
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void URoomLobbyWidgetBase::RebuildMemberList(const UServerSubsystem* Server)
{
	if (!Server) { return; }
	EnsurePlayerSlots();
	const auto Members = Server->GetRoomMembers();
	for (int32 Index = 0; Index < PlayerSlots.Num(); ++Index)
	{
		const FMOURoomMember* Found = Members.FindByPredicate(
			[Index](const FMOURoomMember& Member) { return Member.SlotIndex == Index; });
		if (Found) { PlayerSlots[Index]->SetMember(*Found, Found->UserId == Server->GetLoginResult().UserId); }
		else { PlayerSlots[Index]->ClearMember(); }
	}
}

void URoomLobbyWidgetBase::SetMessage(const FString& Text, bool bIsError) { SetMessageText(MessageText, Text, bIsError); }
void URoomLobbyWidgetBase::HandleReadyClicked() { OnToggleReady.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleStartClicked() { OnStartGame.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleCustomizeClicked() { OnCustomize.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleLeaveClicked() { OnLeaveRoom.ExecuteIfBound(); }

void ULobbySettingsWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbySettingsWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (BackButton) { BackButton->OnClicked.AddUniqueDynamic(this, &ULobbySettingsWidgetBase::HandleBackClicked); }
}

void ULobbySettingsWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbySettingsRoot"), FVector2D(420.f, 320.f));
	AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("환경설정"));
	AddText(WidgetTree, Box, TEXT("DescriptionText"), TEXT("WBP에서 설정 항목을 배치하세요."));
	BackButton = AddButton(WidgetTree, Box, TEXT("BackButton"), TEXT("뒤로가기"));
}

void ULobbySettingsWidgetBase::HandleBackClicked() { OnBack.ExecuteIfBound(); }

void ULobbyCustomizeWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbyCustomizeWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (BackButton) { BackButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleBackClicked); }
}

void ULobbyCustomizeWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbyCustomizeRoot"), FVector2D(520.f, 420.f));
	AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("커스터마이징"));
	AddText(WidgetTree, Box, TEXT("DescriptionText"), TEXT("WBP에서 커스터마이징 항목을 배치하세요."));
	BackButton = AddButton(WidgetTree, Box, TEXT("BackButton"), TEXT("뒤로가기"));
}

void ULobbyCustomizeWidgetBase::HandleBackClicked() { OnBack.ExecuteIfBound(); }
