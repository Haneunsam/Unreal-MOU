#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Server/Lobby/LobbyTypes.h"
#include "RoomPlayerSlotWidgetBase.generated.h"

class UImage;
class UTextBlock;
class UWidget;

/** 서버를 구독하지 않는 좌석 표시 위젯. 캐릭터 이미지는 WBP에서 배치한다. */
UCLASS()
class TEAMPROJECT_MOU_API URoomPlayerSlotWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Slot")
	void SetMember(const FMOURoomMember& InMember, bool bInIsSelf);

	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Slot")
	void ClearMember();

	UPROPERTY(BlueprintReadOnly, Category = "MOU|Lobby|Slot")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "MOU|Lobby|Slot")
	bool bIsSelf = false;

	UPROPERTY(BlueprintReadOnly, Category = "MOU|Lobby|Slot")
	FMOURoomMember Member;

	/** 입장/퇴장/준비 변경 때만 호출. WBP에서 초상화, 테두리, 애니메이션 갱신. */
	UFUNCTION(BlueprintImplementableEvent, Category = "MOU|Lobby|Slot")
	void OnSlotChanged();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyPanel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OccupiedPanel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NicknameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ReadyImage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HostImage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelfHighlight;
private:
	void RefreshVisuals();
};
