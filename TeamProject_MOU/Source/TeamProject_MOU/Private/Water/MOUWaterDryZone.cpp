#include "Water/MOUWaterDryZone.h"

// [DRYZONE-001] 대상 목록이 비어 있으면 물을 제외하지 않는 명시적 지정 모드를 사용한다.
AMOUWaterDryZone::AMOUWaterDryZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExclusionMode = EWaterExclusionMode::AddWaterBodiesListToExclusion;
}
