#include "Item/VehicleKeyItem.h"

#include "Item/VehicleBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

// [VEHKEY-000] 영구 차량 키의 기본값을 설정한다.
AVehicleKeyItem::AVehicleKeyItem()
{
	MaxUseCount = 1;
}

// [VEHKEY-001] 키 사용 요청을 서버로 전달하고 차량을 생성하거나 호출한다.
void AVehicleKeyItem::OnUse_Implementation()
{
	// AItemBase::OnUse는 호출하지 않는다. 사용 횟수를 차감하지 않아 키가 영구히 유지된다.
	if (HasAuthority())
	{
		SpawnOrRecallVehicle();
	}
	else
	{
		ServerUseVehicleKey();
	}
}

// [VEHKEY-002] 키를 처음 집었을 때 서버 RPC를 보낼 수 있도록 소유자를 지정한다.
void AVehicleKeyItem::PickUp_Implementation(AActor* Picker)
{
	Super::PickUp_Implementation(Picker);

	if (HasAuthority() && IsValid(Picker))
	{
		SetOwner(Picker);
	}
}

// [VEHKEY-003] 인벤토리에서 키를 장착할 때 소유자를 갱신한다.
void AVehicleKeyItem::OnEquipped_Implementation(AActor* Equipper)
{
	Super::OnEquipped_Implementation(Equipper);

	if (HasAuthority() && IsValid(Equipper))
	{
		SetOwner(Equipper);
		LastOwner = Equipper;
	}
}

// [VEHKEY-004] 키를 내려놓을 때 네트워크 소유권을 해제한다.
void AVehicleKeyItem::Drop_Implementation(FVector DropLocation, AActor* Dropper)
{
	if (HasAuthority())
	{
		SetOwner(nullptr);
	}

	Super::Drop_Implementation(DropLocation, Dropper);
}

// [VEHKEY-005] 키를 던질 때 네트워크 소유권을 해제한다.
void AVehicleKeyItem::Throw_Implementation(FVector ThrowVelocity, AActor* Thrower)
{
	if (HasAuthority())
	{
		SetOwner(nullptr);
	}

	Super::Throw_Implementation(ThrowVelocity, Thrower);
}

// [VEHKEY-006] 클라이언트의 키 사용 요청을 서버에서 실행한다.
void AVehicleKeyItem::ServerUseVehicleKey_Implementation()
{
	SpawnOrRecallVehicle();
}

// [VEHKEY-007] 서버에서 차량을 처음 생성하거나 기존 차량을 사용자 앞으로 이동한다.
void AVehicleKeyItem::SpawnOrRecallVehicle()
{
	if (!HasAuthority() || !VehicleClass)
	{
		return;
	}

	AActor* User = ResolveKeyUser();
	if (!IsValid(User))
	{
		return;
	}

	const FVector SpawnLocation = User->GetActorLocation()
		+ User->GetActorForwardVector() * SpawnDistance
		+ FVector::UpVector * SpawnHeight;
	const FRotator SpawnRotation(0.0f, User->GetActorRotation().Yaw, 0.0f);

	if (IsValid(SpawnedVehicle))
	{
		SpawnedVehicle->SetActorLocationAndRotation(
			SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = User;
	SpawnParameters.Instigator = Cast<APawn>(User);
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	SpawnedVehicle = GetWorld()->SpawnActor<AVehicleBase>(
		VehicleClass, SpawnLocation, SpawnRotation, SpawnParameters);
}

// [VEHKEY-008] 키를 현재 들고 있거나 소유한 사용자를 찾는다.
AActor* AVehicleKeyItem::ResolveKeyUser() const
{
	if (IsValid(GetOwner()))
	{
		return GetOwner();
	}

	if (IsValid(GetAttachParentActor()))
	{
		return GetAttachParentActor();
	}

	return IsValid(LastOwner) ? LastOwner.Get() : nullptr;
}
