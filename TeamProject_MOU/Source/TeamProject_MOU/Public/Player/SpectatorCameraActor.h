#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpectatorCameraActor.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class TEAMPROJECT_MOU_API ASpectatorCameraActor : public AActor
{
	GENERATED_BODY()

public:
	ASpectatorCameraActor();

	virtual void Tick(float DeltaTime) override;

	void AttachToTarget(AActor* InTarget);

	void AddOrbitInput(float PitchDelta, float YawDelta);

	void AddZoomInput(float WheelDelta);

	UFUNCTION(BlueprintPure, Category = "Spectator")
	AActor* GetSpectateTarget() const { return TargetActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Spectator")
	USpringArmComponent* GetSpringArm() const { return SpringArmComponent; }

	UFUNCTION(BlueprintPure, Category = "Spectator")
	UCameraComponent* GetCamera() const { return CameraComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectator|Zoom")
	float MinZoomDistance = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectator|Zoom")
	float MaxZoomDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectator|Zoom")
	float ZoomStep = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectator|Zoom")
	float ZoomInterpSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectator|Rotation")
	float OrbitPitchMin = -80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectator|Rotation")
	float OrbitPitchMax = 80.0f;

private:
	TWeakObjectPtr<AActor> TargetActor;

	float TargetArmLengthDesired = 350.0f;
	FRotator CurrentOrbitRotation = FRotator(-15.0f, 0.0f, 0.0f);
};
