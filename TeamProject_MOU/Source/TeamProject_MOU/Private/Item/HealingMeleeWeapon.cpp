#include "Item/HealingMeleeWeapon.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Base/BaseAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "Player/MainCharacter.h"

AHealingMeleeWeapon::AHealingMeleeWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	MeshComponent->SetRelativeLocation(FVector::ZeroVector);
	TargetTeam = EWeaponTargetTeam::Player;
	HitMode = EWeaponHitMode::Melee;
	ItemName = FText::FromString(TEXT("Healing Melee Weapon"));
}

void AHealingMeleeWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if ENABLE_DRAW_DEBUG
	if (bShowSwingRange && !IsHidden())
	{
		if (const AMainCharacter* Wielder = Cast<AMainCharacter>(GetOwningPawn()))
		{
			if (GetAttachParentActor() == Wielder)
			{
				const FVector Start = Wielder->GetActorLocation();
				const FVector Forward = Wielder->GetBaseAimRotation().Vector();
				const float Range = FMath::Max(1.0f, SwingRange);
				const float Radius = FMath::Max(1.0f, SwingRadius);
				DrawDebugCapsule(GetWorld(), Start + Forward * Range * 0.5f,
					Range * 0.5f + Radius, Radius, FQuat::FindBetweenNormals(FVector::UpVector, Forward),
					FColor::Yellow, false, -1.0f, 0, 1.5f);
				DrawDebugSphere(GetWorld(), Start, Range, 32, FColor::Blue, false, -1.0f);
				DrawDebugDirectionalArrow(GetWorld(), Start, Start + Forward * Range, 15.0f,
					FColor::Yellow, false, -1.0f);
			}
		}
	}
#endif
	if (HasAuthority() && bSwingHitPending && GetWorld()->GetTimeSeconds() >= SwingHitTime)
	{
		bSwingHitPending = false;
		ResolveSwing();
	}
	if (HasAuthority() && IsInUse() && GetWorld()->GetTimeSeconds() >= NextSwingTime)
	{
		FinishUse();
	}
}

bool AHealingMeleeWeapon::IsValidTarget(AActor* HitActor) const
{
	const AMainCharacter* Player = Cast<AMainCharacter>(HitActor);
	return IsValid(Player) && Player != GetOwningPawn() && !Player->bIsDead;
}

void AHealingMeleeWeapon::Fire()
{
	AMainCharacter* Wielder = Cast<AMainCharacter>(GetOwningPawn());
	if (!HasAuthority() || !IsValid(Wielder) || Wielder->bIsDead ||
		GetOwner() != Wielder || GetAttachParentActor() != Wielder || IsHidden() ||
		GetWorld()->GetTimeSeconds() < NextSwingTime)
	{
		return;
	}

	const float HitDelay = FMath::Max(0.0f, SwingHitDelay);
	NextSwingTime = GetWorld()->GetTimeSeconds() + FMath::Max(HitDelay + 0.01f, SwingCooldown);
	bIsInUse = true;
	SwingWielder = Wielder;
	SwingHitTime = GetWorld()->GetTimeSeconds() + HitDelay;
	bSwingHitPending = true;
	CurrentDurability = FMath::Max(0.0f, CurrentDurability - FMath::Max(0.0f, DurabilityCostPerSwing));
	MulticastSwing(Wielder);
}

void AHealingMeleeWeapon::ResolveSwing()
{
	AMainCharacter* Wielder = SwingWielder.Get();
	if (!HasAuthority() || !IsValid(Wielder) || Wielder->bIsDead || Wielder->bIsGroggy ||
		GetOwningPawn() != Wielder || GetOwner() != Wielder || GetAttachParentActor() != Wielder || IsHidden())
	{
		return;
	}

	// 카메라 위치 대신 캐릭터 중심에서 시작해 3인칭 카메라의 벽 관통을 방지한다.
	const FVector Start = Wielder->GetActorLocation();
	const FVector Forward = Wielder->GetBaseAimRotation().Vector();
	const float Range = FMath::Max(1.0f, SwingRange);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HealingMeleeSwing), false, Wielder);
	Params.AddIgnoredActor(this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, Start + Forward * Range,
		FQuat::Identity, Objects, FCollisionShape::MakeSphere(FMath::Max(1.0f, SwingRadius)), Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValidTarget(Target))
		{
			continue;
		}
		const FVector Offset = Target->GetActorLocation() - Start;
		if (FVector::DotProduct(Offset, Forward) <= 0.0f || Offset.SizeSquared() > FMath::Square(Range))
		{
			continue;
		}
		FHitResult Obstruction;
		if (GetWorld()->LineTraceSingleByChannel(Obstruction, Start, Target->GetActorLocation(), ECC_Visibility, Params)
			&& Obstruction.GetActor() != Target)
		{
			continue;
		}
		bResolvingSwing = true;
		ApplyWeaponHit(Target, Hit);
		bResolvingSwing = false;
		break; // 한 번 휘두를 때 한 명에게 한 번만 적용.
	}
}

void AHealingMeleeWeapon::ApplyWeaponHit_Implementation(AActor* HitActor, const FHitResult& Hit)
{
	if (!HasAuthority() || !bResolvingSwing || !IsValidTarget(HitActor) || HealAmount <= 0.0f)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Cast<AMainCharacter>(HitActor)->GetAbilitySystemComponent();
	if (!ASC || !ASC->HasAttributeSetForAttribute(UBaseAttributeSet::GetHealthAttribute()) ||
		ASC->GetNumericAttribute(UBaseAttributeSet::GetHealthAttribute()) <= 0.0f ||
		ASC->GetNumericAttribute(UBaseAttributeSet::GetHealthAttribute()) >= ASC->GetNumericAttribute(UBaseAttributeSet::GetMaxHealthAttribute()))
	{
		return;
	}

	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBaseAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(HealAmount);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(GetOwningPawn(), this);
	Context.AddSourceObject(this);
	ASC->ApplyGameplayEffectToSelf(Effect, 1.0f, Context);
	MulticastHealEffect(HitActor, HitActor->GetActorLocation());
}

void AHealingMeleeWeapon::MulticastSwing_Implementation(AMainCharacter* Wielder)
{
	if (IsValid(Wielder) && SwingMontage)
	{
		Wielder->PlayAnimMontage(SwingMontage);
	}
	OnSwingEffect();
}

void AHealingMeleeWeapon::MulticastHealEffect_Implementation(AActor* Target, FVector Location)
{
	OnHealEffect(Target, Location);
}
