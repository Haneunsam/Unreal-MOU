#include "Server/Lobby/LobbyCustomizationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

// [LCOMP-000] 편집용 컴포넌트의 네트워크 복제를 비활성화한다.
ULobbyCustomizationComponent::ULobbyCustomizationComponent()
{
	SetIsReplicatedByDefault(false);
}

// [LCOMP-001] 창 전용 메시의 머티리얼을 분리하고 시작 외형을 적용한다.
bool ULobbyCustomizationComponent::InitializeLobbyPreview(USkeletalMeshComponent* Mesh,
	UCustomizationDataAsset* DataAsset, const FCharacterCustomizationData& InitialData)
{
	if (!IsValid(Mesh) || !GetOwner() || Mesh->GetOwner() != GetOwner() || GetOwner()->GetIsReplicated())
		return false;

	CustomizationDataAsset = DataAsset ? DataAsset : NewObject<UCustomizationDataAsset>(this);
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* Material = Mesh->GetMaterial(Index);
		UMaterialInstanceDynamic* SourceDMI = Cast<UMaterialInstanceDynamic>(Material);
		if (SourceDMI) Material = SourceDMI->Parent;
		if (!Material) continue;

		// 슬롯의 DMI를 재사용하면 편집값이 대기실에도 보이므로 새 인스턴스를 만든다.
		UMaterialInstanceDynamic* LocalDMI = UMaterialInstanceDynamic::Create(Material, this);
		if (SourceDMI) LocalDMI->CopyInterpParameters(SourceDMI);
		Mesh->SetMaterial(Index, LocalDMI);
	}

	LocalPreviewMesh = Mesh;
	BodyMaterialInstances.Reset();
	CustomizationData = InitialData;
	SetPreviewMesh(Mesh);
	ApplyPreview(InitialData);
	return true;
}

// [LCOMP-002] 편집값을 창 전용 메시에만 적용하며 저장하거나 서버에 전송하지 않는다.
void ULobbyCustomizationComponent::ApplyLobbyPreview(const FCharacterCustomizationData& Data)
{
	if (!LocalPreviewMesh.IsValid()) return;
	// 이 비복제 컴포넌트에는 편집값만 보관해 재초기화해도 이전 색으로 돌아가지 않는다.
	CustomizationData = Data;
	ApplyPreview(Data);
}

