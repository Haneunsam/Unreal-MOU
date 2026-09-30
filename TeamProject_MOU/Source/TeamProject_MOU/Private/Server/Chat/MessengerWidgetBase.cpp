// MOU 메신저 - 대화창 + 통합 패널 구현 (v7 M9).
// 대응하는 설계 문서: CHAT_DESIGN.md 6절, 10절

#include "Server/Chat/MessengerWidgetBase.h"

#include "Server/ServerSubsystem.h"
#include "Server/Chat/MessengerPartsWidget.h"
#include "Server/Social/FriendPopupWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Server/Social/FriendListWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

// ===========================================================================
// UDmWindowWidget - 대화창 하나
// ===========================================================================

// [MSGUI-001] UDmWindowWidget 동작을 처리하고 관련 위젯 상태를 갱신한다.
UDmWindowWidget::UDmWindowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// [MSGUI-002] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UDmWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

// 대화창의 기본 모양:
//   VerticalBox
//     - 제목줄  [DmTitleText (Fill)] [DmCloseButton]
//     - DmMoreButton      "이전 대화 더 보기"
//     - DmScrollBox
//         └ DmMessageBox   말풍선들
//     - 입력줄  [DmInputBox (Fill)] [DmSendButton]
// [MSGUI-003] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
void UDmWindowWidget::BuildDefaultLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DmRoot"));
	WidgetTree->RootWidget = Root;

	auto AddRow = [&](UWidget* Widget, ESlateSizeRule::Type Rule, float BottomPadding)
	{
		if (UVerticalBoxSlot* BoxSlot = Root->AddChildToVerticalBox(Widget))
		{
			BoxSlot->SetSize(FSlateChildSize(Rule));
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
	};

	auto MakeButton = [&](const TCHAR* Name, const TCHAR* LabelName, const TCHAR* Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		Text->SetText(FText::FromString(Label));
		Button->AddChild(Text);
		return Button;
	};

	// 제목줄
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DmTitleRow"));

	DmTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DmTitleText"));
	if (UHorizontalBoxSlot* BoxSlot = TitleRow->AddChildToHorizontalBox(DmTitleText))
	{
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	DmCloseButton = MakeButton(TEXT("DmCloseButton"), TEXT("DmCloseLabel"), TEXT("X"));
	if (UHorizontalBoxSlot* BoxSlot = TitleRow->AddChildToHorizontalBox(DmCloseButton))
	{
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	AddRow(TitleRow, ESlateSizeRule::Automatic, 4.f);

	DmMoreButton = MakeButton(TEXT("DmMoreButton"), TEXT("DmMoreLabel"), TEXT("이전 대화 더 보기"));
	DmMoreButton->SetVisibility(ESlateVisibility::Collapsed);
	AddRow(DmMoreButton, ESlateSizeRule::Automatic, 2.f);

	DmScrollBox  = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DmScrollBox"));
	DmMessageBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DmMessageBox"));
	DmScrollBox->AddChild(DmMessageBox);
	AddRow(DmScrollBox, ESlateSizeRule::Fill, 4.f);

	// 입력줄
	UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DmInputRow"));

	DmInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DmInputBox"));
	DmInputBox->SetHintText(FText::FromString(TEXT("메시지를 입력하세요.")));
	if (UHorizontalBoxSlot* BoxSlot = InputRow->AddChildToHorizontalBox(DmInputBox))
	{
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BoxSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
	}

	DmSendButton = MakeButton(TEXT("DmSendButton"), TEXT("DmSendLabel"), TEXT("전송"));
	if (UHorizontalBoxSlot* BoxSlot = InputRow->AddChildToHorizontalBox(DmSendButton))
	{
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	AddRow(InputRow, ESlateSizeRule::Automatic, 0.f);
}

// [MSGUI-004] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UDmWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (DmTitleText) DmTitleText->SetText(FText::FromString(PeerNickname));
    if (DmCloseButton && !bShowConversationClose) DmCloseButton->SetVisibility(ESlateVisibility::Collapsed);
    if (DmInputBox && !PendingDraft.IsEmpty()) { DmInputBox->SetText(FText::FromString(PendingDraft)); PendingDraft.Empty(); }

	if (DmSendButton != nullptr)
	{
		DmSendButton->OnClicked.AddUniqueDynamic(this, &UDmWindowWidget::HandleSendClicked);
	}
	if (DmCloseButton != nullptr)
	{
		DmCloseButton->OnClicked.AddUniqueDynamic(this, &UDmWindowWidget::HandleCloseClicked);
	}
	if (DmMoreButton != nullptr)
	{
		DmMoreButton->OnClicked.AddUniqueDynamic(this, &UDmWindowWidget::HandleMoreClicked);
	}
	if (DmInputBox != nullptr)
	{
		// 엔터로도 보낼 수 있게 한다. 채팅창에서 버튼을 누르게 하면 답답하다.
		DmInputBox->OnTextCommitted.AddUniqueDynamic(this, &UDmWindowWidget::HandleInputCommitted);
	}
}

// [MSGUI-005] GetServerSubsystem 동작을 처리하고 관련 위젯 상태를 갱신한다.
UServerSubsystem* UDmWindowWidget::GetServerSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UServerSubsystem>() : nullptr;
}

// [MSGUI-006] 상대 계정과 표시 이름을 설정한다.
void UDmWindowWidget::SetPeer(int64 InPeerUserId, const FString& InPeerNickname)
{
	PeerUserId   = InPeerUserId;
	PeerNickname = InPeerNickname;

	if (DmTitleText != nullptr)
	{
		DmTitleText->SetText(FText::FromString(PeerNickname));
	}
}

// [MSGUI-007] InsertBubble 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::InsertBubble(const FMOUDirectMessage& Message, bool bPrepend)
{
    if (!DmMessageBox) return;
    TSubclassOf<UDmMessageBubbleWidget> Class = MessageBubbleClass;
    if (!Class) Class = UDmMessageBubbleWidget::StaticClass();
    UDmMessageBubbleWidget* Bubble = CreateWidget<UDmMessageBubbleWidget>(GetOwningPlayer(), Class);
    if (!Bubble) return;
    Bubble->SetMessage(Message);
    UVerticalBoxSlot* ChildSlot = DmMessageBox->AddChildToVerticalBox(Bubble);
    ChildSlot->SetHorizontalAlignment(Message.bIsMine ? HAlign_Right : HAlign_Left);
    ChildSlot->SetPadding(FMargin(4.f, 5.f));
    if (bPrepend && DmMessageBox->GetChildrenCount() > 1) DmMessageBox->ShiftChild(0, Bubble);
}

// [MSGUI-008] AppendMessage 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::AppendMessage(const FMOUDirectMessage& Message)
{
    if (LoadedMessages.ContainsByPredicate([&](const FMOUDirectMessage& Existing) { return Existing.MessageId == Message.MessageId; })) return;
    const bool bAtBottom = !DmScrollBox || DmScrollBox->GetScrollOffsetOfEnd() - DmScrollBox->GetScrollOffset() < 20.f;
    LoadedMessages.Add(Message);
    InsertBubble(Message, false);
    if (DmScrollBox && (bAtBottom || Message.bIsMine)) DmScrollBox->ScrollToEnd();
}

// [MSGUI-009] 기록과 라이브 메시지를 서버 ID로 병합해 표시한다.
void UDmWindowWidget::SetHistory(const TArray<FMOUDirectMessage>& Messages, bool bHasMore, bool bOlder)
{
    if (!DmMessageBox) return;
    const bool bHadHistory = OldestMessageId > 0;
    ForceLayoutPrepass();
    const float PreviousContentHeight = DmMessageBox->GetDesiredSize().Y;
    const float PreviousOffset = DmScrollBox ? DmScrollBox->GetScrollOffset() : 0.f;
    const float PreviousEnd = DmScrollBox ? DmScrollBox->GetScrollOffsetOfEnd() : 0.f;
    const bool bAtBottom = PreviousEnd - PreviousOffset < 20.f;
    // 최신 페이지와 라이브 수신이 교차해도 기존 메시지를 지우지 않고 서버 ID로 병합한다.
    for (const FMOUDirectMessage& Message : Messages)
    {
        if (!LoadedMessages.ContainsByPredicate([&](const FMOUDirectMessage& Existing) { return Existing.MessageId == Message.MessageId; })) LoadedMessages.Add(Message);
    }
    LoadedMessages.Sort([](const FMOUDirectMessage& A, const FMOUDirectMessage& B) { return A.MessageId < B.MessageId; });
    DmMessageBox->ClearChildren();
    for (const FMOUDirectMessage& Message : LoadedMessages) InsertBubble(Message, false);
    if (!LoadedMessages.IsEmpty()) OldestMessageId = LoadedMessages[0].MessageId;
    if (bOlder || !bHadHistory) bHasMoreHistory = bHasMore;
    SetHistoryBusy(false);
    ForceLayoutPrepass();
    if (DmScrollBox)
    {
        if (bOlder) DmScrollBox->SetScrollOffset(PreviousOffset + FMath::Max(0.f, DmMessageBox->GetDesiredSize().Y - PreviousContentHeight));
        else if (!bHadHistory || bAtBottom) DmScrollBox->ScrollToEnd();
        else DmScrollBox->SetScrollOffset(PreviousOffset);
    }
}

// [MSGUI-010] SetHistoryBusy 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::SetHistoryBusy(bool bBusy)
{
    bHistoryBusy = bBusy;
    if (DmMoreButton)
    {
        DmMoreButton->SetIsEnabled(!bBusy);
        DmMoreButton->SetVisibility(bHasMoreHistory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (DmStatusText) DmStatusText->SetText(FText::FromString(bBusy ? TEXT("대화를 불러오는 중...") : TEXT("")));
}
// [MSGUI-011] GetDraft 동작을 처리하고 관련 위젯 상태를 갱신한다.
FString UDmWindowWidget::GetDraft() const { return DmInputBox ? DmInputBox->GetText().ToString() : PendingDraft; }
// [MSGUI-012] SetDraft 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::SetDraft(const FString& Draft) { PendingDraft = Draft; if (DmInputBox) DmInputBox->SetText(FText::FromString(Draft)); }

// [MSGUI-013] HandleSendClicked 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::HandleSendClicked()
{
	if (DmInputBox == nullptr)
	{
		return;
	}

	const FString Text = DmInputBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	UServerSubsystem* Chat = GetServerSubsystem();
    if (!Chat || !Chat->GetLoginResult().bSuccess) return;
    Chat->SendDirectMessage(PeerUserId, Text);

	// ★ 화면에 먼저 그리지 않는다. 서버가 저장하고 **나에게도 되돌려주므로**
	//   그때 그린다. 미리 그리면 서버가 거부했을 때(친구가 아님 등) 지워야 하고,
	//   서버가 매긴 MessageId 도 모른다.
	DmInputBox->SetText(FText::GetEmpty());
}

// [MSGUI-014] HandleInputCommitted 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::HandleInputCommitted(const FText& /*Text*/, ETextCommit::Type CommitMethod)
{
	// 포커스를 잃은 것(OnUserMovedFocus)까지 전송으로 처리하면 창을 옮기다가
	// 쓰다 만 글이 나간다. 엔터만 받는다.
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleSendClicked();
	}
}

// [MSGUI-015] HandleCloseClicked 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::HandleCloseClicked()
{
	// 스스로 파괴하지 않는다. 부모의 TMap 에 죽은 포인터가 남기 때문이다(헤더 ★).
	OnCloseRequested.ExecuteIfBound(PeerUserId);
}

// [MSGUI-016] HandleMoreClicked 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UDmWindowWidget::HandleMoreClicked()
{
    if (!bHistoryBusy && OldestMessageId > 0) OnHistoryRequested.ExecuteIfBound(PeerUserId, OldestMessageId);
}

// [MSGUI-017] UMessengerWidgetBase 동작을 처리하고 관련 위젯 상태를 갱신한다.
UMessengerWidgetBase::UMessengerWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    FriendListClass = UFriendListWidgetBase::StaticClass();
    DmWindowClass = UDmWindowWidget::StaticClass();
    MessengerPanelClass = UMessengerPanelWidget::StaticClass();
    AddFriendPopupClass = UAddFriendPopupWidget::StaticClass();
    RequestsPopupClass = UFriendRequestsPopupWidget::StaticClass();
    ContextMenuClass = UFriendContextMenuWidget::StaticClass();
}
// [MSGUI-018] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UMessengerWidgetBase::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget) BuildDefaultLayout();
}
// [MSGUI-019] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
void UMessengerWidgetBase::BuildDefaultLayout()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MessengerRoot"));
    WidgetTree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UVerticalBox* Dock = WidgetTree->ConstructWidget<UVerticalBox>();
    UOverlaySlot* DockSlot = Root->AddChildToOverlay(Dock); DockSlot->SetHorizontalAlignment(HAlign_Right); DockSlot->SetVerticalAlignment(VAlign_Bottom); DockSlot->SetPadding(FMargin(20.f));
    UHorizontalBox* Panels = WidgetTree->ConstructWidget<UHorizontalBox>(); Dock->AddChildToVerticalBox(Panels);
    MessengerPanelSlot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MessengerPanelSlot"));
    Panels->AddChildToHorizontalBox(MessengerPanelSlot)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
    FriendPanelSlot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FriendPanelSlot")); Panels->AddChildToHorizontalBox(FriendPanelSlot);
    UHorizontalBox* Launcher = WidgetTree->ConstructWidget<UHorizontalBox>();
    Dock->AddChildToVerticalBox(Launcher)->SetHorizontalAlignment(HAlign_Right);
    FriendToggleButton = MOUMessengerUI::Button(WidgetTree, TEXT("FriendToggleButton"), TEXT("친구")); Launcher->AddChildToHorizontalBox(FriendToggleButton);
    MessengerToggleButton = MOUMessengerUI::Button(WidgetTree, TEXT("MessengerToggleButton"), TEXT("메신저")); Launcher->AddChildToHorizontalBox(MessengerToggleButton);
    PopupLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PopupLayer")); Root->AddChildToOverlay(PopupLayer);
    ContextMenuLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ContextMenuLayer")); Root->AddChildToOverlay(ContextMenuLayer);
}
// [MSGUI-020] GetServerSubsystem 동작을 처리하고 관련 위젯 상태를 갱신한다.
UServerSubsystem* UMessengerWidgetBase::GetServerSubsystem() const
{
    const UGameInstance* GI = GetGameInstance(); return GI ? GI->GetSubsystem<UServerSubsystem>() : nullptr;
}
// [MSGUI-021] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UMessengerWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (!FriendList && FriendPanelSlot)
    {
        TSubclassOf<UFriendListWidgetBase> Class = FriendListClass; if (!Class) Class = UFriendListWidgetBase::StaticClass();
        FriendList = CreateWidget<UFriendListWidgetBase>(GetOwningPlayer(), Class);
        if (FriendList) FriendPanelSlot->AddChildToVerticalBox(FriendList);
        if (MessengerPanelSlot) FriendPanelSlot->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (FriendList)
    {
        FriendList->OnConversationRequested.BindUObject(this, &UMessengerWidgetBase::HandleConversationRequested);
        FriendList->OnAddFriendPopupRequested.BindUObject(this, &UMessengerWidgetBase::ShowAddFriendPopup);
        FriendList->OnRequestsPopupRequested.BindUObject(this, &UMessengerWidgetBase::ShowRequestsPopup);
        FriendList->OnPanelCloseRequested.BindUObject(this, &UMessengerWidgetBase::HideFriends);
        FriendList->OnContextMenuRequested.BindUObject(this, &UMessengerWidgetBase::ShowFriendContextMenu);
    }
    if (!MessengerPanel && MessengerPanelSlot)
    {
        TSubclassOf<UMessengerPanelWidget> Class = MessengerPanelClass; if (!Class) Class = UMessengerPanelWidget::StaticClass();
        MessengerPanel = CreateWidget<UMessengerPanelWidget>(GetOwningPlayer(), Class);
        if (MessengerPanel) MessengerPanelSlot->AddChildToVerticalBox(MessengerPanel);
        MessengerPanelSlot->SetVisibility(bMessengerVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (MessengerPanel)
    {
        MessengerPanel->OnSelected.BindUObject(this, &UMessengerWidgetBase::SelectConversation);
        MessengerPanel->OnConversationCloseRequested.BindUObject(this, &UMessengerWidgetBase::CloseConversation);
        MessengerPanel->OnCloseRequested.BindUObject(this, &UMessengerWidgetBase::HideMessenger);
    }
    if (FriendToggleButton) FriendToggleButton->OnClicked.AddUniqueDynamic(this, &UMessengerWidgetBase::ToggleFriends);
    if (MessengerToggleButton) MessengerToggleButton->OnClicked.AddUniqueDynamic(this, &UMessengerWidgetBase::ToggleMessenger);
    if (PopupLayer && !bPopupOpen) PopupLayer->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextMenuLayer) ContextMenuLayer->SetVisibility(ESlateVisibility::Collapsed);
    if (UServerSubsystem* Chat = GetServerSubsystem())
    {
        Chat->OnDirectMessageReceived.AddUniqueDynamic(this, &UMessengerWidgetBase::HandleDirectMessageReceived);
        Chat->OnDmHistoryReceived.AddUniqueDynamic(this, &UMessengerWidgetBase::HandleDmHistoryReceived);
        Chat->OnFriendUpdated.AddUniqueDynamic(this, &UMessengerWidgetBase::HandleFriendUpdated);
        Chat->OnFriendListReceived.AddUniqueDynamic(this, &UMessengerWidgetBase::HandleFriendList);
        Chat->OnChatStateChanged.AddUniqueDynamic(this, &UMessengerWidgetBase::HandleChatState);
        bSubscribed = true;
    }
    RefreshBadges();
}
// [MSGUI-022] 서버 구독과 타이머를 정리한다.
void UMessengerWidgetBase::NativeDestruct()
{
    if (UServerSubsystem* Chat = GetServerSubsystem())
    {
        Chat->OnDirectMessageReceived.RemoveAll(this);
        // 분리 중 완료되는 응답도 소비한다. 재부착 시 이미 끝난 요청을 기다리지 않게 한다.
        if (PendingHistory.IsEmpty()) Chat->OnDmHistoryReceived.RemoveAll(this);
        Chat->OnFriendUpdated.RemoveAll(this); Chat->OnFriendListReceived.RemoveAll(this);
        if (PendingHistory.IsEmpty()) Chat->OnChatStateChanged.RemoveAll(this);
    }
    bSubscribed = false; bWasPresented = false;
    Super::NativeDestruct();
}
// [MSGUI-023] 현재 대화 표시 상태의 전환을 확인한다.
void UMessengerWidgetBase::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    const bool bPresented = IsConversationPresented(ActivePeerUserId);
    if (bPresented && !bWasPresented) RequestHistory(ActivePeerUserId, 0);
    bWasPresented = bPresented;
}
// [MSGUI-024] ResolveNickname 동작을 처리하고 관련 위젯 상태를 갱신한다.
FString UMessengerWidgetBase::ResolveNickname(int64 UserId) const
{
    if (UServerSubsystem* Chat = GetServerSubsystem())
        for (const FMOUFriend& Friend : Chat->GetFriendsRef()) if (Friend.UserId == UserId) return Friend.Nickname;
    return TEXT("알 수 없음");
}
// [MSGUI-025] 대화와 탭을 생성하거나 기존 대화를 선택한다.
void UMessengerWidgetBase::OpenConversation(int64 PeerUserId)
{
    if (!PeerUserId || (!MessengerPanel && !ConversationArea)) return;
    UServerSubsystem* Chat = GetServerSubsystem(); if (!Chat) return;
    const FMOUFriend* Friend = Chat->GetFriendsRef().FindByPredicate([PeerUserId](const FMOUFriend& F) { return F.UserId == PeerUserId && F.State == EMOUFriendStateBP::Friend; });
    if (!Friend) return;
    if (Windows.Contains(PeerUserId)) { SelectConversation(PeerUserId); return; }
    while (WindowOrder.Num() >= FMath::Max(1, MaxOpenConversations)) CloseConversation(WindowOrder[0]);
    TSubclassOf<UDmWindowWidget> Class = DmWindowClass; if (!Class) Class = UDmWindowWidget::StaticClass();
    UDmWindowWidget* Window = CreateWidget<UDmWindowWidget>(GetOwningPlayer(), Class); if (!Window) return;
    Window->SetPeer(PeerUserId, Friend->Nickname);
    Window->OnCloseRequested.BindUObject(this, &UMessengerWidgetBase::HandleWindowCloseRequested);
    Window->OnHistoryRequested.BindUObject(this, &UMessengerWidgetBase::RequestHistory);
    if (const FString* Draft = Drafts.Find(PeerUserId)) Window->SetDraft(*Draft);
    Windows.Add(PeerUserId, Window); WindowOrder.Add(PeerUserId);
    if (MessengerPanel) MessengerPanel->AddConversation(PeerUserId, Friend->Nickname, Window);
    else ConversationArea->AddChildToHorizontalBox(Window)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SelectConversation(PeerUserId);
}
// [MSGUI-026] 선택한 대화만 표시하고 읽음·기록 요청을 시작한다.
void UMessengerWidgetBase::SelectConversation(int64 PeerUserId)
{
    if (!Windows.Contains(PeerUserId)) return;
    ActivePeerUserId = PeerUserId;
    WindowOrder.Remove(PeerUserId); WindowOrder.Add(PeerUserId);
    if (MessengerPanel) MessengerPanel->SelectConversation(PeerUserId);
    SetMessengerVisible(true);
    RequestHistory(PeerUserId, 0);
    bWasPresented = IsConversationPresented(PeerUserId);
}
// [MSGUI-027] 입력 초안을 보존하고 해당 대화와 탭을 제거한다.
void UMessengerWidgetBase::CloseConversation(int64 PeerUserId)
{
    if (auto* Window = Windows.Find(PeerUserId))
    {
        Drafts.Add(PeerUserId, (*Window)->GetDraft());
        if (MessengerPanel) MessengerPanel->RemoveConversation(PeerUserId);
        else (*Window)->RemoveFromParent();
    }
    Windows.Remove(PeerUserId); WindowOrder.Remove(PeerUserId);
    // 진행 중 요청은 남겨 둬야 닫은 창의 응답을 새 창의 응답으로 오인하지 않는다.
    if (auto* Pending = PendingHistory.Find(PeerUserId)) Pending->bQueuedLatest = false;
    if (ActivePeerUserId == PeerUserId)
    {
        ActivePeerUserId = WindowOrder.IsEmpty() ? 0 : WindowOrder.Last();
        if (MessengerPanel && ActivePeerUserId) MessengerPanel->SelectConversation(ActivePeerUserId);
        bWasPresented = false;
    }
}
// [MSGUI-028] 읽음 처리 가능한 실제 대화 표시 상태인지 확인한다.
bool UMessengerWidgetBase::IsConversationPresented(int64 PeerUserId) const
{
    if (!bSubscribed || !PeerUserId || bPopupOpen || !Windows.Contains(PeerUserId) || !MOUMessengerUI::IsPresented(this)) return false;
    if (MessengerPanel) return bMessengerVisible && ActivePeerUserId == PeerUserId && MOUMessengerUI::IsPresented(MessengerPanel);
    return MOUMessengerUI::IsPresented(Windows[PeerUserId]);
}
// [MSGUI-029] 같은 상대의 기록 요청을 직렬화하고 읽음 조건을 확인한다.
void UMessengerWidgetBase::RequestHistory(int64 PeerUserId, int64 BeforeMessageId)
{
    auto* Window = Windows.Find(PeerUserId);
    UServerSubsystem* Chat = GetServerSubsystem();
    if (!Window || !Chat || !Chat->GetLoginResult().bSuccess || !IsConversationPresented(PeerUserId)) return;
    if (FMessengerPendingHistory* Pending = PendingHistory.Find(PeerUserId))
    {
        if (BeforeMessageId == 0) Pending->bQueuedLatest = true;
        return;
    }
    FMessengerPendingHistory Request; Request.Owner = *Window; Request.bOlder = BeforeMessageId > 0;
    PendingHistory.Add(PeerUserId, Request); (*Window)->SetHistoryBusy(true);
    if (Request.bOlder) Chat->LoadOlderMessages(PeerUserId, BeforeMessageId);
    else { Chat->OpenConversation(PeerUserId); RefreshBadges(); }
}
// [MSGUI-030] 수신 메시지를 표시하고 현재 보고 있는 대화만 읽음 처리한다.
void UMessengerWidgetBase::HandleDirectMessageReceived(const FMOUDirectMessage& Message)
{
    if (auto* Window = Windows.Find(Message.PeerUserId)) (*Window)->AppendMessage(Message);
    if (!Message.bIsMine && IsConversationPresented(Message.PeerUserId)) RequestHistory(Message.PeerUserId, 0);
    RefreshBadges();
}
// [MSGUI-031] 요청 소유 대화에만 기록을 적용하고 보류 요청을 처리한다.
void UMessengerWidgetBase::HandleDmHistoryReceived(int64 PeerUserId, const TArray<FMOUDirectMessage>& Messages, bool bHasMore)
{
    FMessengerPendingHistory Request;
    if (!PendingHistory.RemoveAndCopyValue(PeerUserId, Request)) return;
    auto* Window = Windows.Find(PeerUserId);
    if (Window && Request.Owner.Get() == Window->Get()) (*Window)->SetHistory(Messages, bHasMore, Request.bOlder);
    if (Request.bQueuedLatest) RequestHistory(PeerUserId, 0);
    RefreshBadges();
    if (!bSubscribed && PendingHistory.IsEmpty())
        if (UServerSubsystem* Chat = GetServerSubsystem()) { Chat->OnDmHistoryReceived.RemoveAll(this); Chat->OnChatStateChanged.RemoveAll(this); }
}
// [MSGUI-032] 친구·탭·전체 미읽음 개수를 캐시에서 다시 표시한다.
void UMessengerWidgetBase::RefreshBadges()
{
    UServerSubsystem* Chat = GetServerSubsystem(); if (!Chat) return;
    if (MessengerPanel) MessengerPanel->RefreshFriends(Chat->GetFriendsRef());
    if (FriendList) FriendList->RefreshFromCache();
    if (TotalUnreadText)
    {
        const int32 Count = Chat->GetTotalUnreadCount();
        TotalUnreadText->SetText(FText::AsNumber(Count));
        TotalUnreadText->SetVisibility(Count > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
// [MSGUI-033] HandleFriendUpdated 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HandleFriendUpdated(const FMOUFriend& Friend, bool bRemoved)
{
    if (bRemoved) { CloseConversation(Friend.UserId); Drafts.Remove(Friend.UserId); CloseContextMenu(); }
    RefreshBadges();
}
// [MSGUI-034] HandleFriendList 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HandleFriendList(const TArray<FMOUFriend>& Friends)
{
    bWasPresented = false;
    RefreshBadges();
}
// [MSGUI-049] 연결 변경 시 만료된 요청을 비우고 재연결 후 활성 대화를 다시 확인한다.
void UMessengerWidgetBase::HandleChatState(EChatConnectionState State, const FString& Detail)
{
    if (State != EChatConnectionState::LoggedIn)
    {
        PendingHistory.Reset();
        for (const auto& Window : Windows) Window.Value->SetHistoryBusy(false);
    }
    bWasPresented = false;
    if (!bSubscribed && PendingHistory.IsEmpty())
        if (UServerSubsystem* Chat = GetServerSubsystem()) { Chat->OnDmHistoryReceived.RemoveAll(this); Chat->OnChatStateChanged.RemoveAll(this); }
}
// [MSGUI-035] HandleConversationRequested 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HandleConversationRequested(int64 PeerUserId) { CloseContextMenu(); OpenConversation(PeerUserId); }
// [MSGUI-036] HandleWindowCloseRequested 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HandleWindowCloseRequested(int64 PeerUserId) { CloseConversation(PeerUserId); }
// [MSGUI-037] SetMessengerVisible 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::SetMessengerVisible(bool bVisible)
{
    bMessengerVisible = bVisible;
    if (MessengerPanelSlot) MessengerPanelSlot->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (MessengerToggleButton) MessengerToggleButton->SetBackgroundColor(bVisible ? FLinearColor(0.25f, 0.8f, 1.f) : FLinearColor::White);
    if (!bVisible) bWasPresented = false;
}
// [MSGUI-038] ToggleMessenger 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ToggleMessenger() { SetMessengerVisible(!bMessengerVisible); }
// [MSGUI-039] HideMessenger 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HideMessenger() { SetMessengerVisible(false); }
// [MSGUI-040] ToggleFriends 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ToggleFriends()
{
    if (FriendPanelSlot) FriendPanelSlot->SetVisibility(FriendPanelSlot->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    CloseContextMenu();
}
// [MSGUI-041] HideFriends 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HideFriends() { if (FriendPanelSlot) FriendPanelSlot->SetVisibility(ESlateVisibility::Collapsed); CloseContextMenu(); }
// [MSGUI-042] ShowPopup 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ShowPopup(UUserWidget* Popup)
{
    if (!Popup || !PopupLayer) return;
    CloseContextMenu(); PopupLayer->ClearChildren();
    UButton* Blocker = MOUMessengerUI::Button(WidgetTree, NAME_None, TEXT(""));
    FSlateBrush Shade; Shade.DrawAs = ESlateBrushDrawType::Image; Shade.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
    FButtonStyle ShadeStyle; ShadeStyle.SetNormal(Shade).SetHovered(Shade).SetPressed(Shade);
    Blocker->SetStyle(ShadeStyle); Blocker->SetBackgroundColor(FLinearColor::White);
    Blocker->OnClicked.AddUniqueDynamic(this, &UMessengerWidgetBase::ClosePopups);
    UOverlaySlot* BlockerSlot = PopupLayer->AddChildToOverlay(Blocker);
    BlockerSlot->SetHorizontalAlignment(HAlign_Fill); BlockerSlot->SetVerticalAlignment(VAlign_Fill);
    UOverlaySlot* ChildSlot = PopupLayer->AddChildToOverlay(Popup); ChildSlot->SetHorizontalAlignment(HAlign_Center); ChildSlot->SetVerticalAlignment(VAlign_Center);
    PopupLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible); bPopupOpen = true; bWasPresented = false;
}
// [MSGUI-043] ShowAddFriendPopup 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ShowAddFriendPopup()
{
    if (!AddPopup)
    {
        TSubclassOf<UAddFriendPopupWidget> Class = AddFriendPopupClass; if (!Class) Class = UAddFriendPopupWidget::StaticClass();
        AddPopup = CreateWidget<UAddFriendPopupWidget>(GetOwningPlayer(), Class);
    }
    if (AddPopup) { AddPopup->OnCloseRequested.BindUObject(this, &UMessengerWidgetBase::ClosePopups); ShowPopup(AddPopup); AddPopup->RefreshList(); }
}
// [MSGUI-044] ShowRequestsPopup 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ShowRequestsPopup()
{
    if (!RequestsPopup)
    {
        TSubclassOf<UFriendRequestsPopupWidget> Class = RequestsPopupClass; if (!Class) Class = UFriendRequestsPopupWidget::StaticClass();
        RequestsPopup = CreateWidget<UFriendRequestsPopupWidget>(GetOwningPlayer(), Class);
    }
    if (RequestsPopup) { RequestsPopup->OnCloseRequested.BindUObject(this, &UMessengerWidgetBase::ClosePopups); ShowPopup(RequestsPopup); RequestsPopup->RefreshList(); }
}
// [MSGUI-045] ClosePopups 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ClosePopups()
{
    if (PopupLayer) { PopupLayer->ClearChildren(); PopupLayer->SetVisibility(ESlateVisibility::Collapsed); }
    bPopupOpen = false; bWasPresented = false;
}
// [MSGUI-046] ShowFriendContextMenu 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::ShowFriendContextMenu(int64 UserId)
{
    if (!ContextMenuLayer) return;
    CloseContextMenu();
    if (!FriendContextMenu)
    {
        TSubclassOf<UFriendContextMenuWidget> Class = ContextMenuClass; if (!Class) Class = UFriendContextMenuWidget::StaticClass();
        FriendContextMenu = CreateWidget<UFriendContextMenuWidget>(GetOwningPlayer(), Class);
    }
    if (!FriendContextMenu) return;
    FriendContextMenu->SetUser(UserId);
    FriendContextMenu->OnAction.BindUObject(this, &UMessengerWidgetBase::HandleContextAction);
    FriendContextMenu->OnCloseRequested.BindUObject(this, &UMessengerWidgetBase::CloseContextMenu);
    UButton* Blocker = MOUMessengerUI::Button(WidgetTree, NAME_None, TEXT("")); Blocker->SetRenderOpacity(0.f);
    Blocker->OnClicked.AddUniqueDynamic(this, &UMessengerWidgetBase::CloseContextMenu); UOverlaySlot* BlockerSlot = ContextMenuLayer->AddChildToOverlay(Blocker);
    BlockerSlot->SetHorizontalAlignment(HAlign_Fill); BlockerSlot->SetVerticalAlignment(VAlign_Fill);
    UOverlaySlot* ChildSlot = ContextMenuLayer->AddChildToOverlay(FriendContextMenu);
    ChildSlot->SetHorizontalAlignment(HAlign_Left); ChildSlot->SetVerticalAlignment(VAlign_Top);
    const FVector2D Size = GetCachedGeometry().GetLocalSize();
    const FVector2D Mouse = GetCachedGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
    ChildSlot->SetPadding(FMargin(FMath::Clamp(Mouse.X, 0.f, FMath::Max(0.f, Size.X-180.f)), FMath::Clamp(Mouse.Y, 0.f, FMath::Max(0.f, Size.Y-150.f)), 0.f, 0.f));
    ContextMenuLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
// [MSGUI-047] HandleContextAction 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::HandleContextAction(int64 UserId, EFriendEntryAction Action)
{
    CloseContextMenu();
    UServerSubsystem* Chat = GetServerSubsystem(); if (!Chat) return;
    const FMOUFriend* Friend = Chat->GetFriendsRef().FindByPredicate([UserId](const FMOUFriend& F) { return F.UserId == UserId && F.State == EMOUFriendStateBP::Friend; });
    if (!Friend) return;
    if (Action == EFriendEntryAction::Message) OpenConversation(UserId);
    else if (Action == EFriendEntryAction::Remove) Chat->RemoveFriend(UserId);
}
// [MSGUI-048] CloseContextMenu 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerWidgetBase::CloseContextMenu()
{
    if (ContextMenuLayer) { ContextMenuLayer->ClearChildren(); ContextMenuLayer->SetVisibility(ESlateVisibility::Collapsed); }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessengerWidgetRegressionTest, "MOU.Messenger.WidgetRegression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// [MSGUI-090] 서버에 접속하지 않는 별도 월드에서 탭·배지·기록·초안 및 실제 WBP 렌더링을 검증한다.
bool FMessengerWidgetRegressionTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    UWorld* World = GI->GetWorld();
    APlayerController* PC = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Test controller"), PC)) { GI->Shutdown(); return false; }
    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    LocalPlayer->PlayerController = PC;
    PC->Player = LocalPlayer;
    PC->SetAsLocalPlayerController();
    World->AddController(PC);
    UServerSubsystem* Chat = GI->GetSubsystem<UServerSubsystem>();
    FArrayProperty* CacheProperty = FindFProperty<FArrayProperty>(UServerSubsystem::StaticClass(), TEXT("CachedFriends"));
    if (!TestNotNull(TEXT("Friend cache property"), CacheProperty)) { GI->Shutdown(); return false; }
    TArray<FMOUFriend>& Friends = *CacheProperty->ContainerPtrToValuePtr<TArray<FMOUFriend>>(Chat);
    const TCHAR* Names[] = {TEXT("하늘"), TEXT("별빛"), TEXT("민준"), TEXT("지우"), TEXT("유나"), TEXT("서준"), TEXT("도윤"), TEXT("보낸 신청")};
    for (int32 Index=0; Index<8; ++Index)
    {
        FMOUFriend F; F.UserId=Index+1; F.Nickname=Names[Index];
        F.State=Index<4 ? EMOUFriendStateBP::Friend : Index<7 ? EMOUFriendStateBP::PendingIncoming : EMOUFriendStateBP::PendingOutgoing;
        F.Presence=Index==1 ? EMOUPresenceBP::InGame : Index<3 ? EMOUPresenceBP::Online : EMOUPresenceBP::Offline;
        F.bIsOnline=Index<3; F.UnreadCount=Index==2 ? 2 : 0; Friends.Add(F);
    }
    UClass* Class = LoadClass<UMessengerWidgetBase>(nullptr, TEXT("/Game/02_JSY/MainLobby/WBP_LobbyMessenger.WBP_LobbyMessenger_C"));
    if (!TestNotNull(TEXT("Messenger WBP class"), Class)) { GI->Shutdown(); return false; }
#if WITH_EDITOR
    FAssetCompilingManager::Get().FinishAllCompilation();
#endif
    // 메신저 단독 CDO 대신 실제 로비에 직렬화된 인스턴스 설정을 검증한다.
    UClass* LobbyClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/02_JSY/MainLobby/WBP_LobbyWidget.WBP_LobbyWidget_C"));
    UUserWidget* Lobby = CreateWidget<UUserWidget>(PC, LobbyClass);
    if (!TestNotNull(TEXT("Lobby instance"), Lobby)) { GI->Shutdown(); return false; }
    UMessengerWidgetBase* Root = Cast<UMessengerWidgetBase>(Lobby->GetWidgetFromName(TEXT("LobbyMessenger")));
    if (!TestNotNull(TEXT("Messenger instance"), Root)) { GI->Shutdown(); return false; }
    // 상위 로비 Slate도 생성해야 실제 표시 상태를 검사할 수 있다.
    if (FBoolProperty* Cursor = FindFProperty<FBoolProperty>(Lobby->GetClass(), TEXT("bManageMouseCursor"))) Cursor->SetPropertyValue_InContainer(Lobby, false);
    TSharedRef<SWidget> SlateLobby = Lobby->TakeWidget();
    TSharedRef<SWidget> SlateRoot = Root->TakeWidget();
    int32 MessengerInstances = 0, FriendPanelInstances = 0;
    for (TObjectIterator<UMessengerWidgetBase> It; It; ++It)
        if (!It->IsTemplate() && It->GetGameInstance() == GI) ++MessengerInstances;
    for (TObjectIterator<UFriendListWidgetBase> It; It; ++It)
        if (!It->IsTemplate() && It->GetGameInstance() == GI) ++FriendPanelInstances;
    TestEqual(TEXT("One messenger across entire game instance after lobby Construct"), MessengerInstances, 1);
    TestEqual(TEXT("No legacy friend panel spawned outside lobby tree"), FriendPanelInstances, 1);
    TestNotNull(TEXT("Friend panel binding"), Root->FriendList.Get());
    TestNotNull(TEXT("Tabbed panel binding"), Root->MessengerPanel.Get());
    if (!Root->FriendList || !Root->MessengerPanel) { GI->Shutdown(); return false; }
    TestEqual(TEXT("Embedded friend panel uses WBP"), Root->FriendList->GetClass()->GetPathName(), FString(TEXT("/Game/02_JSY/MainLobby/Messenger/WBP_FriendPanel.WBP_FriendPanel_C")));
    TestEqual(TEXT("Embedded messenger panel uses WBP"), Root->MessengerPanel->GetClass()->GetPathName(), FString(TEXT("/Game/02_JSY/MainLobby/Messenger/WBP_MessengerPanel.WBP_MessengerPanel_C")));
    TestEqual(TEXT("Exactly one friend panel"), Root->FriendPanelSlot->GetChildrenCount(), 1);
    TestFalse(TEXT("Friends initially hidden"), Root->FriendPanelSlot->IsVisible());
    Root->FriendToggleButton->OnClicked.Broadcast();
    TestTrue(TEXT("Friend launcher opens panel"), Root->FriendPanelSlot->IsVisible());
    Root->FriendToggleButton->OnClicked.Broadcast();
    TestFalse(TEXT("Friend launcher hides panel"), Root->FriendPanelSlot->IsVisible());
    Root->FriendToggleButton->OnClicked.Broadcast();
    TestEqual(TEXT("Only accepted friends in friend panel"), Root->FriendList->GetEntryCount(), 4);
    Root->OpenConversation(1); Root->OpenConversation(2); Root->SelectConversation(1);
    TestEqual(TEXT("Two unique conversations"), Root->GetOpenConversationCount(), 2);
    TestTrue(TEXT("Selected tab is presented"), Root->IsConversationPresented(1));
    TestFalse(TEXT("Inactive tab is not read"), Root->IsConversationPresented(2));
    Root->SetMessengerVisible(false);
    TestFalse(TEXT("Hidden panel is not read"), Root->IsConversationPresented(1));
    Root->SetMessengerVisible(true);
    Root->bPopupOpen=true; TestFalse(TEXT("Modal blocks read"), Root->IsConversationPresented(1)); Root->bPopupOpen=false;
    Root->SetVisibility(ESlateVisibility::Collapsed);
    TestFalse(TEXT("Hidden root is not read"), Root->IsConversationPresented(1)); Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UDmWindowWidget* Dm = Root->Windows[1];
    FMOUDirectMessage A; A.PeerUserId=1; A.MessageId=10; A.Text=TEXT("안녕! 지금 게임 가능해?"); A.Timestamp=1790658840;
    FMOUDirectMessage B=A; B.MessageId=11; B.Text=TEXT("방 만들어 둘게.");
    FMOUDirectMessage C=A; C.MessageId=12; C.bIsMine=true; C.Text=TEXT("응, 바로 들어갈게!");
    Dm->AppendMessage(C); Dm->SetHistory({A,B,C},true,false); Dm->AppendMessage(C);
    TestEqual(TEXT("Live/history overlap deduplicated"), Dm->LoadedMessages.Num(),3);
    TestEqual(TEXT("Merged messages chronological"), Dm->LoadedMessages[0].MessageId,int64(10));
    FMOUDirectMessage Older=A; Older.MessageId=9; Older.Text=TEXT("이전 대화");
    Dm->SetHistory({Older,A},false,true);
    TestEqual(TEXT("Older page merged once"), Dm->LoadedMessages.Num(),4);
    TestEqual(TEXT("Paging cursor tracks oldest"), Dm->OldestMessageId,int64(9));
    TestFalse(TEXT("No more history after last older page"),Dm->bHasMoreHistory);
    Dm->SetDraft(TEXT("작성 중인 메시지"));
    FMessengerPendingHistory Pending; Pending.Owner=Dm; Pending.bOlder=true; Root->PendingHistory.Add(1,Pending);
    Root->CloseConversation(1); Root->OpenConversation(1);
    TestEqual(TEXT("Draft survives close/reopen"),Root->Windows[1]->GetDraft(),FString(TEXT("작성 중인 메시지")));
    Root->HandleDmHistoryReceived(1,{Older},false);
    TestTrue(TEXT("Old window response ignored"),Root->Windows[1]->LoadedMessages.IsEmpty());
    TestFalse(TEXT("Old request drained"),Root->PendingHistory.Contains(1));
    Root->Windows[1]->SetDraft(TEXT("")); Root->Windows[1]->SetHistory({A,B,C},true,false);
    Root->OpenConversation(3); Root->SelectConversation(1); Root->RefreshBadges();

    FString PreviewDirectory;
    if (FParse::Value(FCommandLine::Get(),TEXT("MessengerPreviewDir="),PreviewDirectory))
    {
        FWidgetRenderer Renderer(false);
        auto SavePreview = [&](const TCHAR* Filename)
        {
#if WITH_EDITOR
            FAssetCompilingManager::Get().FinishAllCompilation();
#endif

            Root->ForceLayoutPrepass();
            for (int32 Frame = 0; Frame < 3; ++Frame) { Renderer.DrawWidget(SlateRoot,FVector2D(1024,640));  }
            UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateRoot,FVector2D(1024,640));
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags ReadFlags; ReadFlags.SetLinearToGamma(false);
            if (Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))
            {
                // 선형 렌더 타깃을 PNG의 sRGB 색 공간으로 한 번만 변환한다.
                for (FColor& Pixel : Pixels) Pixel = Pixel.ReinterpretAsLinear().ToFColorSRGB();
                TArray64<uint8> Bytes;
                FImageUtils::PNGCompressImageArray(1024,640,Pixels,Bytes);
                TestTrue(TEXT("Preview image saved"),FFileHelper::SaveArrayToFile(Bytes,*(PreviewDirectory/Filename)));
            }
            else AddError(TEXT("Widget preview rendering failed"));
        };
        SavePreview(TEXT("Messenger_실제_WBP.png"));
        Root->ShowRequestsPopup(); SavePreview(TEXT("Messenger_받은신청_WBP.png")); Root->ClosePopups();
        Root->ShowAddFriendPopup(); SavePreview(TEXT("Messenger_친구추가_WBP.png")); Root->ClosePopups();
    }
    Root->NativeDestruct(); Root->ReleaseSlateResources(true); Lobby->ReleaseSlateResources(true);
    GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
#endif
