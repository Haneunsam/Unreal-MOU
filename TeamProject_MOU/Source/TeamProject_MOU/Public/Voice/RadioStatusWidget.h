// 무전기 UI: RadioIcon(본체), StatusIcon(평시/송신/수신), BatteryBar(배터리).
// 상태는 월드 타이머로 갱신하므로 숨겨진 위젯도 다시 표시할 수 있다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "Voice/VoiceTypes.h"
#include "RadioStatusWidget.generated.h"

class APawn;
class ARadio;
class UImage;
class UProgressBar;
class UTextBlock;
class URadioComponent;
class UVoiceSubsystem;

/**
 * 화면 한쪽에 떠 있는 무전기 상태 표시 + Z/X 조작.
 *
 * Radio.h 의 규칙표(손에 듦 / 인벤토리 / 드롭)를 그대로 화면에 옮긴 것이다.
 * 본체, 상태 이미지, 배터리 잔량으로 현재 상태를 표시한다.
 */
UCLASS()
class TEAMPROJECT_MOU_API URadioStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// [RUI-010] 포커스를 가져가지 않는 무전기 위젯을 초기화한다.
	URadioStatusWidget(const FObjectInitializer& ObjectInitializer);

	// --- UUserWidget --------------------------------------------------------
	// [RUI-002] 입력을 연결하고 상태 갱신 타이머를 시작한다.
	virtual void NativeConstruct() override;
	// [RUI-003] 타이머와 입력을 해제하고 송신을 종료한다.
	virtual void NativeDestruct() override;

	// --- 설정 ---------------------------------------------------------------

	/**
	 * 소유 플레이어의 InputComponent 에 Z/X 를 직접 바인딩할지.
	 *
	 * ★ 기본 true 인 이유: 지금 프로젝트 어디에도 Z/X 바인딩이 없어서, 이걸
	 *   끄면 **무전기를 조작할 방법이 콘솔밖에 없다.** 게임 쪽에서 EnhancedInput
	 *   액션을 만들면 그때 끄고, 그 액션에서 ARadio::TogglePower() /
	 *   StartTransmit() / StopTransmit() 을 직접 부르면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Radio")
	bool bBindKeysToOwningPlayer = true;

	/** 전원 토글 키. 설계상 Z(Radio.h 상단 ★). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Radio", meta = (EditCondition = "bBindKeysToOwningPlayer"))
	FKey PowerToggleKey = EKeys::Z;

	/**
	 * PTT 키. 설계상 X **홀드**다.
	 *
	 * 누름과 뗌이 둘 다 필요해서 좌클릭(OnUse)으로는 안 되는 것이기도 하다
	 * (Radio.h 상단 ★ 2번).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Radio", meta = (EditCondition = "bBindKeysToOwningPlayer"))
	FKey TransmitKey = EKeys::X;

	/** 상태와 배터리 갱신 주기(초). 숨겨져도 월드 타이머는 동작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Radio")
	float RefreshInterval = 0.1f;

	// --- WBP 아이콘 설정 ------------------------------------------------------

	/** StatusIcon 이미지: On=평시, Transmitting=송신, Receiving=수신. OFF는 평시 이미지를 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MOU|Radio|UI")
	TMap<ERadioIconState, FSlateBrush> IconBrushes;

	/** StatusIcon의 켜진 상태별 틴트. 비워두면 흰색, OFF는 회색으로 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MOU|Radio|UI")
	TMap<ERadioIconState, FLinearColor> IconTints;

	/**
	 * 무전기가 없을 때 위젯을 통째로 숨길지.
	 *
	 * ★ 마이크와 다르다. 무전기를 안 가진 것은 **정상 상태**라서 화면을 차지할
	 *   이유가 없다(마이크 없음은 설정이 잘못됐다는 경고라 항상 떠야 한다).
	 *
	 *   테스트 중에는 꺼 두는 것이 낫다 - 숨겨버리면 "무전기를 못 찾은 것" 과
	 *   "위젯이 아예 안 뜬 것" 을 구분할 수 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MOU|Radio|UI")
	bool bHideWhenNoRadio = true;

	// --- 조회 / 블루프린트 훅 -------------------------------------------------

	/** 지금 아이콘이 나타내는 상태. WBP 애니메이션 분기에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "MOU|Radio|UI")
	ERadioIconState GetRadioState() const { return CachedState; }

	/** 배터리가 곧 바닥나는가. 깜빡임 같은 연출은 WBP 에서 이 값으로 건다. */
	UFUNCTION(BlueprintPure, Category = "MOU|Radio|UI")
	bool IsBatteryLow() const { return bBatteryLow; }

	/**
	 * 상태가 **바뀐 순간에만** 불린다.
	 *
	 * ★ 매 틱이 아니라 변경 시에만인 것이 중요하다. 매 틱 애니메이션을 다시
	 *   재생시키면 첫 프레임에서 멈춘 것처럼 보인다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MOU|Radio|UI")
	void OnRadioStateChanged(ERadioIconState NewState, ERadioIconState OldState);

protected:
	/** WBP에 배치한 무전기 본체 이미지. 텍스처는 유지하고 전원에 따라 색만 바꾼다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Radio")
	TObjectPtr<UImage> RadioIcon;

	/** WBP에 배치한 실제 배터리 잔량 바. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Radio")
	TObjectPtr<UProgressBar> BatteryBar;

	/** WBP에 배치한 배터리 잔량 퍼센트 텍스트. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Radio")
	TObjectPtr<UTextBlock> BatteryPercentText;

	/** WBP에 배치한 평시/송신/수신 상태 이미지. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MOU|Radio")
	TObjectPtr<UImage> StatusIcon;

private:
	// [RUI-001] 숨겨진 상태에서도 무전기 상태와 배터리를 갱신한다.
	void RefreshRadioStatus();

	/** 전원, 장착, 송수신 상태를 하나의 아이콘 상태로 축약한다. */
	// [RUI-005] 전원과 장착 여부를 확인하여 송수신 상태를 판정한다.
	ERadioIconState EvaluateRadioState() const;

	/** 축약된 상태를 브러시/색/가시성으로 옮긴다. */
	// [RUI-004] 본체 전원 색과 상태 이미지, 위젯 가시성을 적용한다.
	void ApplyRadioState(ERadioIconState NewState);

	// [RUI-011] UI 브러시의 전용 머티리얼 인스턴스로 전원 OFF 시 채도를 제거한다.
	void ApplyPowerToBrush(FSlateBrush& Brush, bool bOff);

	/** 배터리 바를 갱신한다. 보간하지 않는다 - 배터리는 튀는 값이 아니다. */
	// [RUI-009] 실제 배터리 잔량과 퍼센트 텍스트를 갱신한다.
	void UpdateBatteryBar();

	/**
	 * 지금 로컬 플레이어가 가지고 있는 무전기의 컴포넌트. 없으면 null.
	 *
	 * ARadio 든 AVoiceDebugRadio 든 상관없이 찾는다(헤더 상단 ★).
	 * 부착 관계를 재귀로 훑으므로 손에 든 것과 인벤토리에 있는 것을 모두 잡는다
	 * (ARadio::FindCarriedBy 와 같은 근거).
	 */
	URadioComponent* FindLocalRadioComponent() const;

	/** 위에서 찾은 것이 진짜 아이템이면 그것을. 테스트 무전기면 null. */
	ARadio* FindLocalRadioItem() const;

	APawn* GetLocalPawn() const;
	UVoiceSubsystem* GetVoiceSubsystem() const;

	// --- 입력 콜백 ----------------------------------------------------------

	/**
	 * Z. 전원을 토글한다.
	 *
	 * ★ 진짜 아이템(ARadio)이면 TogglePower() 가 알아서 서버로 넘긴다.
	 *   테스트 무전기는 그 경로가 없어서 UVoiceComponent 의 디버그 RPC 로 돌린다.
	 */
	// [RUI-006] 손에 든 무전기에만 전원 변경을 요청한다.
	void HandlePowerKeyPressed();

	/** X 누름. */
	// [RUI-007] 송신 시작을 요청하고 상태 이미지를 갱신한다.
	void HandleTransmitKeyPressed();

	/**
	 * X 뗌.
	 *
	 * ★ 무전기를 못 찾아도 **무조건 송신을 끊는다.** 누른 사이에 무전기를
	 *   떨어뜨렸거나 뺏겼을 때 송신이 켜진 채로 남으면, 본인은 모르는데 계속
	 *   무전이 나간다 - 이 게임에서 그건 위치가 새는 것이다.
	 */
	// [RUI-008] 송신을 종료하고 상태 이미지를 갱신한다.
	void HandleTransmitKeyReleased();

	// 숨김 여부와 무관하게 상태를 확인하는 타이머.
	FTimerHandle RefreshTimerHandle;

	/** 마지막으로 적용한 상태. 바뀔 때만 브러시를 갈아끼우려고 들고 있다. */
	ERadioIconState CachedState = ERadioIconState::None;

	/** 아직 한 번도 적용한 적이 없다. 첫 갱신은 CachedState 와 같아도 반영해야 한다. */
	bool bRadioStateApplied = false;

	/** 배터리 경고 구간인가. WBP 가 IsBatteryLow 로 읽어간다. */
	bool bBatteryLow = false;
};
