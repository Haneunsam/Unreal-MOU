#include "NPC/NPCSpawner.h"

#include "Base/CharacterBase.h"
#include "Base/ItemBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/NPCItemTransportComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WarehouseComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/RunState.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace NPCSpawnerTags
{
	static const FName SpawnPoint(TEXT("NPCSpawnPoint"));
}

ANPCSpawner::ANPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANPCSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildSpawnPoints();
}

// [NPCSPAWN-000] 기존 시작 스폰을 수행하고 특수 스폰 조건 이벤트를 구독한다.
void ANPCSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	CachedWarehouseActor = ResolveWarehouseActor();
	CachedRunState = ARunState::GetRunState(this);
	BindSpawnConditions();

	if (bSpawnOnBeginPlay)
	{
		SpawnNPCsToLimit();
	}

	EvaluateSpawnConditions();
}

// [NPCSPAWN-001] 스포너 종료 시 창고 이벤트 구독을 해제한다.
void ANPCSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedRunState)
	{
		CachedRunState->OnThreatLevelChanged.RemoveDynamic(
			this,
			&ANPCSpawner::HandleThreatLevelChanged);
	}

	for (UWarehouseComponent* WarehouseComponent : BoundWarehouseComponents)
	{
		if (WarehouseComponent)
		{
			WarehouseComponent->OnWarehouseItemsChanged.RemoveDynamic(
				this,
				&ANPCSpawner::HandleWarehouseItemsChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ANPCSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANPCSpawner, CurrentSpawnCount);
}

ACharacterBase* ANPCSpawner::SpawnOneNPC()
{
	if (!HasAuthority() || !GetWorld() || NPCSpawnDefinitions.IsEmpty())
	{
		return nullptr;
	}

	RemoveInvalidSpawnedNPCs();
	if (CurrentSpawnCount >= MaxSpawnCount)
	{
		return nullptr;
	}

	TArray<USceneComponent*> AvailablePoints;
	for (USceneComponent* SpawnPoint : SpawnPoints)
	{
		if (IsValid(SpawnPoint) && !IsSpawnPointOccupied(SpawnPoint))
		{
			AvailablePoints.Add(SpawnPoint);
		}
	}

	TArray<const FNPCSpawnDefinition*> ValidDefinitions;
	for (const FNPCSpawnDefinition& SpawnDefinition : NPCSpawnDefinitions)
	{
		if (SpawnDefinition.NPCClass && IsSpawnDefinitionAllowedByThreat(SpawnDefinition))
		{
			ValidDefinitions.Add(&SpawnDefinition);
		}
	}

	if (AvailablePoints.IsEmpty() || ValidDefinitions.IsEmpty())
	{
		return nullptr;
	}

	USceneComponent* SelectedPoint = AvailablePoints[FMath::RandRange(0, AvailablePoints.Num() - 1)];
	const FNPCSpawnDefinition& SelectedDefinition =
		*ValidDefinitions[FMath::RandRange(0, ValidDefinitions.Num() - 1)];
	FTransform SpawnTransform = SelectedPoint->GetComponentTransform();

	// Spawn Point가 공중에 있더라도 가장 가까운 NavMesh 바닥을 기준으로 생성합니다.
	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (NavigationSystem->ProjectPointToNavigation(
			SpawnTransform.GetLocation(), ProjectedLocation, FVector(200.0f, 200.0f, 1000.0f)))
		{
			SpawnTransform.SetLocation(ProjectedLocation.Location);
		}
	}

	ACharacterBase* SpawnedNPC = GetWorld()->SpawnActorDeferred<ACharacterBase>(
		SelectedDefinition.NPCClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!SpawnedNPC)
	{
		return nullptr;
	}

	// NPC의 BeginPlay/Blackboard 초기화보다 먼저 정찰 액터를 지정합니다.
	AssignPatrolActors(SpawnedNPC, SelectedDefinition);

	// FinishSpawningActor에서 BeginPlay가 호출되기 전에 AIController가 Pawn을 Possess하도록
	// 런타임 스폰을 자동 Possess 대상에 포함합니다. BP가 PlacedInWorld로만
	// 설정되어 있으면 BeginPlay의 GetBlackboard가 Controller 생성보다 먼저 실행됩니다.
	SpawnedNPC->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCapsuleComponent* CapsuleComponent = SpawnedNPC->GetCapsuleComponent())
	{
		FVector SpawnLocation = SpawnTransform.GetLocation();
		SpawnLocation.Z += CapsuleComponent->GetScaledCapsuleHalfHeight();
		SpawnTransform.SetLocation(SpawnLocation);
	}
	UGameplayStatics::FinishSpawningActor(SpawnedNPC, SpawnTransform);

	// 블루프린트 Construction에서 생성되는 운반 컴포넌트에 첫 월드 Tick 전에 창고를 주입합니다.
	AssignWarehouseActor(SpawnedNPC);

	// 잘못된 AIControllerClass 등으로 자동 Possess가 실패한 경우의 안전망입니다.
	if (!SpawnedNPC->GetController())
	{
		SpawnedNPC->SpawnDefaultController();
	}

	SpawnedNPCs.Add(SpawnedNPC);
	SpawnedNPC->OnDestroyed.AddDynamic(this, &ANPCSpawner::HandleSpawnedNPCDestroyed);
	CurrentSpawnCount = SpawnedNPCs.Num();
	ForceNetUpdate();
	return SpawnedNPC;
}

// [NPCSPAWN-012] 비어 있는 스폰 포인트를 재사용하여 MaxSpawnCount까지 NPC를 채운다.
void ANPCSpawner::SpawnNPCsToLimit()
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 TargetCount = FMath::Max(0, MaxSpawnCount);
	while (CurrentSpawnCount < TargetCount)
	{
		if (!SpawnOneNPC())
		{
			break;
		}
	}

	// 모든 포인트가 사용 중이면 NPC가 포인트에서 벗어난 뒤 다시 채웁니다.
	if (CurrentSpawnCount < TargetCount)
	{
		ScheduleRespawn();
	}
}

// [NPCSPAWN-002] 외부 시스템이나 블루프린트에서 조건부 스폰을 직접 발동한다.
bool ANPCSpawner::TriggerSpecialSpawn()
{
	if (!HasAuthority() || (bTriggerOnlyOnce && bSpecialTriggerConsumed))
	{
		return false;
	}

	const int32 PreviousSpawnCount = CurrentSpawnCount;
	SpawnNPCsToLimit();
	const bool bSpawnedAnyNPC = CurrentSpawnCount > PreviousSpawnCount;
	if (bSpawnedAnyNPC)
	{
		bSpecialTriggerConsumed = true;
	}

	return bSpawnedAnyNPC;
}

// [NPCSPAWN-003] 특수조건 배열에 필요한 월드 이벤트를 구독한다.
void ANPCSpawner::BindSpawnConditions()
{
	if (CachedRunState && !SpecialSpawnConditions.IsEmpty())
	{
		CachedRunState->OnThreatLevelChanged.AddUniqueDynamic(
			this,
			&ANPCSpawner::HandleThreatLevelChanged);
	}

	ConditionWarehouseComponents.SetNum(SpecialSpawnConditions.Num());
	BoundWarehouseComponents.Reset();
	for (int32 ConditionIndex = 0; ConditionIndex < SpecialSpawnConditions.Num(); ++ConditionIndex)
	{
		const FNPCSpecialSpawnCondition& Condition = SpecialSpawnConditions[ConditionIndex];
		if (Condition.ConditionType != ENPCSpecialSpawnConditionType::WarehouseItemStored)
		{
			continue;
		}

		AActor* WarehouseActor = IsValid(Condition.WarehouseActor)
			? Condition.WarehouseActor.Get()
			: CachedWarehouseActor.Get();
		UWarehouseComponent* WarehouseComponent = IsValid(WarehouseActor)
			? WarehouseActor->FindComponentByClass<UWarehouseComponent>()
			: nullptr;
		ConditionWarehouseComponents[ConditionIndex] = WarehouseComponent;

		if (WarehouseComponent && !BoundWarehouseComponents.Contains(WarehouseComponent))
		{
			WarehouseComponent->OnWarehouseItemsChanged.AddUniqueDynamic(
				this,
				&ANPCSpawner::HandleWarehouseItemsChanged);
			BoundWarehouseComponents.Add(WarehouseComponent);
		}
	}
}

// [NPCSPAWN-004] 모든 특수조건을 결합해 현재 스포너 발동 여부를 검사한다.
void ANPCSpawner::EvaluateSpawnConditions()
{
	if (SpecialSpawnConditions.IsEmpty())
	{
		bSpecialConditionWasMet = false;
		return;
	}

	bool bSpecialConditionsMet = SpecialConditionMatchMode == ENPCSpawnConditionMatchMode::All;
	for (int32 ConditionIndex = 0; ConditionIndex < SpecialSpawnConditions.Num(); ++ConditionIndex)
	{
		const bool bConditionMet = IsSpecialSpawnConditionMet(ConditionIndex);
		if (SpecialConditionMatchMode == ENPCSpawnConditionMatchMode::All && !bConditionMet)
		{
			bSpecialConditionsMet = false;
			break;
		}
		if (SpecialConditionMatchMode == ENPCSpawnConditionMatchMode::Any && bConditionMet)
		{
			bSpecialConditionsMet = true;
			break;
		}
	}

	UpdateSpecialSpawnCondition(bSpecialConditionsMet);
}

// [NPCSPAWN-005] 조건의 거짓→참 전환을 감지해 중복 없이 특수 스폰한다.
void ANPCSpawner::UpdateSpecialSpawnCondition(bool bConditionMet)
{
	if (bTriggerOnlyOnce && bSpecialTriggerConsumed)
	{
		return;
	}

	if (!bConditionMet)
	{
		bSpecialConditionWasMet = false;
		bSpecialConditionSpawnedThisCycle = false;
		return;
	}

	if (bSpecialConditionWasMet && bSpecialConditionSpawnedThisCycle)
	{
		return;
	}

	bSpecialConditionWasMet = true;
	bSpecialConditionSpawnedThisCycle = TriggerSpecialSpawn();
}

// [NPCSPAWN-006] 위험도 변경 시 이미 충족된 특수조건에서 새 후보가 생겼는지 다시 검사한다.
void ANPCSpawner::HandleThreatLevelChanged(float ThreatLevel)
{
	EvaluateSpawnConditions();
}

// [NPCSPAWN-007] 창고 내용이 변경되면 전체 특수조건을 다시 검사한다.
void ANPCSpawner::HandleWarehouseItemsChanged()
{
	EvaluateSpawnConditions();
}

// [NPCSPAWN-010] 특수조건 배열의 지정 항목 하나가 충족됐는지 확인한다.
bool ANPCSpawner::IsSpecialSpawnConditionMet(int32 ConditionIndex) const
{
	if (!SpecialSpawnConditions.IsValidIndex(ConditionIndex)
		|| !ConditionWarehouseComponents.IsValidIndex(ConditionIndex))
	{
		return false;
	}

	const FNPCSpecialSpawnCondition& Condition = SpecialSpawnConditions[ConditionIndex];
	if (Condition.ConditionType != ENPCSpecialSpawnConditionType::WarehouseItemStored)
	{
		return false;
	}

	const UWarehouseComponent* WarehouseComponent = ConditionWarehouseComponents[ConditionIndex];
	if (!WarehouseComponent)
	{
		return false;
	}

	int32 StoredQuantity = 0;
	if (Condition.RequiredItemClass)
	{
		StoredQuantity = WarehouseComponent->GetStoredQuantity(Condition.RequiredItemClass);
	}
	else
	{
		for (const FStoredItemData& StoredItem : WarehouseComponent->StoredItems)
		{
			StoredQuantity += FMath::Max(0, StoredItem.Quantity);
		}
	}

	return StoredQuantity >= FMath::Max(1, Condition.RequiredQuantity);
}

// [NPCSPAWN-011] 현재 위험도에서 지정 NPC 정의가 스폰 후보인지 확인한다.
bool ANPCSpawner::IsSpawnDefinitionAllowedByThreat(
	const FNPCSpawnDefinition& SpawnDefinition) const
{
	const float CurrentThreatLevel = CachedRunState
		? FMath::Clamp(CachedRunState->ThreatLevel, 0.0f, 1.0f)
		: 0.0f;
	const float MinimumThreatLevel = FMath::Clamp(
		FMath::Min(SpawnDefinition.MinimumThreatLevel, SpawnDefinition.MaximumThreatLevel),
		0.0f,
		1.0f);
	const float MaximumThreatLevel = FMath::Clamp(
		FMath::Max(SpawnDefinition.MinimumThreatLevel, SpawnDefinition.MaximumThreatLevel),
		0.0f,
		1.0f);

	return CurrentThreatLevel >= MinimumThreatLevel
		&& CurrentThreatLevel <= MaximumThreatLevel;
}

// [NPCSPAWN-008] 명시적으로 지정된 창고 또는 월드에서 가장 가까운 창고를 캐시한다.
AActor* ANPCSpawner::ResolveWarehouseActor()
{
	if (IsValid(WarehouseActorOverride)
		&& WarehouseActorOverride->FindComponentByClass<UWarehouseComponent>())
	{
		CachedWarehouseActor = WarehouseActorOverride;
		return CachedWarehouseActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		CachedWarehouseActor = nullptr;
		return nullptr;
	}

	AActor* NearestWarehouse = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
	{
		AActor* Candidate = *Iterator;
		if (!IsValid(Candidate) || !Candidate->FindComponentByClass<UWarehouseComponent>())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestWarehouse = Candidate;
		}
	}

	CachedWarehouseActor = NearestWarehouse;
	return CachedWarehouseActor;
}

// [NPCSPAWN-009] 운반 컴포넌트가 있는 NPC에 캐시된 창고를 주입한다.
void ANPCSpawner::AssignWarehouseActor(ACharacterBase* SpawnedNPC)
{
	if (!SpawnedNPC)
	{
		return;
	}

	UNPCItemTransportComponent* TransportComponent =
		SpawnedNPC->FindComponentByClass<UNPCItemTransportComponent>();
	if (!TransportComponent)
	{
		return;
	}

	AActor* WarehouseActor = IsValid(CachedWarehouseActor)
		? CachedWarehouseActor.Get()
		: ResolveWarehouseActor();
	TransportComponent->SourceWarehouseActor = WarehouseActor;
}

void ANPCSpawner::HandleSpawnedNPCDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority())
	{
		return;
	}

	SpawnedNPCs.Remove(Cast<ACharacterBase>(DestroyedActor));
	RemoveInvalidSpawnedNPCs();
	ForceNetUpdate();
	ScheduleRespawn();
}

// [NPCSPAWN-013] 살아 있는 NPC 수가 MaxSpawnCount보다 적으면 재시도 타이머를 예약한다.
void ANPCSpawner::ScheduleRespawn()
{
	const int32 TargetCount = FMath::Max(0, MaxSpawnCount);
	if (!HasAuthority() || !bAutoRespawn || CurrentSpawnCount >= TargetCount
		|| GetWorldTimerManager().IsTimerActive(RespawnTimerHandle))
	{
		return;
	}

	// 파괴 처리 중 즉시 Spawn하지 않도록 최소 한 프레임 이후에 실행합니다.
	const float SafeRespawnDelay = FMath::Max(RespawnDelay, KINDA_SMALL_NUMBER);
	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle, this, &ANPCSpawner::RespawnMissingNPCs, SafeRespawnDelay, false);
}

// [NPCSPAWN-014] 비워진 스폰 포인트를 이용해 부족한 NPC를 다시 생성한다.
void ANPCSpawner::RespawnMissingNPCs()
{
	if (!HasAuthority())
	{
		return;
	}

	RemoveInvalidSpawnedNPCs();
	SpawnNPCsToLimit();

	// 일시적으로 모든 Spawn Point가 막혀 있었다면 다음 주기에 다시 시도합니다.
	const int32 TargetCount = FMath::Max(0, MaxSpawnCount);
	if (CurrentSpawnCount < TargetCount)
	{
		ScheduleRespawn();
	}
}

void ANPCSpawner::RebuildSpawnPoints()
{
	SpawnPointCount = FMath::Max(0, SpawnPointCount);

	TArray<USceneComponent*> ExistingPoints;
	GetComponents<USceneComponent>(ExistingPoints);
	ExistingPoints.RemoveAll([](const USceneComponent* Component)
	{
		return !Component || !Component->ComponentHasTag(NPCSpawnerTags::SpawnPoint);
	});

	ExistingPoints.Sort([](const USceneComponent& Left, const USceneComponent& Right)
	{
		return Left.GetName() < Right.GetName();
	});

	while (ExistingPoints.Num() > SpawnPointCount)
	{
		USceneComponent* PointToRemove = ExistingPoints.Pop();
		RemoveInstanceComponent(PointToRemove);
		PointToRemove->DestroyComponent();
	}

	while (ExistingPoints.Num() < SpawnPointCount)
	{
		const int32 NewIndex = ExistingPoints.Num();
		const FName ComponentName = MakeUniqueObjectName(
			this, USceneComponent::StaticClass(), *FString::Printf(TEXT("SpawnPoint_%02d"), NewIndex));
		USceneComponent* NewPoint = NewObject<USceneComponent>(this, ComponentName, RF_Transactional);
		NewPoint->ComponentTags.Add(NPCSpawnerTags::SpawnPoint);
		NewPoint->SetupAttachment(SceneRoot);
		NewPoint->SetRelativeLocation(FVector(NewIndex * 150.0f, 0.0f, 0.0f));
		AddInstanceComponent(NewPoint);
		NewPoint->RegisterComponent();
		ExistingPoints.Add(NewPoint);
	}

	SpawnPoints.Reset(ExistingPoints.Num());
	for (USceneComponent* ExistingPoint : ExistingPoints)
	{
		SpawnPoints.Add(ExistingPoint);
	}
}

void ANPCSpawner::RemoveInvalidSpawnedNPCs()
{
	SpawnedNPCs.RemoveAll([](const TObjectPtr<ACharacterBase>& NPC)
	{
		return !IsValid(NPC);
	});
	CurrentSpawnCount = SpawnedNPCs.Num();
}

bool ANPCSpawner::IsSpawnPointOccupied(const USceneComponent* SpawnPoint) const
{
	if (!SpawnPoint)
	{
		return true;
	}

	constexpr float OccupiedDistanceSquared = 100.0f * 100.0f;
	const FVector SpawnLocation = SpawnPoint->GetComponentLocation();
	for (const ACharacterBase* SpawnedNPC : SpawnedNPCs)
	{
		if (IsValid(SpawnedNPC)
			&& FVector::DistSquared2D(SpawnedNPC->GetActorLocation(), SpawnLocation) <= OccupiedDistanceSquared)
		{
			return true;
		}
	}

	return false;
}

void ANPCSpawner::AssignPatrolActors(
	ACharacterBase* SpawnedNPC, const FNPCSpawnDefinition& SpawnDefinition) const
{
	if (!SpawnedNPC)
	{
		return;
	}

	// 현재 BP_Base_NPC에 정의된 정찰 변수에 BeginPlay 전에 값을 주입합니다.
	if (FObjectPropertyBase* SplineProperty = FindFProperty<FObjectPropertyBase>(
		SpawnedNPC->GetClass(), TEXT("PatrolSplineActor")))
	{
		SplineProperty->SetObjectPropertyValue_InContainer(
			SpawnedNPC, SpawnDefinition.PatrolSplineActor.Get());
	}

	if (FObjectPropertyBase* BoundProperty = FindFProperty<FObjectPropertyBase>(
		SpawnedNPC->GetClass(), TEXT("PatrolBoundActor")))
	{
		BoundProperty->SetObjectPropertyValue_InContainer(
			SpawnedNPC, SpawnDefinition.PatrolBoundActor.Get());
	}
}

