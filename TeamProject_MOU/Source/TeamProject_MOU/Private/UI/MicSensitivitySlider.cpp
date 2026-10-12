#include "UI/MicSensitivitySlider.h"
#include "Voice/VoiceSubsystem.h"
#include "Voice/VoiceTypes.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

class SMicInputMeter : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMicInputMeter) {} SLATE_END_ARGS()
	// [MICBAR-002] 원본 입력과 임계값을 제공하는 슬라이더를 약한 참조로 보관한다.
	void Construct(const FArguments&, UMicSensitivitySlider* InOwner)
	{
		Owner = InOwner;
		SetCanTick(true);
	}
	// [MICBAR-003] 입력량 변화가 매 프레임 다시 그려지게 한다.
	virtual void Tick(const FGeometry&, double, float) override { Invalidate(EInvalidateWidgetReason::Paint); }
	// [MICBAR-004] 게이지의 최소 표시 크기를 지정한다.
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 24.f); }
	// [MICBAR-005] 검은 바탕에 차단 영역은 노랑, 통과 영역은 초록으로 실제 입력만 채운다.
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle&, bool) const override
	{
		const UMicSensitivitySlider* Slider = Owner.Get();
		if (!Slider) return Layer;
		const UVoiceSubsystem* Voice = UVoiceSubsystem::Get(Slider);
		const float Level = MOUVoice::MicRmsToSlider(Voice ? Voice->GetCurrentLoudness() : 0.f);
		const float Threshold = Slider->GetValue();
		const FVector2D Size = Geometry.GetLocalSize();
		const float Left = 8.f, Width = FMath::Max(0.f, static_cast<float>(Size.X) - 16.f);
		auto Bar = [&](float Start, float End, FLinearColor Color, float Height)
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2D(Width * FMath::Max(End - Start, 0.f), Height),
				FSlateLayoutTransform(FVector2D(Left + Width * Start, (Size.Y - Height) * .5f))),
				FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
		};
		Bar(0.f, 1.f, FLinearColor(.025f, .025f, .03f), 10.f);
		Bar(0.f, Threshold, FLinearColor(.25f, .16f, .02f), 4.f);
		Bar(Threshold, 1.f, FLinearColor(.02f, .20f, .08f), 4.f);
		++Layer;
		Bar(0.f, FMath::Min(Level, Threshold), FLinearColor(1.f, .65f, .04f), 10.f);
		if (Level > Threshold) Bar(Threshold, Level, FLinearColor(.08f, .85f, .30f), 10.f);
		return Layer;
	}
private:
	TWeakObjectPtr<UMicSensitivitySlider> Owner;
};

// [MICBAR-001] 입력량 배경 위에 기존 슬라이더의 드래그 손잡이를 겹친다.
TSharedRef<SWidget> UMicSensitivitySlider::RebuildWidget()
{
	FSliderStyle Style = GetWidgetStyle();
	Style.NormalBarImage.TintColor = FSlateColor(FLinearColor::Transparent);
	Style.HoveredBarImage.TintColor = FSlateColor(FLinearColor::Transparent);
	Style.DisabledBarImage.TintColor = FSlateColor(FLinearColor::Transparent);
	Style.NormalThumbImage = FSlateRoundedBoxBrush(FLinearColor::White, 6.f, FVector2D(12.f, 12.f));
	Style.HoveredThumbImage = Style.NormalThumbImage;
	Style.DisabledThumbImage = Style.NormalThumbImage;
	SetWidgetStyle(Style);
	SetSliderHandleColor(FLinearColor::White);
	SetIndentHandle(true);
	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SMicInputMeter, this).Visibility(EVisibility::HitTestInvisible)]
		+ SOverlay::Slot()[Super::RebuildWidget()];
}
