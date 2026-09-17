#include "UI/ColorPickerWidget.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

void UColorSwatchButton::HandleClicked()
{
	if (OwnerPicker.IsValid())
	{
		OwnerPicker->SelectSwatchColor(SwatchIndex);
	}
}

UColorPickerWidget::UColorPickerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitDefaultSwatches();
}

void UColorPickerWidget::PopulateSwatchPalette(UPanelWidget* PaletteContainer)
{
	if (!PaletteContainer)
	{
		return;
	}

	PaletteContainer->ClearChildren();

	if (SwatchColors.Num() == 0)
	{
		InitDefaultSwatches();
	}

	for (int32 i = 0; i < SwatchColors.Num(); ++i)
	{
		const FLinearColor& SwatchColor = SwatchColors[i];

		UColorSwatchButton* SwatchBtn = NewObject<UColorSwatchButton>(PaletteContainer);
		if (!SwatchBtn)
		{
			continue;
		}

		SwatchBtn->SwatchIndex = i;
		SwatchBtn->OwnerPicker = this;

		// 버튼 스타일 세팅 (Normal, Hovered, Pressed 모두 해당 색상으로 세팅)
		FButtonStyle BtnStyle = SwatchBtn->GetStyle();
		FSlateColor SlateColor = FSlateColor(SwatchColor);
		BtnStyle.Normal.TintColor = SlateColor;
		BtnStyle.Hovered.TintColor = FSlateColor(SwatchColor * 1.2f);
		BtnStyle.Pressed.TintColor = FSlateColor(SwatchColor * 0.8f);
		SwatchBtn->SetStyle(BtnStyle);

		// 클릭 시 스와치 색상 선택 이벤트 바인딩
		SwatchBtn->OnClicked.AddDynamic(SwatchBtn, &UColorSwatchButton::HandleClicked);

		// 24x20 크기의 깔끔한 사각형 타일로 고정하기 위해 SizeBox 생성
		USizeBox* SizeBox = NewObject<USizeBox>(PaletteContainer);
		if (SizeBox)
		{
			SizeBox->SetWidthOverride(24.0f);
			SizeBox->SetHeightOverride(20.0f);
			SizeBox->AddChild(SwatchBtn);

			UPanelSlot* PanelSlot = PaletteContainer->AddChild(SizeBox);
			if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(PanelSlot))
			{
				WrapSlot->SetPadding(FMargin(2.0f));
			}
		}
		else
		{
			PaletteContainer->AddChild(SwatchBtn);
		}
	}
}

void UColorPickerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SwatchColors.Num() == 0)
	{
		InitDefaultSwatches();
	}
}

void UColorPickerWidget::InitializeColor(const FLinearColor& InColor)
{
	InitialColor = InColor;
	CurrentColor = InColor;
	UpdateHSVFromCurrentColor();
	OnColorUpdated(CurrentColor, CurrentHue, CurrentSaturation, CurrentValue, CurrentAlpha);
}

void UColorPickerWidget::SetColorFromWheelPosition(FVector2D LocalPosition, FVector2D WheelSize)
{
	const FVector2D Center = WheelSize * 0.5f;
	const FVector2D Offset = LocalPosition - Center;
	const float Radius = FMath::Min(WheelSize.X, WheelSize.Y) * 0.5f;

	if (Radius <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 중심으로부터의 거리를 기반으로 채도(Saturation: 0~1) 계산
	const float Dist = Offset.Size();
	CurrentSaturation = FMath::Clamp(Dist / Radius, 0.0f, 1.0f);

	// 마우스 각도를 기반으로 색상(Hue: 0~360도) 계산 (UI 휠의 시계방향 기준과 일치)
	float AngleRad = FMath::Atan2(Offset.Y, Offset.X);
	float AngleDeg = FMath::RadiansToDegrees(AngleRad);
	if (AngleDeg < 0.0f)
	{
		AngleDeg += 360.0f;
	}
	CurrentHue = AngleDeg;

	UpdateCurrentColorFromHSV();
}

FVector2D UColorPickerWidget::GetHandlePositionOnWheel(FVector2D WheelSize) const
{
	const FVector2D Center = WheelSize * 0.5f;
	const float Radius = FMath::Min(WheelSize.X, WheelSize.Y) * 0.5f;
	const float AngleRad = FMath::DegreesToRadians(CurrentHue);
	const float Dist = CurrentSaturation * Radius;

	return FVector2D(
		Center.X + Dist * FMath::Cos(AngleRad),
		Center.Y + Dist * FMath::Sin(AngleRad)
	);
}

void UColorPickerWidget::SetValue(float InValue)
{
	CurrentValue = FMath::Clamp(InValue, 0.0f, 1.0f);
	UpdateCurrentColorFromHSV();
}

void UColorPickerWidget::SetAlpha(float InAlpha)
{
	CurrentAlpha = FMath::Clamp(InAlpha, 0.0f, 1.0f);
	CurrentColor.A = CurrentAlpha;
	OnColorChanged.Broadcast(CurrentColor);
	OnColorUpdated(CurrentColor, CurrentHue, CurrentSaturation, CurrentValue, CurrentAlpha);
}

void UColorPickerWidget::SetRGB(float R, float G, float B)
{
	CurrentColor.R = FMath::Clamp(R, 0.0f, 1.0f);
	CurrentColor.G = FMath::Clamp(G, 0.0f, 1.0f);
	CurrentColor.B = FMath::Clamp(B, 0.0f, 1.0f);
	CurrentColor.A = CurrentAlpha;

	UpdateHSVFromCurrentColor();
	OnColorChanged.Broadcast(CurrentColor);
	OnColorUpdated(CurrentColor, CurrentHue, CurrentSaturation, CurrentValue, CurrentAlpha);
}

void UColorPickerWidget::SetFromHex(const FString& HexString)
{
	FString CleanHex = HexString.TrimStartAndEnd();
	if (CleanHex.StartsWith(TEXT("#")))
	{
		CleanHex.RightChopInline(1);
	}

	if (CleanHex.Len() == 6 || CleanHex.Len() == 8)
	{
		CurrentColor = FColor::FromHex(CleanHex);
		UpdateHSVFromCurrentColor();
		OnColorChanged.Broadcast(CurrentColor);
		OnColorUpdated(CurrentColor, CurrentHue, CurrentSaturation, CurrentValue, CurrentAlpha);
	}
}

FString UColorPickerWidget::GetHexCode() const
{
	return CurrentColor.ToFColor(true).ToHex();
}

void UColorPickerWidget::SelectSwatchColor(int32 SwatchIndex)
{
	if (SwatchColors.IsValidIndex(SwatchIndex))
	{
		CurrentColor = SwatchColors[SwatchIndex];
		CurrentColor.A = CurrentAlpha;
		UpdateHSVFromCurrentColor();
		OnColorChanged.Broadcast(CurrentColor);
		OnColorUpdated(CurrentColor, CurrentHue, CurrentSaturation, CurrentValue, CurrentAlpha);
	}
}

void UColorPickerWidget::ConfirmColor()
{
	OnColorConfirmed.Broadcast(CurrentColor);
	RemoveFromParent();
}

void UColorPickerWidget::CancelColor()
{
	CurrentColor = InitialColor;
	OnColorCancelled.Broadcast(InitialColor);
	RemoveFromParent();
}

void UColorPickerWidget::UpdateCurrentColorFromHSV()
{
	// 언리얼 표준 HSV -> LinearColor 변환 (R=Hue, G=Saturation, B=Value)
	FLinearColor ConvertedColor = FLinearColor(CurrentHue, CurrentSaturation, CurrentValue).HSVToLinearRGB();
	ConvertedColor.A = CurrentAlpha;
	CurrentColor = ConvertedColor;

	OnColorChanged.Broadcast(CurrentColor);
	OnColorUpdated(CurrentColor, CurrentHue, CurrentSaturation, CurrentValue, CurrentAlpha);
}

void UColorPickerWidget::UpdateHSVFromCurrentColor()
{
	FLinearColor HSV = CurrentColor.LinearRGBToHSV();
	CurrentHue = HSV.R;
	CurrentSaturation = HSV.G;
	CurrentValue = HSV.B;
	CurrentAlpha = CurrentColor.A;
}

void UColorPickerWidget::InitDefaultSwatches()
{
	SwatchColors.Empty();

	// 스크린샷 하단에 표시되는 42종 추천 컬러 팔레트 (행 단위 구성)
	// 행 1: 선명한 기본 원색 및 파스텔톤
	SwatchColors.Add(FLinearColor(1.0f, 0.2f, 0.0f));       // 다홍색
	SwatchColors.Add(FLinearColor(1.0f, 0.0f, 0.0f));       // 빨강
	SwatchColors.Add(FLinearColor::White);                  // 순백
	SwatchColors.Add(FLinearColor(0.85f, 0.85f, 0.9f));     // 연회색
	SwatchColors.Add(FLinearColor(0.0f, 1.0f, 0.3f));       // 네온그린
	SwatchColors.Add(FLinearColor(0.9f, 1.0f, 0.0f));       // 라임옐로우
	SwatchColors.Add(FLinearColor(0.7f, 0.75f, 1.0f));      // 라벤더
	SwatchColors.Add(FLinearColor(0.6f, 1.0f, 0.95f));      // 민트시안
	SwatchColors.Add(FLinearColor(0.75f, 0.0f, 0.65f));     // 마젠타
	SwatchColors.Add(FLinearColor(1.0f, 0.0f, 0.5f));       // 핫핑크
	SwatchColors.Add(FLinearColor(0.5f, 0.0f, 1.0f));       // 보라
	SwatchColors.Add(FLinearColor(0.0f, 0.4f, 1.0f));       // 로열블루
	SwatchColors.Add(FLinearColor(1.0f, 0.0f, 0.2f));       // 진홍색
	SwatchColors.Add(FLinearColor(0.5f, 0.5f, 0.0f));       // 올리브

	// 행 2: 옐로우, 오렌지, 피부톤, 딥톤
	SwatchColors.Add(FLinearColor(1.0f, 0.85f, 0.0f));      // 골드옐로우
	SwatchColors.Add(FLinearColor(1.0f, 0.75f, 0.1f));      // 황토색
	SwatchColors.Add(FLinearColor(1.0f, 0.6f, 0.4f));       // 살구색
	SwatchColors.Add(FLinearColor(1.0f, 0.3f, 0.65f));      // 로즈핑크
	SwatchColors.Add(FLinearColor::Black);                  // 칠흑
	SwatchColors.Add(FLinearColor(0.3f, 0.3f, 0.3f));       // 다크그레이
	SwatchColors.Add(FLinearColor(0.85f, 0.75f, 0.2f));     // 브론즈
	SwatchColors.Add(FLinearColor(0.95f, 0.95f, 0.95f));    // 오프화이트
	SwatchColors.Add(FLinearColor(0.85f, 0.15f, 0.1f));     // 벽돌색
	SwatchColors.Add(FLinearColor(1.0f, 0.8f, 0.9f));       // 베이비핑크
	SwatchColors.Add(FLinearColor(0.9f, 0.0f, 0.0f));       // 카민레드
	SwatchColors.Add(FLinearColor(0.45f, 0.45f, 0.5f));     // 쿨그레이
	SwatchColors.Add(FLinearColor(0.0f, 0.75f, 1.0f));      // 스카이블루
	SwatchColors.Add(FLinearColor(0.0f, 0.95f, 0.65f));     // 에메랄드
	SwatchColors.Add(FLinearColor(0.0f, 0.7f, 0.95f));      // 터키석색

	// 행 3: 딥 시안, 청록, 네이비, 스모크, 형광
	SwatchColors.Add(FLinearColor(0.0f, 0.35f, 0.45f));     // 딥틸
	SwatchColors.Add(FLinearColor(0.0f, 0.55f, 0.65f));     // 바다색
	SwatchColors.Add(FLinearColor(0.0f, 0.9f, 0.95f));      // 밝은시안
	SwatchColors.Add(FLinearColor(0.0f, 0.75f, 0.8f));      // 아쿠아
	SwatchColors.Add(FLinearColor(0.5f, 0.6f, 0.65f));      // 슬레이트그레이
	SwatchColors.Add(FLinearColor(0.3f, 0.35f, 0.4f));      // 차콜
	SwatchColors.Add(FLinearColor(0.2f, 0.25f, 0.3f));      // 스모크
	SwatchColors.Add(FLinearColor(0.05f, 0.05f, 0.08f));    // 오닉스
	SwatchColors.Add(FLinearColor(0.35f, 1.0f, 0.4f));      // 밝은연두
	SwatchColors.Add(FLinearColor(0.2f, 0.4f, 0.5f));       // 페트롤
	SwatchColors.Add(FLinearColor(0.2f, 0.85f, 0.5f));      // 세이지그린
	SwatchColors.Add(FLinearColor(0.0f, 1.0f, 0.0f));       // 비비드그린
	SwatchColors.Add(FLinearColor(0.5f, 0.95f, 1.0f));      // 아이스블루
	SwatchColors.Add(FLinearColor(0.35f, 0.15f, 0.45f));    // 플럼퍼플
}
