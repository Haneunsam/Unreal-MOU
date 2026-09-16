#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ColorPickerWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorPicked, FLinearColor, SelectedColor);

class UColorPickerWidget;

/**
 * 프리셋 팔레트 스와치 타일 전용 버튼
 */
UCLASS()
class TEAMPROJECT_MOU_API UColorSwatchButton : public UButton
{
	GENERATED_BODY()

public:
	int32 SwatchIndex = 0;
	TWeakObjectPtr<UColorPickerWidget> OwnerPicker;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 언리얼 엔진 공식 색 선택 툴(Color Picker) 규격의 UMG 런타임 베이스 위젯
 * 원형 컬러 휠, 밝기 슬라이더, RGB/HSV 상호 변환 및 40종 프리셋 팔레트를 지원합니다.
 */
UCLASS()
class TEAMPROJECT_MOU_API UColorPickerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UColorPickerWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	// 팔레트 컨테이너(WrapBox 등)에 42종 프리셋 색상 버튼들을 자동으로 생성하여 배치
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void PopulateSwatchPalette(class UPanelWidget* PaletteContainer);

	// ---------------------------------------------------------
	// [델리게이트]
	// ---------------------------------------------------------
	// 슬라이더 또는 컬러 휠 조작 시 실시간 브로드캐스트 (머티리얼 실시간 프리뷰용)
	UPROPERTY(BlueprintAssignable, Category = "ColorPicker")
	FOnColorPicked OnColorChanged;

	// 확인 버튼 클릭 시 브로드캐스트
	UPROPERTY(BlueprintAssignable, Category = "ColorPicker")
	FOnColorPicked OnColorConfirmed;

	// 취소 버튼 클릭 시 브로드캐스트
	UPROPERTY(BlueprintAssignable, Category = "ColorPicker")
	FOnColorPicked OnColorCancelled;

	// ---------------------------------------------------------
	// [초기화 및 색상 설정]
	// ---------------------------------------------------------
	// 초기 색상 주입 및 HSV 분해
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void InitializeColor(const FLinearColor& InColor);

	// 원형 컬러 휠 클릭/드래그 시 로컬 마우스 좌표로 색상(Hue, Saturation) 산출
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void SetColorFromWheelPosition(FVector2D LocalPosition, FVector2D WheelSize);

	// 현재 Hue, Saturation에 해당하는 컬러 휠 상의 핸들(작은 원) 로컬 좌표 반환
	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	FVector2D GetHandlePositionOnWheel(FVector2D WheelSize) const;

	// 명도(밝기 / Value) 설정 (0.0 ~ 1.0)
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void SetValue(float InValue);

	// 알파(투명도) 설정 (0.0 ~ 1.0)
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void SetAlpha(float InAlpha);

	// R, G, B 수치 직접 설정
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void SetRGB(float R, float G, float B);

	// 16진수 Hex 코드(예: "#FFFFFF" or "FFFFFF")로 색상 설정
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void SetFromHex(const FString& HexString);

	// 프리셋 스와치 색상 선택
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void SelectSwatchColor(int32 SwatchIndex);

	// ---------------------------------------------------------
	// [확인 및 취소 조작]
	// ---------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void ConfirmColor();

	UFUNCTION(BlueprintCallable, Category = "ColorPicker")
	void CancelColor();

	// ---------------------------------------------------------
	// [Getter 함수들]
	// ---------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	FLinearColor GetCurrentColor() const { return CurrentColor; }

	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	FLinearColor GetInitialColor() const { return InitialColor; }

	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	FString GetHexCode() const;

	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	float GetHue() const { return CurrentHue; }

	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	float GetSaturation() const { return CurrentSaturation; }

	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	float GetValue() const { return CurrentValue; }

	UFUNCTION(BlueprintPure, Category = "ColorPicker")
	float GetAlpha() const { return CurrentAlpha; }

protected:
	// 현재 실시간 선택 중인 색상
	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	FLinearColor CurrentColor = FLinearColor::White;

	// 창 진입 시점의 원래 색상 (비교 프리뷰 상단 및 취소 시 복구용)
	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	FLinearColor InitialColor = FLinearColor::White;

	// HSV 속성값
	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	float CurrentHue = 0.0f; // 0.0 ~ 360.0

	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	float CurrentSaturation = 0.0f; // 0.0 ~ 1.0

	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	float CurrentValue = 1.0f; // 0.0 ~ 1.0

	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	float CurrentAlpha = 1.0f; // 0.0 ~ 1.0

	// 스크린샷 하단에 표시되는 40여 종의 추천 프리셋 스와치 색상 목록
	UPROPERTY(BlueprintReadOnly, Category = "ColorPicker")
	TArray<FLinearColor> SwatchColors;

	// UI 위젯의 슬라이더/핸들 초기 위치 동기화를 위한 Blueprint 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "ColorPicker")
	void OnColorUpdated(const FLinearColor& NewColor, float Hue, float Saturation, float Value, float Alpha);

private:
	void UpdateCurrentColorFromHSV();
	void UpdateHSVFromCurrentColor();
	void InitDefaultSwatches();
};
