#include "AI/NPCAbilityBlueprintLibrary.h"

#include "AbilitySystemComponent.h"

bool UNPCAbilityBlueprintLibrary::HasActiveAbilityWithTag(
	UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTag AbilityTag)
{
	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer AbilityTags(AbilityTag);
	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	AbilitySystemComponent->GetActivatableGameplayAbilitySpecsByAllMatchingTags(
		AbilityTags,
		MatchingSpecs,
		false);

	for (const FGameplayAbilitySpec* AbilitySpec : MatchingSpecs)
	{
		if (AbilitySpec && AbilitySpec->IsActive())
		{
			return true;
		}
	}

	return false;
}

bool UNPCAbilityBlueprintLibrary::CancelAbilitiesByTag(
	UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTag AbilityTag)
{
	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return false;
	}

	const bool bHadActiveAbility = HasActiveAbilityWithTag(AbilitySystemComponent, AbilityTag);
	FGameplayTagContainer AbilityTags(AbilityTag);
	AbilitySystemComponent->CancelAbilities(&AbilityTags);

	return bHadActiveAbility;
}
