#include "Traps/Actors/TrapBase.h"
#include "Components/BoxComponent.h"
#include "Traps/Components/TrapTriggerComponent.h"
#include "Traps/Components/TrapPayloadComponent.h"
#include "Traps/Data/TrapDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

ATrapBase::ATrapBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootScene->SetMobility(EComponentMobility::Movable);
	SetRootComponent(RootScene);

	BaseTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BaseTriggerBox"));
	BaseTriggerBox->SetupAttachment(RootScene);
	BaseTriggerBox->InitBoxExtent(FVector(80.0f, 80.0f, 40.0f));
	BaseTriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BaseTriggerBox->SetGenerateOverlapEvents(true);

	TriggerComponent = CreateDefaultSubobject<UTrapTriggerComponent>(TEXT("TriggerComponent"));
	PayloadComponent = CreateDefaultSubobject<UTrapPayloadComponent>(TEXT("PayloadComponent"));
}

void ATrapBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATrapBase, CurrentState);
}

void ATrapBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld() && bSnapToGroundInEditor)
	{
		SnapToGround();
	}
}

bool ATrapBase::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TraceStart = CurrentLocation + FVector(0.0f, 0.0f, GroundTraceUpOffset);
	const FVector TraceEnd = CurrentLocation - FVector(0.0f, 0.0f, GroundTraceDownOffset);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TrapSnapToGround), false, this);
	QueryParams.bTraceComplex = true;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	bool bHit = World->LineTraceSingleByObjectType(
		HitResult,
		TraceStart,
		TraceEnd,
		ObjectQueryParams,
		QueryParams
	);

	if (!bHit || !HitResult.bBlockingHit)
	{
		bHit = World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams
		);
	}

	if (!bHit || !HitResult.bBlockingHit)
	{
		return false;
	}

	if (RootComponent && RootComponent->Mobility != EComponentMobility::Movable)
	{
		RootComponent->SetMobility(EComponentMobility::Movable);
	}

	const FVector SurfaceNormal = HitResult.ImpactNormal;
	const FVector TargetLocation = HitResult.ImpactPoint + (SurfaceNormal * FloorClearanceOffset) + GroundPlacementOffset;
	SetActorLocation(TargetLocation);

	if (bAlignToGroundNormal)
	{
		const FVector CurrentForward = GetActorForwardVector();
		FVector AlignedForward = FVector::VectorPlaneProject(CurrentForward, SurfaceNormal).GetSafeNormal();
		if (AlignedForward.IsNearlyZero())
		{
			AlignedForward = FVector::VectorPlaneProject(FVector::ForwardVector, SurfaceNormal).GetSafeNormal();
		}
		const FVector AlignedRight = FVector::CrossProduct(SurfaceNormal, AlignedForward).GetSafeNormal();
		const FMatrix RotationMatrix(AlignedForward, AlignedRight, SurfaceNormal, FVector::ZeroVector);
		SetActorRotation(RotationMatrix.Rotator());
	}

	return true;
}

void ATrapBase::BeginPlay()
{
	Super::BeginPlay();

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Prim : PrimitiveComponents)
	{
		if (IsValid(Prim) && Prim->IsSimulatingPhysics())
		{
			Prim->SetSimulatePhysics(false);
		}
	}

	if (HasAuthority() && bSnapToGroundOnBeginPlay)
	{
		SnapToGround();
	}

	if (TriggerComponent)
	{
		if (BaseTriggerBox)
		{
			TriggerComponent->RegisterDetectionVolume(BaseTriggerBox);
		}

		UpdateTriggerBoxCollision();

		TriggerComponent->OnTriggerActivated.AddDynamic(this, &ATrapBase::HandleTriggerActivated);
		TriggerComponent->OnTriggerDeactivated.AddDynamic(this, &ATrapBase::HandleTriggerDeactivated);
	}
}

void ATrapBase::UpdateTriggerBoxCollision()
{
	if (!BaseTriggerBox || !TriggerComponent)
	{
		return;
	}

	// 모든 모드(압력판, 주기 타이머, 레이저 연동)에서 위에 서 있는 대상을 실시간 감지할 수 있도록 콜리전 활성화
	BaseTriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BaseTriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BaseTriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BaseTriggerBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BaseTriggerBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	BaseTriggerBox->SetGenerateOverlapEvents(true);
}

void ATrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ATrapBase::TriggerTrap_Implementation(AActor* InstigatorActor)
{
	if (HasAuthority() && CanActivate())
	{
		LastInstigator = InstigatorActor;

		float Delay = TrapData ? TrapData->ActivationDelay : 0.0f;
		if (Delay > 0.0f)
		{
			SetTrapState(ETrapState::Warning);
		}
		else
		{
			SetTrapState(ETrapState::Active);
		}
	}
}

void ATrapBase::ResetTrap_Implementation()
{
	if (HasAuthority())
	{
		SetTrapState(ETrapState::Idle);
	}
}

void ATrapBase::DisarmTrap_Implementation(AActor* Disarmer)
{
	if (HasAuthority())
	{
		SetTrapState(ETrapState::Disarmed);
	}
}

bool ATrapBase::CanActivate() const
{
	return CurrentState == ETrapState::Idle;
}

void ATrapBase::SetTrapState(ETrapState NewState)
{
	if (!HasAuthority() || CurrentState == NewState)
	{
		return;
	}

	ETrapState PrevState = CurrentState;
	CurrentState = NewState;

	OnStateEntered(NewState);
	OnTrapStateChanged_BP(NewState, PrevState);
	MulticastPlayStateFX(NewState);
}

void ATrapBase::OnRep_TrapState(ETrapState PreviousState)
{
	OnStateEntered(CurrentState);
	OnTrapStateChanged_BP(CurrentState, PreviousState);
}

void ATrapBase::OnStateEntered(ETrapState NewState)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(StateTimerHandle);
	}

	switch (NewState)
	{
	case ETrapState::Idle:
		break;

	case ETrapState::Warning:
		OnTrapWarning_BP();
		if (HasAuthority() && GetWorld())
		{
			float Delay = TrapData ? TrapData->ActivationDelay : 0.5f;
			GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &ATrapBase::OnWarningTimerExpired, Delay, false);
		}
		break;

	case ETrapState::Active:
		OnTrapActivated_BP();
		if (HasAuthority())
		{
			ExecuteTrapPayload();
			if (GetWorld())
			{
				float Duration = TrapData ? TrapData->ActiveDuration : 1.0f;
				GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &ATrapBase::OnActiveTimerExpired, Duration, false);
			}
		}
		break;

	case ETrapState::Cooldown:
		OnTrapCooldown_BP();
		if (HasAuthority() && GetWorld())
		{
			float Cooldown = TrapData ? TrapData->CooldownDuration : 3.0f;
			GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &ATrapBase::OnCooldownTimerExpired, Cooldown, false);
		}
		break;

	case ETrapState::Disarmed:
		OnTrapDisarmed_BP();
		if (TriggerComponent)
		{
			TriggerComponent->SetTriggerEnabled(false);
		}
		break;
	}
}

void ATrapBase::OnWarningTimerExpired()
{
	if (HasAuthority())
	{
		SetTrapState(ETrapState::Active);
	}
}

void ATrapBase::OnActiveTimerExpired()
{
	if (HasAuthority())
	{
		float Cooldown = TrapData ? TrapData->CooldownDuration : 0.0f;
		if (Cooldown > 0.0f)
		{
			SetTrapState(ETrapState::Cooldown);
		}
		else
		{
			SetTrapState(ETrapState::Idle);
		}
	}
}

void ATrapBase::OnCooldownTimerExpired()
{
	if (HasAuthority())
	{
		SetTrapState(ETrapState::Idle);
	}
}

void ATrapBase::ExecuteTrapPayload()
{
	if (!PayloadComponent || !TriggerComponent)
	{
		return;
	}

	const TArray<AActor*>& Overlapped = TriggerComponent->GetOverlappingActors();
	if (Overlapped.Num() > 0)
	{
		PayloadComponent->ExecutePayloadOnActors(Overlapped, TrapData);
	}
}

void ATrapBase::HandleTriggerActivated(AActor* InstigatorActor)
{
	TriggerTrap_Implementation(InstigatorActor);
}

void ATrapBase::HandleTriggerDeactivated()
{
	// 필요 시 서브클래스에서 구현
}

void ATrapBase::MulticastPlayStateFX_Implementation(ETrapState StateToPlay)
{
	if (!TrapData || !GetWorld())
	{
		return;
	}

	FVector SpawnLoc = GetActorLocation();

	switch (StateToPlay)
	{
	case ETrapState::Warning:
		if (TrapData->WarningFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), TrapData->WarningFX, SpawnLoc);
		}
		if (TrapData->WarningSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, TrapData->WarningSound, SpawnLoc);
		}
		break;

	case ETrapState::Active:
		if (TrapData->ActivateFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), TrapData->ActivateFX, SpawnLoc);
		}
		if (TrapData->ActivateSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, TrapData->ActivateSound, SpawnLoc);
		}
		break;

	case ETrapState::Disarmed:
		if (TrapData->DisarmSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, TrapData->DisarmSound, SpawnLoc);
		}
		break;

	default:
		break;
	}
}
