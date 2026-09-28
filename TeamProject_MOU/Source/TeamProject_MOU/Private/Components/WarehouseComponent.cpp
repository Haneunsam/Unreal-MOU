#include "Components/WarehouseComponent.h"

#include "Base/ItemBase.h"
#include "Subsystems/WarehouseDataSubsystem.h"
#include "Net/UnrealNetwork.h"

UWarehouseComponent::UWarehouseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWarehouseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWarehouseComponent, StoredItems);
	DOREPLIFETIME(UWarehouseComponent, StoredItemInstances);
}

void UWarehouseComponent::BeginPlay()
{
	Super::BeginPlay();
	// Component replication also requires a replicated owner.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->SetReplicates(true);
	}
}

void UWarehouseComponent::OnRep_StoredItems()
{
	OnWarehouseItemsChanged.Broadcast();
}

bool UWarehouseComponent::CanStoreItemClass(TSubclassOf<AItemBase> ItemClass) const
{
	return ItemClass && ItemClass->IsChildOf(AItemBase::StaticClass());
}

bool UWarehouseComponent::CanStoreItemInstance(const AItemBase* ItemInstance) const
{
	return ItemInstance && CanStoreItemClass(ItemInstance->GetClass());
}

bool UWarehouseComponent::AddStoredItem(TSubclassOf<AItemBase> ItemClass, int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!CanStoreItemClass(ItemClass) || Quantity <= 0)
	{
		return false;
	}

	const int32 ExistingIndex = FindStoredItemIndex(ItemClass);
	if (ExistingIndex != INDEX_NONE)
	{
		StoredItems[ExistingIndex].Quantity += Quantity;
	}
	else
	{
		FStoredItemData NewItem;
		NewItem.ItemClass = ItemClass;
		NewItem.Quantity = Quantity;
		StoredItems.Add(NewItem);
	}

	BroadcastWarehouseChanged();
	return true;
}

bool UWarehouseComponent::AddStoredItemFromInstance(const AItemBase* ItemInstance, int32 Quantity)
{
	return ItemInstance && AddStoredItem(ItemInstance->GetClass(), Quantity);
}

bool UWarehouseComponent::RemoveStoredItem(TSubclassOf<AItemBase> ItemClass, int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!ItemClass || Quantity <= 0)
	{
		return false;
	}

	const int32 ExistingIndex = FindStoredItemIndex(ItemClass);
	if (ExistingIndex == INDEX_NONE || StoredItems[ExistingIndex].Quantity < Quantity)
	{
		return false;
	}

	StoredItems[ExistingIndex].Quantity -= Quantity;
	if (StoredItems[ExistingIndex].Quantity <= 0)
	{
		StoredItems.RemoveAt(ExistingIndex);
	}

	BroadcastWarehouseChanged();
	return true;
}

int32 UWarehouseComponent::GetStoredQuantity(TSubclassOf<AItemBase> ItemClass) const
{
	const int32 ExistingIndex = FindStoredItemIndex(ItemClass);
	return ExistingIndex != INDEX_NONE ? StoredItems[ExistingIndex].Quantity : 0;
}

bool UWarehouseComponent::ContainsItem(TSubclassOf<AItemBase> ItemClass) const
{
	return GetStoredQuantity(ItemClass) > 0;
}

void UWarehouseComponent::ClearStoredItems()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	StoredItems.Reset();
	StoredItemInstances.Reset();
	ItemReservations.Reset();
	BroadcastWarehouseChanged();
}

TArray<FStoredItemInstanceData> UWarehouseComponent::BuildStoredItemInstanceData() const
{
	TArray<FStoredItemInstanceData> SavedItemInstances;
	SavedItemInstances.Reserve(StoredItemInstances.Num());

	for (const TObjectPtr<AItemBase>& ItemInstance : StoredItemInstances)
	{
		if (!CanStoreItemInstance(ItemInstance))
		{
			continue;
		}

		FStoredItemInstanceData SavedItemInstance;
		ItemInstance->SaveItemToData(SavedItemInstance);

		if (SavedItemInstance.IsValid())
		{
			SavedItemInstances.Add(SavedItemInstance);
		}
	}

	return SavedItemInstances;
}

bool UWarehouseComponent::HandleActorEnteredWarehouse(AActor* OtherActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	AItemBase* ItemInstance = Cast<AItemBase>(OtherActor);
	if (!CanStoreItemInstance(ItemInstance) || StoredItemInstances.Contains(ItemInstance))
	{
		return false;
	}

	StoredItemInstances.Add(ItemInstance);
	return AddStoredItemFromInstance(ItemInstance, 1);
}

bool UWarehouseComponent::HandleActorExitedWarehouse(AActor* OtherActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	AItemBase* ItemInstance = Cast<AItemBase>(OtherActor);
	if (!ItemInstance)
	{
		return false;
	}

	ItemReservations.Remove(ItemInstance);

	const int32 RemovedCount = StoredItemInstances.Remove(ItemInstance);
	if (RemovedCount <= 0)
	{
		return false;
	}

	return RemoveStoredItem(ItemInstance->GetClass(), RemovedCount);
}

AItemBase* UWarehouseComponent::ReserveNearestAvailableItem(AActor* Requester)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Requester))
	{
		return nullptr;
	}

	CleanupItemReservations();

	AItemBase* NearestItem = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (AItemBase* ItemInstance : StoredItemInstances)
	{
		if (!IsValid(ItemInstance) || !ItemInstance->CanBePickedUpBy(Requester))
		{
			continue;
		}

		const TWeakObjectPtr<AActor>* ExistingRequester = ItemReservations.Find(ItemInstance);
		if (ExistingRequester && ExistingRequester->IsValid() && ExistingRequester->Get() != Requester)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			Requester->GetActorLocation(),
			ItemInstance->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestItem = ItemInstance;
		}
	}

	if (NearestItem)
	{
		ItemReservations.FindOrAdd(NearestItem) = Requester;
	}

	return NearestItem;
}

bool UWarehouseComponent::ReserveItemFor(AItemBase* ItemInstance, AActor* Requester)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ItemInstance) || !IsValid(Requester))
	{
		return false;
	}

	CleanupItemReservations();
	if (!StoredItemInstances.Contains(ItemInstance) || !ItemInstance->CanBePickedUpBy(Requester))
	{
		return false;
	}

	const TWeakObjectPtr<AActor>* ExistingRequester = ItemReservations.Find(ItemInstance);
	if (ExistingRequester && ExistingRequester->IsValid() && ExistingRequester->Get() != Requester)
	{
		return false;
	}

	ItemReservations.FindOrAdd(ItemInstance) = Requester;
	return true;
}

void UWarehouseComponent::ReleaseItemReservation(AItemBase* ItemInstance, AActor* Requester)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemInstance)
	{
		return;
	}

	const TWeakObjectPtr<AActor>* ExistingRequester = ItemReservations.Find(ItemInstance);
	if (ExistingRequester && (!Requester || ExistingRequester->Get() == Requester))
	{
		ItemReservations.Remove(ItemInstance);
	}
}

bool UWarehouseComponent::IsItemReservedBy(const AItemBase* ItemInstance, const AActor* Requester) const
{
	if (!IsValid(ItemInstance) || !IsValid(Requester))
	{
		return false;
	}

	const TWeakObjectPtr<AActor>* ExistingRequester = ItemReservations.Find(ItemInstance);
	return ExistingRequester && ExistingRequester->IsValid() && ExistingRequester->Get() == Requester;
}

void UWarehouseComponent::CleanupItemReservations()
{
	for (auto Iterator = ItemReservations.CreateIterator(); Iterator; ++Iterator)
	{
		AItemBase* ItemInstance = Iterator.Key().Get();
		AActor* Requester = Iterator.Value().Get();
		if (!IsValid(ItemInstance) || !IsValid(Requester) || !StoredItemInstances.Contains(ItemInstance))
		{
			Iterator.RemoveCurrent();
		}
	}
}

bool UWarehouseComponent::SaveStoredItemsToGameInstance()
{
	// 실제 저장 책임은 GameInstanceSubsystem이 맡고, 컴포넌트는 현재 데이터만 넘겨줍니다.
	if (UWarehouseDataSubsystem* WarehouseSubsystem = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UWarehouseDataSubsystem>() : nullptr)
	{
		return WarehouseSubsystem->SaveFromWarehouseComponent(this);
	}

	return false;
}

bool UWarehouseComponent::LoadStoredItemsFromGameInstance()
{
	// 로비처럼 오브젝트를 다시 스폰하지 않는 맵에서는 요약 데이터만 복원
	if (UWarehouseDataSubsystem* WarehouseSubsystem = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UWarehouseDataSubsystem>() : nullptr)
	{
		return WarehouseSubsystem->LoadIntoWarehouseComponent(this);
	}

	return false;
}

bool UWarehouseComponent::BuildDeliveryData(const TArray<FStoredItemData>& RequestedItems, FDeliveryData& OutDeliveryData) const
{
	OutDeliveryData.SelectedItems.Reset();
	OutDeliveryData.SelectedItemInstances.Reset();

	TSet<int32> UsedInstanceIndices;

	for (const FStoredItemData& RequestedItem : RequestedItems)
	{
		if (!RequestedItem.ItemClass || RequestedItem.Quantity <= 0)
		{
			continue;
		}

	
		if (GetStoredQuantity(RequestedItem.ItemClass) < RequestedItem.Quantity)
		{
			OutDeliveryData.SelectedItems.Reset();
			return false;
		}

		OutDeliveryData.SelectedItems.Add(RequestedItem);

		int32 AddedInstanceCount = 0;
		for (int32 InstanceIndex = 0; InstanceIndex < StoredItemInstances.Num() && AddedInstanceCount < RequestedItem.Quantity; ++InstanceIndex)
		{
			const AItemBase* StoredItemInstance = StoredItemInstances[InstanceIndex];
			if (UsedInstanceIndices.Contains(InstanceIndex) || !StoredItemInstance || StoredItemInstance->GetClass() != RequestedItem.ItemClass)
			{
				continue;
			}

			FStoredItemInstanceData SavedItemInstance;
			StoredItemInstance->SaveItemToData(SavedItemInstance);
			if (!SavedItemInstance.IsValid())
			{
				continue;
			}

			OutDeliveryData.SelectedItemInstances.Add(SavedItemInstance);
			UsedInstanceIndices.Add(InstanceIndex);
			++AddedInstanceCount;
		}

		// 개별 액터가 없는 요약 데이터만 있는 경우에도 기존 수량 기반 테스트가 유지되도록 기본 데이터를 채웁니다.
		while (AddedInstanceCount < RequestedItem.Quantity)
		{
			FStoredItemInstanceData FallbackItemInstance;
			FallbackItemInstance.ItemClass = RequestedItem.ItemClass;
			OutDeliveryData.SelectedItemInstances.Add(FallbackItemInstance);
			++AddedInstanceCount;
		}
	}

	return !OutDeliveryData.IsEmpty();
}

int32 UWarehouseComponent::FindStoredItemIndex(TSubclassOf<AItemBase> ItemClass) const
{
	for (int32 Index = 0; Index < StoredItems.Num(); ++Index)
	{
		if (StoredItems[Index].ItemClass == ItemClass)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UWarehouseComponent::BroadcastWarehouseChanged()
{
	OnWarehouseItemsChanged.Broadcast();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->ForceNetUpdate();
	}
}
