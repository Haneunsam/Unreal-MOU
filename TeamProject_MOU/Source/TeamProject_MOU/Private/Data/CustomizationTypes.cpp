#include "Data/CustomizationTypes.h"
#include "Engine/Texture2D.h"

const FString UCustomizationSaveGame::SaveSlotName = TEXT("MOU_CustomizationSaveSlot");
const int32 UCustomizationSaveGame::SaveUserIndex = 0;

UCustomizationDataAsset::UCustomizationDataAsset()
{
	// 기본 데칼 텍스처 에셋 경로 프리세팅 (EmoRobot00 데칼 마스크 6종)
	static const TCHAR* DecalPaths[] = {
		TEXT("/Game/EmoRobot00/Textures/T_EmoRobot00_DecalsMask.T_EmoRobot00_DecalsMask"),
		TEXT("/Game/EmoRobot00/Textures/T_EmoRobot00_DecalsMask2.T_EmoRobot00_DecalsMask2"),
		TEXT("/Game/EmoRobot00/Textures/T_EmoRobot00_DecalsMask3.T_EmoRobot00_DecalsMask3"),
		TEXT("/Game/EmoRobot00/Textures/T_EmoRobot00_DecalsMask4.T_EmoRobot00_DecalsMask4"),
		TEXT("/Game/EmoRobot00/Textures/T_EmoRobot00_DecalsMask5.T_EmoRobot00_DecalsMask5"),
		TEXT("/Game/EmoRobot00/Textures/T_EmoRobot00_DecalsMask6.T_EmoRobot00_DecalsMask6")
	};

	for (const TCHAR* Path : DecalPaths)
	{
		AvailableDecals.Add(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)));
	}

	// 기본 디폴트 프리셋 등록 (스크린샷 수치 일치)
	FCharacterCustomizationData DefaultData;
	DefaultData.BodyColor = FLinearColor::White;
	DefaultData.Metallic = 0.38816f;
	DefaultData.RoughnessB = 0.2848f;
	DefaultData.RoughnessA = 0.3712f;
	DefaultData.DecalIndex = 2; // T_EmoRobot00_DecalsMask3 (인덱스 2)
	DefaultData.DecalsColor = FLinearColor(1.0f, 0.0f, 0.176412f, 1.0f);
	DefaultData.TilingX = 1.0f;
	DefaultData.TilingY = 1.0f;
	DefaultPresets.Add(DefaultData);
}

UTexture2D* UCustomizationDataAsset::GetDecalTexture(int32 Index) const
{
	if (AvailableDecals.IsValidIndex(Index))
	{
		if (AvailableDecals[Index].IsNull())
		{
			return nullptr;
		}

		if (UTexture2D* LoadedTexture = AvailableDecals[Index].Get())
		{
			return LoadedTexture;
		}

		return AvailableDecals[Index].LoadSynchronous();
	}

	return nullptr;
}
