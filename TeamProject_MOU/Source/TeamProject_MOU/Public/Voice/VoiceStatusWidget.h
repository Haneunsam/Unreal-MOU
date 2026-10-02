// MOU 음성 - 마이크 이미지와 발화 음량을 표시하는 위젯.
// WBP에 MicGauge(ProgressBar) 하나를 배치하고 텍스처 두 개를 지정한다.
// 마이크 이미지가 배경으로 깔리고, 발화 음량만큼 같은 이미지가 아래에서 위로 초록색으로 차오른다.
// WBP가 없으면 대체 텍스트나 레이아웃을 생성하지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Voice/VoiceTypes.h"
#include "VoiceStatusWidget.generated.h"

class UProgressBar;
class UTexture2D;
class UVoiceSubsystem;

UCLASS()
class TEAMPROJECT_MOU_API UVoiceStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// [VUI-003] 마이크 표시 위젯이 입력 포커스를 가져가지 않도록 설정한다.
	UVoiceStatusWidget(const FObjectInitializer& ObjectInitializer);

	// [VUI-013] 디자이너 미리보기에서도 게이지를 마이크 이미지 모양으로 표시한다.
	virtual void NativePreConstruct() override;

	// [VUI-004] 음소거 입력을 연결하고 아이콘과 음량 바를 초기화한다.
	virtual void NativeConstruct() override;

	// [VUI-005] 위젯이 등록한 음소거 키 바인딩을 제거한다.
	virtual void NativeDestruct() override;

	// [VUI-006] 매 프레임 마이크 상태를 확인하고 음량 바를 갱신한다.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 별도 입력 액션에서 음소거를 처리한다면 false로 설정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Voice")
	bool bBindMuteKeyToOwningPlayer = true;

	/** 플레이어 컨트롤러의 키 설정에서도 사용하는 음소거 토글 키. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Voice", meta = (EditCondition = "bBindMuteKeyToOwningPlayer"))
	FKey MuteToggleKey = EKeys::C;

	/** 기본 및 발화 상태에서 공통으로 사용할 마이크 이미지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MOU|Voice|UI")
	TObjectPtr<UTexture2D> NormalMicTexture;

	/** 음소거 상태에서 사용할 이미지. 원본 PNG의 색상을 유지한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MOU|Voice|UI")
	TObjectPtr<UTexture2D> MutedMicTexture;

	/** 게이지 상한의 감도 배수. 발화 기준에서 0%, 기본값 3배에서 100%가 된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MOU|Voice|UI")
	float LevelBarHeadroom = 3.f;

	/** 마이크 이미지 안에서 차오르는 게이지 색상. 텍스처 색상에 곱해지므로 흰색 아이콘에서 그대로 보인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MOU|Voice|UI")
	FLinearColor LevelFillColor = FLinearColor(0.3f, 1.f, 0.3f);

	// [VUI-010] 블루프린트에서 현재 표시 중인 마이크 상태를 조회한다.
	UFUNCTION(BlueprintPure, Category = "MOU|Voice|UI")
	EMicIconState GetMicState() const { return CachedState; }

	// [VUI-011] 상태가 바뀔 때 블루프린트의 연출 이벤트를 호출한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "MOU|Voice|UI")
	void OnMicStateChanged(EMicIconState NewState, EMicIconState OldState);

protected:
	/**
	 * WBP의 같은 이름을 가진 ProgressBar와 연결된다.
	 * 스타일(배경/채움 이미지, 채움 방향)은 코드가 덮어쓰므로 WBP에서는 크기만 정하면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Voice")
	TObjectPtr<UProgressBar> MicGauge;

private:
	// [VUI-014] 배경과 채움 이미지를 같은 마이크 텍스처로 설정해 아이콘 모양 게이지를 만든다.
	void ApplyGaugeStyle(UTexture2D* Texture, const FLinearColor& IconTint);

	// [VUI-008] 소유 로컬 플레이어의 음성 서브시스템을 조회한다.
	UVoiceSubsystem* GetVoiceSubsystem() const;

	// [VUI-009] 장치와 플레이어의 음성 상태를 아이콘 상태 하나로 판정한다.
	EMicIconState EvaluateMicState() const;

	// [VUI-001] 마이크 상태에 따라 텍스처와 색상을 적용하고 상태 변경을 알린다.
	void ApplyMicState(EMicIconState NewState);

	// [VUI-002] 발화 기준을 넘은 음량을 게이지로 표시하고 비발화 상태에서는 0으로 초기화한다.
	void UpdateLevelBar(float InDeltaTime);

	// [VUI-007] 음소거를 전환하고 아이콘을 즉시 갱신한다.
	void HandleMuteKeyPressed();

	/** 마지막 적용 상태. 상태가 달라질 때만 이미지를 갱신한다. */
	EMicIconState CachedState = EMicIconState::NoDevice;

	/** 첫 갱신은 캐시와 상태가 같아도 적용한다. */
	bool bMicStateApplied = false;

	/** 보간된 화면 표시 음량(0~1). */
	float DisplayLevel = 0.f;
};
