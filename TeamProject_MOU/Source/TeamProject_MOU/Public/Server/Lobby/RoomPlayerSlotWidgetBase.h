#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Server/Lobby/LobbyTypes.h"
#include "RoomPlayerSlotWidgetBase.generated.h"

class UImage;
class UCharacterCustomizationComponent;
class AActor;
class UTextBlock;
class UWidget;
class UTextureRenderTarget2D;

/** 서버를 구독하지 않는 좌석 표시 위젯. 캐릭터 이미지는 WBP에서 배치한다. */
UCLASS()
class TEAMPROJECT_MOU_API URoomPlayerSlotWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	// [LSLOT-002] BP 생성 처리가 끝난 뒤 슬롯 외형과 전용 촬영 텍스처를 다시 연결한다.
	virtual void NativeConstruct() override;

	// [LSLOT-005] 같은 월드의 슬롯 컴포넌트를 연결하고 외형 및 전용 촬영 텍스처를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Slot")
	void SetPreviewComponent(UCharacterCustomizationComponent* Component);

	/** 기존 BP_LobbyCharacterPreview 액터를 이 슬롯의 메쉬 미리보기로 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Slot")
	void SetPreviewActor(AActor* Actor);

	/** PlayerSlotWidget의 PreviewSlotIndex와 동일한 슬롯 액터/컴포넌트 조회 경로. */
	static AActor* FindLobbyPreviewActor(const UObject* WorldContextObject, int32 SlotIndex);
	static UCharacterCustomizationComponent* GetOrCreatePreviewComponent(AActor* Actor);


	// [LSLOT-003] 멤버별 외형을 적용하고 BP 갱신 후 슬롯 전용 텍스처를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Slot")
	void SetMember(const FMOURoomMember& InMember, bool bInIsSelf);

	// [LSLOT-004] 빈 좌석으로 전환한 뒤에도 다른 PIE 창의 텍스처를 사용하지 않게 한다.
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
	TObjectPtr<UImage> CharacterImage;
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
	// [LSLOT-001] 월드·슬롯별 RenderTarget을 생성 또는 재사용해 카메라와 이미지를 함께 연결한다.
	void RefreshPortraitTarget();
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> SlotRenderTarget;
	void RefreshVisuals();
	TWeakObjectPtr<UCharacterCustomizationComponent> PreviewComponent;
	void FindPreviewActorForSlot(int32 SlotIndex);
	FCharacterCustomizationData LastAppliedCustomization;
	bool bHasAppliedCustomization = false;
};
