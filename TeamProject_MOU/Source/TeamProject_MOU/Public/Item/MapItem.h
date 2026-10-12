#pragma once

#include "CoreMinimal.h"
#include "Base/ItemBase.h"
#include "MapItem.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UUserWidget;
class UWidgetComponent;

UCLASS()
class TEAMPROJECT_MOU_API AMapItem : public AItemBase
{
	GENERATED_BODY()

public:
	AMapItem();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// [MAP-017] 홀로그램이 열린 동안 관람자와의 거리 및 홀로그램 방향을 갱신합니다.
	virtual void Tick(float DeltaSeconds) override;

	// [MAP-013] HoldingPlayer 복제 등록 (클라 좌클릭 사용에 필요)
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

public:
#pragma region [CAP] Top-down 캡처 카메라
	// 지도에 부착된 하향 캡처 카메라 (소지 플레이어 위를 비춤)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Capture")
	TObjectPtr<USceneCaptureComponent2D> CaptureCamera;

	// 캡처 카메라 높이 (소지 플레이어 위 cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Capture")
	float CaptureHeight = 3000.0f;

	// 직교 투영 폭 (한 번에 담는 가로 범위 cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Capture")
	float CaptureOrthoWidth = 4096.0f;

	// [MAP-009] RT 해상도 (인스턴스마다 런타임 생성 시 사용, 정사각 픽셀)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Capture")
	int32 RenderTargetSize = 1024;

	// 실시간 캡처 결과 렌더타깃 (BeginPlay에서 인스턴스별 생성 → 공유 애셋 아님).
	//   위젯(WBP_Map)은 이 필드를 Get해서 이미지 소스로 써야 한다(공유 애셋 직접참조 금지).
	UPROPERTY(BlueprintReadOnly, Category = "Map|Capture")
	TObjectPtr<UTextureRenderTarget2D> CaptureRT;
#pragma endregion

#pragma region [MAP] 기존 WBP_Map 호환용 전체 공개 마스크
	// 기존 WBP_Map의 텍스처 입력을 깨뜨리지 않기 위한 흰색 호환 마스크입니다.
	UPROPERTY(BlueprintReadOnly, Category = "Map|Fog")
	TObjectPtr<UTextureRenderTarget2D> FogMaskRT;

	// 탐험 구역을 구분하지 않으므로 항상 전체 공개 상태로 유지합니다.
	UPROPERTY(BlueprintReadOnly, Category = "Map|Fog")
	TObjectPtr<UTextureRenderTarget2D> FogRevealRT;

	// 맵 월드 원점(좌하단 XY) — 좌표→UV 변환 기준
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Fog")
	FVector2D MapWorldOrigin = FVector2D::ZeroVector;

	// 맵 전체 크기 (cm) — 좌표→UV 변환 기준
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Fog")
	FVector2D MapWorldSize = FVector2D(20000.0f, 20000.0f);
#pragma endregion
 
#pragma region [MAP] 소지/갱신/토글
	// 화면 UI가 아닌 월드 공간에서 WBP_Map을 표시하는 충돌 없는 홀로그램 컴포넌트입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Hologram")
	TObjectPtr<UWidgetComponent> HologramWidgetComponent;

	// 플레이어 시점 기준 홀로그램 높이 오프셋입니다. BP에서 cm 단위로 조절합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Hologram", meta = (Units = "cm"))
	float HologramHeightOffset = 120.0f;

	// WBP_Map을 그릴 월드 위젯 해상도입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Hologram")
	FVector2D HologramDrawSize = FVector2D(1024.0f, 1024.0f);

	// 위젯의 실제 월드 크기를 조절합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Hologram")
	FVector HologramRelativeScale = FVector(0.1f);

	// 관람자가 지도에서 이 거리보다 멀어지면 홀로그램을 자동으로 닫습니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|Hologram", meta = (ClampMin = "0.0", Units = "cm"))
	float HologramAutoCloseDistance = 400.0f;

	// 이 지도를 현재 소지 중인 플레이어 (PickUp/OnEquipped 기준으로 세팅).
	//   서버에서 세팅되므로 클라의 좌클릭 사용(OnUse)에서도 쓰려면 복제 필수. [MAP-013]
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Map|State")
	TObjectPtr<AActor> HoldingPlayer;

	// 지도 오버레이 위젯 클래스 (에디터 지정, WBP)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map|UI")
	TSubclassOf<UUserWidget> MapWidgetClass;

	// [MAP-001] 좌클릭: 전체 지도 오버레이 켜기/끄기 토글
	virtual void OnUse_Implementation() override;

	// [MAP-016] F키 상호작용 가능 여부를 확인합니다. 지도는 줍지 않고 홀로그램을 여닫습니다.
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	// [MAP-018] F키 상호작용 시 해당 플레이어 로컬 화면에서 월드 홀로그램을 토글합니다.
	virtual void Interact_Implementation(AActor* Interactor) override;

	// [MAP-019] 현재 홀로그램 상태에 맞는 F키 안내 문구를 반환합니다.
	virtual FText GetInteractPrompt_Implementation() const override;

	// [MAP-022] 지도 장치는 고정형 오브젝트이므로 플레이어·NPC·창고의 모든 집기 경로를 차단합니다.
	virtual bool CanBePickedUpBy(AActor* PotentialPicker) const override;

	// [MAP-002] 레거시 장착 경로에서 소지자 상태와 메시 위치를 갱신
	virtual void OnEquipped_Implementation(AActor* Equipper) override;

	// [MAP-003] 레거시 장착 해제 경로에서 소지자 상태와 열린 지도를 정리
	virtual void OnUnequipped_Implementation(AActor* Equipper) override;

	// [MAP-011] 직접 PickUp 호출이 들어와도 고정형 지도 장치가 집히지 않도록 차단합니다.
	virtual void PickUp_Implementation(AActor* Picker) override;

	// [MAP-012] 놓기/던지기: 소지자 해제 (HoldingPlayer null)
	virtual void Drop_Implementation(FVector DropLocation, AActor* Dropper) override;
	virtual void Throw_Implementation(FVector ThrowVelocity, AActor* Thrower) override;

	// [MAP-004] 월드 좌표 → 지도 UV(0~1). 위젯 마커용
	UFUNCTION(BlueprintCallable, Category = "Map")
	FVector2D WorldToMapUV(const FVector& WorldLocation) const;

	// [MAP-008] 레벨 이동/재시작 시 호환 마스크 초기화 및 지형 재캡처.
	//   일반 레벨 이동(액터 재생성)이면 BeginPlay가 자동 초기화하므로 불필요.
	//   Seamless Travel 등 아이템이 유지되는 경우 BP에서 이 함수를 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Map")
	void ResetMapForNewLevel();
#pragma endregion

private:
	// [MAP-020] WBP_Map을 월드 위젯에 연결하고 관람자 앞에 홀로그램을 엽니다.
	void OpenHologram(AActor* Interactor);

	// [MAP-021] 월드 홀로그램을 닫고 거리 감시를 중지합니다.
	void CloseHologram();

	// [MAP-023] Img_Map 표시 머티리얼을 C++에서 한 번 생성하고 캡처 RT와 흰색 호환 마스크를 연결합니다.
	void RefreshHologramContent();

	// [MAP-014] 부착 시 스케일·위치가 튀지 않도록, 시작 시점의 메시 스케일을 기억했다가 복원.
	//   SnapToTargetNotIncludingScale 부착이 RelativeScale을 재계산하고,
	//   CarryingComponent 위치보정이 캡처카메라까지 박스에 넣어 지도를 밀어내는 것을 되돌린다.
	FVector InitialMeshScale = FVector(1.0f);
	void RestoreMeshScale();

	// 손(캐릭터)에 붙어 있는지 (바닥에 놓인 지도 위치는 건드리지 않기 위함)
	bool IsAttachedToPlayer() const;

	// [MAP-007] 캡처 RT와 기존 WBP_Map 호환용 흰색 마스크 RT를 런타임 생성
	void CreatePerInstanceRenderTargets();

	// [MAP-005] 레벨의 MapBounds 태그 볼륨을 찾아 Origin/Size 자동 설정
	void ResolveMapBoundsFromVolume();

	// [MAP-006] 레벨 로딩 시 지형을 1회만 캡처 (이후 고정)
	void CaptureMapOnce();

	// [CAP-001] 캡처 카메라를 소지 플레이어 위로 이동/정렬
	void UpdateCaptureTransform();

	// [MAP-015] 열려 있는 지도 오버레이 위젯을 로컬에서 닫는다 (없으면 무시).
	//   던지기/버리기/수납 등 손에서 사라질 때 UI가 화면에 박제되는 것 방지.
	void CloseMapWidget();

	// [MAP-015] 위젯은 소지 클라이언트 로컬에만 있는데 Drop/Throw는 서버에서만 실행되므로,
	//   전 클라에 닫기를 전파해 원격 소지 클라의 위젯까지 확실히 닫는다.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastCloseMap();

	// 지도 오버레이 위젯 인스턴스 (토글 상태 추적)
	UPROPERTY()
	TObjectPtr<UUserWidget> MapWidgetInstance;

	// 홀로그램 전용 지도 표시 MID입니다. FogBrush가 아니라 캡처 RT의 RGB/알파를 올바르게 출력하는 용도입니다.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HologramMapMID;

	// 지형 1회 캡처 완료 여부 (중복 캡처 방지)
	bool bMapCaptured = false;

	// 지형 캡처 지연 타이머 (레벨 로드 대기용)
	FTimerHandle CaptureDelayTimerHandle;

	// 오버레이 열림 상태
	bool bMapOpen = false;

	// 현재 이 클라이언트에서 홀로그램을 보고 있는 플레이어입니다.
	TWeakObjectPtr<AActor> HologramViewer;

	// 각 클라이언트가 자기 상호작용 결과를 독립적으로 관리하는 로컬 홀로그램 상태입니다.
	bool bHologramOpen = false;
};
