// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Base/EventObjectBase.h"
#include "JumpPad.generated.h"

class UBoxComponent;
class USoundBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJumpPadActivated, ACharacter*, Character);

UCLASS()
class TEAMPROJECT_MOU_API AJumpPad : public AEventObjectBase
{
	GENERATED_BODY()

public:
	AJumpPad();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void PickUp_Implementation(AActor* Picker) override;
	virtual void Drop_Implementation(FVector DropLocation, AActor* Dropper = nullptr) override;
	virtual void Throw_Implementation(FVector ThrowVelocity, AActor* Thrower = nullptr) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Settings")
	float BoostedJumpZVelocity = 1300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Settings")
	float LaunchVelocityBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Settings")
	bool bAutoLaunchOnStep = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Visual")
	int32 MaterialSlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Visual")
	FName ColorParameterName = FName("Color");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Visual")
	FLinearColor DefaultColor = FLinearColor(0.1f, 0.4f, 0.8f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Visual")
	FLinearColor ActiveColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Visual")
	float ActiveColorDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Visual")
	TObjectPtr<UMaterialInterface> ActiveMaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPad|Sound")
	TObjectPtr<USoundBase> ActivationSound;

	UPROPERTY(BlueprintAssignable, Category = "JumpPad|Events")
	FOnJumpPadActivated OnJumpPadActivated;

	UFUNCTION(BlueprintImplementableEvent, Category = "JumpPad|Events")
	void OnJumpPadTriggered(ACharacter* Character);

protected:
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayActivationEffects(FVector Location);

	void ActivateJumpPad(ACharacter* Character);
	void ResetMaterialColor();
	void ClearAllOverlappingCharacters();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_IsDeployed, BlueprintReadOnly, Category = "JumpPad|State")
	bool bIsDeployed = true;

	UFUNCTION()
	void OnRep_IsDeployed();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	FTimerHandle ResetColorTimerHandle;

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<ACharacter>, float> OriginalJumpZVelocityMap;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ACharacter>> OverlappingCharacters;
};
