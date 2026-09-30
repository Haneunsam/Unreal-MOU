#include "Server/Social/FriendPopupWidget.h"
#include "Server/ServerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

// [FRUI-101] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UFriendRequestEntryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(); WidgetTree->RootWidget = Root;
    RequestNameText = MOUMessengerUI::Text(WidgetTree, TEXT("RequestNameText"), TEXT(""));
    Root->AddChildToHorizontalBox(RequestNameText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AcceptButton = MOUMessengerUI::Button(WidgetTree, TEXT("AcceptButton"), TEXT("수락")); Root->AddChildToHorizontalBox(AcceptButton);
    DeclineButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DeclineButton"));
    DeclineLabel = MOUMessengerUI::Text(WidgetTree, TEXT("DeclineLabel"), TEXT("거절"));
    DeclineButton->AddChild(DeclineLabel); Root->AddChildToHorizontalBox(DeclineButton);
}
// [FRUI-102] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UFriendRequestEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (AcceptButton) AcceptButton->OnClicked.AddUniqueDynamic(this, &UFriendRequestEntryWidget::HandleAccept);
    if (DeclineButton) DeclineButton->OnClicked.AddUniqueDynamic(this, &UFriendRequestEntryWidget::HandleDecline);
    RefreshVisuals();
}
// [FRUI-103] SetRequest 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendRequestEntryWidget::SetRequest(const FMOUFriend& Friend) { Cached = Friend; RefreshVisuals(); }
// [FRUI-104] 저장된 데이터로 위젯 표시와 상태를 갱신한다.
void UFriendRequestEntryWidget::RefreshVisuals()
{
    if (RequestNameText) RequestNameText->SetText(FText::FromString(Cached.Nickname));
    const bool bIncoming = Cached.State == EMOUFriendStateBP::PendingIncoming;
    if (AcceptButton) AcceptButton->SetVisibility(bIncoming ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (DeclineLabel) DeclineLabel->SetText(FText::FromString(bIncoming ? TEXT("거절") : TEXT("취소")));
}
// [FRUI-105] HandleAccept 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendRequestEntryWidget::HandleAccept() { OnAction.ExecuteIfBound(Cached.UserId, EFriendEntryAction::Accept); }
// [FRUI-106] HandleDecline 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendRequestEntryWidget::HandleDecline()
{
    OnAction.ExecuteIfBound(Cached.UserId, Cached.State == EMOUFriendStateBP::PendingIncoming ? EFriendEntryAction::Decline : EFriendEntryAction::Remove);
}

// [FRUI-107] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UFriendRequestsPopupWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget) BuildDefaultLayout();
}
// [FRUI-108] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
void UFriendRequestsPopupWidget::BuildDefaultLayout()
{
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(489.f); Size->SetHeightOverride(280.f); WidgetTree->RootWidget = Size;
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(); Size->AddChild(Root);
    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(); Root->AddChildToVerticalBox(Header);
    Header->AddChildToHorizontalBox(MOUMessengerUI::Text(WidgetTree, NAME_None, TEXT("받은 친구 신청"), 22))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CloseButton = MOUMessengerUI::Button(WidgetTree, TEXT("CloseButton"), TEXT("×")); Header->AddChildToHorizontalBox(CloseButton);
    EmptyText = MOUMessengerUI::Text(WidgetTree, TEXT("EmptyText"), TEXT("받은 친구 신청이 없습니다.")); Root->AddChildToVerticalBox(EmptyText);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(); Root->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    RequestListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RequestListBox")); Scroll->AddChild(RequestListBox);
}
// [FRUI-109] GetServerSubsystem 동작을 처리하고 관련 위젯 상태를 갱신한다.
UServerSubsystem* UFriendRequestsPopupWidget::GetServerSubsystem() const
{
    const UGameInstance* GI = GetGameInstance(); return GI ? GI->GetSubsystem<UServerSubsystem>() : nullptr;
}
// [FRUI-110] GetRequestState 동작을 처리하고 관련 위젯 상태를 갱신한다.
EMOUFriendStateBP UFriendRequestsPopupWidget::GetRequestState() const { return EMOUFriendStateBP::PendingIncoming; }
// [FRUI-111] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UFriendRequestsPopupWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UFriendRequestsPopupWidget::HandleClose);
    if (UServerSubsystem* Chat = GetServerSubsystem())
    {
        Chat->OnFriendListReceived.AddUniqueDynamic(this, &UFriendRequestsPopupWidget::HandleList);
        Chat->OnFriendUpdated.AddUniqueDynamic(this, &UFriendRequestsPopupWidget::HandleUpdate);
    }
    RefreshList();
}
// [FRUI-112] 서버 구독과 타이머를 정리한다.
void UFriendRequestsPopupWidget::NativeDestruct()
{
    if (UServerSubsystem* Chat = GetServerSubsystem())
    {
        Chat->OnFriendListReceived.RemoveAll(this); Chat->OnFriendUpdated.RemoveAll(this);
    }
    Super::NativeDestruct();
}
// [FRUI-113] 서버 캐시에서 해당 신청 목록을 다시 구성한다.
void UFriendRequestsPopupWidget::RefreshList()
{
    if (!RequestListBox) return;
    RequestListBox->ClearChildren();
    UServerSubsystem* Chat = GetServerSubsystem();
    if (Chat)
    {
        TArray<FMOUFriend> Requests = Chat->GetFriends();
        Requests.Sort([](const FMOUFriend& A, const FMOUFriend& B) { return A.Nickname < B.Nickname; });
        TSubclassOf<UFriendRequestEntryWidget> Class = RequestEntryClass;
        if (!Class) Class = UFriendRequestEntryWidget::StaticClass();
        for (const FMOUFriend& Friend : Requests)
        {
            if (Friend.State != GetRequestState()) continue;
            UFriendRequestEntryWidget* Row = CreateWidget<UFriendRequestEntryWidget>(GetOwningPlayer(), Class);
            if (!Row) continue;
            Row->SetRequest(Friend);
            Row->OnAction.BindUObject(this, &UFriendRequestsPopupWidget::HandleRequestAction);
            RequestListBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
        }
    }
    if (EmptyText) EmptyText->SetVisibility(RequestListBox->GetChildrenCount() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
// [FRUI-114] 현재 신청 관계를 확인하고 수락·거절·취소 요청을 전달한다.
void UFriendRequestsPopupWidget::HandleRequestAction(int64 UserId, EFriendEntryAction Action)
{
    UServerSubsystem* Chat = GetServerSubsystem(); if (!Chat || !Chat->GetLoginResult().bSuccess) return;
    const FMOUFriend* Current = Chat->GetFriendsRef().FindByPredicate([UserId](const FMOUFriend& F) { return F.UserId == UserId; });
    if (!Current || Current->State != GetRequestState()) { RefreshList(); return; }
    if (Current->State == EMOUFriendStateBP::PendingIncoming)
    {
        if (Action == EFriendEntryAction::Accept) Chat->AcceptFriendRequest(UserId);
        else if (Action == EFriendEntryAction::Decline) Chat->DeclineFriendRequest(UserId);
    }
    else if (Current->State == EMOUFriendStateBP::PendingOutgoing && Action == EFriendEntryAction::Remove) Chat->RemoveFriend(UserId);
}
// [FRUI-115] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendRequestsPopupWidget::HandleClose() { OnCloseRequested.ExecuteIfBound(); }
// [FRUI-116] HandleList 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendRequestsPopupWidget::HandleList(const TArray<FMOUFriend>& Friends) { RefreshList(); }
// [FRUI-117] HandleUpdate 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendRequestsPopupWidget::HandleUpdate(const FMOUFriend& Friend, bool bRemoved) { RefreshList(); }

// [FRUI-118] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
void UAddFriendPopupWidget::BuildDefaultLayout()
{
    Super::BuildDefaultLayout();
    UVerticalBox* Root = Cast<UVerticalBox>(Cast<USizeBox>(WidgetTree->RootWidget)->GetChildAt(0));
    NameInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("NameInputBox"));
    NameInputBox->SetHintText(FText::FromString(TEXT("닉네임을 입력하세요"))); Root->AddChildToVerticalBox(NameInputBox);
    SubmitRequestButton = MOUMessengerUI::Button(WidgetTree, TEXT("SubmitRequestButton"), TEXT("친구 신청")); Root->AddChildToVerticalBox(SubmitRequestButton);
    ResultText = MOUMessengerUI::Text(WidgetTree, TEXT("ResultText"), TEXT("")); Root->AddChildToVerticalBox(ResultText);
    if (EmptyText) EmptyText->SetText(FText::FromString(TEXT("보낸 친구 신청이 없습니다.")));
}
// [FRUI-119] GetRequestState 동작을 처리하고 관련 위젯 상태를 갱신한다.
EMOUFriendStateBP UAddFriendPopupWidget::GetRequestState() const { return EMOUFriendStateBP::PendingOutgoing; }
// [FRUI-120] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UAddFriendPopupWidget::NativeConstruct()
{
    if (OutgoingRequestBox) RequestListBox = OutgoingRequestBox;
    Super::NativeConstruct();
    if (SubmitRequestButton) SubmitRequestButton->OnClicked.AddUniqueDynamic(this, &UAddFriendPopupWidget::HandleSubmit);
    if (NameInputBox) NameInputBox->OnTextCommitted.AddUniqueDynamic(this, &UAddFriendPopupWidget::HandleCommitted);
    if (UServerSubsystem* Chat = GetServerSubsystem()) Chat->OnFriendAddCompleted.AddUniqueDynamic(this, &UAddFriendPopupWidget::HandleResult);
}
// [FRUI-121] 서버 구독과 타이머를 정리한다.
void UAddFriendPopupWidget::NativeDestruct()
{
    if (UServerSubsystem* Chat = GetServerSubsystem()) Chat->OnFriendAddCompleted.RemoveAll(this);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SubmitTimer);
    bSubmitting = false;
    if (SubmitRequestButton) SubmitRequestButton->SetIsEnabled(true);
    Super::NativeDestruct();
}
// [FRUI-122] 닉네임과 연결 상태를 검사한 뒤 친구 신청을 전송한다.
void UAddFriendPopupWidget::HandleSubmit()
{
    if (bSubmitting || !NameInputBox) return;
    const FString Query = NameInputBox->GetText().ToString().TrimStartAndEnd();
    UServerSubsystem* Chat = GetServerSubsystem();
    if (Query.IsEmpty() || !Chat || !Chat->GetLoginResult().bSuccess)
    {
        if (ResultText) ResultText->SetText(FText::FromString(Query.IsEmpty() ? TEXT("닉네임을 입력하세요.") : TEXT("로그인이 필요합니다.")));
        return;
    }
    bSubmitting = true;
    if (SubmitRequestButton) SubmitRequestButton->SetIsEnabled(false);
    if (ResultText) ResultText->SetText(FText::FromString(TEXT("신청 중...")));
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(SubmitTimer, this, &UAddFriendPopupWidget::HandleTimeout, 10.f, false);
    Chat->AddFriend(Query);
}
// [FRUI-123] 친구 신청 결과를 표시하고 입력·목록을 갱신한다.
void UAddFriendPopupWidget::HandleResult(bool bSuccess, EMOUFriendResultBP Result)
{
    if (!bSubmitting) return;
    bSubmitting = false;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SubmitTimer);
    if (SubmitRequestButton) SubmitRequestButton->SetIsEnabled(true);
    FString Message = TEXT("친구 신청에 실패했습니다.");
    if (bSuccess) Message = TEXT("친구 신청을 보냈습니다.");
    else switch (Result)
    {
        case EMOUFriendResultBP::NotAuthed: Message = TEXT("로그인이 필요합니다."); break;
        case EMOUFriendResultBP::NotFound: Message = TEXT("해당 닉네임을 찾을 수 없습니다."); break;
        case EMOUFriendResultBP::AmbiguousName: Message = TEXT("같은 닉네임이 여러 명입니다."); break;
        case EMOUFriendResultBP::AlreadyFriend: Message = TEXT("이미 친구입니다."); break;
        case EMOUFriendResultBP::AlreadyPending: Message = TEXT("이미 신청 중입니다."); break;
        case EMOUFriendResultBP::SelfRequest: Message = TEXT("자신에게 신청할 수 없습니다."); break;
        case EMOUFriendResultBP::LimitReached: Message = TEXT("친구 수가 가득 찼습니다."); break;
        case EMOUFriendResultBP::InvalidFormat: Message = TEXT("닉네임 형식을 확인하세요."); break;
        default: break;
    }
    if (ResultText) ResultText->SetText(FText::FromString(Message));
    if (bSuccess && NameInputBox) NameInputBox->SetText(FText::GetEmpty());
    RefreshList();
}
// [FRUI-124] HandleCommitted 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UAddFriendPopupWidget::HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter) HandleSubmit();
}
// [FRUI-125] HandleTimeout 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UAddFriendPopupWidget::HandleTimeout()
{
    bSubmitting = false;
    if (SubmitRequestButton) SubmitRequestButton->SetIsEnabled(true);
    if (ResultText) ResultText->SetText(FText::FromString(TEXT("응답을 확인하지 못했습니다. 신청 목록과 연결 상태를 확인하세요.")));
}

// [FRUI-126] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UFriendContextMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget = Root;
    MessageButton = MOUMessengerUI::Button(WidgetTree, TEXT("MessageButton"), TEXT("메시지")); Root->AddChildToVerticalBox(MessageButton);
    DeleteFriendButton = MOUMessengerUI::Button(WidgetTree, TEXT("DeleteFriendButton"), TEXT("친구 삭제")); Root->AddChildToVerticalBox(DeleteFriendButton);
    CloseButton = MOUMessengerUI::Button(WidgetTree, TEXT("CloseButton"), TEXT("닫기")); Root->AddChildToVerticalBox(CloseButton);
}
// [FRUI-127] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UFriendContextMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (MessageButton) MessageButton->OnClicked.AddUniqueDynamic(this, &UFriendContextMenuWidget::HandleMessage);
    if (DeleteFriendButton) DeleteFriendButton->OnClicked.AddUniqueDynamic(this, &UFriendContextMenuWidget::HandleDelete);
    if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UFriendContextMenuWidget::HandleClose);
}
// [FRUI-128] SetUser 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendContextMenuWidget::SetUser(int64 UserId) { SelectedUserId = UserId; }
// [FRUI-129] HandleMessage 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendContextMenuWidget::HandleMessage() { OnAction.ExecuteIfBound(SelectedUserId, EFriendEntryAction::Message); }
// [FRUI-130] HandleDelete 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendContextMenuWidget::HandleDelete() { OnAction.ExecuteIfBound(SelectedUserId, EFriendEntryAction::Remove); }
// [FRUI-131] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UFriendContextMenuWidget::HandleClose() { OnCloseRequested.ExecuteIfBound(); }
