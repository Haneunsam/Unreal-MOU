#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Server/Social/FriendTypes.h"
#include "MessengerPartsWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UHorizontalBox;
class UWidgetSwitcher;
class UTexture2D;
class UDmWindowWidget;
class UWidgetTree;

DECLARE_DELEGATE_OneParam(FOnMessengerPeerAction, int64);
DECLARE_DELEGATE(FOnMessengerPanelAction);

// 기본 레이아웃은 에셋 없이도 동작하며 WBP가 같은 바인딩 이름으로 교체할 수 있다.
namespace MOUMessengerUI
{
    // [MSGUI-101] Text 동작을 처리하고 관련 위젯 상태를 갱신한다.
    TEAMPROJECT_MOU_API UTextBlock* Text(UWidgetTree* Tree, FName Name, const FString& Value, int32 Size = 16);
    // [MSGUI-102] Button 동작을 처리하고 관련 위젯 상태를 갱신한다.
    TEAMPROJECT_MOU_API UButton* Button(UWidgetTree* Tree, FName Name, const FString& Label);
    // [MSGUI-103] 위젯의 상위 트리와 활성 페이지까지 표시 여부를 확인한다.
    TEAMPROJECT_MOU_API bool IsPresented(const UWidget* Widget);
}

UCLASS()
class TEAMPROJECT_MOU_API UConversationTabWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // [MSGUI-104] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [MSGUI-105] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [MSGUI-106] NativeOnMouseButtonDown 동작을 처리하고 관련 위젯 상태를 갱신한다.
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    // [MSGUI-107] 상대 계정과 표시 이름을 설정한다.
    void SetPeer(int64 UserId, const FString& Name);
    // [MSGUI-108] SetSelected 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void SetSelected(bool bSelected);
    // [MSGUI-109] SetUnread 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void SetUnread(int32 Count);
    FOnMessengerPeerAction OnSelected;
    FOnMessengerPeerAction OnCloseRequested;
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> SelectButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> CloseTabButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> NicknameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> UnreadText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> TabBackground;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TObjectPtr<UTexture2D> SelectedTexture;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TObjectPtr<UTexture2D> DefaultTexture;
private:
    // [MSGUI-111] HandleSelected 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleSelected();
    // [MSGUI-112] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleClose();
    // [MSGUI-110] 저장된 데이터로 위젯 표시와 상태를 갱신한다.
    void RefreshVisuals();
    int64 PeerUserId = 0;
    FString Nickname;
    int32 UnreadCount = 0;
    bool bIsSelected = false;
};

UCLASS()
class TEAMPROJECT_MOU_API UDmMessageBubbleWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // [MSGUI-113] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [MSGUI-114] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [MSGUI-115] 메시지 내용을 저장하고 표시를 갱신한다.
    void SetMessage(const FMOUDirectMessage& Message);
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> BubbleBackground;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TimeText;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TObjectPtr<UTexture2D> IncomingTexture;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TObjectPtr<UTexture2D> OutgoingTexture;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger", meta=(ClampMin="80")) float WrapWidth = 310.f;
private:
    // [MSGUI-116] 저장된 데이터로 위젯 표시와 상태를 갱신한다.
    void RefreshVisuals();
    FMOUDirectMessage CachedMessage;
};

UCLASS()
class TEAMPROJECT_MOU_API UMessengerPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // [MSGUI-117] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [MSGUI-118] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [MSGUI-119] AddConversation 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void AddConversation(int64 PeerUserId, const FString& Nickname, UDmWindowWidget* Window);
    // [MSGUI-120] 선택한 대화만 표시하고 읽음·기록 요청을 시작한다.
    void SelectConversation(int64 PeerUserId);
    // [MSGUI-121] RemoveConversation 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void RemoveConversation(int64 PeerUserId);
    // [MSGUI-122] RefreshFriends 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void RefreshFriends(const TArray<FMOUFriend>& Friends);
    FOnMessengerPeerAction OnSelected;
    FOnMessengerPeerAction OnConversationCloseRequested;
    FOnMessengerPanelAction OnCloseRequested;
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UHorizontalBox> ConversationTabBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> ConversationSwitcher;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> PanelCloseButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyConversationText;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TSubclassOf<UConversationTabWidget> TabWidgetClass;
private:
    // [MSGUI-123] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleClose();
    // [MSGUI-124] HandleSelected 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void HandleSelected(int64 UserId);
    // [MSGUI-125] HandleTabClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void HandleTabClose(int64 UserId);
    UPROPERTY() TMap<int64, TObjectPtr<UConversationTabWidget>> Tabs;
    UPROPERTY() TMap<int64, TObjectPtr<UDmWindowWidget>> Conversations;
};
