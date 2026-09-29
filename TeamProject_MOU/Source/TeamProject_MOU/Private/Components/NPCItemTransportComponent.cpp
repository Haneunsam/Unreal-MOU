#include "Components/NPCItemTransportComponent.h"

#include "Base/ItemBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StatusComponent.h"
#include "Components/WarehouseComponent.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"

UNPCItemTransportComponent::UNPCItemTransportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// [NPCWORK-000] 시작 시 상태 컴포넌트의 CC 변경 이벤트를 구독한다.
void UNPCItemTransportComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	StatusComponent = Owner ? Owner->FindComponentByClass<UStatusComponent>() : nullptr;
	if (StatusComponent)
	{
		StatusComponent->OnCrowdControlChanged.AddUniqueDynamic(
			this,
			&UNPCItemTransportComponent::HandleCrowdControlChanged);
	}
}

void UNPCItemTransportComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StatusComponent)
	{
		StatusComponent->OnCrowdControlChanged.RemoveDynamic(
			this,
			&UNPCItemTransportComponent::HandleCrowdControlChanged);
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ReleaseReservation();

		if (IsValid(CarriedItem))
		{
			CarriedItem->Drop(GetOwner()->GetActorLocation(), GetOwner());
			CarriedItem = nullptr;
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UNPCItemTransportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNPCItemTransportComponent, CarriedItem);
}

AItemBase* UNPCItemTransportComponent::TryReserveWorkItem()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || IsValid(CarriedItem))
	{
		return nullptr;
	}

	UWarehouseComponent* Warehouse = GetSourceWarehouseComponent();
	if (!Warehouse)
	{
		return nullptr;
	}

	if (IsValid(ReservedItem) && Warehouse->IsItemReservedBy(ReservedItem, Owner) && bHasDestination)
	{
		return ReservedItem;
	}

	ReleaseReservation();
	ReservedItem = Warehouse->ReserveNearestAvailableItem(Owner);
	if (IsValid(ReservedItem) && !TryGenerateDestination(ReservedItem->GetActorLocation()))
	{
		ReleaseReservation();
	}

	return ReservedItem;
}

bool UNPCItemTransportComponent::PickUpReservedItem()
{
	AActor* Owner = GetOwner();
	ACharacter* OwnerCharacter = Cast<ACharacter>(Owner);
	UWarehouseComponent* Warehouse = GetSourceWarehouseComponent();
	if (!Owner || !Owner->HasAuthority() || !OwnerCharacter || !Warehouse || !IsValid(ReservedItem) || IsValid(CarriedItem))
	{
		return false;
	}

	if (!Warehouse->IsItemReservedBy(ReservedItem, Owner)
		|| !ReservedItem->CanBePickedUpBy(Owner)
		|| FVector::DistSquared(Owner->GetActorLocation(), ReservedItem->GetActorLocation()) > FMath::Square(PickupDistance))
	{
		ReleaseReservation();
		return false;
	}

	CarriedItem = ReservedItem;
	CarriedItem->PickUp(Owner);
	AttachCarriedItem();

	Warehouse->ReleaseItemReservation(ReservedItem, Owner);
	ReservedItem = nullptr;
	Owner->ForceNetUpdate();
	CarriedItem->ForceNetUpdate();
	return true;
}

bool UNPCItemTransportComponent::DropCarriedItemAtDestination()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !IsValid(CarriedItem) || !bHasDestination)
	{
		return false;
	}

	const FVector DropLocation = GetDestinationLocation();
	if (FVector::DistSquared(Owner->GetActorLocation(), DropLocation) > FMath::Square(DropDistance))
	{
		return false;
	}

	AItemBase* ItemToDrop = CarriedItem;
	CarriedItem = nullptr;
	ItemToDrop->Drop(DropLocation, Owner);
	DestinationLocation = FVector::ZeroVector;
	bHasDestination = false;
	Owner->ForceNetUpdate();
	ItemToDrop->ForceNetUpdate();
	return true;
}

void UNPCItemTransportComponent::ReleaseReservation()
{
	if (UWarehouseComponent* Warehouse = GetSourceWarehouseComponent())
	{
		Warehouse->ReleaseItemReservation(ReservedItem, GetOwner());
	}

	ReservedItem = nullptr;
	if (!IsValid(CarriedItem))
	{
		DestinationLocation = FVector::ZeroVector;
		bHasDestination = false;
	}
}

FVector UNPCItemTransportComponent::GetDestinationLocation() const
{
	return bHasDestination ? DestinationLocation : FVector::ZeroVector;
}

bool UNPCItemTransportComponent::IsCarryingItem() const
{
	return IsValid(CarriedItem);
}

UWarehouseComponent* UNPCItemTransportComponent::GetSourceWarehouseComponent() const
{
	return IsValid(SourceWarehouseActor)
		? SourceWarehouseActor->FindComponentByClass<UWarehouseComponent>()
		: nullptr;
}

bool UNPCItemTransportComponent::TryGenerateDestination(const FVector& Origin)
{
	bHasDestination = false;
	DestinationLocation = FVector::ZeroVector;

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;
	if (!NavigationSystem)
	{
		return false;
	}

	const float MinimumDistance = FMath::Max(0.0f, MinTransportDistance);
	const float MaximumDistance = FMath::Max(MinimumDistance, MaxTransportDistance);
	const float MinimumDistanceSquared = FMath::Square(MinimumDistance);

	for (int32 Attempt = 0; Attempt < FMath::Max(1, DestinationSearchAttempts); ++Attempt)
	{
		FNavLocation CandidateLocation;
		if (!NavigationSystem->GetRandomReachablePointInRadius(Origin, MaximumDistance, CandidateLocation))
		{
			continue;
		}

		if (FVector::DistSquared2D(Origin, CandidateLocation.Location) < MinimumDistanceSquared)
		{
			continue;
		}

		DestinationLocation = CandidateLocation.Location;
		bHasDestination = true;
		return true;
	}

	return false;
}

void UNPCItemTransportComponent::AttachCarriedItem()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !IsValid(CarriedItem))
	{
		return;
	}

	const FName SocketName = CarriedItem->GetCarrySocketOverride().IsNone()
		? CarrySocketName
		: CarriedItem->GetCarrySocketOverride();
	CarriedItem->AttachToComponent(
		OwnerCharacter->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName);
	CarriedItem->SetActorRelativeLocation(CarriedItem->GetCarryLocationOffset());
	CarriedItem->SetActorRelativeRotation(CarriedItem->GetCarryRotationOffset());

	if (CarriedItem->ShouldCenterOnCarrySocket())
	{
		const FVector BoxCenter = CarriedItem->GetComponentsBoundingBox().GetCenter();
		const FVector Origin = CarriedItem->GetActorLocation();
		if (!Origin.Equals(BoxCenter, 1.0f))
		{
			const FVector Offset = CarriedItem->GetActorTransform().InverseTransformVectorNoScale(Origin - BoxCenter);
			CarriedItem->SetActorRelativeLocation(Offset);
		}
	}
}

// [NPCWORK-001] 이동 불가 CC가 적용되면 운반물을 현재 위치에 놓고 NPC를 제거한다.
void UNPCItemTransportComponent::HandleCrowdControlChanged(bool bIsCrowdControlled)
{
	AActor* Owner = GetOwner();
	if (!bIsCrowdControlled || bHandledCrowdControlDeath || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	bHandledCrowdControlDeath = true;
	ReleaseReservation();

	if (IsValid(CarriedItem))
	{
		AItemBase* ItemToDrop = CarriedItem;
		CarriedItem = nullptr;
		ItemToDrop->Drop(Owner->GetActorLocation(), Owner);
		ItemToDrop->ForceNetUpdate();
	}

	DestinationLocation = FVector::ZeroVector;
	bHasDestination = false;
	Owner->ForceNetUpdate();
	Owner->Destroy();
}

void UNPCItemTransportComponent::OnRep_CarriedItem()
{
	AttachCarriedItem();
}
