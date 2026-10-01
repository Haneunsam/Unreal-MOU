#pragma once

#include "CoreMinimal.h"
#include "Item/ConsumableItemBase.h"
#include "GameplayTagContainer.h"
#include "PotionItem.generated.h"

class UGameplayEffect;
class ACharacterBase;

/**
 * APotionItem
 * 회복/버프 계열 소비 아이템. 좌클릭 시 자기 자신(SelfOnly)에게 효과 적용.
 *
 * "다용성"은 클래스를 여러 개 만들지 않고 GameplayEffect 목록으로 해결한다:
 *   - 체력 회복  = Health를 올리는 Instant/Periodic GE
 *   - 힘 증가    = LiftPower/MaxLiftPower를 올리는 Duration GE
 *   - 속도 증가  = MoveSpeed/MaxMoveSpeed를 올리는 Duration GE
 *   - 경량화     = MaxWeight를 올리는(또는 CurrentWeight를 낮추는) Duration GE
 *   - 상태이상 제거 = TagsToRemove로 StatusComponent 태그 제거 (즉시)
 * 지속시간/원복/복제는 전부 GAS가 처리한다. 버프 태그(Buff.*)는 각 GE가 Granted Tags로 부여.
 *
 * 실제 포션 종류(힐 포션, 속도 포션 등)는 BP에서 EffectsToApply/TagsToRemove만 바꿔 만든다.
 */
UCLASS()
class TEAMPROJECT_MOU_API APotionItem : public AConsumableItemBase
{
	GENERATED_BODY()

public:
	APotionItem();

	// [POTION-007] 전용 Anim Notify가 호출하면 대기 중인 사용 투척을 서버에서 확정한다.
	void HandleThrowAnimNotify(AActor* NotifyOwner);

	// [POTION-012] 포션은 손 소켓에 피벗을 직접 맞추므로 공통 바운딩박스 중심 보정을 사용하지 않는다.
	virtual bool ShouldCenterOnCarrySocket() const override;

	// [POTION-013] 포션 전용 오른손 소켓 이름을 반환한다.
	virtual FName GetCarrySocketOverride() const override;

	// [POTION-014] 오른손 소켓 기준 포션 위치 보정값을 반환한다.
	virtual FVector GetCarryLocationOffset() const override;

	// [POTION-015] 오른손 소켓 기준 포션 회전 보정값을 반환한다.
	virtual FRotator GetCarryRotationOffset() const override;

protected:
#pragma region [POTION] 설정값
	// 사용 시 대상 ASC에 적용할 GameplayEffect 목록 (회복/힘/속도/경량화 모두 여기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion")
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;

	// GameplayEffect 적용 레벨 (GE 안에서 레벨로 수치를 스케일링할 때 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion")
	float EffectLevel = 1.0f;

	// 사용 시 제거할 상태이상 태그 (예: State.Slowed, State.Exhausted, State.CC.Stuned)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion")
	FGameplayTagContainer TagsToRemove;

	// 부여할 상태이상 태그 (감전 등). 테이저와 동일하게 Loose 태그로 부여.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion")
	FGameplayTagContainer TagsToApply;

	// 부여한 태그의 지속시간(초). 0 이하면 자동 해제 안 함.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion")
	float AppliedTagDuration = 5.0f;

	// 대상에게 보낼 상태이상 발동 이벤트 태그 (예: Event.Reaction.Eletric).
	// 지정하면 대상 ASC에 Grant된 상태이상 GA를 발동시켜 GE적용+애니+자동해제를 GA에 위임한다.
	// 비워두면(None) 아무 이벤트도 보내지 않는다. (정석 방식: 태그 직접 부여 대신 이 이벤트 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion", meta = (Categories = "Event"))
	FGameplayTag StatusEventTag;

	// 이동속도 가감 수치 (신속=+150, 슬로우=-100). 0이면 이동속도 효과 미사용.
	// GE로 MoveSpeed를 직접 바꾸면 UpdateCharacterSpeed가 덮어쓰므로, CharacterBase의 SpeedBuffFlat에 가감한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Speed")
	float SpeedFlatDelta = 0.0f;

	// 이동속도 가감 지속시간(초). 0 이하면 영구(원복 안 함).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Speed")
	float SpeedBuffDuration = 5.0f;
#pragma endregion

#pragma region [POTION] 투척 설정값
	// 던져서 충돌 시 광역 발동 여부. false면 일반 포션(던져도 안 터짐).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Throw")
	bool bApplyOnImpact = false;

	// 충돌 발동 시 효과가 미치는 반경(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Throw")
	float ImpactRadius = 400.0f;
#pragma endregion

#pragma region [POTION] 손 장착 설정값
	// 포션을 붙일 오른손 소켓. 현재 캐릭터 스켈레톤의 hand_R 아래 SpannerSocket을 기본 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Carry")
	FName HandSocketName = TEXT("SpannerSocket");

	// 오른손 소켓 기준 포션 위치 보정값(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Carry", meta = (Units = "cm"))
	FVector HandLocationOffset = FVector::ZeroVector;

	// 오른손 소켓 기준 포션 회전 보정값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Potion|Carry")
	FRotator HandRotationOffset = FRotator::ZeroRotator;
#pragma endregion

#pragma region [POTION] 효과 적용
	// [POTION-001] 소비 효과: 대상 ASC에 GE 적용 + 상태이상 태그 제거 (서버에서만 호출됨)
	virtual void ApplyEffect_Implementation() override;

	// [POTION-002] 투척형: 물리 투척 전에 충돌 감지를 준비하고 첫 충돌 시 효과가 발동되게 한다.
	virtual void Throw_Implementation(FVector ThrowVelocity, AActor* Thrower = nullptr) override;

	// [POTION-004] 좌클릭 사용: bApplyOnImpact가 켜져 있으면 던진다(충돌 시 발동),
	// 꺼져 있으면 제자리에서 마신다. Q(단순 투척)와 구분하려고 좌클릭 경로에서만 발동 플래그를 세운다.
	virtual void OnUse_Implementation() override;
#pragma endregion

private:
#pragma region [POTION] 투척 발동
	// [POTION-005] 클라이언트의 좌클릭 투척 요청을 서버로 전달해 서버의 bThrowAsUse 상태로 실행한다.
	UFUNCTION(Server, Reliable)
	void ServerThrowAsUse();

	// [POTION-006] 투척형 포션에 설정된 UseMontage를 모든 클라이언트의 사용 캐릭터에게 재생한다.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayThrowMontage(ACharacterBase* ThrowerCharacter);

	// [POTION-008] 소유 클라이언트에서 발생한 투척 Anim Notify를 서버에 전달한다.
	UFUNCTION(Server, Reliable)
	void ServerConfirmThrowNotify();

	// [POTION-009] 서버에서 대기 상태와 손의 포션을 검증한 뒤 실제 물리 투척을 실행한다.
	void ExecutePendingImpactThrow();

	// [POTION-003] 첫 충돌 시 반경 내 플레이어 전원에게 광역 적용 후 소멸
	UFUNCTION()
	void OnImpact(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	// 실제 GE 적용 + 태그 제거를 한 대상에게 수행 (자기 사용 / 광역 공용)
	void ApplyPotionEffectToTarget(AActor* Target);

	// 중복 발동 방지 (OnComponentHit이 여러 번 불릴 수 있음)
	bool bHasImpacted = false;

	// 이번 던지기가 좌클릭 "사용" 발(發)인지 여부. true일 때만 충돌 발동(터짐).
	// Q(단순 투척)는 이 플래그가 false라 어떤 포션이든 절대 안 터진다.
	bool bThrowAsUse = false;

	// 좌클릭 후 투척 몽타주의 Potion Throw Notify를 기다리는 중인지 여부 (서버 권한 상태).
	bool bWaitingForThrowNotify = false;
#pragma endregion
};
