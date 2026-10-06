// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "MOUWaterHazardComponent.generated.h"

class ACharacter;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaterHazardStateChanged, bool, bIsActive);

UCLASS(ClassGroup = (Water), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_MOU_API UMOUWaterHazardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOUWaterHazardComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// [WATERHAZARD-002] 건조 영역 경계를 감시하고 감속 및 서버 대미지 타이머를 동기화한다.
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void HandleActorEntered(AActor* OtherActor);
	void HandleActorExited(AActor* OtherActor);

	UFUNCTION(BlueprintCallable, Category = "Water|Hazard")
	void SetHazardActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Water|Hazard")
	void CheckWaterLevel(float CurrentWaterZ);

	UFUNCTION(BlueprintPure, Category = "Water|Hazard")
	bool IsHazardActive() const { return bIsHazardActive; }

	void SetAlwaysActive(bool bAlways) { bAlwaysActive = bAlways; }
	void SetDeactivationZThreshold(float Threshold) { DeactivationZThreshold = Threshold; }

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard")
	float SlowSpeedMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard")
	float DamagePerTick = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard")
	float DamageInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard")
	bool bApplyDamageImmediatelyOnEnter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard")
	bool bAlwaysActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard")
	float DeactivationZThreshold = -50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water|Hazard|GAS")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water|Hazard|GAS")
	TSubclassOf<UGameplayEffect> SlowGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Hazard|Debug")
	bool bShowDebugVisualizer = false;

	UPROPERTY(BlueprintAssignable, Category = "Water|Hazard|Events")
	FOnWaterHazardStateChanged OnHazardStateChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_IsHazardActive, BlueprintReadOnly, Category = "Water|Hazard")
	bool bIsHazardActive = true;

	UFUNCTION()
	void OnRep_IsHazardActive();

	// [WATERHAZARD-001] 활성 상태와 해당 물의 제외 볼륨을 기준으로 효과 적용 여부를 판정한다.
	bool ShouldAffectCharacter(const ACharacter* Character) const;
	// [WATERHAZARD-003] 제외 영역 밖의 캐릭터에게 감속을 한 번 적용한다.
	void ApplySlowToCharacter(ACharacter* Character);
	// [WATERHAZARD-004] 이 컴포넌트가 적용한 감속과 달리기 차단만 해제한다.
	void RestoreCharacterMovement(ACharacter* Character);

	// [WATERHAZARD-005] 서버에서 제외 영역을 재확인한 뒤 물 대미지를 적용한다.
	void ApplyDamageToCharacter(ACharacter* Character);
	void OnDamageTick();

	void DrawDebugVisualizer();

protected:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ACharacter>> OverlappingCharacters;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<ACharacter>, float> OriginalSpeedMap;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<ACharacter>, FActiveGameplayEffectHandle> ActiveSlowEffectMap;

	// 이 컴포넌트가 직접 추가한 달리기 차단 태그의 소유권을 기록한다.
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<ACharacter>> SprintBlockedCharacters;

	FTimerHandle DamageTimerHandle;
};
