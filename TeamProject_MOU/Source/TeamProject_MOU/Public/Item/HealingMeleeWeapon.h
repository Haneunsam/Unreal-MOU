#pragma once

#include "CoreMinimal.h"
#include "Item/WeaponItemBase.h"
#include "HealingMeleeWeapon.generated.h"

class UAnimMontage;
class AMainCharacter;
class UGA_SpannerSwing;

/** 전방의 다른 플레이어 한 명을 치유하는 근접 무기. 판정과 회복은 서버 전용. */
UCLASS()
class TEAMPROJECT_MOU_API AHealingMeleeWeapon : public AWeaponItemBase
{
	GENERATED_BODY()

public:
	// [HEAL-000] 초기 컴포넌트와 기본값 설정
	AHealingMeleeWeapon();
	// [HEAL-012] 디버그 표시 타이머 초기화
	virtual void BeginPlay() override;
	// [HEAL-013] 제거 시 실행 중인 어빌리티와 타이머 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// [HEAL-002] 사용 상태에 따른 내려놓기 허용 여부 반환
	virtual bool CanBeDropped() const override { return !IsInUse(); }

protected:
	// [HEAL-003] 서버 ASC에 스패너 어빌리티를 일회 부여하고 실행
	virtual void Fire() override;
	// 서버에서 쿨다운/소유자를 검사한 뒤 직접 차감한다.
	// [HEAL-004] 발사 시 공통 내구도 차감 여부 반환
	virtual bool ShouldConsumeUseOnFire() const override { return false; }
	// [HEAL-005] 효과 적용 가능한 대상인지 검사
	virtual bool IsValidTarget(AActor* HitActor) const override;
	// [HEAL-006] 명중 대상의 효과 처리
	virtual void ApplyWeaponHit_Implementation(AActor* HitActor, const FHitResult& Hit) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Healing Melee", meta = (ClampMin = "0.0"))
	float HealAmount = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Melee", meta = (ClampMin = "1.0", Units = "cm"))
	float SwingRange = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Melee", meta = (ClampMin = "1.0", Units = "cm"))
	float SwingRadius = 45.0f;

	// PIE에서 든 무기의 후보 탐색 범위(노랑), 대상 중심 거리 제한(파랑)을 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Melee|Debug")
	bool bShowSwingRange = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Healing Melee", meta = (ClampMin = "0.01", Units = "s"))
	float SwingCooldown = 0.7f;

	// 준비 동작 후 스패너가 전방을 지나는 시점.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Healing Melee", meta = (ClampMin = "0.0", Units = "s"))
	float SwingHitDelay = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Healing Melee", meta = (ClampMin = "0.0"))
	float DurabilityCostPerSwing = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Healing Melee|Animation")
	TObjectPtr<UAnimMontage> SwingMontage;

	UFUNCTION(NetMulticast, Reliable)
	// [HEAL-007] 모든 클라이언트에서 추가 휘두르기 연출 호출 (몽타주는 GAS에서 재생)
	void MulticastSwing(AMainCharacter* Wielder);

	UFUNCTION(NetMulticast, Unreliable)
	// [HEAL-008] 모든 클라이언트에서 치유 연출 호출
	void MulticastHealEffect(AActor* Target, FVector Location);

	// 몽타주 외 무기 애니메이션/사운드를 연결하는 연출 전용 이벤트.
	UFUNCTION(BlueprintImplementableEvent, Category = "Healing Melee|FX")
	// [HEAL-009] BP에서 구현하는 휘두르기 연출 이벤트
	void OnSwingEffect();

	UFUNCTION(BlueprintImplementableEvent, Category = "Healing Melee|FX")
	// [HEAL-010] BP에서 구현하는 치유 연출 이벤트
	void OnHealEffect(AActor* Target, FVector Location);

private:
	friend class UGA_SpannerSwing;
	// [HEAL-014] 현재 소유자의 장착·행동 가능 상태 검사
	bool IsSwingOwnerValid() const;
	// [HEAL-015] 새 공격의 내구도·사용 상태 검사
	bool CanStartSwing() const;
	// [HEAL-016] 어빌리티 시작 시 내구도 차감과 사용 상태 설정
	void BeginAbilitySwing(UGA_SpannerSwing* Ability);
	// [HEAL-017] 어빌리티 종료 시 사용 상태 복원
	void EndAbilitySwing(UGA_SpannerSwing* Ability);
	// [HEAL-001] 타이머에서 개발용 판정 범위 표시
	void DrawSwingRange();
	// [HEAL-011] 서버 전방 스윕과 거리·가림 검사 후 한 명에게 치유 시도
	void ResolveSwing();
	TWeakObjectPtr<AMainCharacter> SwingWielder;
	TWeakObjectPtr<UGA_SpannerSwing> ActiveSwingAbility;
	FTimerHandle DebugRangeTimer;
	bool bResolvingSwing = false;
};
