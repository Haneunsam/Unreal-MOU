// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gimmick/JumpPad.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AJumpPad::AJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(MeshComponent);
	TriggerBox->InitBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	ItemName = FText::FromString(TEXT("JumpPad"));
	ItemWeight = 10.0f;
	bCanBeStoredInInventory = false;
	bIsDeployed = true;
	bIsPushable = true;

	BoostedJumpZVelocity = 1300.0f;
	LaunchVelocityBonus = 0.0f;
	bAutoLaunchOnStep = false;

	MaterialSlotIndex = 0;
	ColorParameterName = FName("Color");
	DefaultColor = FLinearColor(0.1f, 0.4f, 0.8f, 1.0f);
	ActiveColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);
	ActiveColorDuration = 0.5f;
}

void AJumpPad::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AJumpPad::HandleTriggerBeginOverlap);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AJumpPad::HandleTriggerEndOverlap);
		TriggerBox->SetCollisionEnabled(bIsDeployed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (MeshComponent)
	{
		DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialSlotIndex);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(ColorParameterName, DefaultColor);
		}
	}
}

void AJumpPad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResetColorTimerHandle);
	}

	ClearAllOverlappingCharacters();

	Super::EndPlay(EndPlayReason);
}

void AJumpPad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJumpPad, bIsDeployed);
}

void AJumpPad::PickUp_Implementation(AActor* Picker)
{
	if (HasAuthority())
	{
		bIsDeployed = false;
	}

	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ClearAllOverlappingCharacters();

	Super::PickUp_Implementation(Picker);
}

void AJumpPad::Drop_Implementation(FVector DropLocation, AActor* Dropper)
{
	Super::Drop_Implementation(DropLocation, Dropper);

	if (HasAuthority())
	{
		bIsDeployed = true;
	}

	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AJumpPad::Throw_Implementation(FVector ThrowVelocity, AActor* Thrower)
{
	Super::Throw_Implementation(ThrowVelocity, Thrower);

	if (HasAuthority())
	{
		bIsDeployed = true;
	}

	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AJumpPad::OnRep_IsDeployed()
{
	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(bIsDeployed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (!bIsDeployed)
	{
		ClearAllOverlappingCharacters();
	}
}

void AJumpPad::ClearAllOverlappingCharacters()
{
	for (auto It = OriginalJumpZVelocityMap.CreateIterator(); It; ++It)
	{
		if (ACharacter* Char = It.Key().Get())
		{
			if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
			{
				MoveComp->JumpZVelocity = It.Value();
			}
			Char->MovementModeChangedDelegate.RemoveDynamic(this, &AJumpPad::HandleMovementModeChanged);
		}
	}

	OriginalJumpZVelocityMap.Empty();
	OverlappingCharacters.Empty();
}

void AJumpPad::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsDeployed || !OtherActor || OtherActor == this)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (!OverlappingCharacters.Contains(Character))
	{
		OverlappingCharacters.Add(Character);
	}

	if (!OriginalJumpZVelocityMap.Contains(Character))
	{
		OriginalJumpZVelocityMap.Add(Character, MoveComp->JumpZVelocity);
	}

	MoveComp->JumpZVelocity = BoostedJumpZVelocity;
	Character->MovementModeChangedDelegate.AddUniqueDynamic(this, &AJumpPad::HandleMovementModeChanged);

	if (bAutoLaunchOnStep)
	{
		ActivateJumpPad(Character);
	}
}

void AJumpPad::HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
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

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp)
	{
		if (float* OrigJump = OriginalJumpZVelocityMap.Find(Character))
		{
			MoveComp->JumpZVelocity = *OrigJump;
			OriginalJumpZVelocityMap.Remove(Character);
		}
	}

	Character->MovementModeChangedDelegate.RemoveDynamic(this, &AJumpPad::HandleMovementModeChanged);
	OverlappingCharacters.Remove(Character);
}

void AJumpPad::HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (!bIsDeployed || !Character || !OverlappingCharacters.Contains(Character))
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (PrevMovementMode == MOVE_Walking && MoveComp->MovementMode == MOVE_Falling && Character->GetVelocity().Z > 50.0f)
	{
		ActivateJumpPad(Character);
	}
}

void AJumpPad::ActivateJumpPad(ACharacter* Character)
{
	if (!bIsDeployed || !Character)
	{
		return;
	}

	if (LaunchVelocityBonus > 0.0f)
	{
		Character->LaunchCharacter(FVector(0.0f, 0.0f, LaunchVelocityBonus), false, false);
	}

	if (HasAuthority())
	{
		MulticastPlayActivationEffects(GetActorLocation());
	}
	else
	{
		MulticastPlayActivationEffects_Implementation(GetActorLocation());
	}

	OnJumpPadActivated.Broadcast(Character);
	OnJumpPadTriggered(Character);
}

void AJumpPad::MulticastPlayActivationEffects_Implementation(FVector Location)
{
	if (ActivationSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ActivationSound, Location);
	}

	if (ActiveMaterialOverride && MeshComponent)
	{
		MeshComponent->SetMaterial(MaterialSlotIndex, ActiveMaterialOverride);
	}
	else if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(ColorParameterName, ActiveColor);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResetColorTimerHandle);
		World->GetTimerManager().SetTimer(
			ResetColorTimerHandle,
			this,
			&AJumpPad::ResetMaterialColor,
			ActiveColorDuration,
			false
		);
	}
}

void AJumpPad::ResetMaterialColor()
{
	if (DynamicMaterial)
	{
		if (ActiveMaterialOverride && MeshComponent)
		{
			MeshComponent->SetMaterial(MaterialSlotIndex, DynamicMaterial);
		}
		DynamicMaterial->SetVectorParameterValue(ColorParameterName, DefaultColor);
	}
}
