// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/MOUWaterHazardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "Engine/DamageEvents.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Base/BaseAttributeSet.h"

UMOUWaterHazardComponent::UMOUWaterHazardComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);

	SlowSpeedMultiplier = 0.35f;
	DamagePerTick = 15.0f;
	DamageInterval = 1.0f;
	bApplyDamageImmediatelyOnEnter = true;
	bAlwaysActive = false;
	DeactivationZThreshold = -50.0f;
	bShowDebugVisualizer = false;
	bIsHazardActive = true;
}

void UMOUWaterHazardComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CheckWaterLevel(Owner->GetActorLocation().Z);
	}
}

void UMOUWaterHazardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}

	for (TWeakObjectPtr<ACharacter>& CharPtr : OverlappingCharacters)
	{
		if (ACharacter* Char = CharPtr.Get())
		{
			RestoreCharacterMovement(Char);
		}
	}

	OverlappingCharacters.Empty();
	OriginalSpeedMap.Empty();
	ActiveSlowEffectMap.Empty();

	Super::EndPlay(EndPlayReason);
}

void UMOUWaterHazardComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOUWaterHazardComponent, bIsHazardActive);
}

void UMOUWaterHazardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsHazardActive)
	{
		for (TWeakObjectPtr<ACharacter>& CharPtr : OverlappingCharacters)
		{
			if (ACharacter* Char = CharPtr.Get())
			{
				if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
				{
					float BaseSpeed = OriginalSpeedMap.Contains(Char) ? OriginalSpeedMap[Char] : 300.0f;
					float MaxAllowedSpeed = BaseSpeed * SlowSpeedMultiplier;
					if (MoveComp->MaxWalkSpeed > MaxAllowedSpeed + 1.0f)
					{
						MoveComp->MaxWalkSpeed = MaxAllowedSpeed;
					}
				}
			}
		}
	}

	if (bShowDebugVisualizer)
	{
		DrawDebugVisualizer();
	}
}

void UMOUWaterHazardComponent::HandleActorEntered(AActor* OtherActor)
{
	if (!OtherActor || OtherActor == GetOwner())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	if (!OverlappingCharacters.Contains(Character))
	{
		OverlappingCharacters.Add(Character);
	}

	if (bIsHazardActive)
	{
		ApplySlowToCharacter(Character);

		if (GetOwner() && GetOwner()->HasAuthority())
		{
			if (bApplyDamageImmediatelyOnEnter)
			{
				ApplyDamageToCharacter(Character);
			}

			if (UWorld* World = GetWorld())
			{
				if (!World->GetTimerManager().IsTimerActive(DamageTimerHandle))
				{
					World->GetTimerManager().SetTimer(
						DamageTimerHandle,
						this,
						&UMOUWaterHazardComponent::OnDamageTick,
						DamageInterval,
						true
					);
				}
			}
		}
	}
}

void UMOUWaterHazardComponent::HandleActorExited(AActor* OtherActor)
{
	if (!OtherActor)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	RestoreCharacterMovement(Character);
	OverlappingCharacters.Remove(Character);

	if (OverlappingCharacters.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DamageTimerHandle);
		}
	}
}

void UMOUWaterHazardComponent::SetHazardActive(bool bActive)
{
	if (bIsHazardActive == bActive)
	{
		return;
	}

	bIsHazardActive = bActive;
	OnHazardStateChanged.Broadcast(bIsHazardActive);

	if (!bIsHazardActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DamageTimerHandle);
		}

		for (TWeakObjectPtr<ACharacter>& CharPtr : OverlappingCharacters)
		{
			if (ACharacter* Char = CharPtr.Get())
			{
				RestoreCharacterMovement(Char);
			}
		}
	}
	else
	{
		for (TWeakObjectPtr<ACharacter>& CharPtr : OverlappingCharacters)
		{
			if (ACharacter* Char = CharPtr.Get())
			{
				ApplySlowToCharacter(Char);

				if (GetOwner() && GetOwner()->HasAuthority() && bApplyDamageImmediatelyOnEnter)
				{
					ApplyDamageToCharacter(Char);
				}
			}
		}

		if (GetOwner() && GetOwner()->HasAuthority() && !OverlappingCharacters.IsEmpty())
		{
			if (UWorld* World = GetWorld())
			{
				if (!World->GetTimerManager().IsTimerActive(DamageTimerHandle))
				{
					World->GetTimerManager().SetTimer(
						DamageTimerHandle,
						this,
						&UMOUWaterHazardComponent::OnDamageTick,
						DamageInterval,
						true
					);
				}
			}
		}
	}
}

void UMOUWaterHazardComponent::CheckWaterLevel(float CurrentWaterZ)
{
	if (bAlwaysActive)
	{
		return;
	}

	bool bShouldBeActive = (CurrentWaterZ > DeactivationZThreshold);
	SetHazardActive(bShouldBeActive);
}

void UMOUWaterHazardComponent::OnRep_IsHazardActive()
{
	OnHazardStateChanged.Broadcast(bIsHazardActive);

	if (!bIsHazardActive)
	{
		for (TWeakObjectPtr<ACharacter>& CharPtr : OverlappingCharacters)
		{
			if (ACharacter* Char = CharPtr.Get())
			{
				RestoreCharacterMovement(Char);
			}
		}
	}
	else
	{
		for (TWeakObjectPtr<ACharacter>& CharPtr : OverlappingCharacters)
		{
			if (ACharacter* Char = CharPtr.Get())
			{
				ApplySlowToCharacter(Char);
			}
		}
	}
}

void UMOUWaterHazardComponent::ApplySlowToCharacter(ACharacter* Character)
{
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (!OriginalSpeedMap.Contains(Character))
	{
		OriginalSpeedMap.Add(Character, MoveComp->MaxWalkSpeed);
	}

	float BaseSpeed = OriginalSpeedMap[Character];
	MoveComp->MaxWalkSpeed = BaseSpeed * SlowSpeedMultiplier;

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character))
	{
		static const FGameplayTag BlockSprintTag = FGameplayTag::RequestGameplayTag(FName("Ability.Player.Block.Sprint"), false);
		if (BlockSprintTag.IsValid())
		{
			ASC->AddLooseGameplayTag(BlockSprintTag);

			static const FGameplayTag SprintTag = FGameplayTag::RequestGameplayTag(FName("Ability.Player.Sprint"), false);
			if (SprintTag.IsValid())
			{
				FGameplayTagContainer SprintContainer;
				SprintContainer.AddTag(SprintTag);
				ASC->CancelAbilities(&SprintContainer);
			}
		}

		if (SlowGameplayEffectClass && !ActiveSlowEffectMap.Contains(Character))
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(GetOwner());
			FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(SlowGameplayEffectClass.GetDefaultObject(), 1.0f, Context);
			ActiveSlowEffectMap.Add(Character, Handle);
		}
	}
}

void UMOUWaterHazardComponent::RestoreCharacterMovement(ACharacter* Character)
{
	if (!Character)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character))
	{
		static const FGameplayTag BlockSprintTag = FGameplayTag::RequestGameplayTag(FName("Ability.Player.Block.Sprint"), false);
		if (BlockSprintTag.IsValid())
		{
			ASC->RemoveLooseGameplayTag(BlockSprintTag);
		}

		if (FActiveGameplayEffectHandle* Handle = ActiveSlowEffectMap.Find(Character))
		{
			ASC->RemoveActiveGameplayEffect(*Handle);
			ActiveSlowEffectMap.Remove(Character);
		}
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp)
	{
		if (float* OrigSpeed = OriginalSpeedMap.Find(Character))
		{
			MoveComp->MaxWalkSpeed = *OrigSpeed;
			OriginalSpeedMap.Remove(Character);
		}
	}
}

void UMOUWaterHazardComponent::ApplyDamageToCharacter(ACharacter* Character)
{
	if (!Character || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (DamageGameplayEffectClass)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character))
		{
			FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
			ContextHandle.AddSourceObject(GetOwner());
			ASC->ApplyGameplayEffectToSelf(DamageGameplayEffectClass.GetDefaultObject(), 1.0f, ContextHandle);
			return;
		}
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character))
	{
		UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("WaterDamageInstant")));
		DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
		DamageEffect->Modifiers.SetNum(1);

		FGameplayModifierInfo& HealthModifier = DamageEffect->Modifiers[0];
		HealthModifier.Attribute = UBaseAttributeSet::GetHealthAttribute();
		HealthModifier.ModifierOp = EGameplayModOp::Additive;
		HealthModifier.ModifierMagnitude = FScalableFloat(-DamagePerTick);

		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddSourceObject(GetOwner());
		ASC->ApplyGameplayEffectToSelf(DamageEffect, 1.0f, ContextHandle);
	}
	else
	{
		Character->TakeDamage(DamagePerTick, FDamageEvent(), nullptr, GetOwner());
	}
}

void UMOUWaterHazardComponent::OnDamageTick()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	for (int32 i = OverlappingCharacters.Num() - 1; i >= 0; --i)
	{
		ACharacter* Char = OverlappingCharacters[i].Get();
		if (Char)
		{
			ApplyDamageToCharacter(Char);
		}
		else
		{
			OverlappingCharacters.RemoveAt(i);
		}
	}

	if (OverlappingCharacters.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DamageTimerHandle);
		}
	}
}

void UMOUWaterHazardComponent::DrawDebugVisualizer()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	FBox BoundsBox(EForceInit::ForceInit);
	if (AWaterBody* WaterBody = Cast<AWaterBody>(Owner))
	{
		if (UWaterBodyComponent* WaterBodyComp = WaterBody->GetWaterBodyComponent())
		{
			BoundsBox = WaterBodyComp->GetCollisionComponentBounds();
		}
	}

	if (!BoundsBox.IsValid)
	{
		BoundsBox = Owner->GetComponentsBoundingBox(true);
	}

	FVector Center = BoundsBox.GetCenter();
	FVector Extent = BoundsBox.GetExtent();
	if (Extent.IsNearlyZero())
	{
		Extent = FVector(300.0f, 300.0f, 150.0f);
		Center = Owner->GetActorLocation();
	}

	FColor DisplayColor = bIsHazardActive ? FColor::Red : FColor::Green;
	DrawDebugBox(World, Center, Extent, DisplayColor, false, -1.0f, 0, 4.0f);

	FVector OwnerLoc = Owner->GetActorLocation();
	DrawDebugCoordinateSystem(World, OwnerLoc, Owner->GetActorRotation(), 100.0f, false, -1.0f, 0, 2.0f);

	FString StatusText = FString::Printf(
		TEXT("[%s] %s\nWater Z: %.1f | Threshold Z: %.1f\nSlow: %.2f | Overlapping: %d"),
		*Owner->GetName(),
		bIsHazardActive ? TEXT("HAZARD ACTIVE (DANGER)") : TEXT("HAZARD INACTIVE (SAFE)"),
		OwnerLoc.Z,
		DeactivationZThreshold,
		SlowSpeedMultiplier,
		OverlappingCharacters.Num()
	);

	DrawDebugString(World, Center + FVector(0.0f, 0.0f, Extent.Z + 40.0f), StatusText, nullptr, DisplayColor, 0.0f, true, 1.2f);
}
