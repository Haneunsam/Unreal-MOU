#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "TerminalShop.generated.h"

class APlayerController;
class UBoxComponent;
class UStaticMeshComponent;
class UTerminalShopWidget;

/** 가까이에서 바라보고 상호작용하면 로컬 플레이어에게 상점 UI를 여는 액터. */
UCLASS(Blueprintable)
class TEAMPROJECT_MOU_API ATerminalShop : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	// [TSHOP-000] 상호작용 판정 상자와 임시 모니터 메시를 구성한다.
	ATerminalShop();

	// [TSHOP-001] 플레이어가 상점 사용 거리 안에 있는지 확인한다.
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	// [TSHOP-002] 상호작용한 로컬 플레이어에게 상점 UI를 연다.
	virtual void Interact_Implementation(AActor* Interactor) override;

	// [TSHOP-003] 상호작용 안내 문구를 반환한다.
	virtual FText GetInteractPrompt_Implementation() const override;

	// [TSHOP-004] UI가 닫혔을 때 로컬 참조를 해제한다.
	void NotifyWidgetClosed(UTerminalShopWidget* ClosedWidget);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terminal Shop")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terminal Shop")
	TObjectPtr<UStaticMeshComponent> MonitorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terminal Shop", meta=(ClampMin="50.0"))
	float InteractionRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Terminal Shop|UI")
	TSubclassOf<UTerminalShopWidget> ShopWidgetClass;

private:
	TWeakObjectPtr<UTerminalShopWidget> OpenWidget;
};
