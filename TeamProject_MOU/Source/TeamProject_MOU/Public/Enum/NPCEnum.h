#pragma once

#include "CoreMinimal.h"
#include "NPCEnum.generated.h"

/* NPC 종류 */
UENUM(BlueprintType)
enum class ENpcType : uint8
{
	Normal		UMETA(DisplayName = "Nomal", ToolTip = "기본 NPC"),
	Push		UMETA(DisplayName = "Push", ToolTip = "플레이어를 미는 NPC"),
	Shop		UMETA(DisplayName = "Shop", ToolTip = "상점 NPC"),
	Quest		UMETA(DisplayName = "Quest", ToolTip = "퀘스트 NPC"),
	Stun		UMETA(DisplayName = "Stun", ToolTip = "플레이어를 기절시키는 NPC"),
	Snatch		UMETA(DisplayName = "Snatch", ToolTip = "플레이어의 물건을 뺏어서 도망가는 NPC"),
	Hold		UMETA(DisplayName = "Hold", ToolTip = "플레이어를 드는 NPC"),
	Blocking	UMETA(DisplayName = "Blocking", ToolTip = "플레이어의 길을 막는 NPC")
};

/* NPC 시작 상태*/
UENUM(BlueprintType)
enum class ENPCStartState : uint8
{
    Stay,
    Patrol,
    /*홈 위치로 복귀*/
    Home
};

/* NPC 정찰 형태 */
UENUM(BlueprintType)
enum class ENPCPatrolType : uint8
{
    /* NPC 현재 위치를 기준으로 지정 반경 안에서 정찰 */
    RandomRadius UMETA(DisplayName = "근처 반경"),
    /* 지정한 스플라인의 임의 지점 주변에서 정찰 */
    Spline UMETA(DisplayName = "스플라인"),
    /* 지정한 영역 안에서만 정찰 */
    Area UMETA(DisplayName = "영역")
};

/*NPC 행동 후 정책*/
UENUM(BlueprintType)
enum class ENPCAfterActionPolicy : uint8
{
    /*타깃이 계속 보이면 다시 추적/공격 반복*/
    RepeatWhileTargetVisible,
    /*행동 후 정지*/
    OneShotThenStay,
    /*기존 에셋 직렬화 호환용. 새 에셋에서는 사용하지 않음*/
    OneShotThenTracking UMETA(Hidden, Deprecated, DeprecationMessage = "OneShot Tracking은 RepeatWhileTargetVisible을 사용하세요."),
    /*행동 후 정찰*/
    OneShotThenPatrol
};

/*NPC 타겟을 잃었을 때 정책*/
UENUM(BlueprintType)
enum class ENPCLostTargetPolicy : uint8
{
    /*행동 후 대기*/
    ReturnToStay,
    /*행동 후 정찰*/
    ReturnToPatrol,
    /*행동 후 원래 위치로*/
    ReturnHome
};

/* NPC가 여러 타깃을 감지했을 때 타깃을 선택하는 방식 */
UENUM(BlueprintType)
enum class ENPCTargetSelectionPolicy : uint8
{
    /* 현재 타깃을 완전히 잃기 전까지 새 타깃으로 변경하지 않음 */
    LockUntilLost UMETA(DisplayName = "현재 타깃 유지"),
    /* 감지 중인 타깃 가운데 NPC와 가장 가까운 타깃을 선택 */
    NearestDynamic UMETA(DisplayName = "가장 가까운 타깃")
};

/* 추적 상태에서 NPC가 타깃에게 이동하는 방식 */
UENUM(BlueprintType)
enum class ENPCTrackingMovementPolicy : uint8
{
    /* 타깃이 감지되면 항상 추적 */
    AlwaysChase UMETA(DisplayName = "항상 추적"),
    /* 타깃이 NPC를 보고 있지 않을 때만 추적 */
    ChaseWhenNotObserved UMETA(DisplayName = "보이지 않을 때만 추적"),
    /* 타깃을 감지하고 행동은 하지만 현재 위치에서 이동하지 않음 */
    Stationary UMETA(DisplayName = "이동하지 않음")
};

/* NPC가 우선적으로 수행할 목표 유형 */
UENUM(BlueprintType)
enum class ENPCObjectiveType : uint8
{
    /* 감지한 플레이어를 추적하고 Primary Ability를 실행 */
    PlayerAction UMETA(DisplayName = "플레이어 대상 행동"),
    /* 창고 아이템을 예약해 지정된 목적지로 운반 */
    WarehouseItemTransport UMETA(DisplayName = "창고 아이템 운반")
};
