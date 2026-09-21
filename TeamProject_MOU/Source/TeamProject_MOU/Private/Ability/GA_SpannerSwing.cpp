#include "Ability/GA_SpannerSwing.h"
#include "Item/HealingMeleeWeapon.h"
#include "Player/MainCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SpannerAbility, "Ability.Weapon.SpannerSwing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SpannerUsing, "State.Weapon.SpannerSwing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SpannerCooldown, "Cooldown.Weapon.SpannerSwing");

// [SPANNER-000] 복제 가능한 지속형 쿨다운 GE 기본값 설정
UGE_SpannerCooldown::UGE_SpannerCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.0f);
}

// [SPANNER-001] 서버 실행 정책과 사용 중·차단 태그 설정
UGA_SpannerSwing::UGA_SpannerSwing()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(TAG_SpannerAbility));
	ActivationOwnedTags.AddTag(TAG_SpannerUsing);
	ActivationBlockedTags.AddTag(TAG_SpannerUsing);
	ActivationBlockedTags.AddTag(TAG_SpannerCooldown);
	for (const TCHAR* Name : {TEXT("State.Player.Dead"), TEXT("State.Player.Groggy"), TEXT("State.Stunned"), TEXT("State.Player.Held")})
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), false);
		if (Tag.IsValid()) ActivationBlockedTags.AddTag(Tag);
	}
}

// [SPANNER-002] 소스 무기·소유자·쿨다운 활성화 조건 검사
bool UGA_SpannerSwing::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const AHealingMeleeWeapon* Source = Cast<AHealingMeleeWeapon>(GetSourceObject(Handle, ActorInfo));
	return Source && Source->CanStartSwing() && ActorInfo && ActorInfo->AvatarActor.Get() == Source->GetOwningPawn();
}

// [SPANNER-003] 내구도·쿨다운 적용 후 몽타주와 지연 판정 태스크 실행
void UGA_SpannerSwing::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	Weapon = Cast<AHealingMeleeWeapon>(GetSourceObject(Handle, ActorInfo));
	if (!Weapon.IsValid() || !Weapon->CanStartSwing() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AHealingMeleeWeapon* Source = Weapon.Get();
	const float HitDelay = FMath::Max(0.0f, Source->SwingHitDelay);
	const float Duration = FMath::Max(HitDelay + 0.01f, Source->SwingCooldown);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	FGameplayEffectSpecHandle Cooldown = ASC->MakeOutgoingSpec(UGE_SpannerCooldown::StaticClass(), 1.0f, ASC->MakeEffectContext());
	Cooldown.Data->SetDuration(Duration, true);
	Cooldown.Data->DynamicGrantedTags.AddTag(TAG_SpannerCooldown);
	if (!ASC->ApplyGameplayEffectSpecToSelf(*Cooldown.Data.Get()).IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Source->BeginAbilitySwing(this);
	bHitResolved = false;
	for (const TCHAR* Name : {TEXT("State.Player.Dead"), TEXT("State.Player.Groggy"), TEXT("State.Stunned"), TEXT("State.Player.Held")})
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), false);
		if (!Tag.IsValid()) continue;
		auto* Task = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tag, nullptr, true);
		Task->Added.AddDynamic(this, &UGA_SpannerSwing::CancelSwing);
		Task->ReadyForActivation();
		if (!IsActive()) return;
	}
	if (Source->SwingMontage)
	{
		auto* Montage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Source->SwingMontage, 1.0f, NAME_None, false);
		Montage->OnInterrupted.AddDynamic(this, &UGA_SpannerSwing::CancelSwing);
		Montage->OnCancelled.AddDynamic(this, &UGA_SpannerSwing::CancelSwing);
		Montage->ReadyForActivation();
		if (!IsActive()) return;
	}
	auto* Finish = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	Finish->OnFinish.AddDynamic(this, &UGA_SpannerSwing::CompleteSwing);
	Finish->ReadyForActivation();
	if (HitDelay <= 0.0f) ResolveHit();
	else
	{
		auto* Hit = UAbilityTask_WaitDelay::WaitDelay(this, HitDelay);
		Hit->OnFinish.AddDynamic(this, &UGA_SpannerSwing::ResolveHit);
		Hit->ReadyForActivation();
	}
}

// [SPANNER-004] 종료·취소 시 무기 사용 상태와 태스크 정리
void UGA_SpannerSwing::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive() || bEnding) return;
	bEnding = true;
	if (bWasCancelled) MontageStop();
	if (Weapon.IsValid()) Weapon->EndAbilitySwing(this);
	Weapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// [SPANNER-005] 지연 완료 시 유효성 재검사 후 서버 치유 판정
void UGA_SpannerSwing::ResolveHit()
{
	if (bHitResolved || !IsActive()) return;
	bHitResolved = true;
	if (!Weapon.IsValid() || !Weapon->IsSwingOwnerValid()) { CancelSwing(); return; }
	Weapon->ResolveSwing();
}

// [SPANNER-006] 사용 시간 만료 시 정상 종료
void UGA_SpannerSwing::CompleteSwing()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// [SPANNER-007] 사망·행동 불능·몽타주 중단 시 취소
void UGA_SpannerSwing::CancelSwing()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
