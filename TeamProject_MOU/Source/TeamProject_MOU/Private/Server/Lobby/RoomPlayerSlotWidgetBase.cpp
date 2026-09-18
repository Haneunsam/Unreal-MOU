#include "Server/Lobby/RoomPlayerSlotWidgetBase.h"
#include "Components/CharacterCustomizationComponent.h"
#include "GameFramework/Actor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void URoomPlayerSlotWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetMinDesiredWidth(200.f);
		Size->SetMinDesiredHeight(140.f);
		WidgetTree->RootWidget = Size;
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(FLinearColor(0.08f, 0.14f, 0.22f, 1.f));
		Background->SetPadding(FMargin(12.f));
		Size->AddChild(Background);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		Background->AddChild(Layers);
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
		Empty->SetText(FText::FromString(TEXT("참가자 대기 중")));
		EmptyPanel = Empty;
		Layers->AddChild(Empty);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		OccupiedPanel = Content;
		Layers->AddChild(Content);
		NicknameText = WidgetTree->ConstructWidget<UTextBlock>();
		HostImage = WidgetTree->ConstructWidget<UImage>();
		ReadyImage = WidgetTree->ConstructWidget<UImage>();
		UTextBlock* Self = WidgetTree->ConstructWidget<UTextBlock>();
		Self->SetText(FText::FromString(TEXT("나")));
		SelfHighlight = Self;
		Content->AddChild(NicknameText);
		Content->AddChild(HostImage);
		Content->AddChild(ReadyImage);
		Content->AddChild(Self);
	}
	RefreshVisuals();
}

void URoomPlayerSlotWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	// Restore current member state after Blueprint PreConstruct/Construct and on reattachment.
	RefreshVisuals();
}

void URoomPlayerSlotWidgetBase::SetMember(const FMOURoomMember& InMember, bool bInIsSelf)
{
	if (bOccupied && Member.UserId == InMember.UserId && Member.Name == InMember.Name &&
		Member.bReady == InMember.bReady && Member.bIsHost == InMember.bIsHost &&
		Member.SlotIndex == InMember.SlotIndex && Member.Customization == InMember.Customization && bIsSelf == bInIsSelf)
	{
		return;
	}
	Member = InMember;
	bOccupied = true;
	bIsSelf = bInIsSelf;
	RefreshVisuals();
	OnSlotChanged();
}

void URoomPlayerSlotWidgetBase::ClearMember()
{
	if (!bOccupied) { return; }
	Member = FMOURoomMember();
	bOccupied = false;
	bIsSelf = false;
	RefreshVisuals();
	OnSlotChanged();
}

void URoomPlayerSlotWidgetBase::SetPreviewComponent(UCharacterCustomizationComponent* Component)
{
	PreviewComponent = Component;
	RefreshVisuals();
}

void URoomPlayerSlotWidgetBase::RefreshVisuals()
{
	if (PreviewComponent.IsValid())
	{
		if (AActor* Actor = PreviewComponent->GetOwner()) Actor->SetActorHiddenInGame(!bOccupied);
		if (bOccupied) PreviewComponent->ApplyPreview(Member.Customization);
	}

	if (EmptyPanel) { EmptyPanel->SetVisibility(bOccupied ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
	if (OccupiedPanel) { OccupiedPanel->SetVisibility(bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (NicknameText) { NicknameText->SetText(FText::FromString(Member.Name)); }
	if (HostImage)
	{
		HostImage->SetVisibility(bOccupied && Member.bIsHost
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (SelfHighlight) { SelfHighlight->SetVisibility(bOccupied && bIsSelf ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (ReadyImage)
	{
		ReadyImage->SetVisibility(bOccupied && !Member.bIsHost && Member.bReady
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
}
