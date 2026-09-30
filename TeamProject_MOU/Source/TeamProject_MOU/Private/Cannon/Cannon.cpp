// Fill out your copyright notice in the Description page of Project Settings.

#include "Cannon/Cannon.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "Player/MainCharacter.h"
#include "Components/InteractionComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"

#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

ACannon::ACannon()
{
	PrimaryActorTick.bCanEverTick = true;

	// 캐릭터 Tick에서 회전이 변경된 뒤
	// 마지막에 Passenger 위치/회전을 다시 SeatPoint에 고정
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// 네트워크 복제
	bReplicates = true;
	SetReplicateMovement(true);

	// =========================================================
	// Root
	// =========================================================
	CannonRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CannonRoot"));
	SetRootComponent(CannonRoot);

	// =========================================================
	// Base Mesh
	// =========================================================
	CannonBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CannonBaseMesh"));
	CannonBaseMesh->SetupAttachment(CannonRoot);

	// =========================================================
	// Yaw Pivot
	// 좌 / 우 회전 담당
	// =========================================================
	YawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("YawPivot"));
	YawPivot->SetupAttachment(CannonRoot);

	// =========================================================
	// Passenger Camera
	// 탑승자 전용 대포 카메라
	// 좌우 조준은 따라가지만 포신의 상하 기울기까지 따라가진 않음
	// =========================================================
	PassengerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PassengerCamera"));
	PassengerCamera->SetupAttachment(YawPivot);

	// =========================================================
	// Pitch Pivot
	// 위 / 아래 회전 담당
	// =========================================================
	PitchPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PitchPivot"));
	PitchPivot->SetupAttachment(YawPivot);

	// =========================================================
	// Barrel
	// =========================================================
	CannonBarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CannonBarrelMesh"));
	CannonBarrelMesh->SetupAttachment(PitchPivot);

	// =========================================================
	// Launch Point
	// =========================================================
	LaunchPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaunchPoint"));
	LaunchPoint->SetupAttachment(PitchPivot);

	// =========================================================
	// Launch Arrow
	// =========================================================
	LaunchArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("LaunchArrow"));
	LaunchArrow->SetupAttachment(LaunchPoint);

	// =========================================================
	// Passenger Seat
	// =========================================================
	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(PitchPivot);

	// =========================================================
	// Exit Point
	// =========================================================
	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(CannonRoot);
}

void ACannon::BeginPlay()
{
	Super::BeginPlay();

	// BP_Cannon에서 설정한 Pivot의 기본 회전값을 저장
	if (YawPivot)
	{
		InitialYawPivotRotation = YawPivot->GetRelativeRotation();
	}

	if (PitchPivot)
	{
		InitialPitchPivotRotation = PitchPivot->GetRelativeRotation();
	}

	// 초기 조준 각도 적용
	ApplyCannonAim();
}


void ACannon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 대포가 삭제되더라도 플레이어가 붙은 채로 남지 않게 처리
	if (HasAuthority())
	{
		ReleasePassenger(true);
		ReleaseOperator();
	}
	Super::EndPlay(EndPlayReason);
}


void ACannon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// =========================================================
	// Operator 조준
	// =========================================================
	if (HasAuthority() && Operator)
	{
		UpdateCannonAim(DeltaTime);
	}

	// =========================================================
	// 궤적 미리보기
	// Operator 본인 화면에서만 표시
	// =========================================================
	if (Operator && Operator->IsLocallyControlled())
	{
		UpdateTrajectoryPreview();
	}

	// =========================================================
	// Passenger Seat 고정
	// =========================================================
	if (Passenger)
	{
		LockPassengerToSeat();
	}

	// =========================================================
	// 서버 상태 정리
	// =========================================================
	if (HasAuthority())
	{
		CleanupInvalidUsers();
		UpdateLaunchedPassenger();
	}
}

// =========================================================
// Replication
// =========================================================
void ACannon::GetLifetimeReplicatedProps( TArray<FLifetimeProperty>& OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACannon, Passenger);
	DOREPLIFETIME(ACannon, Operator);
	DOREPLIFETIME(ACannon, CurrentAimYaw);
	DOREPLIFETIME(ACannon, CurrentAimPitch);
}


// =========================================================
// Interaction
// =========================================================
bool ACannon::CanInteract_Implementation(AActor* Interactor) const
{
	AMainCharacter* Character = Cast<AMainCharacter>(Interactor);

	if (!Character)
	{
		return false;
	}

	// 죽거나 그로기면 대포 사용 불가
	if (Character->IsDead() || Character->IsGroggy())
	{
		return false;
	}

	// 본인이 이미 탑승 중
	// → 다시 상호작용해서 탑승 해제 가능
	if (Passenger == Character)
	{
		return true;
	}

	// 본인이 이미 조작 중
	// → 다시 상호작용해서 조작 해제 가능
	if (Operator == Character)
	{
		return true;
	}

	// 빈 대포
	if (!Passenger)
	{
		return true;
	}

	// 탑승자는 있지만 조작자가 없음
	if (!Operator && Passenger != Character)
	{
		return true;
	}

	return false;
}


void ACannon::Interact_Implementation(AActor* Interactor)
{
	AMainCharacter* Character = Cast<AMainCharacter>(Interactor);

	if (!Character)
	{
		return;
	}

	// =========================================================
	// 클라이언트에서 상호작용한 경우
	// Cannon 자체는 클라이언트가 소유한 Actor가 아니기 때문에
	// Cannon에 Server RPC를 만들어 직접 호출하는 방식은 사용X
	// 대신 플레이어가 소유하고 있는 InteractionComponent의
	// 기존 ServerRunInteract RPC를 이용해서 서버에 상호작용을 요청
	// 서버에서는 ServerRunInteract가 다시 CanInteract를 확인, Cannon의 Interact를 실행
	// =========================================================
	if (!HasAuthority())
	{
		if (UInteractionComponent* InteractionComp =
			Character->GetInteractionComponent())
		{
			InteractionComp->ServerRunInteract(this);
		}

		return;
	}

	// =========================================================
	// 서버에서만 실행
	// =========================================================
	if (!CanCharacterUseCannon(Character))
	{
		return;
	}

	// ---------------------------------------------------------
	// 현재 플레이어가 탑승자라면 하차
	// ---------------------------------------------------------
	if (Passenger == Character)
	{
		ReleasePassenger(true);
		return;
	}

	// ---------------------------------------------------------
	// 현재 플레이어가 조작자라면 조작 해제
	// ---------------------------------------------------------
	if (Operator == Character)
	{
		ReleaseOperator();
		return;
	}

	// ---------------------------------------------------------
	// Passenger가 없으면 첫 번째 플레이어가 탑승
	// ---------------------------------------------------------
	if (!Passenger)
	{
		TryEnterPassenger(Character);
		return;
	}

	// ---------------------------------------------------------
	// Passenger는 있고 Operator가 없으면
	// 두 번째 플레이어가 조작권 획득
	// ---------------------------------------------------------
	if (!Operator && Passenger != Character)
	{
		TryStartOperating(Character);
		return;
	}
}

// =========================================================
// Passenger
// =========================================================
bool ACannon::TryEnterPassenger(AMainCharacter* Character)
{
	// Passenger 상태 변경은 반드시 서버에서만 처리
	if (!HasAuthority())
	{
		return false;
	}

	// Character가 유효한지, 죽음/그로기 상태인지, 다른 액터에 이미 붙어있는지 등을 검사
	if (!CanCharacterUseCannon(Character))
	{
		return false;
	}

	// 탑승자가 있다면 추가 탑승 불가능
	if (Passenger)
	{
		return false;
	}

	// 현재 발사자인 플레이어가 탑승자가 되는 것을 방지
	if (Operator == Character)
	{
		return false;
	}

	// 플레이어를 현재 대포의 탑승자로 등록
	Passenger = Character;

	// 현재 이동 즉시 정지
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		// 가지고 있는 속도 0으로 조정
		Movement->StopMovementImmediately();

		// 플레이어의 이동을 막음
		Movement->DisableMovement();
	}

	// 대포 좌석에 부착
	Character->AttachToComponent(SeatPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// SeatPoint 기준으로 위치/회전 완전히 0
	if (USceneComponent* Root = Character->GetRootComponent())
	{
		Root->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	
	// Listen Server의 로컬 플레이어 처리
	ApplyLocalPassengerView(Character);

	ForceNetUpdate();

	OnPassengerChanged(Passenger);

	return true;
}


void ACannon::ReleasePassenger(bool bMoveToExit)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Passenger)
	{
		return;
	}

	AMainCharacter* OldPassenger = Passenger;

	// Listen Server 로컬 탑승자의 카메라 / 입력 복구
	RestoreLocalPassengerView();

	// 먼저 상태 비움
	Passenger = nullptr;

	if (IsValid(OldPassenger))
	{
		OldPassenger->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// 정상 하차인 경우 ExitPoint로 이동
		if (bMoveToExit && ExitPoint)
		{
			OldPassenger->SetActorLocationAndRotation(
				ExitPoint->GetComponentLocation(),
				ExitPoint->GetComponentRotation(),
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
		}

		// 걷기 복구
		if (UCharacterMovementComponent* Movement = OldPassenger->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	// 탑승자가 없어졌다면 더 이상 발사할 대상이 없으므로
	// 현재 조작 중인 플레이어의 대포 조작 권한도 같이 해제
	if (Operator)
	{
		ReleaseOperator();
	}

	ForceNetUpdate();

	OnPassengerChanged(nullptr);
}


// =========================================================
// Operator
// =========================================================
bool ACannon::TryStartOperating(AMainCharacter* Character)
{
	// Operator 변경은 서버에서만 처리
	if (!HasAuthority())
	{
		return false;
	}

	// 플레이어가 대포를 사용할 수 있는 상태인지 확인
	if (!CanCharacterUseCannon(Character))
	{
		return false;
	}

	// 발사될 사람이 있어야 조작 가능
	if (!Passenger)
	{
		return false;
	}

	// 이미 다른 플레이어가 조작 중
	if (Operator)
	{
		return false;
	}

	// 탑승자 본인이 조작할 수 없음
	if (Passenger == Character)
	{
		return false;
	}

	// 대포를 어떤 플레이어가 조작 중인지 확인
	Operator = Character;
	
	// =========================================================
	// Cannon의 네트워크 Owner를 현재 Operator의 Controller로 설정
	// Cannon은 원래 아무 플레이어 소유가 아니기 때문에
	// 클라이언트가 Cannon Server RPC를 직접 호출할 수 없음
	// Operator가 생긴 동안만 해당 플레이어가 Cannon을 소유
	// =========================================================
	if (AController* Controller = Character->GetController())
	{
		SetOwner(Controller);
	}

	// Listen Server 로컬 Operator 입력 적용
	ApplyLocalOperatorInput(Character);

	// =========================================================
	// Operator 조준 시작 기준 저장
	// F를 누른 순간 플레이어가 바라보던 방향을 기준 0
	// 따라서 Operator가 등록되는 순간 대포가 갑자기 돌아가지 않음
	// =========================================================
	if (AController* Controller = Character->GetController())
	{
		OperatorStartControlRotation = Controller->GetControlRotation();
	}

	OperatorStartAimYaw = CurrentAimYaw;
	OperatorStartAimPitch = CurrentAimPitch;

	// 변경된 Operator를 빠르게 클라이언트에 동기화
	ForceNetUpdate();

	// BP에서 UI / 조준 표시 등을 처리할 수 있도록 이벤트 호출
	OnOperatorChanged(Operator);

	return true;
}

void ACannon::ReleaseOperator()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Operator)
	{
		return;
	}

	// Listen Server 로컬 Operator 입력 제거
	RestoreLocalOperatorInput();

	// 대포 조작 권한 해제
	Operator = nullptr;

	ForceNetUpdate();

	OnOperatorChanged(nullptr);
}

bool ACannon::HasPassenger() const
{
	return IsValid(Passenger.Get());
}

bool ACannon::HasOperator() const
{
	return IsValid(Operator.Get());
}

void ACannon::ServerFireCannon_Implementation()
{
	// Operator / Passenger가 둘 다 있어야 발사 가능
	if (!Operator || !Passenger)
	{
		return;
	}

	// 현재 Cannon Owner가 실제 Operator Controller인지 검증
	if (GetOwner() != Operator->GetController())
	{
		return;
	}

	FireCannon();
}

void ACannon::MulticastPlayFireEffects_Implementation()
{
	OnCannonFired();
}

void ACannon::HandleFireInput()
{
	if (!Operator)
	{
		return;
	}

	if (!Operator->IsLocallyControlled())
	{
		return;
	}

	if (!Passenger)
	{
		return;
	}

	// Listen Server Operator
	if (HasAuthority())
	{
		FireCannon();
		return;
	}

	// 일반 Client Operator
	ServerFireCannon();
}

void ACannon::FireCannon()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Passenger || !Operator)
	{
		return;
	}

	if (!LaunchPoint || !LaunchArrow)
	{
		return;
	}

	AMainCharacter* FiredPassenger = Passenger.Get();

	if (!IsValid(FiredPassenger))
	{
		return;
	}

	// =========================================================
	// 발사 방향
	// 궤적 Preview와 완전히 동일한 값 사용
	// =========================================================
	const FVector LaunchDirection = LaunchArrow->GetForwardVector().GetSafeNormal();
	const FVector LaunchVelocity = LaunchDirection * LaunchSpeed;

	// =========================================================
	// Listen Server Passenger라면 먼저 카메라 / 입력 복구
	// 일반 Client Passenger는 Passenger Replication이 nullptr이 되면서
	// OnRep_Passenger에서 자동 복구
	// =========================================================
	RestoreLocalPassengerView();

	// =========================================================
	// Passenger 상태 먼저 제거
	// =========================================================
	Passenger = nullptr;

	// =========================================================
	// SeatPoint에서 분리
	// =========================================================
	FiredPassenger->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// =========================================================
	// Attach 상태에서 따라가던 SeatPoint 회전을 제거
	// 플레이어를 다시 정상적으로 세운다.
	// =========================================================
	FRotator UprightRotation = LaunchDirection.Rotation();

	// 캐릭터는 위아래로 기울지 않고 항상 직립
	UprightRotation.Pitch = 0.0f;
	UprightRotation.Roll = 0.0f;

	FiredPassenger->SetActorRotation(UprightRotation, ETeleportType::TeleportPhysics);

	// =========================================================
	// 포구 위치로 이동
	// 포신 Collision 안쪽에서 LaunchCharacter를 하면
	// 바로 벽에 걸릴 수 있으므로 살짝 앞에서 시작
	// =========================================================
	const FVector LaunchLocation = LaunchPoint->GetComponentLocation() + LaunchDirection * LaunchStartOffset;
	FiredPassenger->SetActorLocation(LaunchLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// =========================================================
	// Movement 발사 준비
	// =========================================================
	// 대포 비행용 CharacterMovement 설정 적용
	ApplyLaunchMovementSettings(FiredPassenger);
	if (UCharacterMovementComponent* Movement = FiredPassenger->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Falling);
	}

	// =========================================================
	// 실제 발사
	// =========================================================
	FiredPassenger->LaunchCharacter(LaunchVelocity, true, true);// XYZ 기존 속도 무시

	// 모든 플레이어에게 대포 발사 연출 재생
	MulticastPlayFireEffects();

	// Passenger 변경 즉시 동기화
	ForceNetUpdate();
	OnPassengerChanged(nullptr);

	// =========================================================
	// 한 번 발사했으면 Operator도 조작 종료
	// =========================================================
	ReleaseOperator();
}

void ACannon::ApplyLocalOperatorInput(AMainCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	// 자기 자신인 Operator 클라이언트만
	if (!Character->IsLocallyControlled())
	{
		return;
	}

	if (bLocalOperatorInputActive)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());

	if (!PC)
	{
		return;
	}

	LocalOperatorInputCharacter = Character;
	bLocalOperatorInputActive = true;

	// Cannon을 해당 PlayerController 입력 스택에 추가
	EnableInput(PC);

	// 한 번만 Binding 생성
	if (InputComponent && !bFireInputBound)
	{
		FInputKeyBinding& FireBinding = InputComponent->BindKey(
				EKeys::LeftMouseButton,
				IE_Pressed,
				this,
				&ACannon::HandleFireInput);

		// 조작 중 좌클릭이 기존 아이템 Use까지 내려가지 않도록 막음
		FireBinding.bConsumeInput = true;

		bFireInputBound = true;
	}
}

void ACannon::RestoreLocalOperatorInput()
{
	if (!bLocalOperatorInputActive)
	{
		return;
	}

	AMainCharacter* Character = LocalOperatorInputCharacter.Get();

	if (Character)
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			DisableInput(PC);
		}
	}
	LocalOperatorInputCharacter = nullptr;
	bLocalOperatorInputActive = false;
}

// =========================================================
// Rep Notify
// =========================================================
void ACannon::OnRep_Passenger()
{
	if (Passenger)
	{
		// Passenger가 자기 자신인 클라이언트만
		// 대포 카메라 + 입력 차단 적용
		ApplyLocalPassengerView(Passenger.Get());
	}
	else
	{
		// Passenger가 없어졌다면
		// 원래 카메라 + 입력 복구
		RestoreLocalPassengerView();
	}

	OnPassengerChanged(Passenger);
}

void ACannon::OnRep_Operator()
{
	// 이전 로컬 Operator 입력 제거
	RestoreLocalOperatorInput();

	// 새 Operator가 자기 자신인 클라이언트라면
	// 좌클릭 발사 입력 적용
	if (Operator)
	{
		ApplyLocalOperatorInput(Operator.Get());
	}

	OnOperatorChanged(Operator);
}

void ACannon::OnRep_AimRotation()
{
	ApplyCannonAim();
}

// =========================================================
// Helper
// =========================================================
void ACannon::ApplyLocalPassengerView(AMainCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	// 자기 화면에 해당하는 캐릭터만 처리
	if (!Character->IsLocallyControlled())
	{
		return;
	}

	// 중복 적용 방지
	if (bLocalPassengerViewActive)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());

	if (!PC)
	{
		return;
	}

	LocalPassengerViewCharacter = Character;
	bLocalPassengerViewActive = true;

	// 탑승 중 WASD 입력 차단
	PC->SetIgnoreMoveInput(true);

	// 탑승 중 마우스 Look 입력 차단
	PC->SetIgnoreLookInput(true);

	// 플레이어 카메라 → 대포 탑승자 카메라
	PC->SetViewTargetWithBlend(this, PassengerCameraBlendTime);
}

void ACannon::RestoreLocalPassengerView()
{
	if (!bLocalPassengerViewActive)
	{
		return;
	}

	AMainCharacter* Character = LocalPassengerViewCharacter.Get();

	if (Character)
	{
		APlayerController* PC = Cast<APlayerController>(Character->GetController());

		if (PC)
		{
			// 이동 입력 복구
			PC->SetIgnoreMoveInput(false);

			// 마우스 입력 복구
			PC->SetIgnoreLookInput(false);

			// 대포 카메라 → 플레이어 카메라
			PC->SetViewTargetWithBlend(Character, PassengerCameraBlendTime);
		}
	}

	LocalPassengerViewCharacter = nullptr;
	bLocalPassengerViewActive = false;
}

void ACannon::LockPassengerToSeat()
{
	if (!Passenger || !SeatPoint)
	{
		return;
	}

	USceneComponent* Root = Passenger->GetRootComponent();

	if (!Root)
	{
		return;
	}

	// Character가 SeatPoint에 Attach되어 있을 때만
	if (Root->GetAttachParent() == SeatPoint)
	{
		// SeatPoint 기준 위치 0, SeatPoint 기준 회전 0
		// 즉 BP에서 설정한 SeatPoint Transform을 플레이어가 정확히 그대로 사용
		Root->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}
}

void ACannon::UpdateCannonAim(float DeltaTime)
{
	if (!Operator)
	{
		return;
	}

	AController* Controller = Operator->GetController();

	if (!Controller)
	{
		return;
	}

	// 현재 Operator가 바라보는 방향
	const FRotator CurrentControlRotation = Controller->GetControlRotation();

	// =========================================================
	// 조작 시작 순간에서 마우스가 얼마나 움직였는지 계산
	// =========================================================
	const float DeltaYaw = FRotator::NormalizeAxis(CurrentControlRotation.Yaw - OperatorStartControlRotation.Yaw);
	const float DeltaPitch = FRotator::NormalizeAxis(CurrentControlRotation.Pitch - OperatorStartControlRotation.Pitch);

	// =========================================================
	// 목표 좌우 각도
	// =========================================================
	const float TargetYaw = FMath::Clamp(
			OperatorStartAimYaw + (DeltaYaw * AimSensitivity),
			MinAimYaw,
			MaxAimYaw
		);

	// =========================================================
	// 목표 상하 각도
	// 현재 제한 : 0 ~ 25도
	// =========================================================
	const float TargetPitch = FMath::Clamp(
			OperatorStartAimPitch + (DeltaPitch * AimSensitivity),
			MinAimPitch,
			MaxAimPitch
		);

	// =========================================================
	// 부드럽게 목표 각도로 이동
	// =========================================================
	CurrentAimYaw = FMath::FInterpTo(
			CurrentAimYaw,
			TargetYaw,
			DeltaTime,
			AimInterpSpeed
		);

	CurrentAimPitch = FMath::FInterpTo(
			CurrentAimPitch,
			TargetPitch,
			DeltaTime,
			AimInterpSpeed
		);

	// 실제 Pivot에 적용
	ApplyCannonAim();
}

void ACannon::ApplyCannonAim()
{
	// =========================================================
	// 좌 / 우 회전
	// =========================================================
	if (YawPivot)
	{
		FRotator NewYawRotation = InitialYawPivotRotation;
		NewYawRotation.Yaw += CurrentAimYaw;
		YawPivot->SetRelativeRotation(NewYawRotation);
	}

	// =========================================================
	// 상 / 하 회전
	// =========================================================
	if (PitchPivot)
	{
		FRotator NewPitchRotation = InitialPitchPivotRotation;
		NewPitchRotation.Pitch += CurrentAimPitch;
		PitchPivot->SetRelativeRotation(NewPitchRotation);
	}
}

void ACannon::UpdateTrajectoryPreview()
{
	if (!bShowTrajectoryPreview)
	{
		return;
	}

	if (!Operator || !Operator->IsLocallyControlled())
	{
		return;
	}

	if (!LaunchPoint || !LaunchArrow)
	{
		return;
	}

	// =========================================================
	// 실제 발사와 완전히 동일한 시작 위치 / 속도
	// =========================================================
	const FVector LaunchDirection = LaunchArrow->GetForwardVector().GetSafeNormal();
	const FVector StartLocation = LaunchPoint->GetComponentLocation() + LaunchDirection * LaunchStartOffset;
	const FVector LaunchVelocity = LaunchDirection * LaunchSpeed;

	// =========================================================
	// 궤적 계산 설정
	// =========================================================
	FPredictProjectilePathParams Params;

	Params.StartLocation = StartLocation;
	Params.LaunchVelocity = LaunchVelocity;

	// 지형 충돌까지 계산
	Params.bTraceWithCollision = true;

	// 플레이어 크기를 대략 고려
	Params.ProjectileRadius = TrajectoryRadius;
	Params.MaxSimTime = TrajectoryMaxSimTime;
	Params.SimFrequency = TrajectorySimFrequency;

	// =========================================================
	// ObjectType 전체를 잡는 대신 Visibility 기준으로 충돌
	// =========================================================
	Params.bTraceWithChannel = true;
	Params.TraceChannel = ECC_Visibility;

	// =========================================================
	// 실제 CharacterMovement 중력값과 맞춤
	// =========================================================
	if (Passenger)
	{
		if (UCharacterMovementComponent* Movement = Passenger->GetCharacterMovement())
		{
			Params.OverrideGravityZ = GetWorld()->GetGravityZ() * Movement->GravityScale;
		}
	}

	// =========================================================
	// 무시 대상
	// =========================================================
	Params.ActorsToIgnore.Add(this);

	if (Passenger)
	{
		Params.ActorsToIgnore.Add(Passenger.Get());
	}

	if (Operator)
	{
		Params.ActorsToIgnore.Add(Operator.Get());
	}

	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(this, Params, Result);

	// =========================================================
	// 궤적 전체
	// =========================================================
	for (int32 i = 1; i < Result.PathData.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), Result.PathData[i - 1].Location, Result.PathData[i].Location, FColor::Cyan, false, 0.0f, 0, 3.0f);
	}

	// =========================================================
	// 예상 착지점 표시
	// =========================================================
	if (Result.HitResult.bBlockingHit)
	{
		DrawDebugSphere(GetWorld(), Result.HitResult.ImpactPoint, 30.0f, 16, FColor::Yellow, false, 0.0f, 0, 3.0f);
	}
}

void ACannon::ApplyLaunchMovementSettings(AMainCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	if (!Movement)
	{
		return;
	}

	// 원래 플레이어 이동값 저장
	SavedFallingLateralFriction = Movement->FallingLateralFriction;
	SavedBrakingDecelerationFalling = Movement->BrakingDecelerationFalling;
	SavedAirControl = Movement->AirControl;
	SavedUseSeparateBrakingFriction = Movement->bUseSeparateBrakingFriction;
	SavedBrakingFriction = Movement->BrakingFriction;
	bLaunchMovementSettingsSaved = true;
	LaunchedPassenger = Character;

	// =========================================================
	// 대포 발사 중에는 순수 포물선 운동
	// =========================================================
	// 공중에서 XY 속도를 마찰로 깎지 않음
	Movement->FallingLateralFriction = 0.0f;

	// Falling 상태에서 자동 감속 금지
	Movement->BrakingDecelerationFalling = 0.0f;

	// 플레이어 WASD가 비행 궤적을 틀지 못하게 함
	Movement->AirControl = 0.0f;

	// 별도의 BrakingFriction 사용 방지
	Movement->bUseSeparateBrakingFriction = false;
}

void ACannon::UpdateLaunchedPassenger()
{
	if (!LaunchedPassenger)
	{
		return;
	}

	AMainCharacter* Character = LaunchedPassenger.Get();

	if (!IsValid(Character))
	{
		bLaunchMovementSettingsSaved = false;
		LaunchedPassenger = nullptr;
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	if (!Movement)
	{
		RestoreLaunchMovementSettings();
		return;
	}

	// 발사 후 땅에 착지하면 원래 캐릭터 이동 설정으로 복구
	if (Movement->IsMovingOnGround())
	{
		RestoreLaunchMovementSettings();
	}
}

void ACannon::RestoreLaunchMovementSettings()
{
	if (!bLaunchMovementSettingsSaved)
	{
		LaunchedPassenger = nullptr;
		return;
	}

	AMainCharacter* Character = LaunchedPassenger.Get();

	if (IsValid(Character))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->FallingLateralFriction = SavedFallingLateralFriction;
			Movement->BrakingDecelerationFalling = SavedBrakingDecelerationFalling;
			Movement->AirControl = SavedAirControl;
			Movement->bUseSeparateBrakingFriction = SavedUseSeparateBrakingFriction;
			Movement->BrakingFriction = SavedBrakingFriction;
		}
	}
	bLaunchMovementSettingsSaved = false;
	LaunchedPassenger = nullptr;
}

bool ACannon::CanCharacterUseCannon(AMainCharacter* Character) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	if (Character->IsDead())
	{
		return false;
	}

	if (Character->IsGroggy())
	{
		return false;
	}

	// 다른 액터에 이미 Attach된 플레이어가
	// 갑자기 대포로 들어가는 상황 방지
	AActor* AttachParent = Character->GetAttachParentActor();

	if (AttachParent && AttachParent != this)
	{
		return false;
	}

	return true;
}


void ACannon::CleanupInvalidUsers()
{
	// ---------------------------------------------------------
	// Passenger 예외처리
	// ---------------------------------------------------------
	if (Passenger)
	{
		if (!IsValid(Passenger.Get()))
		{
			Passenger = nullptr;
			ForceNetUpdate();
		}
		else if (Passenger->IsDead() || Passenger->IsGroggy())
		{
			// 죽은 캐릭터를 ExitPoint로 텔레포트하진 않음
			ReleasePassenger(false);
		}
	}

	// ---------------------------------------------------------
	// Operator 예외처리
	// ---------------------------------------------------------
	if (Operator)
	{
		if (!IsValid(Operator.Get()))
		{
			Operator = nullptr;
			ForceNetUpdate();
		}
		else if (Operator->IsDead() || Operator->IsGroggy())
		{
			ReleaseOperator();
		}
		else
		{
			// 조작자는 대포에 붙어 있지 않으므로 너무 멀리 걸어가면 조작권을 자동으로 해제
			const float Distance =
				FVector::Dist(
					Operator->GetActorLocation(),
					GetActorLocation()
				);

			if (Distance > OperatorMaxDistance)
			{
				ReleaseOperator();
			}
		}
	}
}