#include "Server/Chat/MessengerPartsWidget.h"
#include "Server/Chat/MessengerWidgetBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

// [MSGUI-101] Text 동작을 처리하고 관련 위젯 상태를 갱신한다.
UTextBlock* MOUMessengerUI::Text(UWidgetTree* Tree, FName Name, const FString& Value, int32 Size)
{
    UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Result->SetText(FText::FromString(Value));
    FSlateFontInfo Font = Result->GetFont();
    Font.Size = Size;
    Result->SetFont(Font);
    Result->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.93f, 1.f)));
    Result->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Result;
}

// [MSGUI-102] Button 동작을 처리하고 관련 위젯 상태를 갱신한다.
UButton* MOUMessengerUI::Button(UWidgetTree* Tree, FName Name, const FString& Label)
{
    UButton* Result = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Result->SetBackgroundColor(FLinearColor(0.03f, 0.2f, 0.32f));
    Result->AddChild(Text(Tree, NAME_None, Label));
    return Result;
}

// [MSGUI-103] 위젯의 상위 트리와 활성 페이지까지 표시 여부를 확인한다.
bool MOUMessengerUI::IsPresented(const UWidget* Widget)
{
    // UUserWidget 내부 루트에서는 GetParent가 끊기므로 소유 WidgetTree도 따라간다.
    TSet<const UWidget*> Visited;
    while (Widget && !Visited.Contains(Widget))
    {
        Visited.Add(Widget);
        if (!Widget->IsVisible()) return false;
        if (const UPanelWidget* Parent = Widget->GetParent())
        {
            if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Parent))
            {
                if (Switcher->GetActiveWidget() != Widget) return false;
            }
            Widget = Parent;
        }
        else if (const UWidgetTree* Tree = Cast<UWidgetTree>(Widget->GetOuter()))
        {
            Widget = Cast<UUserWidget>(Tree->GetOuter());
        }
        else
        {
            return true;
        }
    }
    return true;
}

// [MSGUI-104] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UConversationTabWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    WidgetTree->RootWidget = Row;
    SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectButton"));
    NicknameText = MOUMessengerUI::Text(WidgetTree, TEXT("NicknameText"), TEXT("대화"));
    SelectButton->AddChild(NicknameText);
    Row->AddChildToHorizontalBox(SelectButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UnreadText = MOUMessengerUI::Text(WidgetTree, TEXT("UnreadText"), TEXT(""));
    Row->AddChildToHorizontalBox(UnreadText);
    CloseTabButton = MOUMessengerUI::Button(WidgetTree, TEXT("CloseTabButton"), TEXT("×"));
    Row->AddChildToHorizontalBox(CloseTabButton);
}

// [MSGUI-105] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UConversationTabWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SelectButton) SelectButton->OnClicked.AddUniqueDynamic(this, &UConversationTabWidget::HandleSelected);
    if (CloseTabButton) CloseTabButton->OnClicked.AddUniqueDynamic(this, &UConversationTabWidget::HandleClose);
    RefreshVisuals();
}

// [MSGUI-106] NativeOnMouseButtonDown 동작을 처리하고 관련 위젯 상태를 갱신한다.
FReply UConversationTabWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    // 우클릭은 닫기 컨트롤을 노출하며 바로 대화를 지우지 않는다.
    if (Event.GetEffectingButton() == EKeys::RightMouseButton && CloseTabButton)
    {
        CloseTabButton->SetVisibility(ESlateVisibility::Visible);
        CloseTabButton->SetToolTipText(FText::FromString(TEXT("대화 닫기")));
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}

// [MSGUI-107] 상대 계정과 표시 이름을 설정한다.
void UConversationTabWidget::SetPeer(int64 UserId, const FString& Name)
{
    PeerUserId = UserId;
    Nickname = Name;
    RefreshVisuals();
}
// [MSGUI-108] SetSelected 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UConversationTabWidget::SetSelected(bool bSelected)
{
    bIsSelected = bSelected;
    RefreshVisuals();
}
// [MSGUI-109] SetUnread 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UConversationTabWidget::SetUnread(int32 Count)
{
    UnreadCount = Count;
    RefreshVisuals();
}
// [MSGUI-110] 저장된 데이터로 위젯 표시와 상태를 갱신한다.
void UConversationTabWidget::RefreshVisuals()
{
    if (NicknameText) { NicknameText->SetText(FText::FromString(Nickname)); NicknameText->SetToolTipText(FText::FromString(Nickname)); }
    if (UnreadText)
    {
        UnreadText->SetText(FText::AsNumber(UnreadCount));
        UnreadText->SetVisibility(UnreadCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    UTexture2D* Texture = bIsSelected ? SelectedTexture.Get() : DefaultTexture.Get();
    if (TabBackground && Texture) TabBackground->SetBrushFromTexture(Texture);
    if (SelectButton) SelectButton->SetBackgroundColor(bIsSelected ? FLinearColor(0.1f, 0.6f, 0.9f) : FLinearColor::White);
}
// [MSGUI-111] HandleSelected 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UConversationTabWidget::HandleSelected() { OnSelected.ExecuteIfBound(PeerUserId); }
// [MSGUI-112] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UConversationTabWidget::HandleClose() { OnCloseRequested.ExecuteIfBound(PeerUserId); }

// [MSGUI-113] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UDmMessageBubbleWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    BubbleBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BubbleBackground"));
    BubbleBackground->SetColorAndOpacity(FLinearColor(0.02f, 0.13f, 0.22f));
    Root->AddChildToOverlay(BubbleBackground);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Root->AddChildToOverlay(Content)->SetPadding(FMargin(14.f, 8.f));
    MessageText = MOUMessengerUI::Text(WidgetTree, TEXT("MessageText"), TEXT(""));
    MessageText->SetAutoWrapText(true);
    Content->AddChildToVerticalBox(MessageText);
    TimeText = MOUMessengerUI::Text(WidgetTree, TEXT("TimeText"), TEXT(""), 10);
    Content->AddChildToVerticalBox(TimeText);
}
// [MSGUI-114] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UDmMessageBubbleWidget::NativeConstruct() { Super::NativeConstruct(); RefreshVisuals(); }
// [MSGUI-115] 메시지 내용을 저장하고 표시를 갱신한다.
void UDmMessageBubbleWidget::SetMessage(const FMOUDirectMessage& Message) { CachedMessage = Message; RefreshVisuals(); }
// [MSGUI-116] 저장된 데이터로 위젯 표시와 상태를 갱신한다.
void UDmMessageBubbleWidget::RefreshVisuals()
{
    if (MessageText)
    {
        MessageText->SetText(FText::FromString(CachedMessage.Text));
        MessageText->SetWrapTextAt(WrapWidth);
        MessageText->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    }
    if (TimeText) TimeText->SetText(CachedMessage.Timestamp > 0
        ? FText::AsTime(FDateTime::FromUnixTimestamp(CachedMessage.Timestamp), EDateTimeStyle::Short)
        : FText::GetEmpty());
    if (BubbleBackground)
    {
        UTexture2D* Texture = CachedMessage.bIsMine ? OutgoingTexture.Get() : IncomingTexture.Get();
        if (Texture) { BubbleBackground->SetBrushFromTexture(Texture); BubbleBackground->SetColorAndOpacity(FLinearColor::White); }
    }
}

// [MSGUI-117] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
void UMessengerPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(480.f); Size->SetHeightOverride(390.f);
    WidgetTree->RootWidget = Size;
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(); Size->AddChild(Root);
    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Header->AddChildToHorizontalBox(MOUMessengerUI::Text(WidgetTree, NAME_None, TEXT("메신저"), 22))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    PanelCloseButton = MOUMessengerUI::Button(WidgetTree, TEXT("PanelCloseButton"), TEXT("×")); Header->AddChildToHorizontalBox(PanelCloseButton);
    Root->AddChildToVerticalBox(Header);
    ConversationTabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConversationTabBar"));
    Root->AddChildToVerticalBox(ConversationTabBar);
    EmptyConversationText = MOUMessengerUI::Text(WidgetTree, TEXT("EmptyConversationText"), TEXT("친구를 선택해 대화를 시작하세요."));
    Root->AddChildToVerticalBox(EmptyConversationText);
    ConversationSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("ConversationSwitcher"));
    Root->AddChildToVerticalBox(ConversationSwitcher)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}
// [MSGUI-118] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
void UMessengerPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (PanelCloseButton) PanelCloseButton->OnClicked.AddUniqueDynamic(this, &UMessengerPanelWidget::HandleClose);
}
// [MSGUI-119] AddConversation 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerPanelWidget::AddConversation(int64 PeerUserId, const FString& Nickname, UDmWindowWidget* Window)
{
    if (!Window || !ConversationSwitcher || !ConversationTabBar || Conversations.Contains(PeerUserId)) return;
    TSubclassOf<UConversationTabWidget> Class = TabWidgetClass;
    if (!Class) Class = UConversationTabWidget::StaticClass();
    UConversationTabWidget* Tab = CreateWidget<UConversationTabWidget>(GetOwningPlayer(), Class);
    if (!Tab) return;
    Tab->SetPeer(PeerUserId, Nickname);
    Tab->OnSelected.BindUObject(this, &UMessengerPanelWidget::HandleSelected);
    Tab->OnCloseRequested.BindUObject(this, &UMessengerPanelWidget::HandleTabClose);
    ConversationTabBar->AddChildToHorizontalBox(Tab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ConversationSwitcher->AddChild(Window);
    Tabs.Add(PeerUserId, Tab); Conversations.Add(PeerUserId, Window);
    if (EmptyConversationText) EmptyConversationText->SetVisibility(ESlateVisibility::Collapsed);
}
// [MSGUI-120] 선택한 대화만 표시하고 읽음·기록 요청을 시작한다.
void UMessengerPanelWidget::SelectConversation(int64 PeerUserId)
{
    if (const auto* Window = Conversations.Find(PeerUserId))
        if (ConversationSwitcher) ConversationSwitcher->SetActiveWidget(Window->Get());
    for (const auto& Tab : Tabs) Tab.Value->SetSelected(Tab.Key == PeerUserId);
}
// [MSGUI-121] RemoveConversation 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerPanelWidget::RemoveConversation(int64 PeerUserId)
{
    if (auto* Tab = Tabs.Find(PeerUserId)) (*Tab)->RemoveFromParent();
    if (auto* Window = Conversations.Find(PeerUserId)) (*Window)->RemoveFromParent();
    Tabs.Remove(PeerUserId); Conversations.Remove(PeerUserId);
    if (EmptyConversationText) EmptyConversationText->SetVisibility(Conversations.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
// [MSGUI-122] RefreshFriends 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerPanelWidget::RefreshFriends(const TArray<FMOUFriend>& Friends)
{
    for (const FMOUFriend& Friend : Friends)
        if (auto* Tab = Tabs.Find(Friend.UserId)) { (*Tab)->SetPeer(Friend.UserId, Friend.Nickname); (*Tab)->SetUnread(Friend.UnreadCount); }
}
// [MSGUI-123] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerPanelWidget::HandleClose() { OnCloseRequested.ExecuteIfBound(); }
// [MSGUI-124] HandleSelected 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerPanelWidget::HandleSelected(int64 UserId) { OnSelected.ExecuteIfBound(UserId); }
// [MSGUI-125] HandleTabClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
void UMessengerPanelWidget::HandleTabClose(int64 UserId) { OnConversationCloseRequested.ExecuteIfBound(UserId); }
