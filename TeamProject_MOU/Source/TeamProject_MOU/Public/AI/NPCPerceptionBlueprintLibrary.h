#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NPCPerceptionBlueprintLibrary.generated.h"

class UAIPerceptionComponent;
class UNPCData;

UCLASS()
class TEAMPROJECT_MOU_API UNPCPerceptionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** AI Perception의 Sight 설정값을 런타임에 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "NPC|Perception")
	static bool ApplySightConfig(
		UAIPerceptionComponent* PerceptionComponent,
		float SightRadius,
		float LoseSightRadius);

	/** 타깃의 시점 방향과 장애물을 기준으로 ObservedActor를 보고 있는지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "NPC|Perception")
	static bool IsTargetLookingAtActor(
		AActor* TargetActor,
		AActor* ObservedActor,
		float ViewDotThreshold = 0.65f,
		bool bRequireLineOfSight = true);

	/** NPC 데이터의 이동 정책과 주변 모든 플레이어의 시선에 따라 현재 타깃을 추적해도 되는지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "NPC|Perception")
	static bool CanChaseTarget(
		const UNPCData* NPCData,
		AActor* ControlledPawn,
		AActor* TargetActor);

	// [NPCMOVE-000] NPCData의 정찰 또는 추적 속도를 캐릭터 이동 컴포넌트에 적용한다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	static bool ApplyMovementSpeed(
		const UNPCData* NPCData,
		AActor* ControlledPawn,
		bool bIsChasing);
};
