#include "Components/NPCTargetLockComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"

UNPCTargetLockComponent::UNPCTargetLockComponent()
	: SelectionPolicy(ENPCTargetSelectionPolicy::LockUntilLost)
	, TargetLoseGraceTime(2.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
	RequiredTargetTag = FGameplayTag::RequestGameplayTag(TEXT("Character.Player"), false);
}

void UNPCTargetLockComponent::InitializeTargetLock(
	UAIPerceptionComponent* InPerceptionComponent,
	ENPCTargetSelectionPolicy InSelectionPolicy,
	float InTargetLoseGraceTime)
{
	if (PerceptionComponent != InPerceptionComponent)
	{
		if (PerceptionComponent)
		{
			PerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
				this, &UNPCTargetLockComponent::HandleTargetPerceptionUpdated);
		}

		PerceptionComponent = InPerceptionComponent;
		if (PerceptionComponent)
		{
			PerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
				this, &UNPCTargetLockComponent::HandleTargetPerceptionUpdated);
		}
	}

	SelectionPolicy = InSelectionPolicy;
	TargetLoseGraceTime = FMath::Max(0.0f, InTargetLoseGraceTime);
}

void UNPCTargetLockComponent::ClearTarget()
{
	CancelTargetLoseTimer();
	SetCurrentTarget(nullptr);
}

void UNPCTargetLockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelTargetLoseTimer();

	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
			this, &UNPCTargetLockComponent::HandleTargetPerceptionUpdated);
	}

	if (CurrentTarget)
	{
		CurrentTarget->OnDestroyed.RemoveDynamic(this, &UNPCTargetLockComponent::HandleCurrentTargetDestroyed);
	}

	Super::EndPlay(EndPlayReason);
}

void UNPCTargetLockComponent::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Actor ||
		Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (!IsEligibleTarget(Actor))
		{
			return;
		}

		if (Actor == CurrentTarget)
		{
			CancelTargetLoseTimer();
			return;
		}

		if (!IsValid(CurrentTarget))
		{
			SetCurrentTarget(Actor);
			return;
		}

		if (SelectionPolicy == ENPCTargetSelectionPolicy::NearestDynamic)
		{
			SetCurrentTarget(FindBestPerceivedTarget());
		}
		return;
	}

	if (Actor == CurrentTarget)
	{
		StartTargetLoseTimer();
	}
}

void UNPCTargetLockComponent::HandleCurrentTargetDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor != CurrentTarget)
	{
		return;
	}

	CancelTargetLoseTimer();
	SetCurrentTarget(FindBestPerceivedTarget());
}

void UNPCTargetLockComponent::SetCurrentTarget(AActor* NewTarget)
{
	if (NewTarget == CurrentTarget)
	{
		return;
	}

	AActor* PreviousTarget = CurrentTarget;
	if (PreviousTarget)
	{
		PreviousTarget->OnDestroyed.RemoveDynamic(this, &UNPCTargetLockComponent::HandleCurrentTargetDestroyed);
	}

	CancelTargetLoseTimer();
	CurrentTarget = NewTarget;

	if (CurrentTarget)
	{
		CurrentTarget->OnDestroyed.AddUniqueDynamic(this, &UNPCTargetLockComponent::HandleCurrentTargetDestroyed);
	}

	OnTargetChanged.Broadcast(PreviousTarget, CurrentTarget);
}

void UNPCTargetLockComponent::StartTargetLoseTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		ResolveTargetLoss();
		return;
	}

	World->GetTimerManager().ClearTimer(TargetLoseTimerHandle);
	if (TargetLoseGraceTime <= 0.0f)
	{
		ResolveTargetLoss();
		return;
	}

	World->GetTimerManager().SetTimer(
		TargetLoseTimerHandle,
		this,
		&UNPCTargetLockComponent::ResolveTargetLoss,
		TargetLoseGraceTime,
		false);
}

void UNPCTargetLockComponent::CancelTargetLoseTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetLoseTimerHandle);
	}
}

void UNPCTargetLockComponent::ResolveTargetLoss()
{
	if (IsCurrentlyPerceived(CurrentTarget))
	{
		return;
	}

	SetCurrentTarget(FindBestPerceivedTarget());
}

AActor* UNPCTargetLockComponent::FindBestPerceivedTarget() const
{
	if (!PerceptionComponent)
	{
		return nullptr;
	}

	TArray<AActor*> PerceivedActors;
	PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	const FVector Origin = GetSelectionOrigin();
	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (AActor* Candidate : PerceivedActors)
	{
		if (!IsEligibleTarget(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UNPCTargetLockComponent::IsEligibleTarget(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (!RequiredTargetTag.IsValid())
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Actor));
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(RequiredTargetTag);
}

bool UNPCTargetLockComponent::IsCurrentlyPerceived(const AActor* Actor) const
{
	if (!PerceptionComponent || !IsValid(Actor))
	{
		return false;
	}

	FActorPerceptionBlueprintInfo PerceptionInfo;
	if (!PerceptionComponent->GetActorsPerception(const_cast<AActor*>(Actor), PerceptionInfo))
	{
		return false;
	}

	const FAISenseID SightSenseId = UAISense::GetSenseID<UAISense_Sight>();
	for (const FAIStimulus& Stimulus : PerceptionInfo.LastSensedStimuli)
	{
		if (Stimulus.Type == SightSenseId && Stimulus.WasSuccessfullySensed())
		{
			return true;
		}
	}

	return false;
}

FVector UNPCTargetLockComponent::GetSelectionOrigin() const
{
	if (const AAIController* AIController = Cast<AAIController>(GetOwner()))
	{
		if (const APawn* ControlledPawn = AIController->GetPawn())
		{
			return ControlledPawn->GetActorLocation();
		}
	}

	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}
