#pragma once

#include "CoreMinimal.h"
#include "Item/WeaponItemBase.h"
#include "HealingMeleeWeapon.generated.h"

class UAnimMontage;
class AMainCharacter;

/** 전방의 다른 플레이어 한 명을 치유하는 근접 무기. 판정과 회복은 서버 전용. */
UCLASS()
class TEAMPROJECT_MOU_API AHealingMeleeWeapon : public AWeaponItemBase
{
	GENERATED_BODY()

public:
	AHealingMeleeWeapon();
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanBeDropped() const override { return !IsInUse(); }

protected:
	virtual void Fire() override;
	// 서버에서 쿨다운/소유자를 검사한 뒤 직접 차감한다.
	virtual bool ShouldConsumeUseOnFire() const override { return false; }
	virtual bool IsValidTarget(AActor* HitActor) const override;
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
	void MulticastSwing(AMainCharacter* Wielder);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHealEffect(AActor* Target, FVector Location);

	// 몽타주 외 무기 애니메이션/사운드를 연결하는 연출 전용 이벤트.
	UFUNCTION(BlueprintImplementableEvent, Category = "Healing Melee|FX")
	void OnSwingEffect();

	UFUNCTION(BlueprintImplementableEvent, Category = "Healing Melee|FX")
	void OnHealEffect(AActor* Target, FVector Location);

private:
	void ResolveSwing();
	TWeakObjectPtr<AMainCharacter> SwingWielder;
	double SwingHitTime = 0.0;
	bool bSwingHitPending = false;
	double NextSwingTime = 0.0;
	bool bResolvingSwing = false;
};
