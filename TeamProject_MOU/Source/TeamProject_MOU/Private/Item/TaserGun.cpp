#include "Item/TaserGun.h"
#include "Base/CharacterBase.h"
#include "Components/StatusComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "AbilitySystemBlueprintLibrary.h" // SendGameplayEventToActor
#include "DrawDebugHelpers.h" // [DEBUG-TASER] 확인용 임시
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// [TASER-008] 초기 컴포넌트와 기본값 설정
ATaserGun::ATaserGun()
{
	// 최대 내구도 100, 발사당 25 차감(GetDurabilityCostPerUse) → 4회 발사 가능.
	MaxDurability = 100.0f;
	CurrentDurability = MaxDurability;

	// 총구 지점 컴포넌트 (VFX 시작 위치용). BP에서 메시 총구로 이동시킴
	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(MeshComponent);

	static ConstructorHelpers::FObjectFinder<USoundBase> FireSoundFinder(
		TEXT("/Game/04_JJO/Sound/SFX_TazerGun.SFX_TazerGun"));
	if (FireSoundFinder.Succeeded())
	{
		FireSound = FireSoundFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FireEffectFinder(
		TEXT("/Game/04_JJO/NIagara/TagerBeam.TagerBeam"));
	if (FireEffectFinder.Succeeded())
	{
		FireEffect = FireEffectFinder.Object;
	}
}

// [TASER-012] 발사한 클라이언트의 실제 카메라 위치와 방향을 서버에 전달한다.
void ATaserGun::OnUse_Implementation()
{
	FVector ViewLocation = GetActorLocation();
	FRotator ViewRotation = GetActorRotation();

	if (APawn* OwnerPawn = GetOwningPawn())
	{
		if (AController* OwnerController = OwnerPawn->GetController())
		{
			OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
		else
		{
			OwnerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		}
	}

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (HasAuthority())
	{
		ValidatedViewLocation = ViewLocation;
		ValidatedViewDirection = ViewDirection;
		bHasValidatedView = true;
		AWeaponItemBase::OnUse_Implementation();
		bHasValidatedView = false;
		return;
	}

	ServerFireFromView(ViewLocation, ViewDirection);
}

// [TASER-013] 클라이언트 카메라 기준의 발사 시점을 서버에서 검증한 후 발사한다.
void ATaserGun::ServerFireFromView_Implementation(FVector_NetQuantize ViewLocation,
	FVector_NetQuantizeNormal ViewDirection)
{
	APawn* OwnerPawn = GetOwningPawn();
	if (!OwnerPawn)
	{
		return;
	}

	FVector ServerEyeLocation;
	FRotator ServerEyeRotation;
	OwnerPawn->GetActorEyesViewPoint(ServerEyeLocation, ServerEyeRotation);

	// 3인칭 스프링암 카메라는 머리에서 수백 cm 떨어질 수 있다.
	// 실제 카메라 위치는 허용하되 비정상적으로 먼 RPC 위치만 서버 눈 위치로 보정한다.
	constexpr float MaxViewOriginError = 1000.0f;
	const bool bValidLocation = !ViewLocation.ContainsNaN()
		&& FVector::DistSquared(ViewLocation, ServerEyeLocation) <= FMath::Square(MaxViewOriginError);
	const FVector SafeDirection = ViewDirection.ContainsNaN()
		? ServerEyeRotation.Vector()
		: ViewDirection.GetSafeNormal();

	ValidatedViewLocation = bValidLocation ? FVector(ViewLocation) : ServerEyeLocation;
	ValidatedViewDirection = SafeDirection.IsNearlyZero() ? ServerEyeRotation.Vector() : SafeDirection;
	bHasValidatedView = true;
	AWeaponItemBase::OnUse_Implementation();
	bHasValidatedView = false;
}

// TASER-001 발사 override: 카메라 조준 방향으로 피아식별 트레이스 + VFX
// (좌클릭→횟수차감→서버권한 분기는 부모 WeaponItemBase::OnUse에서 처리)
// [TASER-001] 서버에서 무기 사용과 발사 처리
void ATaserGun::Fire()
{
	// 원격 플레이어는 서버의 시점이 아닌, 발사 순간에 클라이언트가 전달한 실제 카메라 시점을 사용한다.
	FVector ViewLocation = bHasValidatedView ? ValidatedViewLocation : GetActorLocation();
	FVector ViewDirection = bHasValidatedView ? ValidatedViewDirection : GetActorForwardVector();

	if (!bHasValidatedView)
	{
		if (APawn* OwnerPawn = GetOwningPawn())
		{
			FRotator ViewRotation;
			if (AController* OwnerController = OwnerPawn->GetController())
			{
				OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
			}
			else
			{
				OwnerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
			}
			ViewDirection = ViewRotation.Vector();
		}
	}

	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = TraceStart + ViewDirection.GetSafeNormal() * TraceDistance;

	// 피아식별 트레이스는 부모(WeaponItemBase)에 위임.
	// TargetTeam 채널로만 트레이스하고, 맞으면 ApplyWeaponHit(=기절)을 내부에서 호출.
	FHitResult Hit;
	// 총구가 벽 뒤에 있는 경우에는 카메라에서 보이는 대상에게도 효과를 적용하지 않는다.
	bool bHit = FireHitscan(TraceStart, TraceEnd, Hit, false);
	const FVector AimEnd = Hit.bBlockingHit ? Hit.ImpactPoint : TraceEnd;
	FHitResult MuzzleWall;
	const FVector MuzzleStart = MuzzlePoint ? MuzzlePoint->GetComponentLocation() : TraceStart;
	const bool bMuzzleBlocked = TraceWeaponObstacle(MuzzleStart, AimEnd, 0.1f, MuzzleWall);
	if (bMuzzleBlocked)
	{
		Hit = MuzzleWall;
		bHit = false;
	}
	else if (bHit)
	{
		ApplyWeaponHit(Hit.GetActor(), Hit);
	}

	// [DEBUG-TASER] 트레이스 선 시각화 (맞으면 초록/빨강, 히트 지점에 구) - 확인 후 제거
	/*DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.5f);
	if (bHit)
	{
		DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 15.0f, 12, FColor::Yellow, false, 2.0f);
		UE_LOG(LogTemp, Warning, TEXT("[TASER] Hit Actor: %s"), *GetNameSafe(Hit.GetActor()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TASER] Miss (nothing hit)"));
	}*/

	// VFX 시작점은 총구(MuzzlePoint), 끝점은 트레이스 도착지점(히트면 히트, 아니면 최대거리)
	const FVector FxStart = MuzzlePoint ? MuzzlePoint->GetComponentLocation() : TraceStart;
	const FVector FxEnd = Hit.bBlockingHit ? Hit.ImpactPoint : TraceEnd;

	MulticastPlayFireSound();
	MulticastPlayFireEffect(FxStart, FxEnd, bHit);

	// 발사 쿨다운 동안 "사용 중" 유지 → FireCooldown 후 슬롯 변경 다시 허용 WEAPON-017
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FireCooldownTimer, this, &ATaserGun::FinishUse, FireCooldown, false);
	}
}

// TASER-006 무기 공통 히트 처리 override: 맞은 캐릭터에 감전 태그 부여 + 타이머로 해제 예약
// [TASER-006] 명중 대상의 효과 처리
void ATaserGun::ApplyWeaponHit_Implementation(AActor* HitActor, const FHitResult& Hit)
{
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!TargetCharacter)
	{
		// [DEBUG-TASER] 맞은 대상이 ACharacterBase가 아님 - 확인 후 제거
		//UE_LOG(LogTemp, Warning, TEXT("[TASER] Hit but NOT ACharacterBase: %s"), *GetNameSafe(HitActor));
		return;
	}

	UStatusComponent* StatusComp = TargetCharacter->GetStatusComponent();
	if (!StatusComp)
	{
		// [DEBUG-TASER] StatusComponent 없음 - 확인 후 제거
		//UE_LOG(LogTemp, Warning, TEXT("[TASER] Target has NO StatusComponent: %s"), *GetNameSafe(HitActor));
		return;
	}

	// 감전 발동 이벤트 태그 (GA_CC_Electric의 Trigger. 철자 'Eletric' 그대로여야 매칭됨)
	// 이 이벤트를 받으면 대상 ASC에 Grant된 GA_CC_Electric이 발동되어
	// GE 적용 + 감전 몽타주 재생 + 지속시간 후 자동 해제까지 모두 처리한다.
	static const FGameplayTag ElectricEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Reaction.Eletric"), false);
	if (!ElectricEventTag.IsValid())
	{
		// [DEBUG-TASER] Event.Reaction.Eletric 태그가 프로젝트에 정의 안 됨 - 확인 후 제거
		//UE_LOG(LogTemp, Warning, TEXT("[TASER] EventTag 'Event.Reaction.Eletric' is INVALID (not registered)"));
		return;
	}

	// 감전 GA 발동 (지속시간/해제/애니는 GA가 담당. 던진 주체를 Instigator로 전달)
	FGameplayEventData EventData;
	EventData.EventTag = ElectricEventTag;
	EventData.Instigator = LastOwner;       // 발사한 플레이어
	EventData.Target = TargetCharacter;     // 감전당하는 대상
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetCharacter, ElectricEventTag, EventData);

	// [DEBUG-TASER] 이벤트 전송 로그 + 화면 메시지 - 확인 후 제거
	/*UE_LOG(LogTemp, Warning, TEXT("[TASER] ELECTRIC event sent to %s"), *GetNameSafe(TargetCharacter));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT("ELECTRIC event -> %s"), *GetNameSafe(TargetCharacter)));
	}*/
}

// TASER-005 발사 이펙트 훅 - 모든 클라이언트에서 BP VFX 이벤트 재생
// [TASER-005] 발사 이펙트 훅 (전기 줄기 등). 시작/끝 지점 전달, 모든 클라 재생
void ATaserGun::MulticastPlayFireEffect_Implementation(FVector Start, FVector End, bool bHit)
{
	if (!FireEffect)
	{
		OnFireEffect(Start, End, bHit);
		return;
	}

	// Beam End를 생성 후에 넣으면 Niagara가 기본 도착점으로 먼저 한 프레임 재생될 수 있다.
	// 비활성 상태로 생성 → 월드 도착점 설정 → 활성화 순서를 보장한다.
	UNiagaraComponent* BeamComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		FireEffect,
		Start,
		FRotator(0.0f, 0.0f, 90.0f),
		FVector::OneVector,
		true,
		false,
		ENCPoolMethod::AutoRelease,
		false);

	if (BeamComponent)
	{
		// Niagara 에셋에 등록된 정확한 User Position 변수명과 타입을 사용한다.
		// 에셋에 노출된 Position 사용자 파라미터 이름은 "Beam End"이다.
		BeamComponent->SetVariablePosition(TEXT("Beam End"), End);
		BeamComponent->Activate(true);
	}
}

// [TASER-011] 서버의 발사 판정을 모든 클라이언트에 전달해 총구 위치에서 효과음을 재생한다.
void ATaserGun::MulticastPlayFireSound_Implementation()
{
	if (!FireSound)
	{
		return;
	}

	const FVector SoundLocation = MuzzlePoint ? MuzzlePoint->GetComponentLocation() : GetActorLocation();
	UGameplayStatics::PlaySoundAtLocation(this, FireSound, SoundLocation, FireSoundVolume);
}
