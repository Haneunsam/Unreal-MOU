#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Enum/NPCEnum.h"
#include "NPC/NPCActionStruct.h"
#include "NPCData.generated.h"

UCLASS()
class TEAMPROJECT_MOU_API UNPCData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    UNPCData();

    /* NPC가 우선적으로 수행할 목표 유형 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior", meta = (ToolTip = "플레이어 대상 행동 또는 창고 아이템 운반 중 NPC의 주 목표를 선택합니다."))
    ENPCObjectiveType ObjectiveType;

    /* NPC 시작 상태 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior", meta = (ToolTip = "NPC 시작 상태"))
    ENPCStartState StartState;

    /* 정찰 사용 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior", meta = (ToolTip = "정찰 사용 여부"))
    bool UsePatrol;

    /* NPC 정찰 위치를 선택하는 방식 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Patrol", meta = (EditCondition = "UsePatrol", EditConditionHides, ToolTip = "근처 반경, 스플라인, 영역 중 정찰 방식을 선택합니다."))
    ENPCPatrolType PatrolType;

    /* 근처 반경 정찰에서 사용할 탐색 반경 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Patrol", meta = (EditCondition = "UsePatrol && PatrolType == ENPCPatrolType::RandomRadius", EditConditionHides, ClampMin = "0.0", Units = "cm", ToolTip = "NPC 현재 위치를 기준으로 정찰 지점을 찾을 반경입니다."))
    float PatrolRadius;

    /* 스플라인 정찰 지점 주변에서 허용할 무작위 반경 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Patrol", meta = (EditCondition = "UsePatrol && PatrolType == ENPCPatrolType::Spline", EditConditionHides, ClampMin = "0.0", Units = "cm", ToolTip = "스플라인 위 임의 지점을 기준으로 주변 정찰 지점을 찾을 반경입니다."))
    float SplinePatrolRadius;

    /* 정찰, 복귀 및 일반 이동 중 사용할 최대 걷기 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement", meta = (ClampMin = "0.0", Units = "cm/s", ToolTip = "정찰, 복귀 및 일반 이동 중 사용할 최대 이동 속도입니다."))
    float PatrolMoveSpeed = 250.0f;

    /* 플레이어를 추적할 때 사용할 최대 걷기 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement", meta = (ClampMin = "0.0", Units = "cm/s", ToolTip = "플레이어 타깃을 추적할 때 사용할 최대 이동 속도입니다."))
    float ChaseMoveSpeed = 500.0f;

    /* NPC 행동 후 정책 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior", meta = (ToolTip = "행동 후 정책"))
    ENPCAfterActionPolicy AfterActionPolicy;

    /* NPC 타깃 상실 시 정책 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior", meta = (ToolTip = "타깃 상실 시 정책"))
    ENPCLostTargetPolicy LostTargetPolicy;

    /* 공용 GA 태그 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Ability", meta = (ToolTip = "공용 GA 태그"))
    FGameplayTag PrimaryAbilityTag;

    /* NPC 밀기, 잡기, 던지기 행동 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Action", meta = (ToolTip = "NPC별 밀기, 잡기, 던지기 행동 설정"))
    FNPCActionStruct ActionData;

    /* NPC 행동 시작 범위 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Combat", meta = (ToolTip = "액션 시작 거리"))
    float ActionRange;

    /* NPC 행동 인터벌 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Combat", meta = (ToolTip = "액션 후 인터벌"))
    float ActionInterval;

    /* 감지 범위 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Perception", meta = (ToolTip = "타깃 감지 범위"))
    float SightRadius;

    /* 감지 해제 범위 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Perception", meta = (ToolTip = "타깃 감지 해제 범위"))
    float LoseSightRadius;

    /* 여러 타깃을 감지했을 때 타깃 선택 방식 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Perception", meta = (ToolTip = "현재 타깃을 유지할지, 가장 가까운 타깃으로 계속 변경할지 선택합니다."))
    ENPCTargetSelectionPolicy TargetSelectionPolicy;

    /* 시야에서 사라진 타깃을 유지하는 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Perception", meta = (ClampMin = "0.0", Units = "s", ToolTip = "타깃을 순간적으로 놓쳐도 이 시간 동안 유지합니다. 시간 안에 다시 감지하면 타깃 상실 처리를 취소합니다."))
    float TargetLoseGraceTime;

    /* 추적 상태에서 타깃에게 이동하는 방식 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Tracking", meta = (ToolTip = "항상 추적, 타깃이 보고 있지 않을 때만 추적, 이동하지 않음 중 하나를 선택합니다."))
    ENPCTrackingMovementPolicy TrackingMovementPolicy;

    /* 플레이어가 NPC를 바라본다고 판단할 시선 내적 기준 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Tracking", meta = (EditCondition = "TrackingMovementPolicy == ENPCTrackingMovementPolicy::ChaseWhenNotObserved", EditConditionHides, ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "값이 클수록 플레이어가 NPC를 더 정확히 바라봐야 추적을 멈춥니다. 여러 플레이어 중 한 명이라도 조건을 만족하면 멈춥니다. 0.65는 약 49도의 시야각입니다."))
    float ObservedViewDotThreshold;

    /* 플레이어와 NPC 사이 장애물 검사 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Tracking", meta = (EditCondition = "TrackingMovementPolicy == ENPCTrackingMovementPolicy::ChaseWhenNotObserved", EditConditionHides, ToolTip = "활성화하면 벽이나 장애물에 가려진 NPC는 플레이어가 보고 있는 것으로 처리하지 않습니다."))
    bool RequireLineOfSightForObservation;

    /* 시작 시 적용할 상태 태그 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    FGameplayTag GetStartStateTag() const;

    /* 플레이어 감지와 추적을 사용하는 NPC인지 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    bool UsesPlayerActionObjective() const;

    /* 창고 아이템 운반 작업을 사용하는 NPC인지 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    bool UsesWarehouseItemTransportObjective() const;

    /* 타깃이 보이는 동안 반복 행동하는지 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    bool ShouldRepeatActionWhileTargetVisible() const;

    /* 1회 행동 후 적용할 다음 상태 태그 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    FGameplayTag GetOneShotAfterActionStateTag() const;

    /* 타깃 상실 시 Home 복귀를 사용하는지 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    bool ShouldReturnHomeOnLostTarget() const;

    /* 타깃 상실 시 적용할 상태 태그 */
    UFUNCTION(BlueprintPure, Category = "NPC|Behavior")
    FGameplayTag GetLostTargetStateTag() const;

    /*태그 정보 받아오기*/
    FGameplayTag PatrolTag() const;
    FGameplayTag TrackingTag() const;
    FGameplayTag StayTag() const;
    FGameplayTag HomeTag() const;
};
