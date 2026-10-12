#pragma once

#include "CoreMinimal.h"
#include "Components/Slider.h"
#include "MicSensitivitySlider.generated.h"

/** 차단 전 입력량과 사용자가 정한 발화 경계를 같은 눈금으로 표시한다. */
UCLASS()
class TEAMPROJECT_MOU_API UMicSensitivitySlider : public USlider
{
	GENERATED_BODY()
protected:
	// [MICBAR-001] 입력량 배경 위에 기존 슬라이더의 드래그 손잡이를 겹친다.
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
