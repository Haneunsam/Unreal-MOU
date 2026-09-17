#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "CustomizationTypes.generated.h"

class UTexture2D;

/**
 * 캐릭터 외형 머티리얼 커스터마이징 파라미터 구조체 (네트워크 복제 지원)
 */
USTRUCT(BlueprintType)
struct TEAMPROJECT_MOU_API FCharacterCustomizationData
{
	GENERATED_BODY()

	// ---------------------------------------------------------
	// [Base 파라미터 그룹]
	// ---------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Base")
	FLinearColor BodyColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Base", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Metallic = 0.38816f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Base", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoughnessB = 0.2848f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Base", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoughnessA = 0.3712f;

	// ---------------------------------------------------------
	// [Decals 파라미터 그룹]
	// ---------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Decals")
	int32 DecalIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Decals")
	FLinearColor DecalsColor = FLinearColor(1.0f, 0.0f, 0.176412f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Decals")
	float TilingX = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|Decals")
	float TilingY = 1.0f;

	FCharacterCustomizationData() = default;

	bool operator==(const FCharacterCustomizationData& Other) const
	{
		return BodyColor == Other.BodyColor &&
			FMath::IsNearlyEqual(Metallic, Other.Metallic) &&
			FMath::IsNearlyEqual(RoughnessB, Other.RoughnessB) &&
			FMath::IsNearlyEqual(RoughnessA, Other.RoughnessA) &&
			DecalIndex == Other.DecalIndex &&
			DecalsColor == Other.DecalsColor &&
			FMath::IsNearlyEqual(TilingX, Other.TilingX) &&
			FMath::IsNearlyEqual(TilingY, Other.TilingY);
	}

	bool operator!=(const FCharacterCustomizationData& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * 커스터마이징 데칼 텍스처 및 파라미터명 설정 데이터 에셋
 */
UCLASS(BlueprintType)
class TEAMPROJECT_MOU_API UCustomizationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCustomizationDataAsset();

	// 선택 가능한 데칼 텍스처 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Decals")
	TArray<TSoftObjectPtr<UTexture2D>> AvailableDecals;

	// 기본 추천 프리셋 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Presets")
	TArray<FCharacterCustomizationData> DefaultPresets;

	// 머티리얼 파라미터 이름 매핑
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName BodyColorParamName = FName("Body Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName MetallicParamName = FName("Metallic");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName RoughnessBParamName = FName("RoughnessB");

	// 머티리얼 원본 오타(RougndessA) 대응용 파라미터 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName RoughnessAParamName = FName("RougndessA");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName RoughnessAFallbackParamName = FName("RoughnessA");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName DecalsParamName = FName("Decals");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName DecalsColorParamName = FName("Decals Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName TilingXParamName = FName("Tiling X");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|ParamNames")
	FName TilingYParamName = FName("Tiling Y");

	// 인덱스에 해당하는 데칼 텍스처 동기 로드 및 반환
	UFUNCTION(BlueprintCallable, Category = "Customization")
	UTexture2D* GetDecalTexture(int32 Index) const;
};

/**
 * 로컬 디스크 저장용 SaveGame 클래스
 */
UCLASS()
class TEAMPROJECT_MOU_API UCustomizationSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame")
	FCharacterCustomizationData SavedCustomization;

	static const FString SaveSlotName;
	static const int32 SaveUserIndex;
};
