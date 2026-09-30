#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GridBlockPaletteDataAsset.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UTexture2D;

USTRUCT(BlueprintType)
struct FGridBlockEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Block")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Block")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Block")
	TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Block")
	bool bHasCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Block")
	bool bIsOpaque = true;
};

USTRUCT(BlueprintType)
struct FGridBlockUIEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grid Block")
	FName BlockName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Block")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
};

UCLASS(BlueprintType)
class TEAMPROJECT_MOU_API UGridBlockPaletteDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	TMap<FName, FGridBlockEntry> Blocks;

	const FGridBlockEntry* FindBlockEntry(const FName& BlockName) const;

	UFUNCTION(BlueprintPure, Category = "Grid Block Palette")
	void GetAvailableBlockNames(TArray<FName>& OutBlockNames) const;

	UFUNCTION(BlueprintPure, Category = "Grid Block Palette")
	UTexture2D* GetBlockIcon(const FName& BlockName) const;

	UFUNCTION(BlueprintPure, Category = "Grid Block Palette")
	TArray<FGridBlockUIEntry> GetPaletteUIEntries() const;
};
