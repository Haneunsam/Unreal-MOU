#include "AI/NPCPerceptionBlueprintLibrary.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Data/NPCData.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
bool IsObservedByAnyPlayer(const UNPCData* NPCData, AActor* ObservedActor)
{
	const UWorld* World = IsValid(ObservedActor) ? ObservedActor->GetWorld() : nullptr;
	if (!IsValid(NPCData) || !World)
	{
		return false;
	}

	const float ObservationRadius = FMath::Max(NPCData->SightRadius, NPCData->LoseSightRadius);
	const float ObservationRadiusSquared = FMath::Square(ObservationRadius);

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!IsValid(PlayerPawn))
		{
			continue;
		}

		if (FVector::DistSquared(PlayerPawn->GetActorLocation(), ObservedActor->GetActorLocation())
			> ObservationRadiusSquared)
		{
			continue;
		}

		if (UNPCPerceptionBlueprintLibrary::IsTargetLookingAtActor(
			PlayerPawn,
			ObservedActor,
			NPCData->ObservedViewDotThreshold,
			NPCData->RequireLineOfSightForObservation))
		{
			return true;
		}
	}

	return false;
}
}

bool UNPCPerceptionBlueprintLibrary::ApplySightConfig(
	UAIPerceptionComponent* PerceptionComponent,
	float SightRadius,
	float LoseSightRadius)
{
	if (!PerceptionComponent)
	{
		return false;
	}

	UAISenseConfig* SenseConfig = PerceptionComponent->GetSenseConfig(UAISense::GetSenseID<UAISense_Sight>());
	UAISenseConfig_Sight* SightConfig = Cast<UAISenseConfig_Sight>(SenseConfig);
	if (!SightConfig)
	{
		return false;
	}

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(LoseSightRadius, SightRadius);

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
	PerceptionComponent->RequestStimuliListenerUpdate();

	return true;
}

bool UNPCPerceptionBlueprintLibrary::IsTargetLookingAtActor(
	AActor* TargetActor,
	AActor* ObservedActor,
	float ViewDotThreshold,
	bool bRequireLineOfSight)
{
	if (!IsValid(TargetActor) || !IsValid(ObservedActor) || TargetActor == ObservedActor)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	TargetActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector ToObservedActor = ObservedActor->GetActorLocation() - ViewLocation;
	const FVector DirectionToObservedActor = ToObservedActor.GetSafeNormal();
	if (DirectionToObservedActor.IsNearlyZero())
	{
		return true;
	}

	const float ViewDot = FVector::DotProduct(ViewRotation.Vector(), DirectionToObservedActor);
	if (ViewDot < FMath::Clamp(ViewDotThreshold, -1.0f, 1.0f))
	{
		return false;
	}

	if (!bRequireLineOfSight)
	{
		return true;
	}

	const UWorld* World = TargetActor->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NPCTargetObservation), false, TargetActor);
	QueryParams.AddIgnoredActor(TargetActor);

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		ObservedActor->GetActorLocation(),
		ECC_Visibility,
		QueryParams);

	return !bHit || HitResult.GetActor() == ObservedActor;
}

bool UNPCPerceptionBlueprintLibrary::CanChaseTarget(
	const UNPCData* NPCData,
	AActor* ControlledPawn,
	AActor* TargetActor)
{
	if (!IsValid(NPCData) || !IsValid(ControlledPawn) || !IsValid(TargetActor))
	{
		return false;
	}

	switch (NPCData->TrackingMovementPolicy)
	{
	case ENPCTrackingMovementPolicy::Stationary:
		return false;
	case ENPCTrackingMovementPolicy::ChaseWhenNotObserved:
		return !IsObservedByAnyPlayer(NPCData, ControlledPawn);
	case ENPCTrackingMovementPolicy::AlwaysChase:
	default:
		return true;
	}
}

// [NPCMOVE-000] NPCData의 정찰 또는 추적 속도를 캐릭터 이동 컴포넌트에 적용한다.
bool UNPCPerceptionBlueprintLibrary::ApplyMovementSpeed(
	const UNPCData* NPCData,
	AActor* ControlledPawn,
	bool bIsChasing)
{
	ACharacter* Character = Cast<ACharacter>(ControlledPawn);
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!IsValid(NPCData) || !MovementComponent)
	{
		return false;
	}

	MovementComponent->MaxWalkSpeed = FMath::Max(
		0.0f,
		bIsChasing ? NPCData->ChaseMoveSpeed : NPCData->PatrolMoveSpeed);
	return true;
}
