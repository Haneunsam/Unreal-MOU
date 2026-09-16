#include "Player/SpectatorCameraActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

ASpectatorCameraActor::ASpectatorCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(SceneRoot);
	SpringArmComponent->TargetArmLength = 350.0f;
	SpringArmComponent->TargetOffset = FVector(0.0f, 0.0f, 60.0f);
	SpringArmComponent->SocketOffset = FVector::ZeroVector;
	SpringArmComponent->bDoCollisionTest = true;
	SpringArmComponent->ProbeChannel = ECC_WorldStatic;
	SpringArmComponent->ProbeSize = 8.0f;
	SpringArmComponent->bUsePawnControlRotation = false;
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->bInheritRoll = false;
	SpringArmComponent->bEnableCameraLag = true;
	SpringArmComponent->CameraLagSpeed = 15.0f;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;

	TargetArmLengthDesired = 350.0f;
	CurrentOrbitRotation = FRotator(-15.0f, 0.0f, 0.0f);
	SpringArmComponent->SetRelativeRotation(CurrentOrbitRotation);
}

void ASpectatorCameraActor::BeginPlay()
{
	Super::BeginPlay();
	if (SpringArmComponent)
	{
		SpringArmComponent->SetRelativeRotation(CurrentOrbitRotation);
	}
}

void ASpectatorCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TargetActor.IsValid())
	{
		SetActorLocation(TargetActor->GetActorLocation());
	}

	if (SpringArmComponent)
	{
		if (!FMath::IsNearlyEqual(SpringArmComponent->TargetArmLength, TargetArmLengthDesired, 1.0f))
		{
			SpringArmComponent->TargetArmLength = FMath::FInterpTo(
				SpringArmComponent->TargetArmLength,
				TargetArmLengthDesired,
				DeltaTime,
				ZoomInterpSpeed
			);
		}
	}
}

void ASpectatorCameraActor::AttachToTarget(AActor* InTarget)
{
	TargetActor = InTarget;
	if (InTarget)
	{
		SetActorLocation(InTarget->GetActorLocation());
	}
}

void ASpectatorCameraActor::AddOrbitInput(float PitchDelta, float YawDelta)
{
	CurrentOrbitRotation.Pitch = FMath::ClampAngle(CurrentOrbitRotation.Pitch + PitchDelta, OrbitPitchMin, OrbitPitchMax);
	CurrentOrbitRotation.Yaw = FRotator::NormalizeAxis(CurrentOrbitRotation.Yaw + YawDelta);

	if (SpringArmComponent)
	{
		SpringArmComponent->SetRelativeRotation(CurrentOrbitRotation);
	}
}

void ASpectatorCameraActor::AddZoomInput(float WheelDelta)
{
	TargetArmLengthDesired = FMath::Clamp(
		TargetArmLengthDesired - (WheelDelta * ZoomStep),
		MinZoomDistance,
		MaxZoomDistance
	);
}
