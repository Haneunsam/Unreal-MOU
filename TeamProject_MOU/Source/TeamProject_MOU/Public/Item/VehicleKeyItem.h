#pragma once

#include "CoreMinimal.h"
#include "Base/ItemBase.h"
#include "VehicleKeyItem.generated.h"

class AVehicleBase;

/**
 * 영구 차량 키 아이템입니다.
 * 사용하면 지정 차량을 처음 한 번 생성하고, 이후 사용에서는 기존 차량을 사용자 앞으로 호출합니다.
 * AItemBase 계열이므로 DT_Item, 터미널 상점, 창고 저장 흐름을 그대로 사용할 수 있습니다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_MOU_API AVehicleKeyItem : public AItemBase
{
	GENERATED_BODY()

public:
	// [VEHKEY-000] 영구 차량 키의 기본값을 설정한다.
	AVehicleKeyItem();

	// [VEHKEY-001] 키 사용 요청을 서버로 전달하고 차량을 생성하거나 호출한다.
	virtual void OnUse_Implementation() override;

	// [VEHKEY-002] 키를 처음 집었을 때 서버 RPC를 보낼 수 있도록 소유자를 지정한다.
	virtual void PickUp_Implementation(AActor* Picker) override;

	// [VEHKEY-003] 인벤토리에서 키를 장착할 때 소유자를 갱신한다.
	virtual void OnEquipped_Implementation(AActor* Equipper) override;

	// [VEHKEY-004] 키를 내려놓을 때 네트워크 소유권을 해제한다.
	virtual void Drop_Implementation(FVector DropLocation, AActor* Dropper = nullptr) override;

	// [VEHKEY-005] 키를 던질 때 네트워크 소유권을 해제한다.
	virtual void Throw_Implementation(FVector ThrowVelocity, AActor* Thrower = nullptr) override;

protected:
	// 이 키가 생성하거나 호출할 차량 BP 클래스입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle Key")
	TSubclassOf<AVehicleBase> VehicleClass;

	// 사용자 앞에서 차량을 생성하거나 호출할 거리입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle Key", meta = (ClampMin = "100.0", Units = "cm"))
	float SpawnDistance = 700.0f;

	// 차량이 지면과 겹치지 않도록 더할 높이입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle Key", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnHeight = 150.0f;

	// 이 키로 생성한 차량입니다. 유효하면 새로 만들지 않고 기존 차량을 호출합니다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vehicle Key")
	TObjectPtr<AVehicleBase> SpawnedVehicle;

private:
	// [VEHKEY-006] 클라이언트의 키 사용 요청을 서버에서 실행한다.
	UFUNCTION(Server, Reliable)
	void ServerUseVehicleKey();

	// [VEHKEY-007] 서버에서 차량을 처음 생성하거나 기존 차량을 사용자 앞으로 이동한다.
	void SpawnOrRecallVehicle();

	// [VEHKEY-008] 키를 현재 들고 있거나 소유한 사용자를 찾는다.
	AActor* ResolveKeyUser() const;
};
