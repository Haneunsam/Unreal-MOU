#include "Item/AnimNotify_PotionThrow.h"

#include "Base/ItemBase.h"
#include "Components/CarryingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Item/PotionItem.h"

// [POTION-010] 몽타주 Notify 트랙에서 식별하기 쉬운 이름을 표시한다.
FString UAnimNotify_PotionThrow::GetNotifyName_Implementation() const
{
	return TEXT("Potion Throw");
}

// [POTION-011] 로컬 소유 캐릭터에서만 서버 투척 확정을 요청해 멀티캐스트 중복 실행을 막는다.
void UAnimNotify_PotionThrow::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	APawn* OwnerPawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	UCarryingComponent* Carrying = OwnerPawn->FindComponentByClass<UCarryingComponent>();
	APotionItem* Potion = Carrying ? Cast<APotionItem>(Carrying->GetCarriedActor()) : nullptr;
	if (Potion)
	{
		Potion->HandleThrowAnimNotify(OwnerPawn);
	}
}
