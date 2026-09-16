// MOU 로비 스택에서 사용하는 고정 역할 페이지들.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyPageWidgetBase.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UServerSubsystem;
class UUniformGridPanel;
class URoomPlayerSlotWidgetBase;

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
	void RebuildMemberList(const UServerSubsystem* Server);

	UPROPERTY()
	TArray<TObjectPtr<URoomPlayerSlotWidgetBase>> PlayerSlots;
};

/** 실제 환경설정 UI가 들어오기 전에도 Push/Pop 흐름을 검증할 수 있는 페이지. */
UCLASS()
class TEAMPROJECT_MOU_API ULobbySettingsWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

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
class TEAMPROJECT_MOU_API ULobbyCustomizeWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	FOnLobbyPageAction OnBack;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Lobby")
	TObjectPtr<UButton> BackButton;

private:
	UFUNCTION() void HandleBackClicked();
	void BuildDefaultLayout();
};
