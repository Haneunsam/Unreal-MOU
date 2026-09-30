// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "Cannon.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UArrowComponent;
class AMainCharacter;
class UCameraComponent;

UCLASS()
class TEAMPROJECT_MOU_API ACannon : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ACannon();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// 변수를 네트워크로 보냄
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// =========================================================
	// Components
	// =========================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<USceneComponent> CannonRoot;

	// 대포 하단 고정 몸체
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<UStaticMeshComponent> CannonBaseMesh;

	// 좌우 회전 Pivot
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<USceneComponent> YawPivot;

	// 상하 회전 Pivot
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<USceneComponent> PitchPivot;

	// 실제 포신
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<UStaticMeshComponent> CannonBarrelMesh;

	// 발사가 시작되는 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<USceneComponent> LaunchPoint;

	// 발사 방향 확인용 Arrow
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<UArrowComponent> LaunchArrow;

	// 발사될 플레이어 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<USceneComponent> SeatPoint;

	// 탑승 해제 후 이동시킬 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<USceneComponent> ExitPoint;

	// 대포 탑승자 전용 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cannon|Components")
	TObjectPtr<UCameraComponent> PassengerCamera;

public:
	// =========================================================
	// Multiplayer State
	// =========================================================
	// 현재 대포 안에 들어가 있는 플레이어
	// 서버에서만 값이 변경되고,변경된 Passenger 값은 모든 클라이언트에 Replication
	// ReplicatedUsing을 사용했기 때문에 클라이언트에서 값이 갱신되면 OnRep_Passenger()가 자동 호출
	UPROPERTY(ReplicatedUsing = OnRep_Passenger,BlueprintReadOnly,Category = "Cannon|State")
	TObjectPtr<AMainCharacter> Passenger;

	// 현재 대포를 조작하고 있는 플레이어
	// Passenger와 Operator가 같은 플레이어가 되지 않도록 TryStartOperating()에서 검사
	UPROPERTY(ReplicatedUsing = OnRep_Operator,BlueprintReadOnly,Category = "Cannon|State")
	TObjectPtr<AMainCharacter> Operator;

	// 조작자가 대포에서 이 거리 이상 멀어지면 자동으로 조작 해제
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Operator")
	float OperatorMaxDistance = 350.0f;

	// 플레이어 카메라 ↔ 대포 카메라 전환 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Camera")
	float PassengerCameraBlendTime = 0.25f;

	UPROPERTY(Transient)
	TObjectPtr<AMainCharacter> LocalPassengerViewCharacter;

	bool bLocalPassengerViewActive = false;

	// =========================================================
	// Cannon Aim
	// =========================================================
	// 좌 / 우 조준 제한
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Aim")
	float MinAimYaw = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Aim")
	float MaxAimYaw = 15.0f;

	// 상 / 하 조준 제한
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Aim")
	float MinAimPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Aim")
	float MaxAimPitch = 20.0f;

	// Operator 마우스 움직임에 대한 대포 조준 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Aim")
	float AimSensitivity = 1.0f;

	// 포신이 목표 각도로 따라가는 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Aim")
	float AimInterpSpeed = 10.0f;

	// 현재 좌우 조준 각도
	UPROPERTY(ReplicatedUsing = OnRep_AimRotation, BlueprintReadOnly, Category = "Cannon|Aim")
	float CurrentAimYaw = 0.0f;

	// 현재 상하 조준 각도
	UPROPERTY(ReplicatedUsing = OnRep_AimRotation, BlueprintReadOnly, Category = "Cannon|Aim")
	float CurrentAimPitch = 0.0f;

	// =========================================================
	// Cannon Launch / Trajectory
	// =========================================================
	// 실제 발사할 때도 이 값을 그대로 사용할 예정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Launch")
	float LaunchSpeed = 2400.0f;

	// 궤적을 몇 초까지 계산할지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Trajectory")
	float TrajectoryMaxSimTime = 3.0f;

	// 궤적 계산 정밀도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Trajectory")
	float TrajectorySimFrequency = 20.0f;

	// 플레이어 충돌 크기를 대략 반영
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Trajectory")
	float TrajectoryRadius = 34.0f;

	// 디버그 궤적 표시 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Trajectory")
	bool bShowTrajectoryPreview = true;

	// Operator가 처음 조작을 시작했을 때 바라보던 방향
	FRotator OperatorStartControlRotation = FRotator::ZeroRotator;

	// 조작 시작 당시 대포 각도
	float OperatorStartAimYaw = 0.0f;
	float OperatorStartAimPitch = 0.0f;

	// BP_Cannon에서 설정한 Pivot 기본 회전값
	FRotator InitialYawPivotRotation = FRotator::ZeroRotator;
	FRotator InitialPitchPivotRotation = FRotator::ZeroRotator;

public:
	// =========================================================
	// Interaction Interface
	// =========================================================
	// 지금 플레이어가 이 대포에 상호작용이 가능한가 판단 여부 함수
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	// 실제 대포 상호작용을 결정하는 함수
	virtual void Interact_Implementation(AActor* Interactor) override;

public:
	// =========================================================
	// Passenger
	// =========================================================
	// 실제 탑승처리 함수
	UFUNCTION(BlueprintCallable, Category = "Cannon|Passenger")
	bool TryEnterPassenger(AMainCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Cannon|Passenger")
	void ReleasePassenger(bool bMoveToExit = true);

	UFUNCTION(BlueprintPure, Category = "Cannon|Passenger")
	bool HasPassenger() const;


public:
	// =========================================================
	// Operator
	// =========================================================
	UFUNCTION(BlueprintCallable, Category = "Cannon|Operator")
	bool TryStartOperating(AMainCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Cannon|Operator")
	void ReleaseOperator();

	UFUNCTION(BlueprintPure, Category = "Cannon|Operator")
	bool HasOperator() const;

public:
	// =========================================================
	// Fire
	// =========================================================
	// 실제 서버 발사
	UFUNCTION(Server, Reliable)
	void ServerFireCannon();

	// 모든 플레이어에게 발사 연출 재생
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFireEffects();

protected:
	// =========================================================
	// Fire
	// =========================================================
	// 좌클릭 입력
	void HandleFireInput();

	// 서버에서 실제 Passenger 발사 처리
	void FireCannon();

	// 로컬 Operator에게 발사 입력 활성화
	void ApplyLocalOperatorInput(AMainCharacter* Character);

	// Operator 발사 입력 복구
	void RestoreLocalOperatorInput();

	// 포구에서 살짝 앞으로 빼고 발사
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Launch")
	float LaunchStartOffset = 120.0f;

	// 로컬 입력 상태
	UPROPERTY(Transient)
	TObjectPtr<AMainCharacter> LocalOperatorInputCharacter;

	bool bLocalOperatorInputActive = false;
	bool bFireInputBound = false;

	// =========================================================
	// Launched Passenger Movement
	// =========================================================
	UPROPERTY(Transient)
	TObjectPtr<AMainCharacter> LaunchedPassenger;

	bool bLaunchMovementSettingsSaved = false;

	float SavedFallingLateralFriction = 0.0f;
	float SavedBrakingDecelerationFalling = 0.0f;
	float SavedAirControl = 0.0f;

	bool SavedUseSeparateBrakingFriction = false;
	float SavedBrakingFriction = 0.0f;


protected:
	// =========================================================
	// RepNotify
	// =========================================================
	UFUNCTION()
	void OnRep_Passenger();

	UFUNCTION()
	void OnRep_Operator();

	UFUNCTION()
	void OnRep_AimRotation();

	// =========================================================
	// Helpers
	// =========================================================
	// 로컬 탑승자에게 대포 카메라 적용 + 입력 차단
	void ApplyLocalPassengerView(AMainCharacter* Character);

	// 원래 플레이어 카메라 복구 + 입력 복구
	void RestoreLocalPassengerView();

	// 탑승 중인 플레이어를 SeatPoint에 정확히 고정
	void LockPassengerToSeat();

	// Operator의 시선을 이용해 대포 조준 갱신
	void UpdateCannonAim(float DeltaTime);

	// 현재 조준값을 YawPivot / PitchPivot에 실제 적용
	void ApplyCannonAim();

	// Operator에게 예상 발사 궤적 표시
	void UpdateTrajectoryPreview();

	void ApplyLaunchMovementSettings(AMainCharacter* Character);
	void UpdateLaunchedPassenger();
	void RestoreLaunchMovementSettings();

protected:
	bool CanCharacterUseCannon(AMainCharacter* Character) const;

	void CleanupInvalidUsers();

public:
	// =========================================================
	// Blueprint Events
	// =========================================================
	UFUNCTION(BlueprintImplementableEvent, Category = "Cannon|Event")
	void OnPassengerChanged(AMainCharacter* NewPassenger);

	UFUNCTION(BlueprintImplementableEvent, Category = "Cannon|Event")
	void OnOperatorChanged(AMainCharacter* NewOperator);

	// BP_Cannon에서 사운드 / Niagara 처리
	UFUNCTION(BlueprintImplementableEvent, Category = "Cannon|Event")
	void OnCannonFired();
};