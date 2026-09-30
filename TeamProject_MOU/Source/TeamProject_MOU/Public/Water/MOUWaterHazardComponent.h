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

	void ApplySlowToCharacter(ACharacter* Character);
	void RestoreCharacterMovement(ACharacter* Character);

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

	FTimerHandle DamageTimerHandle;
};
