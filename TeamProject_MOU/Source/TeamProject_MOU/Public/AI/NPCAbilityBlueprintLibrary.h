#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NPCAbilityBlueprintLibrary.generated.h"

class UAbilitySystemComponent;

UCLASS()
class TEAMPROJECT_MOU_API UNPCAbilityBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 지정한 Ability Tag를 가진 GA가 현재 하나라도 실행 중인지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "NPC|Ability")
	static bool HasActiveAbilityWithTag(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag AbilityTag);

	/** 지정한 Ability Tag를 가진 실행 중인 GA를 모두 취소한다. 취소 대상이 있었으면 true를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "NPC|Ability")
	static bool CancelAbilitiesByTag(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag AbilityTag);
};
