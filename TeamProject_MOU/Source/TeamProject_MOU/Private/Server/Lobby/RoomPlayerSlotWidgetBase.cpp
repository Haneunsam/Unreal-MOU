#include "Server/Lobby/RoomPlayerSlotWidgetBase.h"
#include "Components/CharacterCustomizationComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
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

// [LSLOT-002] BP 생성 처리가 끝난 뒤 슬롯 외형과 전용 촬영 텍스처를 다시 연결한다.
void URoomPlayerSlotWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	// Restore current member state after Blueprint PreConstruct/Construct and on reattachment.
	RefreshVisuals();
	RefreshPortraitTarget();
}

// [LSLOT-003] 멤버별 외형을 적용하고 BP 갱신 후 슬롯 전용 텍스처를 연결한다.
void URoomPlayerSlotWidgetBase::SetMember(const FMOURoomMember& InMember, bool bInIsSelf)
{
	if (bOccupied && Member.UserId == InMember.UserId && Member.Name == InMember.Name &&
		Member.bReady == InMember.bReady && Member.bIsHost == InMember.bIsHost &&
		Member.SlotIndex == InMember.SlotIndex && Member.Customization == InMember.Customization && bIsSelf == bInIsSelf)
	{
		RefreshPortraitTarget();
		return;
	}
	const bool bSlotChanged = Member.SlotIndex != InMember.SlotIndex;
	Member = InMember;
	bOccupied = true;
	bIsSelf = bInIsSelf;
	if (bSlotChanged || !PreviewComponent.IsValid()) FindPreviewActorForSlot(Member.SlotIndex);
	RefreshVisuals();
	OnSlotChanged();
	RefreshPortraitTarget();
}

// [LSLOT-004] 빈 좌석으로 전환한 뒤에도 다른 PIE 창의 텍스처를 사용하지 않게 한다.
void URoomPlayerSlotWidgetBase::ClearMember()
{
	if (!bOccupied) { return; }
	Member = FMOURoomMember();
	bOccupied = false;
	bIsSelf = false;
	bHasAppliedCustomization = false;
	RefreshVisuals();
	OnSlotChanged();
	RefreshPortraitTarget();
}

// [LSLOT-005] 같은 월드의 슬롯 컴포넌트를 연결하고 외형 및 전용 촬영 텍스처를 갱신한다.
void URoomPlayerSlotWidgetBase::SetPreviewComponent(UCharacterCustomizationComponent* Component)
{
	PreviewComponent = IsValid(Component) && Component->GetWorld() == GetWorld() ? Component : nullptr;
	bHasAppliedCustomization = false;
	RefreshVisuals();
	RefreshPortraitTarget();
}

// [LSLOT-001] 월드·슬롯별 RenderTarget을 생성 또는 재사용해 카메라와 이미지를 함께 연결한다.
void URoomPlayerSlotWidgetBase::RefreshPortraitTarget()
{
	AActor* Actor = PreviewComponent.IsValid() ? PreviewComponent->GetOwner() : nullptr;
	USceneCaptureComponent2D* Capture = Actor ? Actor->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
	if (!Capture)
	{
		SlotRenderTarget = nullptr;
		return;
	}

	if (!SlotRenderTarget || SlotRenderTarget->GetOuter() != Actor)
	{
		UTextureRenderTarget2D* SourceTarget = Capture->TextureTarget;
		if (!SourceTarget) return;
		if (SourceTarget->HasAnyFlags(RF_Transient) && SourceTarget->GetOuter() == Actor)
		{
			// 슬롯 위젯을 다시 열어도 해당 월드 액터가 이미 소유한 텍스처를 재사용한다.
			SlotRenderTarget = SourceTarget;
		}
		else
		{
			// 에셋 텍스처는 같은 프로세스의 PIE 월드들이 공유하므로 직접 촬영하지 않는다.
			SlotRenderTarget = NewObject<UTextureRenderTarget2D>(Actor, NAME_None, RF_Transient);
			SlotRenderTarget->RenderTargetFormat = SourceTarget->RenderTargetFormat;
			SlotRenderTarget->ClearColor = SourceTarget->ClearColor;
			SlotRenderTarget->TargetGamma = SourceTarget->TargetGamma;
			SlotRenderTarget->InitCustomFormat(FMath::Max(1, SourceTarget->SizeX), FMath::Max(1, SourceTarget->SizeY),
				SourceTarget->GetFormat(), SourceTarget->bForceLinearGamma);
			SlotRenderTarget->UpdateResourceImmediate(true);
		}
	}
	Capture->TextureTarget = SlotRenderTarget;

	// OnSlotChanged의 BP가 공용 SlotRenderTargets를 선택한 뒤, 실제 촬영 텍스처로 교체한다.
	if (CharacterImage)
	{
		if (UMaterialInstanceDynamic* Material = CharacterImage->GetDynamicMaterial())
			Material->SetTextureParameterValue(TEXT("PortraitRT"), SlotRenderTarget);
	}
	if (bOccupied && !Capture->bCaptureEveryFrame) Capture->CaptureScene();
}

UCharacterCustomizationComponent* URoomPlayerSlotWidgetBase::GetOrCreatePreviewComponent(AActor* Actor)
{
	if (!Actor) return nullptr;
	auto* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return nullptr;
	auto* Component = Actor->FindComponentByClass<UCharacterCustomizationComponent>();
	if (!Component)
	{
		Component = NewObject<UCharacterCustomizationComponent>(Actor);
		Component->SetIsReplicated(false);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
	}
	Component->SetPreviewMesh(Mesh);
	return Component;
}

void URoomPlayerSlotWidgetBase::SetPreviewActor(AActor* Actor)
{
	if (auto* Component = GetOrCreatePreviewComponent(Actor)) SetPreviewComponent(Component);
}

AActor* URoomPlayerSlotWidgetBase::FindLobbyPreviewActor(const UObject* WorldContextObject, int32 SlotIndex)
{
	if (SlotIndex < 0 || !WorldContextObject) return nullptr;
	UClass* PreviewClass = LoadClass<AActor>(nullptr,
		TEXT("/Game/02_JSY/MainLobby/LobbyCharacter/BP_LobbyCharacterPreview.BP_LobbyCharacterPreview_C"));
	if (!PreviewClass) return nullptr;
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject, PreviewClass, Actors);
	for (AActor* Actor : Actors)
	{
		if (!Actor) continue;
		const auto* Property = FindFProperty<FNumericProperty>(Actor->GetClass(), TEXT("PreviewSlotIndex"));
		if (Property && Property->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor)) == SlotIndex)
			return Actor;
	}
	return nullptr;
}

void URoomPlayerSlotWidgetBase::FindPreviewActorForSlot(int32 SlotIndex)
{
	SetPreviewActor(FindLobbyPreviewActor(this, SlotIndex));
}

void URoomPlayerSlotWidgetBase::RefreshVisuals()
{
	if (PreviewComponent.IsValid())
	{
		if (AActor* Actor = PreviewComponent->GetOwner()) Actor->SetActorHiddenInGame(!bOccupied);
		if (bOccupied && (!bHasAppliedCustomization || LastAppliedCustomization != Member.Customization))
		{
			PreviewComponent->ApplyPreview(Member.Customization);
			LastAppliedCustomization = Member.Customization;
			bHasAppliedCustomization = true;
		}
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
