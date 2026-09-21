#pragma once

#include "CoreMinimal.h"
#include "Base/GameplayAbilityBase.h"
#include "GameplayEffect.h"
#include "GA_SpannerSwing.generated.h"

class AHealingMeleeWeapon;

UCLASS()
class TEAMPROJECT_MOU_API UGE_SpannerCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	// [SPANNER-000] 복제 가능한 지속형 쿨다운 GE 기본값 설정
	UGE_SpannerCooldown();
};

UCLASS()
class TEAMPROJECT_MOU_API UGA_SpannerSwing : public UGameplayAbilityBase
{
	GENERATED_BODY()
public:
	// [SPANNER-001] 서버 실행 정책과 사용 중·차단 태그 설정
	UGA_SpannerSwing();
	// [SPANNER-002] 소스 무기·소유자·쿨다운 활성화 조건 검사
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	// [SPANNER-003] 내구도·쿨다운 적용 후 몽타주와 지연 판정 태스크 실행
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	// [SPANNER-004] 종료·취소 시 무기 사용 상태와 태스크 정리
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	TWeakObjectPtr<AHealingMeleeWeapon> Weapon;
	bool bHitResolved = false;
	bool bEnding = false;
	UFUNCTION()
	// [SPANNER-005] 지연 완료 시 유효성 재검사 후 서버 치유 판정
	void ResolveHit();
	UFUNCTION()
	// [SPANNER-006] 사용 시간 만료 시 정상 종료
	void CompleteSwing();
	UFUNCTION()
	// [SPANNER-007] 사망·행동 불능·몽타주 중단 시 취소
	void CancelSwing();
};
