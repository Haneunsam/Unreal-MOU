#pragma once

#include "CoreMinimal.h"
#include "Components/CharacterCustomizationComponent.h"
#include "LobbyCustomizationComponent.generated.h"

/** 로비 편집창 전용 메시의 외형을 적용한다. 확정값 전송은 로비 위젯이 담당한다. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_MOU_API ULobbyCustomizationComponent : public UCharacterCustomizationComponent
{
	GENERATED_BODY()

public:
	// [LCOMP-000] 편집용 컴포넌트의 네트워크 복제를 비활성화한다.
	ULobbyCustomizationComponent();

	// [LCOMP-001] 창 전용 메시의 머티리얼을 분리하고 시작 외형을 적용한다.
	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Customization")
	bool InitializeLobbyPreview(USkeletalMeshComponent* Mesh, UCustomizationDataAsset* DataAsset,
		const FCharacterCustomizationData& InitialData);

	// [LCOMP-002] 편집값을 창 전용 메시에만 적용하며 저장하거나 서버에 전송하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "MOU|Lobby|Customization")
	void ApplyLobbyPreview(const FCharacterCustomizationData& Data);

private:
	TWeakObjectPtr<USkeletalMeshComponent> LocalPreviewMesh;
};
