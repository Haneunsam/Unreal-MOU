// MOU 로비 스택에서 사용하는 고정 역할 페이지들.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CharacterCustomizationWidget.h"
#include "UI/SettingsMenuWidget.h"
#include "Server/Lobby/LobbyTypes.h"
#include "LobbyPageWidgetBase.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UServerSubsystem;
class UUniformGridPanel;
class URoomPlayerSlotWidgetBase;
class UImage;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class USceneCaptureComponent;
class USkeletalMeshComponent;
class ULobbyCustomizationComponent;
class AActor;

DECLARE_DELEGATE(FOnLobbyPageAction);

/** 방 만들기/참여/설정/종료가 각각 독립 버튼인 스택의 루트 페이지. */
UCLASS()
class TEAMPROJECT_MOU_API ULobbyMainWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	void Refresh(const UServerSubsystem* Server);
	void SetMessage(const FString& Text, bool bIsError);

	FOnLobbyPageAction OnCreateRoom;
	FOnLobbyPageAction OnJoinRoom;
	FOnLobbyPageAction OnOpenSettings;
	FOnLobbyPageAction OnQuitGame;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> CreateRoomButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> JoinRoomButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> QuitGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> MessageText;

private:
	UFUNCTION() void HandleCreateRoomClicked();
	UFUNCTION() void HandleJoinRoomClicked();
	UFUNCTION() void HandleSettingsClicked();
	UFUNCTION() void HandleQuitGameClicked();
	void BuildDefaultLayout();
};

/** 준비/시작/커스터마이징/나가기가 서로 다른 버튼인 방 대기실 페이지. */
UCLASS()
class TEAMPROJECT_MOU_API URoomLobbyWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOU|Lobby|Slots")
	TSubclassOf<URoomPlayerSlotWidgetBase> PlayerSlotWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOU|Lobby|Slots", meta = (ClampMin = "1", ClampMax = "4"))
	int32 SlotColumns = 2;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// [RTITLE-005] 현재 방 제목과 준비 상태를 대기실 위젯에 반영한다.
	void Refresh(const UServerSubsystem* Server);
	void SetMessage(const FString& Text, bool bIsError);

	FOnLobbyPageAction OnToggleReady;
	FOnLobbyPageAction OnStartGame;
	FOnLobbyPageAction OnCustomize;
	FOnLobbyPageAction OnLeaveRoom;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UVerticalBox> MemberListBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby|Slots")
	TObjectPtr<UUniformGridPanel> PlayerSlotGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> ReadyButtonLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> StartGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> StartGameButtonLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> CustomizeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> LeaveRoomButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> MessageText;

private:
	UFUNCTION() void HandleReadyClicked();
	UFUNCTION() void HandleStartClicked();
	UFUNCTION() void HandleCustomizeClicked();
	UFUNCTION() void HandleLeaveClicked();
	void BuildDefaultLayout();
	void EnsurePlayerSlots();
	void RebuildMemberList(const UServerSubsystem* Server);

	UPROPERTY()
	TArray<TObjectPtr<URoomPlayerSlotWidgetBase>> PlayerSlots;
};

/** 실제 환경설정 UI가 들어오기 전에도 Push/Pop 흐름을 검증할 수 있는 페이지. */
UCLASS()
class TEAMPROJECT_MOU_API ULobbySettingsWidgetBase : public USettingsMenuWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	// [LSET-001] 팀원 설정 기능을 초기화하고 닫기를 로비 복귀에 연결한다.
	virtual void NativeConstruct() override;
	// [LSET-002] 닫기 연결과 키 설정 팝업을 정리한다.
	virtual void NativeDestruct() override;

	FOnLobbyPageAction OnBack;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> BackButton;

private:
	UFUNCTION() void HandleBackClicked();
	void BuildDefaultLayout();
};

/** 방 대기실 위에 Push되는 커스터마이징 페이지. */
UCLASS()
class TEAMPROJECT_MOU_API ULobbyCustomizeWidgetBase : public UCharacterCustomizationWidget
{
	GENERATED_BODY()

public:
	// [LCUI-003] 디자이너 루트가 없으면 기본 편집 화면을 생성한다.
	virtual void NativeOnInitialized() override;
	// [LCUI-004] 편집 상태를 초기화하고 본인 미리보기와 버튼 및 서버 이벤트를 연결한다.
	virtual void NativeConstruct() override;
	// [LCUI-005] 이벤트 연결과 창 전용 미리보기를 해제한다.
	virtual void NativeDestruct() override;
	// [LCUI-006] 확인 버튼에서만 편집값을 서버로 전송하고 응답을 기다린다.
	virtual void ConfirmAndSave() override;
	// [LCUI-007] 편집값과 전용 미리보기를 폐기한 뒤 이전 화면으로 돌아간다.
	virtual void CancelAndExit() override;
	// [LCUI-008] 대기실 캐릭터와 카메라는 유지하고 편집용 메시만 회전한다.
	virtual void RotateCharacter(float DeltaX) override;

	// [LCUI-009] 기존 BP 연결을 받되 실제 편집 대상은 본인 슬롯에서 복사한 전용 메시로 제한한다.
	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Customization")
	void SetPreviewComponent(UCharacterCustomizationComponent* Component);

	UPROPERTY(BlueprintReadOnly, Category = "MOU|Lobby|Customization")
	bool bWaitingForConfirmation = false;

	UPROPERTY(BlueprintReadOnly, Category = "MOU|Lobby|Customization")
	int32 LocalSlotIndex = INDEX_NONE;

	UFUNCTION(BlueprintImplementableEvent, Category = "MOU|Lobby|Customization")
	void OnCustomizationStatus(const FText& Message, bool bSuccess);

	UFUNCTION(BlueprintImplementableEvent, Category = "MOU|Lobby|Customization")
	void OnCustomizationPreviewChanged(const FCharacterCustomizationData& Data);

	FOnLobbyPageAction OnBack;

protected:
	// [LCUI-012] 본인의 확정된 외형으로 편집값과 UI를 초기화한다.
	virtual void InitializeCustomization() override;
	// [LCUI-013] 색상과 문양 편집을 창 전용 메시 및 RenderTarget에만 반영한다.
	virtual void UpdatePreview() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> ResetButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UTextBlock> CustomizationStatusText;

	/** WBP에 같은 이름의 Image를 만들면 그 위치를 사용한다. 없으면 Canvas에 자동 추가한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby|Customization")
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> BackButton;

private:
	// [LCUI-014] 확인 버튼을 서버 전송 처리에 연결한다.
	UFUNCTION() void HandleConfirmClicked();
	// [LCUI-015] 전송 대기 중이 아닐 때 기본 외형을 미리본다.
	UFUNCTION() void HandleResetClicked();
	// [LCUI-016] 서버 승인 결과를 반영하고 실패 시 다시 편집할 수 있게 한다.
	UFUNCTION() void HandleCustomizationResult(bool bSuccess, bool bSavedToDisk);
	// [LCUI-017] 멤버 목록 갱신 시 본인 슬롯 연결만 확인하고 편집값은 보존한다.
	UFUNCTION() void HandlePreviewRoomMembersChanged(int32 RoomId, const TArray<FMOURoomMember>& Members, bool bAllReady);
	// [LCUI-018] 편집 상태 메시지를 텍스트와 BP 이벤트에 전달한다.
	void ShowStatus(const FText& Message, bool bSuccess);
	TWeakObjectPtr<ULobbyCustomizationComponent> PreviewComponent;
	TWeakObjectPtr<AActor> LocalPreviewActor;
	TWeakObjectPtr<AActor> SourcePreviewActor;
	TWeakObjectPtr<USkeletalMeshComponent> LocalPreviewMesh;
	TWeakObjectPtr<USceneCaptureComponent2D> PreviewCapture;
	// 기존 대기실 캡처에는 편집용 액터를 숨기고, 종료 시 이 제외 항목만 제거한다.
	TArray<TWeakObjectPtr<USceneCaptureComponent>> OtherPreviewCaptures;
	int32 PreviewRoomId = 0;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewUIMaterial;
	// [LCUI-001] 본인 슬롯을 원본으로 창 전용 미리보기를 생성한다.
	bool CreateLocalPreview(AActor* SourceActor);
	// [LCUI-002] 창 전용 미리보기 자원을 해제한다.
	void ReleaseLocalPreview();
	// [LCUI-019] 로그인한 본인 슬롯을 찾아 독립 미리보기를 PreviewImage에 연결한다.
	void ConnectOwnSlotPreview();
	// [LCUI-020] 뒤로가기 버튼을 편집 취소 처리에 연결한다.
	UFUNCTION() void HandleBackClicked();
	// [LCUI-021] BP 레이아웃이 없는 경우의 기본 버튼과 상태 표시를 만든다.
	void BuildDefaultLayout();
};
