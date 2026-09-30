#include "LevelDesign/GridBlockPaletteDataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"

const FGridBlockEntry* UGridBlockPaletteDataAsset::FindBlockEntry(const FName& BlockName) const
{
	return Blocks.Find(BlockName);
}

void UGridBlockPaletteDataAsset::GetAvailableBlockNames(TArray<FName>& OutBlockNames) const
{
	OutBlockNames.Reset();
	Blocks.GetKeys(OutBlockNames);
}

UTexture2D* UGridBlockPaletteDataAsset::GetBlockIcon(const FName& BlockName) const
{
	if (const FGridBlockEntry* Entry = Blocks.Find(BlockName))
	{
		if (Entry->IconTexture)
		{
			return Entry->IconTexture;
		}

		if (Entry->StaticMesh)
		{
			UMaterialInterface* Mat = Entry->MaterialOverride ? Entry->MaterialOverride.Get() : Entry->StaticMesh->GetMaterial(0);
			if (Mat)
			{
				TArray<FMaterialParameterInfo> OutParameterInfo;
				TArray<FGuid> OutParameterIds;
				Mat->GetAllTextureParameterInfo(OutParameterInfo, OutParameterIds);
				for (const FMaterialParameterInfo& ParamInfo : OutParameterInfo)
				{
					UTexture* Tex = nullptr;
					if (Mat->GetTextureParameterValue(ParamInfo, Tex) && Tex)
					{
						if (UTexture2D* Tex2D = Cast<UTexture2D>(Tex))
						{
							if (!Tex2D->IsNormalMap())
							{
								return Tex2D;
							}
						}
					}
				}

#if WITH_EDITOR
				if (UMaterial* BaseMat = Mat->GetMaterial())
				{
					for (UMaterialExpression* Expr : BaseMat->GetExpressions())
					{
						if (UMaterialExpressionTextureBase* TexExpr = Cast<UMaterialExpressionTextureBase>(Expr))
						{
							if (UTexture2D* Tex2D = Cast<UTexture2D>(TexExpr->Texture))
							{
								if (!Tex2D->IsNormalMap())
								{
									return Tex2D;
								}
							}
						}
					}
				}
#endif

				TArray<UTexture*> OutTextures;
				Mat->GetUsedTextures(OutTextures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);
				for (UTexture* Tex : OutTextures)
				{
					if (UTexture2D* Tex2D = Cast<UTexture2D>(Tex))
					{
						if (!Tex2D->IsNormalMap())
						{
							return Tex2D;
						}
					}
				}
			}
		}
	}
	return nullptr;
}

TArray<FGridBlockUIEntry> UGridBlockPaletteDataAsset::GetPaletteUIEntries() const
{
	TArray<FGridBlockUIEntry> Result;
	for (const auto& Pair : Blocks)
	{
		FGridBlockUIEntry Entry;
		Entry.BlockName = Pair.Key;
		Entry.IconTexture = GetBlockIcon(Pair.Key);
		Result.Add(Entry);
	}
	return Result;
}
