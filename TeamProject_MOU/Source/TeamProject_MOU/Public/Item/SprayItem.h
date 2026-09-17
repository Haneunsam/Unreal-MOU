#pragma once

#include "CoreMinimal.h"
#include "Item/ConsumableItemBase.h"
#include "SprayItem.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class TEAMPROJECT_MOU_API ASprayItem : public AConsumableItemBase
{
	GENERATED_BODY()
public:
	// [SPRAY-000] 기본 분사 주기와 용량 설정.
	ASprayItem();
	// [SPRAY-001] 서버에서 표면 검사 및 지속 소비.
	virtual void Tick(float DeltaSeconds) override;
	// [SPRAY-002] 좌클릭으로 분사 시작 요청.
	virtual void OnUse_Implementation() override;
	// [SPRAY-003] 입력 해제 시 서버에 분사 중지를 요청한다.
	virtual void OnUseReleased_Implementation() override;
	// [SPRAY-004] 장착 시 서버 RPC 소유권 복원.
	virtual void OnEquipped_Implementation(AActor* Equipper) override;
	// [SPRAY-005] 수납 시 분사 중지.
	virtual void OnUnequipped_Implementation(AActor* Equipper) override;
	// [SPRAY-006] 내려놓기 전에 분사 중지.
	virtual void Drop_Implementation(FVector Location, AActor* Dropper = nullptr) override;
	// [SPRAY-007] 던지기 전에 분사 중지.
	virtual void Throw_Implementation(FVector Velocity, AActor* Thrower = nullptr) override;
	// [SPRAY-008] 분사 상태 복제 등록.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// [SPRAY-015] 소진 및 레벨 종료 시 분사 이펙트 정리.
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// [SPRAY-018] 노즐을 포함한 공통 바운딩박스 중심 보정을 사용하지 않는다.
	virtual bool ShouldCenterOnCarrySocket() const override;
	// [SPRAY-019] 몸통의 잡는 지점을 스케일과 회전을 반영하여 손 소켓에 맞춘다.
	virtual FVector GetCarryLocationOffset() const override;
	// [SPRAY-020] 손 소켓 기준 스프레이 방향을 반환한다.
	virtual FRotator GetCarryRotationOffset() const override;

protected:
	// 스케일 적용 전 몸통 메시 중심에서 잡는 지점까지의 로컬 이동량.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Grip", meta=(Units="cm"))
	FVector GripPointOffset = FVector::ZeroVector;
	// 손 소켓 좌표 기준의 추가 위치 보정. 아이템 스케일을 곱하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Grip", meta=(Units="cm"))
	FVector HandLocationOffset = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Grip")
	FRotator HandRotationOffset = FRotator::ZeroRotator;

	// [SPRAY-016] BP에서 배치한 노즐의 기본 위치를 저장한다.
	virtual void BeginPlay() override;
	// [SPRAY-017] 분사 상태에 따라 노즐을 누르거나 기본 위치로 복귀시킨다.
	void UpdateNozzle(float DeltaSeconds);

	// 몸통 MeshComponent에 부착되는 분사 노즐. BP에서 메시와 기본 위치를 지정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spray|Nozzle")
	TObjectPtr<UStaticMeshComponent> NozzleMesh;
	// 몸통 로컬 좌표 기준의 눌림 이동량. 스프레이를 회전해도 몸통 기준으로 움직인다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Nozzle", meta=(Units="cm"))
	FVector NozzlePressedOffset = FVector(0.f, 0.f, -0.3f);
	// 완전히 누르거나 복귀하는 데 걸리는 시간. 0이면 즉시 이동한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Nozzle", meta=(ClampMin="0", Units="s"))
	float NozzleTravelSeconds = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray")
	TObjectPtr<UMaterialInterface> DecalMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray", meta=(ClampMin="1", Units="cm"))
	float SprayRange = 300.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray", meta=(ClampMin="1", Units="cm"))
	float DecalRadius = 12.f;
	UPROPERTY(EditDefaultsOnly, Category="Spray", meta=(ClampMin="0.1", Units="cm"))
	float ProjectionDepth = 2.f;
	UPROPERTY(EditDefaultsOnly, Category="Spray", meta=(ClampMin="0.1"))
	float SecondsPerUse = 1.f;
	UPROPERTY(EditDefaultsOnly, Category="Spray", meta=(ClampMin="1"))
	float DecalLifeSeconds = 120.f;
	UPROPERTY(EditDefaultsOnly, Category="Spray", meta=(ClampMin="1"))
	int32 MaxDecals = 256;
	UPROPERTY(ReplicatedUsing=OnRep_Spraying, BlueprintReadOnly, Category="Spray")
	bool bSpraying = false;
	// [SPRAY-009] 분사 이펙트 시작/중지 BP 연결점.
	UFUNCTION(BlueprintImplementableEvent, Category="Spray")
	void OnSprayStateChanged(bool bActive);
	// [SPRAY-010] 복제된 분사 상태를 이펙트에 전달.
	UFUNCTION()
	void OnRep_Spraying();
	// [SPRAY-011] 서버에서 분사 시작/중지 요청 검증.
	UFUNCTION(Server, Reliable)
	void ServerSetSpraying(bool bActive);
	// [SPRAY-012] 서버 분사 상태 전환.
	void SetSpraying(bool bActive);
	// [SPRAY-013] 표면 검사 후 반지름 밖에서만 새 데칼 생성.
	void SpraySurface();
	// [SPRAY-014] 각 클라이언트에서 피격 컴포넌트에 데칼 부착.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStamp(UPrimitiveComponent* Surface, FVector Location, FVector Normal, FName Bone);

private:
	FVector NozzleRestLocation = FVector::ZeroVector;
	float NozzlePressAlpha = 0.f;
	float ConsumeElapsed = 0.f;
	int32 NextSortOrder = 0;
	TWeakObjectPtr<UPrimitiveComponent> LastSurface;
	FVector LastLocalPoint = FVector::ZeroVector;
	FVector LastLocalNormal = FVector::UpVector;
	FName LastBone;
	TArray<TWeakObjectPtr<UDecalComponent>> Decals;
};
