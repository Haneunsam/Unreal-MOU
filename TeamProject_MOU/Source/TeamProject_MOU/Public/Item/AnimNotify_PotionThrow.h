#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PotionThrow.generated.h"

/**
 * 포션 투척 몽타주에서 손을 놓는 정확한 프레임에 배치하는 전용 Notify.
 * 로컬 소유 캐릭터가 들고 있는 APotionItem에 투척 확정을 전달한다.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Potion Throw"))
class TEAMPROJECT_MOU_API UAnimNotify_PotionThrow : public UAnimNotify
{
	GENERATED_BODY()

public:
	// [POTION-010] Notify 트랙에 표시할 이름을 반환한다.
	virtual FString GetNotifyName_Implementation() const override;

	// [POTION-011] 로컬 소유 캐릭터가 든 포션에 원하는 프레임의 투척 신호를 전달한다.
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
