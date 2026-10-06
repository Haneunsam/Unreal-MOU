#pragma once

#include "CoreMinimal.h"
#include "WaterBodyExclusionVolume.h"
#include "MOUWaterDryZone.generated.h"

/** 동굴 등 지정한 공간을 대상 호수/강의 수중 판정에서 제외하는 볼륨. */
UCLASS(Blueprintable)
class TEAMPROJECT_MOU_API AMOUWaterDryZone : public AWaterBodyExclusionVolume
{
	GENERATED_BODY()

public:
	// [DRYZONE-001] Water Bodies 목록에 지정한 물만 제외하도록 초기화한다.
	AMOUWaterDryZone(const FObjectInitializer& ObjectInitializer);
};
