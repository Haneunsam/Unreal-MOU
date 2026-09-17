#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/InteractableInterface.h"
#include "CustomizationNPC.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class UCharacterCustomizationComponent;

/**
 * 플레이어 캐릭터와 동일한 메시를 사용하여 외형 커스터마이징을 제공하는 스타일리스트 NPC
 */
UCLASS()
class TEAMPROJECT_MOU_API ACustomizationNPC : public ACharacter, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ACustomizationNPC();

	// ---------------------------------------------------------
	// [IInteractableInterface 구현]
	// ---------------------------------------------------------
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	// 프리뷰 카메라 컴포넌트 반환
	UFUNCTION(BlueprintPure, Category = "Customization|Camera")
	UCameraComponent* GetPreviewCamera() const { return PreviewCamera; }

	// 플레이어가 서 있어야 하는 권장 위치 스팟 반환
	UFUNCTION(BlueprintPure, Category = "Customization|Spot")
	USceneComponent* GetPlayerStandSpot() const { return PlayerStandSpot; }

	// NPC 자체 외형 커스터마이징 컴포넌트 반환
	UFUNCTION(BlueprintPure, Category = "Customization")
	UCharacterCustomizationComponent* GetCustomizationComponent() const { return CustomizationComponent; }

protected:
	virtual void BeginPlay() override;

	// 프리뷰용 카메라 스프링암 (에디터에서 길이, 회전 조절 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> PreviewCameraBoom;

	// 프리뷰용 카메라 컴포넌트 (SetViewTargetWithBlend 전환 대상)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> PreviewCamera;

	// 플레이어가 상호작용 시 위치할 권장 스팟
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Spot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PlayerStandSpot;

	// NPC 본인의 커스터마이징 외형 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterCustomizationComponent> CustomizationComponent;

	// 화면에 표시될 상호작용 프롬프트 텍스트 (예: "스타일리스트와 대화하기")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Dialogue")
	FText InteractPromptText;

	// NPC의 인사말 / 대화 멘트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Dialogue")
	FText DialogueGreetingText;

	// 카메라 뷰타겟 블렌드 전환 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Camera")
	float CameraBlendDuration = 0.75f;
};
