#include "UI/CharacterCustomizationWidget.h"
#include "UI/ColorPickerWidget.h"
#include "Player/MainCharacter.h"
#include "Components/CharacterCustomizationComponent.h"
#include "Data/CustomizationTypes.h"
#include "GameFramework/PlayerController.h"

void UCharacterCustomizationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		CachedCharacter = Cast<AMainCharacter>(PC->GetPawn());
		if (CachedCharacter.IsValid())
		{
			CachedCustomizationComp = CachedCharacter->GetCustomizationComponent();
			if (CachedCustomizationComp.IsValid())
			{
				CurrentData = CachedCustomizationComp->GetCustomizationData();
				OriginalData = CurrentData;
				OnCustomizationDataInitialized(CurrentData);
			}
		}
	}
}

void UCharacterCustomizationWidget::SetBodyColor(FLinearColor InColor)
{
	CurrentData.BodyColor = InColor;
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetMetallic(float InMetallic)
{
	CurrentData.Metallic = FMath::Clamp(InMetallic, 0.0f, 1.0f);
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetRoughnessB(float InRoughnessB)
{
	CurrentData.RoughnessB = FMath::Clamp(InRoughnessB, 0.0f, 1.0f);
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetRoughnessA(float InRoughnessA)
{
	CurrentData.RoughnessA = FMath::Clamp(InRoughnessA, 0.0f, 1.0f);
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetDecalIndex(int32 InIndex)
{
	CurrentData.DecalIndex = InIndex;
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetDecalsColor(FLinearColor InColor)
{
	CurrentData.DecalsColor = InColor;
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetTilingX(float InTilingX)
{
	CurrentData.TilingX = InTilingX;
	UpdatePreview();
}

void UCharacterCustomizationWidget::SetTilingY(float InTilingY)
{
	CurrentData.TilingY = InTilingY;
	UpdatePreview();
}

void UCharacterCustomizationWidget::ConfirmAndSave()
{
	if (CachedCustomizationComp.IsValid())
	{
		CachedCustomizationComp->ConfirmAndApplyCustomization(CurrentData);
	}

	if (CachedCharacter.IsValid())
	{
		CachedCharacter->EndCustomization();
	}

	RemoveFromParent();
}

void UCharacterCustomizationWidget::CancelAndExit()
{
	if (CachedCustomizationComp.IsValid())
	{
		CachedCustomizationComp->ApplyPreview(OriginalData);
	}

	if (CachedCharacter.IsValid())
	{
		CachedCharacter->EndCustomization();
	}

	RemoveFromParent();
}

void UCharacterCustomizationWidget::ResetToDefault()
{
	if (CachedCustomizationComp.IsValid())
	{
		if (UCustomizationDataAsset* DataAsset = CachedCustomizationComp->GetCustomizationDataAsset())
		{
			if (DataAsset->DefaultPresets.Num() > 0)
			{
				CurrentData = DataAsset->DefaultPresets[0];
				UpdatePreview();
				OnCustomizationDataInitialized(CurrentData);
				return;
			}
		}
	}

	CurrentData = FCharacterCustomizationData();
	UpdatePreview();
	OnCustomizationDataInitialized(CurrentData);
}

void UCharacterCustomizationWidget::ApplyPreset(int32 PresetIndex)
{
	if (CachedCustomizationComp.IsValid())
	{
		if (UCustomizationDataAsset* DataAsset = CachedCustomizationComp->GetCustomizationDataAsset())
		{
			if (DataAsset->DefaultPresets.IsValidIndex(PresetIndex))
			{
				CurrentData = DataAsset->DefaultPresets[PresetIndex];
				UpdatePreview();
				OnCustomizationDataInitialized(CurrentData);
			}
		}
	}
}

void UCharacterCustomizationWidget::RotateCharacter(float DeltaX)
{
	if (CachedCharacter.IsValid())
	{
		CachedCharacter->AddCustomizationCharacterYaw(DeltaX * DragRotationSpeed);
	}
}

int32 UCharacterCustomizationWidget::GetAvailableDecalCount() const
{
	if (CachedCustomizationComp.IsValid())
	{
		if (UCustomizationDataAsset* DataAsset = CachedCustomizationComp->GetCustomizationDataAsset())
		{
			return DataAsset->AvailableDecals.Num();
		}
	}
	return 0;
}

UTexture2D* UCharacterCustomizationWidget::GetDecalTexture(int32 Index) const
{
	if (CachedCustomizationComp.IsValid())
	{
		if (UCustomizationDataAsset* DataAsset = CachedCustomizationComp->GetCustomizationDataAsset())
		{
			return DataAsset->GetDecalTexture(Index);
		}
	}
	return nullptr;
}

void UCharacterCustomizationWidget::UpdatePreview()
{
	if (CachedCustomizationComp.IsValid())
	{
		CachedCustomizationComp->ApplyPreview(CurrentData);
	}
}

UColorPickerWidget* UCharacterCustomizationWidget::OpenBodyColorPicker()
{
	if (!ColorPickerWidgetClass)
	{
		return nullptr;
	}

	UColorPickerWidget* Picker = CreateWidget<UColorPickerWidget>(GetOwningPlayer(), ColorPickerWidgetClass);
	if (Picker)
	{
		Picker->InitializeColor(CurrentData.BodyColor);
		Picker->OnColorChanged.AddDynamic(this, &UCharacterCustomizationWidget::SetBodyColor);
		Picker->OnColorCancelled.AddDynamic(this, &UCharacterCustomizationWidget::SetBodyColor);
		Picker->AddToViewport(100);
	}
	return Picker;
}

UColorPickerWidget* UCharacterCustomizationWidget::OpenDecalColorPicker()
{
	if (!ColorPickerWidgetClass)
	{
		return nullptr;
	}

	UColorPickerWidget* Picker = CreateWidget<UColorPickerWidget>(GetOwningPlayer(), ColorPickerWidgetClass);
	if (Picker)
	{
		Picker->InitializeColor(CurrentData.DecalsColor);
		Picker->OnColorChanged.AddDynamic(this, &UCharacterCustomizationWidget::SetDecalsColor);
		Picker->OnColorCancelled.AddDynamic(this, &UCharacterCustomizationWidget::SetDecalsColor);
		Picker->AddToViewport(100);
	}
	return Picker;
}

