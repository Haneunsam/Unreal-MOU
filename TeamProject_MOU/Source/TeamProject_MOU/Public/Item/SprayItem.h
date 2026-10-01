#pragma once

#include "CoreMinimal.h"
#include "Item/ConsumableItemBase.h"
#include "SprayItem.generated.h"

class UDecalComponent;
class UAnimMontage;
class UAnimSequenceBase;
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
	// [SPRAY-021] 바닥에서 처음 집었을 때 모든 클라이언트에서 스프레이 Idle 애니메이션을 시작한다.
	virtual void PickUp_Implementation(AActor* Picker) override;
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

	// 들고 있지만 분사하지 않을 때 반복할 오른팔 애니메이션 시퀀스.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Animation")
	TObjectPtr<UAnimSequenceBase> SprayIdleAnimation;
	// 분사 버튼을 누르고 있는 동안 반복할 오른팔 애니메이션 시퀀스.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Animation")
	TObjectPtr<UAnimSequenceBase> SprayPressAnimation;
	// 캐릭터 AnimBP에서 스프레이 애니메이션을 받을 슬롯 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Animation")
	FName SprayAnimationSlot = TEXT("DefaultSlot");
	// Idle/Press 전환과 장착 해제 시 사용할 블렌드 시간.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Animation", meta=(ClampMin="0", Units="s"))
	float SprayAnimationBlendTime = 0.12f;
	// 입력 해제 후 Idle로 돌아가기 전에 Press 자세를 유지하는 시간. 0.5초 반복 클릭에서도 몽타주 재시작 떨림을 방지한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spray|Animation", meta=(ClampMin="0", Units="s"))
	float SprayAnimationReleaseDelay = 0.65f;

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
	// [SPRAY-022] 모든 클라이언트에서 장착 캐릭터의 Idle 또는 Press 시퀀스를 반복 재생한다.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySprayAnimation(AActor* Holder, bool bPressed);
	// [SPRAY-023] 현재 클라이언트의 장착 캐릭터에 스프레이 동적 몽타주를 적용한다.
	void PlaySprayAnimation(AActor* Holder, bool bPressed);
	// [SPRAY-024] 모든 클라이언트에서 스프레이가 시작한 동적 몽타주만 정지한다.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopSprayAnimation(AActor* Holder);
	// [SPRAY-025] 현재 클라이언트에서 활성 스프레이 동적 몽타주를 정지한다.
	void StopSprayAnimation(AActor* Holder);
	// [SPRAY-026] Press는 즉시 적용하고 Idle 복귀는 지연·취소 가능하게 처리한다.
	void HandleSprayAnimationState(AActor* Holder, bool bPressed);
	// [SPRAY-027] 입력 해제 지연이 끝났을 때 여전히 해제 상태면 Idle 애니메이션으로 복귀한다.
	void PlayIdleAfterRelease();

private:
	TWeakObjectPtr<UAnimMontage> ActiveSprayMontage;
	TWeakObjectPtr<AActor> SprayAnimationHolder;
	FTimerHandle SprayIdleReturnTimer;
	bool bHasActiveSprayAnimation = false;
	bool bShowingPressAnimation = false;
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
