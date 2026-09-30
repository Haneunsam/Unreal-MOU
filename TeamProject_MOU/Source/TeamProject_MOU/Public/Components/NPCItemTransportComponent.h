#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPCItemTransportComponent.generated.h"

class AItemBase;
class UStatusComponent;
class UWarehouseComponent;

/**
 * 작업 NPC가 창고 아이템 하나를 예약하고 목적지까지 운반하도록 관리합니다.
 * 필요한 NPC 블루프린트에만 직접 추가해서 사용합니다.
 */
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_MOU_API UNPCItemTransportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPCItemTransportComponent();

	// [NPCWORK-000] 시작 시 상태 컴포넌트의 CC 변경 이벤트를 구독한다.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 아이템을 가져올 WarehouseComponent를 가진 액터입니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "NPC|Item Transport")
	TObjectPtr<AActor> SourceWarehouseActor;

	// NPC 스켈레탈 메시에서 아이템을 붙일 소켓입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Item Transport")
	FName CarrySocketName = TEXT("CarrySocket");

	// 창고에서 목적지까지 떨어져야 하는 최소 거리입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Item Transport", meta = (ClampMin = "0.0", Units = "cm"))
	float MinTransportDistance = 1000.0f;

	// 창고를 기준으로 목적지를 탐색할 최대 거리입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Item Transport", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxTransportDistance = 2000.0f;

	// 최소 거리 조건을 만족하는 NavMesh 위치를 찾기 위한 최대 시도 횟수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Item Transport", meta = (ClampMin = "1", ClampMax = "100"))
	int32 DestinationSearchAttempts = 12;

	// 아이템을 집을 수 있는 최대 거리입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Item Transport", meta = (ClampMin = "0.0", Units = "cm"))
	float PickupDistance = 200.0f;

	// 목적지에서 아이템을 내려놓을 수 있는 최대 거리입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Item Transport", meta = (ClampMin = "0.0", Units = "cm"))
	float DropDistance = 250.0f;

	// 원본 창고에서 가장 가까운 운반 가능 아이템을 예약합니다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Item Transport")
	AItemBase* TryReserveWorkItem();

	// 예약된 아이템을 소켓에 부착해 운반을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Item Transport")
	bool PickUpReservedItem();

	// 목적지 위치에 현재 아이템을 내려놓습니다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Item Transport")
	bool DropCarriedItemAtDestination();

	// 현재 예약을 취소합니다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Item Transport")
	void ReleaseReservation();

	UFUNCTION(BlueprintPure, Category = "NPC|Item Transport")
	AItemBase* GetReservedItem() const { return ReservedItem; }

	UFUNCTION(BlueprintPure, Category = "NPC|Item Transport")
	AItemBase* GetCarriedItem() const { return CarriedItem; }

	UFUNCTION(BlueprintPure, Category = "NPC|Item Transport")
	FVector GetDestinationLocation() const;

	UFUNCTION(BlueprintPure, Category = "NPC|Item Transport")
	bool HasDestination() const { return bHasDestination; }

	UFUNCTION(BlueprintPure, Category = "NPC|Item Transport")
	bool IsCarryingItem() const;

private:
	UWarehouseComponent* GetSourceWarehouseComponent() const;
	bool TryGenerateDestination(const FVector& Origin);
	void AttachCarriedItem();

	// [NPCWORK-001] 이동 불가 CC가 적용되면 운반물을 현재 위치에 놓고 NPC를 제거한다.
	UFUNCTION()
	void HandleCrowdControlChanged(bool bIsCrowdControlled);

	UPROPERTY(Transient)
	TObjectPtr<UStatusComponent> StatusComponent;

	UPROPERTY(Transient)
	TObjectPtr<AItemBase> ReservedItem;

	UPROPERTY(ReplicatedUsing = OnRep_CarriedItem)
	TObjectPtr<AItemBase> CarriedItem;

	UPROPERTY(Transient)
	FVector DestinationLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasDestination = false;

	bool bHandledCrowdControlDeath = false;

	UFUNCTION()
	void OnRep_CarriedItem();
};
