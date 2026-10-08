#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPCSpawner.generated.h"

class ACharacterBase;
class AItemBase;
class ARunState;
class UNPCItemTransportComponent;
class USceneComponent;
class UWarehouseComponent;

/** 위험도와 함께 검사할 NPC 특수 스폰 조건 종류입니다. */
UENUM(BlueprintType)
enum class ENPCSpecialSpawnConditionType : uint8
{
	WarehouseItemStored UMETA(DisplayName = "창고 아이템 보관")
};

/** 여러 특수조건을 모두 요구할지 하나만 요구할지 선택합니다. */
UENUM(BlueprintType)
enum class ENPCSpawnConditionMatchMode : uint8
{
	All UMETA(DisplayName = "모든 조건 충족 (AND)"),
	Any UMETA(DisplayName = "하나 이상 충족 (OR)")
};

/** 위험도 외에 추가로 검사할 조건 하나의 설정입니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_MOU_API FNPCSpecialSpawnCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Trigger")
	ENPCSpecialSpawnConditionType ConditionType = ENPCSpecialSpawnConditionType::WarehouseItemStored;

	/** 감시할 창고입니다. 비워두면 스포너에 캐시된 창고를 사용합니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NPC Spawner|Trigger")
	TObjectPtr<AActor> WarehouseActor;

	/** 지정하면 이 종류만 계산하고 비워두면 창고의 모든 아이템을 계산합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Trigger")
	TSubclassOf<AItemBase> RequiredItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Trigger", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredQuantity = 1;
};

/** 스포너에서 선택할 NPC 종류와 해당 NPC가 사용할 정찰 액터 설정입니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_MOU_API FNPCSpawnDefinition
{
	GENERATED_BODY()

	/** 생성할 NPC 클래스입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner")
	TSubclassOf<ACharacterBase> NPCClass;

	/** 이 NPC가 스폰 후보에 포함되는 최소 위험도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Threat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumThreatLevel = 0.0f;

	/** 이 NPC가 스폰 후보에 포함되는 최대 위험도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Threat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumThreatLevel = 1.0f;

	/** 스플라인 정찰 NPC가 사용할 스플라인 액터입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NPC Spawner|Patrol")
	TObjectPtr<AActor> PatrolSplineActor = nullptr;

	/** 영역 정찰 NPC가 사용할 Bound 액터입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NPC Spawner|Patrol")
	TObjectPtr<AActor> PatrolBoundActor = nullptr;
};

/**
 * 월드에 배치한 Spawn Point에서 NPC를 생성하는 서버 권위 스포너입니다.
 * SpawnPointCount를 변경하면 Construction 단계에서 위치 조절용 SceneComponent가 생성됩니다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_MOU_API ANPCSpawner : public AActor
{
	GENERATED_BODY()

public:
	ANPCSpawner();

	virtual void OnConstruction(const FTransform& Transform) override;
	// [NPCSPAWN-000] 기존 시작 스폰을 수행하고 특수 스폰 조건 이벤트를 구독한다.
	virtual void BeginPlay() override;
	// [NPCSPAWN-001] 스포너 종료 시 창고 이벤트 구독을 해제한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 현재 비어 있는 Spawn Point 한 곳에 NPC 하나를 생성합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC Spawner")
	ACharacterBase* SpawnOneNPC();

	// [NPCSPAWN-012] 비어 있는 스폰 포인트를 재사용하여 MaxSpawnCount까지 NPC를 채운다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC Spawner")
	void SpawnNPCsToLimit();

	// [NPCSPAWN-002] 외부 시스템이나 블루프린트에서 조건부 스폰을 직접 발동한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NPC Spawner|Trigger")
	bool TriggerSpecialSpawn();

protected:
	/** 소환할 NPC 종류와 각 종류가 사용할 스플라인/영역 정찰 액터 목록입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner")
	TArray<FNPCSpawnDefinition> NPCSpawnDefinitions;

	/** 동시에 존재할 수 있는 최대 NPC 수입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxSpawnCount = 3;

	/** 현재 이 스포너가 생성하여 살아 있는 NPC 수입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "NPC Spawner")
	int32 CurrentSpawnCount = 0;

	/** Construction에서 생성할 Spawn Point의 개수입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner", meta = (ClampMin = "0", UIMin = "0"))
	int32 SpawnPointCount = 3;

	/** 게임 시작 시 자동으로 NPC를 생성할지 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner")
	bool bSpawnOnBeginPlay = true;

	/** 도둑 NPC가 사용할 창고입니다. 비워두면 스포너에서 가장 가까운 창고를 찾습니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NPC Spawner|Warehouse")
	TObjectPtr<AActor> WarehouseActorOverride;

	/** 기본 위험도 후보 선택과 별도로 스포너를 발동할 추가 조건 목록입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Trigger")
	TArray<FNPCSpecialSpawnCondition> SpecialSpawnConditions;

	/** 추가 조건 목록의 결합 방식입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Trigger", meta = (EditCondition = "SpecialSpawnConditions.Num > 1", EditConditionHides))
	ENPCSpawnConditionMatchMode SpecialConditionMatchMode = ENPCSpawnConditionMatchMode::All;

	/** 활성화하면 조건을 처음 만족한 한 번만 특수 스폰합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Trigger")
	bool bTriggerOnlyOnce = true;

	/** 런타임에 지정하거나 월드에서 찾아 캐시한 창고 액터입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "NPC Spawner|Warehouse")
	TObjectPtr<AActor> CachedWarehouseActor;

	/** 소환된 NPC가 제거되었을 때 최대 수량까지 자동으로 다시 채울지 결정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Respawn")
	bool bAutoRespawn = true;

	/** NPC가 제거된 뒤 부족한 수량을 다시 소환하기까지 기다리는 시간입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Spawner|Respawn", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float RespawnDelay = 5.0f;

	/** Construction에서 생성된 위치 조절용 Spawn Point 목록입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "NPC Spawner")
	TArray<TObjectPtr<USceneComponent>> SpawnPoints;

	/** 현재 이 스포너가 생성하여 살아 있는 NPC 목록입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "NPC Spawner")
	TArray<TObjectPtr<ACharacterBase>> SpawnedNPCs;

private:
	UPROPERTY(VisibleAnywhere, Category = "NPC Spawner")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 이 스포너가 생성한 NPC 목록입니다. */
	FTimerHandle RespawnTimerHandle;

	UFUNCTION()
	void HandleSpawnedNPCDestroyed(AActor* DestroyedActor);

	// [NPCSPAWN-003] 특수조건 배열에 필요한 월드 이벤트를 구독한다.
	void BindSpawnConditions();
	// [NPCSPAWN-004] 모든 특수조건을 결합해 현재 스포너 발동 여부를 검사한다.
	void EvaluateSpawnConditions();
	// [NPCSPAWN-005] 조건의 거짓→참 전환을 감지해 중복 없이 특수 스폰한다.
	void UpdateSpecialSpawnCondition(bool bConditionMet);

	// [NPCSPAWN-006] 위험도 변경 시 이미 충족된 특수조건에서 새 후보가 생겼는지 다시 검사한다.
	UFUNCTION()
	void HandleThreatLevelChanged(float ThreatLevel);

	// [NPCSPAWN-007] 창고 내용이 변경되면 전체 특수조건을 다시 검사한다.
	UFUNCTION()
	void HandleWarehouseItemsChanged();

	// [NPCSPAWN-010] 특수조건 배열의 지정 항목 하나가 충족됐는지 확인한다.
	bool IsSpecialSpawnConditionMet(int32 ConditionIndex) const;
	// [NPCSPAWN-011] 현재 위험도에서 지정 NPC 정의가 스폰 후보인지 확인한다.
	bool IsSpawnDefinitionAllowedByThreat(const FNPCSpawnDefinition& SpawnDefinition) const;

	// [NPCSPAWN-008] 명시적으로 지정된 창고 또는 월드에서 가장 가까운 창고를 캐시한다.
	AActor* ResolveWarehouseActor();
	// [NPCSPAWN-009] 운반 컴포넌트가 있는 NPC에 캐시된 창고를 주입한다.
	void AssignWarehouseActor(ACharacterBase* SpawnedNPC);

	void RebuildSpawnPoints();
	void RemoveInvalidSpawnedNPCs();
	// [NPCSPAWN-013] 살아 있는 NPC 수가 MaxSpawnCount보다 적으면 재시도 타이머를 예약한다.
	void ScheduleRespawn();
	// [NPCSPAWN-014] 비워진 스폰 포인트를 이용해 부족한 NPC를 다시 생성한다.
	void RespawnMissingNPCs();
	bool IsSpawnPointOccupied(const USceneComponent* SpawnPoint) const;
	void AssignPatrolActors(ACharacterBase* SpawnedNPC, const FNPCSpawnDefinition& SpawnDefinition) const;

	UPROPERTY(Transient)
	TObjectPtr<ARunState> CachedRunState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWarehouseComponent>> ConditionWarehouseComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWarehouseComponent>> BoundWarehouseComponents;

	bool bSpecialConditionWasMet = false;
	bool bSpecialConditionSpawnedThisCycle = false;
	bool bSpecialTriggerConsumed = false;
};

