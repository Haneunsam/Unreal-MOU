#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enum/NPCEnum.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "NPCTargetLockComponent.generated.h"

class UAIPerceptionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnNPCTargetChanged,
	AActor*, PreviousTarget,
	AActor*, NewTarget);

/**
 * AI Perception의 Sight 결과에서 현재 타깃을 잠그고 감지 해제 유예 시간을 관리한다.
 * Blackboard와 NPC 상태 변경은 OnTargetChanged를 받은 AIController가 담당한다.
 */
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_MOU_API UNPCTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPCTargetLockComponent();

	/** 사용할 Perception 컴포넌트와 NPCData의 타깃 정책을 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "NPC|Perception")
	void InitializeTargetLock(
		UAIPerceptionComponent* InPerceptionComponent,
		ENPCTargetSelectionPolicy InSelectionPolicy,
		float InTargetLoseGraceTime);

	/** 현재 타깃을 즉시 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "NPC|Perception")
	void ClearTarget();

	/** 현재 잠긴 타깃을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "NPC|Perception")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/** 타깃이 실제로 변경되거나 최종 상실됐을 때 한 번만 호출된다. */
	UPROPERTY(BlueprintAssignable, Category = "NPC|Perception")
	FOnNPCTargetChanged OnTargetChanged;

	/** 이 태그를 가진 ASC의 Actor만 타깃 후보로 사용한다. 비어 있으면 모든 Actor를 허용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Perception")
	FGameplayTag RequiredTargetTag;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleCurrentTargetDestroyed(AActor* DestroyedActor);

	void SetCurrentTarget(AActor* NewTarget);
	void StartTargetLoseTimer();
	void CancelTargetLoseTimer();
	void ResolveTargetLoss();
	AActor* FindBestPerceivedTarget() const;
	bool IsEligibleTarget(const AActor* Actor) const;
	bool IsCurrentlyPerceived(const AActor* Actor) const;
	FVector GetSelectionOrigin() const;

	UPROPERTY(Transient)
	TObjectPtr<UAIPerceptionComponent> PerceptionComponent;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentTarget;

	ENPCTargetSelectionPolicy SelectionPolicy;
	float TargetLoseGraceTime;
	FTimerHandle TargetLoseTimerHandle;
};
