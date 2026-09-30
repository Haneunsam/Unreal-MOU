#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Server/Social/FriendListWidgetBase.h"
#include "Server/Chat/MessengerPartsWidget.h"
#include "FriendPopupWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UEditableTextBox;
class UServerSubsystem;

UCLASS()
class TEAMPROJECT_MOU_API UFriendRequestEntryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // [FRUI-101] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [FRUI-102] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [FRUI-103] SetRequest 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void SetRequest(const FMOUFriend& Friend);
    FOnFriendEntryAction OnAction;
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RequestNameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> AcceptButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> DeclineButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DeclineLabel;
private:
    // [FRUI-104] 저장된 데이터로 위젯 표시와 상태를 갱신한다.
    void RefreshVisuals();
    // [FRUI-105] HandleAccept 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleAccept();
    // [FRUI-106] HandleDecline 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleDecline();
    FMOUFriend Cached;
};

UCLASS()
class TEAMPROJECT_MOU_API UFriendRequestsPopupWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // [FRUI-107] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [FRUI-111] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [FRUI-112] 서버 구독과 타이머를 정리한다.
    virtual void NativeDestruct() override;
    // [FRUI-113] 서버 캐시에서 해당 신청 목록을 다시 구성한다.
    void RefreshList();
    FOnMessengerPanelAction OnCloseRequested;
protected:
    // [FRUI-108] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
    virtual void BuildDefaultLayout();
    // [FRUI-109] GetServerSubsystem 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UServerSubsystem* GetServerSubsystem() const;
    // [FRUI-110] GetRequestState 동작을 처리하고 관련 위젯 상태를 갱신한다.
    virtual EMOUFriendStateBP GetRequestState() const;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> RequestListBox;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
    UPROPERTY(EditAnywhere, Category="MOU|Friend") TSubclassOf<UFriendRequestEntryWidget> RequestEntryClass;
    // [FRUI-115] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleClose();
private:
    // [FRUI-116] HandleList 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleList(const TArray<FMOUFriend>& Friends);
    // [FRUI-117] HandleUpdate 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleUpdate(const FMOUFriend& Friend, bool bRemoved);
    // [FRUI-114] 현재 신청 관계를 확인하고 수락·거절·취소 요청을 전달한다.
    void HandleRequestAction(int64 UserId, EFriendEntryAction Action);
};

UCLASS()
class TEAMPROJECT_MOU_API UAddFriendPopupWidget : public UFriendRequestsPopupWidget
{
    GENERATED_BODY()
public:
    // [FRUI-120] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [FRUI-121] 서버 구독과 타이머를 정리한다.
    virtual void NativeDestruct() override;
protected:
    // [FRUI-118] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
    virtual void BuildDefaultLayout() override;
    // [FRUI-119] GetRequestState 동작을 처리하고 관련 위젯 상태를 갱신한다.
    virtual EMOUFriendStateBP GetRequestState() const override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> NameInputBox;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> SubmitRequestButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ResultText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> OutgoingRequestBox;
private:
    // [FRUI-122] 닉네임과 연결 상태를 검사한 뒤 친구 신청을 전송한다.
    UFUNCTION() void HandleSubmit();
    // [FRUI-123] 친구 신청 결과를 표시하고 입력·목록을 갱신한다.
    UFUNCTION() void HandleResult(bool bSuccess, EMOUFriendResultBP Result);
    // [FRUI-124] HandleCommitted 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    // [FRUI-125] HandleTimeout 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void HandleTimeout();
    bool bSubmitting = false;
    FTimerHandle SubmitTimer;
};

UCLASS()
class TEAMPROJECT_MOU_API UFriendContextMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // [FRUI-126] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [FRUI-127] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [FRUI-128] SetUser 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void SetUser(int64 UserId);
    FOnFriendEntryAction OnAction;
    FOnMessengerPanelAction OnCloseRequested;
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> MessageButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> DeleteFriendButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
private:
    // [FRUI-129] HandleMessage 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleMessage();
    // [FRUI-130] HandleDelete 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleDelete();
    // [FRUI-131] HandleClose 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleClose();
    int64 SelectedUserId = 0;
};
