// 메신저: 최상위 창 관리, 탭 패널, 친구 목록, 독립 팝업. 기존 ConversationArea WBP도 지원.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Server/Social/FriendTypes.h"
#include "Server/Chat/ChatTypes.h"
#include "MessengerWidgetBase.generated.h"

class UButton;
class UServerSubsystem;
class UDmWindowWidget;
class UEditableTextBox;
class UFriendListWidgetBase;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UOverlay;
class UMessengerPanelWidget;
class UDmMessageBubbleWidget;
class UAddFriendPopupWidget;
class UFriendRequestsPopupWidget;
class UFriendContextMenuWidget;
enum class EFriendEntryAction : uint8;

DECLARE_DELEGATE_TwoParams(FOnDmHistoryRequested, int64, int64);

struct FMessengerPendingHistory
{
    TWeakObjectPtr<UDmWindowWidget> Owner;
    bool bOlder = false;
    bool bQueuedLatest = false;
};

/** 대화창이 스스로 닫히겠다고 알린다. 부모가 목록에서 지운다. */
DECLARE_DELEGATE_OneParam(FOnDmWindowCloseRequested, int64 /*PeerUserId*/);

/**
 * 한 사람과의 대화창.
 *
 * ★ 자기 수명을 스스로 정하지 않는다. 닫기 버튼은 부모에게 알리기만 하고,
 *   실제로 지우는 것은 부모다 - 창이 스스로를 파괴하면 부모의 TMap 에
 *   죽은 포인터가 남는다.
 */
UCLASS()
class TEAMPROJECT_MOU_API UDmWindowWidget : public UUserWidget
{
	GENERATED_BODY()
    friend class FMessengerWidgetRegressionTest;

public:
    // [MSGUI-001] UDmWindowWidget 동작을 처리하고 관련 위젯 상태를 갱신한다.
	UDmWindowWidget(const FObjectInitializer& ObjectInitializer);

    // [MSGUI-002] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
	virtual void NativeOnInitialized() override;
    // [MSGUI-004] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
	virtual void NativeConstruct() override;

	/** 이 창이 누구와의 대화인지 정한다. 창을 만든 직후 한 번 부른다. */
    // [MSGUI-006] 상대 계정과 표시 이름을 설정한다.
	void SetPeer(int64 InPeerUserId, const FString& InPeerNickname);

	int64 GetPeerUserId() const { return PeerUserId; }

	/** 메시지 한 통을 아래에 붙인다. */
    // [MSGUI-008] AppendMessage 동작을 처리하고 관련 위젯 상태를 갱신한다.
	void AppendMessage(const FMOUDirectMessage& Message);

	/** 부모가 직렬화한 요청 종류를 받아 기록을 병합한다. */
    // [MSGUI-009] 기록과 라이브 메시지를 서버 ID로 병합해 표시한다.
	void SetHistory(const TArray<FMOUDirectMessage>& Messages, bool bHasMore, bool bOlder = false);
    // [MSGUI-010] SetHistoryBusy 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void SetHistoryBusy(bool bBusy);
    // [MSGUI-011] GetDraft 동작을 처리하고 관련 위젯 상태를 갱신한다.
    FString GetDraft() const;
    // [MSGUI-012] SetDraft 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void SetDraft(const FString& Draft);
    FOnDmHistoryRequested OnHistoryRequested;

	/** 닫기 요청. 부모가 받는다. */
	FOnDmWindowCloseRequested OnCloseRequested;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOU|Messenger")
    TSubclassOf<UDmMessageBubbleWidget> MessageBubbleClass;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DmStatusText;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") bool bShowConversationClose = true;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UTextBlock> DmTitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UScrollBox> DmScrollBox;

	/** 말풍선들이 쌓이는 곳 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UVerticalBox> DmMessageBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UEditableTextBox> DmInputBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UButton> DmSendButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UButton> DmCloseButton;

	/** "이전 대화 더 보기". 더 받을 것이 없으면 숨긴다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Messenger")
	TObjectPtr<UButton> DmMoreButton;

private:
    // [MSGUI-003] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
	void BuildDefaultLayout();

	/** 말풍선 한 줄을 만들어 넣는다. bPrepend 면 맨 위에. */
    // [MSGUI-007] InsertBubble 동작을 처리하고 관련 위젯 상태를 갱신한다.
	void InsertBubble(const FMOUDirectMessage& Message, bool bPrepend);

    // [MSGUI-005] GetServerSubsystem 동작을 처리하고 관련 위젯 상태를 갱신한다.
	UServerSubsystem* GetServerSubsystem() const;

	UFUNCTION()
    // [MSGUI-013] HandleSendClicked 동작을 처리하고 관련 위젯 상태를 갱신한다.
	void HandleSendClicked();

	UFUNCTION()
    // [MSGUI-015] HandleCloseClicked 동작을 처리하고 관련 위젯 상태를 갱신한다.
	void HandleCloseClicked();

	UFUNCTION()
    // [MSGUI-016] HandleMoreClicked 동작을 처리하고 관련 위젯 상태를 갱신한다.
	void HandleMoreClicked();

	UFUNCTION()
    // [MSGUI-014] HandleInputCommitted 동작을 처리하고 관련 위젯 상태를 갱신한다.
	void HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	int64   PeerUserId = 0;
	FString PeerNickname;

	/**
	 * 지금 화면에 있는 가장 오래된 메시지의 번호. 위로 스크롤할 때 커서로 쓴다.
	 *
	 * ★ 0 이면 안 된다. 0 을 서버에 보내면 "최신 페이지" 로 해석되어
	 *   읽음 처리까지 일어난다(UServerSubsystem::LoadOlderMessages 가 막아준다).
	 */
	int64 OldestMessageId = 0;

	/**
	 * "이전 대화 더 보기" 를 눌러 응답을 기다리는 중인가.
	 *
	 * ★ 이 플래그가 곧 "다음에 올 기록을 앞에 붙일지 갈아끼울지" 를 정한다.
	 *   SetHistory 가 소비하고 false 로 되돌린다.
	 */
	bool bHistoryBusy = false;
    bool bHasMoreHistory = false;
    TArray<FMOUDirectMessage> LoadedMessages;
    FString PendingDraft;
};

/** 패널 수명과 서버 이벤트 라우팅을 관리한다. */
UCLASS()
class TEAMPROJECT_MOU_API UMessengerWidgetBase : public UUserWidget
{
    GENERATED_BODY()
    friend class FMessengerWidgetRegressionTest;
public:
    // [MSGUI-017] UMessengerWidgetBase 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UMessengerWidgetBase(const FObjectInitializer& ObjectInitializer);
    // [MSGUI-018] 초기 바인딩이 없는 경우 기본 위젯 트리를 구성한다.
    virtual void NativeOnInitialized() override;
    // [MSGUI-021] 위젯 이벤트를 연결하고 저장된 데이터를 표시한다.
    virtual void NativeConstruct() override;
    // [MSGUI-022] 서버 구독과 타이머를 정리한다.
    virtual void NativeDestruct() override;
    // [MSGUI-023] 현재 대화 표시 상태의 전환을 확인한다.
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    // [MSGUI-025] 대화와 탭을 생성하거나 기존 대화를 선택한다.
    UFUNCTION(BlueprintCallable, Category="MOU|Messenger") void OpenConversation(int64 PeerUserId);
    // [MSGUI-027] 입력 초안을 보존하고 해당 대화와 탭을 제거한다.
    UFUNCTION(BlueprintCallable, Category="MOU|Messenger") void CloseConversation(int64 PeerUserId);
    // [MSGUI-026] 선택한 대화만 표시하고 읽음·기록 요청을 시작한다.
    UFUNCTION(BlueprintCallable, Category="MOU|Messenger") void SelectConversation(int64 PeerUserId);
    UFUNCTION(BlueprintPure, Category="MOU|Messenger") int32 GetOpenConversationCount() const { return Windows.Num(); }
    // [MSGUI-037] SetMessengerVisible 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION(BlueprintCallable, Category="MOU|Messenger") void SetMessengerVisible(bool bVisible);
protected:
    // 이전 WBP용 바인딩. 신규 WBP는 MessengerPanelSlot을 사용한다.
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MOU|Messenger") TObjectPtr<UHorizontalBox> ConversationArea;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MOU|Messenger") TObjectPtr<UVerticalBox> FriendPanelSlot;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MOU|Messenger") TObjectPtr<UVerticalBox> MessengerPanelSlot;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MOU|Messenger") TObjectPtr<UOverlay> PopupLayer;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MOU|Messenger") TObjectPtr<UOverlay> ContextMenuLayer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> FriendToggleButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> MessengerToggleButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TotalUnreadText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOU|Messenger") TSubclassOf<UFriendListWidgetBase> FriendListClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOU|Messenger") TSubclassOf<UDmWindowWidget> DmWindowClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOU|Messenger") TSubclassOf<UMessengerPanelWidget> MessengerPanelClass;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TSubclassOf<UAddFriendPopupWidget> AddFriendPopupClass;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TSubclassOf<UFriendRequestsPopupWidget> RequestsPopupClass;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger") TSubclassOf<UFriendContextMenuWidget> ContextMenuClass;
    UPROPERTY(EditAnywhere, Category="MOU|Messenger", meta=(ClampMin="1")) int32 MaxOpenConversations = 3;
private:
    // [MSGUI-019] WBP가 없을 때 사용할 기본 레이아웃을 구성한다.
    void BuildDefaultLayout();
    // [MSGUI-020] GetServerSubsystem 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UServerSubsystem* GetServerSubsystem() const;
    // [MSGUI-024] ResolveNickname 동작을 처리하고 관련 위젯 상태를 갱신한다.
    FString ResolveNickname(int64 UserId) const;
    // [MSGUI-035] HandleConversationRequested 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void HandleConversationRequested(int64 PeerUserId);
    // [MSGUI-036] HandleWindowCloseRequested 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void HandleWindowCloseRequested(int64 PeerUserId);
    // [MSGUI-028] 읽음 처리 가능한 실제 대화 표시 상태인지 확인한다.
    bool IsConversationPresented(int64 PeerUserId) const;
    // [MSGUI-029] 같은 상대의 기록 요청을 직렬화하고 읽음 조건을 확인한다.
    void RequestHistory(int64 PeerUserId, int64 BeforeMessageId);
    // [MSGUI-032] 친구·탭·전체 미읽음 개수를 캐시에서 다시 표시한다.
    void RefreshBadges();
    // [MSGUI-042] ShowPopup 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void ShowPopup(UUserWidget* Popup);
    // [MSGUI-046] ShowFriendContextMenu 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void ShowFriendContextMenu(int64 UserId);
    // [MSGUI-047] HandleContextAction 동작을 처리하고 관련 위젯 상태를 갱신한다.
    void HandleContextAction(int64 UserId, EFriendEntryAction Action);
    // [MSGUI-040] ToggleFriends 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void ToggleFriends();
    // [MSGUI-038] ToggleMessenger 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void ToggleMessenger();
    // [MSGUI-041] HideFriends 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HideFriends();
    // [MSGUI-039] HideMessenger 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HideMessenger();
    // [MSGUI-043] ShowAddFriendPopup 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void ShowAddFriendPopup();
    // [MSGUI-044] ShowRequestsPopup 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void ShowRequestsPopup();
    // [MSGUI-045] ClosePopups 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void ClosePopups();
    // [MSGUI-048] CloseContextMenu 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void CloseContextMenu();
    // [MSGUI-030] 수신 메시지를 표시하고 현재 보고 있는 대화만 읽음 처리한다.
    UFUNCTION() void HandleDirectMessageReceived(const FMOUDirectMessage& Message);
    // [MSGUI-031] 요청 소유 대화에만 기록을 적용하고 보류 요청을 처리한다.
    UFUNCTION() void HandleDmHistoryReceived(int64 PeerUserId, const TArray<FMOUDirectMessage>& Messages, bool bHasMore);
    // [MSGUI-033] HandleFriendUpdated 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleFriendUpdated(const FMOUFriend& Friend, bool bRemoved);
    // [MSGUI-034] HandleFriendList 동작을 처리하고 관련 위젯 상태를 갱신한다.
    UFUNCTION() void HandleFriendList(const TArray<FMOUFriend>& Friends);
    // [MSGUI-049] 연결 변경 시 만료된 요청을 비우고 재연결 후 활성 대화를 다시 확인한다.
    UFUNCTION() void HandleChatState(EChatConnectionState State, const FString& Detail);
    UPROPERTY() TObjectPtr<UFriendListWidgetBase> FriendList;
    UPROPERTY() TObjectPtr<UMessengerPanelWidget> MessengerPanel;
    UPROPERTY() TObjectPtr<UAddFriendPopupWidget> AddPopup;
    UPROPERTY() TObjectPtr<UFriendRequestsPopupWidget> RequestsPopup;
    UPROPERTY() TObjectPtr<UFriendContextMenuWidget> FriendContextMenu;
    UPROPERTY() TMap<int64, TObjectPtr<UDmWindowWidget>> Windows;
    TArray<int64> WindowOrder;
    TMap<int64, FString> Drafts;
    TMap<int64, FMessengerPendingHistory> PendingHistory;
    int64 ActivePeerUserId = 0;
    bool bMessengerVisible = false;
    bool bPopupOpen = false;
    bool bWasPresented = false;
    bool bSubscribed = false;
};
