#include "LevelDesign/GridLevelBuilderActor.h"
#include "LevelDesign/GridBlockPaletteDataAsset.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WrapBox.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/StaticMesh.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

class FGridLevelBuilderInputProcessor : public IInputProcessor
{
public:
	TWeakObjectPtr<AGridLevelBuilderActor> BuilderActor;
	TWeakObjectPtr<UUserWidget> BoundWidget;
	bool bIsLeftMouseDown = false;
	FIntVector LastPaintedCoord = FIntVector(INT_MAX, INT_MAX, INT_MAX);

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		if (!BoundWidget.IsValid() || !BuilderActor.IsValid())
		{
			return;
		}

		if (bIsLeftMouseDown && !SlateApp.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
		{
			bIsLeftMouseDown = false;
			LastPaintedCoord = FIntVector(INT_MAX, INT_MAX, INT_MAX);
		}

		BuilderActor->UpdateCursorGuide();
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (!BoundWidget.IsValid() || !BuilderActor.IsValid())
		{
			return false;
		}

		TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
		if (FocusedWidget.IsValid())
		{
			const FString WidgetType = FocusedWidget->GetTypeAsString();
			if (WidgetType == TEXT("SEditableText") || WidgetType == TEXT("SMultiLineEditableText"))
			{
				return false;
			}
		}

		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::One || Key == EKeys::NumPadOne)
		{
			BuilderActor->SetPaintMode(EGridPaintAction::Add);
			return true;
		}
		else if (Key == EKeys::Two || Key == EKeys::NumPadTwo)
		{
			BuilderActor->SetPaintMode(EGridPaintAction::Replace);
			return true;
		}
		else if (Key == EKeys::Three || Key == EKeys::NumPadThree)
		{
			BuilderActor->SetPaintMode(EGridPaintAction::Delete);
			return true;
		}
		else if (Key == EKeys::Four || Key == EKeys::NumPadFour)
		{
			BuilderActor->SetPaintMode(EGridPaintAction::None);
			return true;
		}

		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (!BoundWidget.IsValid() || !BuilderActor.IsValid())
		{
			return false;
		}

		if (BuilderActor->CurrentPaintMode == EGridPaintAction::None)
		{
			return false;
		}

		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (BuilderActor->IsCursorInsideEditorViewport())
			{
				bIsLeftMouseDown = true;
				FHitResult HitResult;
				if (BuilderActor->TraceUnderEditorCursor(HitResult))
				{
					const FIntVector TargetCoord = BuilderActor->GetTargetGridCoord(HitResult.Location, HitResult.ImpactNormal, BuilderActor->CurrentPaintMode);
					BuilderActor->PaintAtEditorCursor(BuilderActor->CurrentPaintMode, BuilderActor->ActiveBlockType);
					LastPaintedCoord = TargetCoord;
					BuilderActor->UpdateCursorGuide();
				}
				else
				{
					LastPaintedCoord = FIntVector(INT_MAX, INT_MAX, INT_MAX);
				}
				return true;
			}
		}

		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (!BoundWidget.IsValid() || !BuilderActor.IsValid())
		{
			return false;
		}

		if (BuilderActor->CurrentPaintMode == EGridPaintAction::None)
		{
			return false;
		}

		if (bIsLeftMouseDown && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			if (BuilderActor->IsCursorInsideEditorViewport())
			{
				FHitResult HitResult;
				if (BuilderActor->TraceUnderEditorCursor(HitResult))
				{
					const FIntVector TargetCoord = BuilderActor->GetTargetGridCoord(HitResult.Location, HitResult.ImpactNormal, BuilderActor->CurrentPaintMode);
					if (TargetCoord != LastPaintedCoord)
					{
						BuilderActor->PaintAtEditorCursor(BuilderActor->CurrentPaintMode, BuilderActor->ActiveBlockType);
						LastPaintedCoord = TargetCoord;
						BuilderActor->UpdateCursorGuide();
					}
				}
				return true;
			}
		}

		return false;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (bIsLeftMouseDown)
			{
				bIsLeftMouseDown = false;
				LastPaintedCoord = FIntVector(INT_MAX, INT_MAX, INT_MAX);
				return true;
			}
		}

		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return false; }
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return false; }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return false; }
};
#endif

void UGridPaletteButtonBinder::OnClicked()
{
	if (BuilderActor.IsValid())
	{
		BuilderActor->SelectActiveBlock(BoundBlockName);
	}
}

AGridLevelBuilderActor::AGridLevelBuilderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BoxVisualizer = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxVisualizer"));
	BoxVisualizer->SetupAttachment(SceneRoot);
	BoxVisualizer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxVisualizer->SetLineThickness(2.0f);
	BoxVisualizer->bHiddenInGame = true;
#if WITH_EDITOR
	BoxVisualizer->bDrawOnlyIfSelected = false;
#endif

	GuideBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("GuideBoxComponent"));
	GuideBoxComponent->SetupAttachment(SceneRoot);
	GuideBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideBoxComponent->SetLineThickness(3.0f);
	GuideBoxComponent->ShapeColor = FColor(0, 255, 0);
	GuideBoxComponent->SetVisibility(false);
	GuideBoxComponent->bHiddenInGame = true;
#if WITH_EDITOR
	GuideBoxComponent->bDrawOnlyIfSelected = false;
#endif

	PreviewComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewComponent"));
	PreviewComponent->SetupAttachment(SceneRoot);
	PreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewComponent->SetCastShadow(false);
	PreviewComponent->SetVisibility(false);
	PreviewComponent->bHiddenInGame = true;
}

void AGridLevelBuilderActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (BlockPivotOffset == FVector(50.0f, 50.0f, 50.0f))
	{
		BlockPivotOffset = FVector(50.0f, 50.0f, 0.0f);
	}

	UpdateBoxVisualizer();
	RebuildAllInstances();
}

void AGridLevelBuilderActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisableGlobalHotkeys();
	HideCursorGuide();
	Super::EndPlay(EndPlayReason);
}

void AGridLevelBuilderActor::BeginDestroy()
{
	DisableGlobalHotkeys();
	HideCursorGuide();
	Super::BeginDestroy();
}

#if WITH_EDITOR
void AGridLevelBuilderActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGridLevelBuilderActor, BoxMinCoord) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGridLevelBuilderActor, BoxMaxCoord) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGridLevelBuilderActor, GridUnitSize))
	{
		UpdateBoxVisualizer();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AGridLevelBuilderActor, PaletteDataAsset) ||
			 PropertyName == GET_MEMBER_NAME_CHECKED(AGridLevelBuilderActor, bEnableHiddenCubeCulling) ||
			 PropertyName == GET_MEMBER_NAME_CHECKED(AGridLevelBuilderActor, BlockPivotOffset))
	{
		RebuildAllInstances();
	}
}
#endif

void AGridLevelBuilderActor::SetPaintMode(EGridPaintAction NewMode)
{
	CurrentPaintMode = NewMode;
	if (CurrentPaintMode == EGridPaintAction::None)
	{
		HideCursorGuide();
	}
	else
	{
		UpdateCursorGuide();
	}

#if WITH_EDITOR
	if (GEngine)
	{
		FString ModeStr;
		FColor MsgColor = FColor::White;
		switch (CurrentPaintMode)
		{
		case EGridPaintAction::Add:
			ModeStr = TEXT("[그리드 빌더] 1번 [추가 (Add) 모드] 활성화 - 좌클릭/드래그로 설치");
			MsgColor = FColor::Green;
			break;
		case EGridPaintAction::Replace:
			ModeStr = TEXT("[그리드 빌더] 2번 [교체 (Replace) 모드] 활성화 - 좌클릭/드래그로 교체");
			MsgColor = FColor::Yellow;
			break;
		case EGridPaintAction::Delete:
			ModeStr = TEXT("[그리드 빌더] 3번 [삭제 (Delete) 모드] 활성화 - 좌클릭/드래그로 삭제");
			MsgColor = FColor::Red;
			break;
		case EGridPaintAction::None:
			ModeStr = TEXT("[그리드 빌더] 4번 [모드 해제 (None)] - 일반 에디터 조작으로 복귀");
			MsgColor = FColor::Cyan;
			break;
		default:
			break;
		}

		if (!ModeStr.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(91823, 3.0f, MsgColor, ModeStr);
		}
	}
#endif
}

void AGridLevelBuilderActor::SelectActiveBlock(FName NewBlockName)
{
	ActiveBlockType = NewBlockName;
	UpdateCursorGuide();
}

AGridLevelBuilderActor* AGridLevelBuilderActor::GetOrFindBuilderActor(UObject* WorldContextObject)
{
#if WITH_EDITOR
	if (GEditor)
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if (SelectedActors)
		{
			for (FSelectionIterator It(*SelectedActors); It; ++It)
			{
				if (AGridLevelBuilderActor* Builder = Cast<AGridLevelBuilderActor>(*It))
				{
					return Builder;
				}
			}
		}
	}
#endif

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
#if WITH_EDITOR
	if (!World && GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
#endif

	if (World)
	{
		for (TActorIterator<AGridLevelBuilderActor> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void AGridLevelBuilderActor::PopulatePaletteWrapBox(UWrapBox* InWrapBox, FVector2D ButtonSize)
{
	if (!InWrapBox)
	{
		return;
	}

	InWrapBox->ClearChildren();
	ButtonBinders.Empty();

	if (!PaletteDataAsset)
	{
		return;
	}

	const TArray<FGridBlockUIEntry> Entries = PaletteDataAsset->GetPaletteUIEntries();
	for (const FGridBlockUIEntry& Entry : Entries)
	{
		UButton* NewButton = NewObject<UButton>(InWrapBox);
		if (!NewButton)
		{
			continue;
		}

		UOverlay* NewOverlay = NewObject<UOverlay>(NewButton);
		if (NewOverlay)
		{
			UImage* NewImage = NewObject<UImage>(NewOverlay);
			if (NewImage)
			{
				if (Entry.IconTexture)
				{
					NewImage->SetBrushFromTexture(Entry.IconTexture, false);
				}
				NewImage->SetDesiredSizeOverride(ButtonSize);
				UOverlaySlot* ImageSlot = Cast<UOverlaySlot>(NewOverlay->AddChild(NewImage));
				if (ImageSlot)
				{
					ImageSlot->SetHorizontalAlignment(HAlign_Fill);
					ImageSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}

			UTextBlock* NewText = NewObject<UTextBlock>(NewOverlay);
			if (NewText)
			{
				NewText->SetText(FText::FromName(Entry.BlockName));
				NewText->SetJustification(ETextJustify::Center);
				FSlateFontInfo FontInfo = NewText->GetFont();
				FontInfo.Size = 9.0f;
				NewText->SetFont(FontInfo);
				NewText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				NewText->SetShadowOffset(FVector2D(1.0f, 1.0f));
				NewText->SetShadowColorAndOpacity(FLinearColor::Black);

				UOverlaySlot* TextSlot = Cast<UOverlaySlot>(NewOverlay->AddChild(NewText));
				if (TextSlot)
				{
					TextSlot->SetHorizontalAlignment(HAlign_Center);
					TextSlot->SetVerticalAlignment(VAlign_Bottom);
					TextSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 2.0f));
				}
			}

			NewButton->AddChild(NewOverlay);
		}

		NewButton->SetToolTipText(FText::FromName(Entry.BlockName));

		UGridPaletteButtonBinder* Binder = NewObject<UGridPaletteButtonBinder>(this);
		Binder->BoundBlockName = Entry.BlockName;
		Binder->BuilderActor = this;
		NewButton->OnClicked.AddDynamic(Binder, &UGridPaletteButtonBinder::OnClicked);
		ButtonBinders.Add(Binder);

		UPanelSlot* AddedSlot = InWrapBox->AddChild(NewButton);
		if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(AddedSlot))
		{
			WrapSlot->SetPadding(FMargin(4.0f));
		}
	}
}

void AGridLevelBuilderActor::ExecuteBoxFill()
{
	FillBoxRange(BoxMinCoord, BoxMaxCoord, ActiveBlockType, true);
}

void AGridLevelBuilderActor::ExecuteBoxClear()
{
	ClearBoxRange(BoxMinCoord, BoxMaxCoord, true);
}

void AGridLevelBuilderActor::ClearAllBlocks()
{
	PlacedBlocks.Empty();
	ClearAllHISMInstances();
	HideGhostPreview();
	HideCursorGuide();
}

bool AGridLevelBuilderActor::SetBlock(const FIntVector& GridCoord, const FName& BlockType, bool bAutoRebuild)
{
	if (BlockType.IsNone())
	{
		return RemoveBlock(GridCoord, bAutoRebuild);
	}

	PlacedBlocks.Add(GridCoord, BlockType);

	if (bAutoRebuild)
	{
		RebuildAllInstances();
	}
	return true;
}

bool AGridLevelBuilderActor::RemoveBlock(const FIntVector& GridCoord, bool bAutoRebuild)
{
	const int32 RemovedCount = PlacedBlocks.Remove(GridCoord);
	if (RemovedCount > 0 && bAutoRebuild)
	{
		RebuildAllInstances();
		return true;
	}
	return false;
}

void AGridLevelBuilderActor::FillBoxRange(const FIntVector& MinCoord, const FIntVector& MaxCoord, const FName& BlockType, bool bAutoRebuild)
{
	if (BlockType.IsNone())
	{
		return;
	}

	const int32 MinX = FMath::Min(MinCoord.X, MaxCoord.X);
	const int32 MaxX = FMath::Max(MinCoord.X, MaxCoord.X);
	const int32 MinY = FMath::Min(MinCoord.Y, MaxCoord.Y);
	const int32 MaxY = FMath::Max(MinCoord.Y, MaxCoord.Y);
	const int32 MinZ = FMath::Min(MinCoord.Z, MaxCoord.Z);
	const int32 MaxZ = FMath::Max(MinCoord.Z, MaxCoord.Z);

	for (int32 X = MinX; X <= MaxX; ++X)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 Z = MinZ; Z <= MaxZ; ++Z)
			{
				PlacedBlocks.Add(FIntVector(X, Y, Z), BlockType);
			}
		}
	}

	if (bAutoRebuild)
	{
		RebuildAllInstances();
	}
}

void AGridLevelBuilderActor::ClearBoxRange(const FIntVector& MinCoord, const FIntVector& MaxCoord, bool bAutoRebuild)
{
	const int32 MinX = FMath::Min(MinCoord.X, MaxCoord.X);
	const int32 MaxX = FMath::Max(MinCoord.X, MaxCoord.X);
	const int32 MinY = FMath::Min(MinCoord.Y, MaxCoord.Y);
	const int32 MaxY = FMath::Max(MinCoord.Y, MaxCoord.Y);
	const int32 MinZ = FMath::Min(MinCoord.Z, MaxCoord.Z);
	const int32 MaxZ = FMath::Max(MinCoord.Z, MaxCoord.Z);

	for (int32 X = MinX; X <= MaxX; ++X)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 Z = MinZ; Z <= MaxZ; ++Z)
			{
				PlacedBlocks.Remove(FIntVector(X, Y, Z));
			}
		}
	}

	if (bAutoRebuild)
	{
		RebuildAllInstances();
	}
}

bool AGridLevelBuilderActor::AddBlockOnSurface(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType, bool bAutoRebuild)
{
	const FIntVector TargetCoord = GetTargetGridCoord(HitLocation, HitNormal, EGridPaintAction::Add);
	return SetBlock(TargetCoord, BlockType, bAutoRebuild);
}

bool AGridLevelBuilderActor::ReplaceBlockAtHit(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType, bool bAutoRebuild)
{
	const FIntVector TargetCoord = GetTargetGridCoord(HitLocation, HitNormal, EGridPaintAction::Replace);
	if (PlacedBlocks.Contains(TargetCoord))
	{
		return SetBlock(TargetCoord, BlockType, bAutoRebuild);
	}
	return false;
}

bool AGridLevelBuilderActor::RemoveBlockAtHit(const FVector& HitLocation, const FVector& HitNormal, bool bAutoRebuild)
{
	const FIntVector TargetCoord = GetTargetGridCoord(HitLocation, HitNormal, EGridPaintAction::Delete);
	return RemoveBlock(TargetCoord, bAutoRebuild);
}

bool AGridLevelBuilderActor::AddBlockOnSurfaceFast(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType)
{
	if (BlockType.IsNone())
	{
		return false;
	}

	const FIntVector TargetCoord = GetTargetGridCoord(HitLocation, HitNormal, EGridPaintAction::Add);
	if (PlacedBlocks.Contains(TargetCoord))
	{
		return false;
	}

	PlacedBlocks.Add(TargetCoord, BlockType);

	UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHISMComponent(BlockType);
	if (HISM)
	{
		const FTransform LocalTransform = GetBlockRelativeTransform(TargetCoord, BlockType);
		HISM->AddInstance(LocalTransform, false);
		return true;
	}
	return false;
}

bool AGridLevelBuilderActor::ReplaceBlockAtHitFast(const FHitResult& HitResult, const FName& BlockType)
{
	if (BlockType.IsNone())
	{
		return false;
	}

	const FIntVector TargetCoord = GetTargetGridCoord(HitResult.Location, HitResult.ImpactNormal, EGridPaintAction::Replace);
	const FName* OldBlockType = PlacedBlocks.Find(TargetCoord);
	if (!OldBlockType || *OldBlockType == BlockType)
	{
		return false;
	}

	PlacedBlocks.Add(TargetCoord, BlockType);

	if (UHierarchicalInstancedStaticMeshComponent* OldHISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.Component.Get()))
	{
		if (HitResult.Item != INDEX_NONE && HitResult.Item < OldHISM->GetInstanceCount())
		{
			OldHISM->RemoveInstance(HitResult.Item);
		}
	}

	UHierarchicalInstancedStaticMeshComponent* NewHISM = GetOrCreateHISMComponent(BlockType);
	if (NewHISM)
	{
		const FTransform LocalTransform = GetBlockRelativeTransform(TargetCoord, BlockType);
		NewHISM->AddInstance(LocalTransform, false);
		return true;
	}
	return false;
}

bool AGridLevelBuilderActor::RemoveBlockAtHitFast(const FHitResult& HitResult)
{
	const FIntVector TargetCoord = GetTargetGridCoord(HitResult.Location, HitResult.ImpactNormal, EGridPaintAction::Delete);
	const FName* OldBlockType = PlacedBlocks.Find(TargetCoord);
	if (!OldBlockType)
	{
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.Component.Get()))
		{
			if (HitResult.Item != INDEX_NONE && HitResult.Item < HISM->GetInstanceCount())
			{
				FTransform InstTransform;
				if (HISM->GetInstanceTransform(HitResult.Item, InstTransform, false))
				{
					const FIntVector InstCoord = WorldToGrid(GetActorTransform().TransformPosition(InstTransform.GetLocation() + FVector(0, 0, GridUnitSize * 0.5f)));
					if (PlacedBlocks.Contains(InstCoord))
					{
						PlacedBlocks.Remove(InstCoord);
						HISM->RemoveInstance(HitResult.Item);
						return true;
					}
				}
			}
		}
		return false;
	}

	PlacedBlocks.Remove(TargetCoord);

	if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.Component.Get()))
	{
		if (HitResult.Item != INDEX_NONE && HitResult.Item < HISM->GetInstanceCount())
		{
			HISM->RemoveInstance(HitResult.Item);
			return true;
		}
	}
	return false;
}

bool AGridLevelBuilderActor::IsCursorInsideEditorViewport() const
{
#if WITH_EDITOR
	if (!GEditor)
	{
		return false;
	}

	FEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
		{
			ViewportClient = static_cast<FEditorViewportClient*>(ActiveViewport->GetClient());
		}
	}

	if (!ViewportClient || !ViewportClient->Viewport)
	{
		return false;
	}

	const int32 MouseX = ViewportClient->Viewport->GetMouseX();
	const int32 MouseY = ViewportClient->Viewport->GetMouseY();
	const FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();

	return (MouseX >= 0 && MouseY >= 0 && MouseX < ViewportSize.X && MouseY < ViewportSize.Y);
#else
	return false;
#endif
}

bool AGridLevelBuilderActor::TraceUnderEditorCursor(FHitResult& OutHit)
{
#if WITH_EDITOR
	if (!IsCursorInsideEditorViewport())
	{
		return false;
	}

	FEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
		{
			ViewportClient = static_cast<FEditorViewportClient*>(ActiveViewport->GetClient());
		}
	}

	if (!ViewportClient || !ViewportClient->Viewport)
	{
		return false;
	}

	const FViewportCursorLocation CursorLoc = ViewportClient->GetCursorWorldLocationFromMousePos();
	const FVector RayOrigin = CursorLoc.GetOrigin();
	const FVector RayDirection = CursorLoc.GetDirection();

	if (RayDirection.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(TEXT("GridCursorTrace"), true);
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnFaceIndex = true;
	return GetWorld()->LineTraceSingleByChannel(OutHit, RayOrigin, RayOrigin + (RayDirection * 100000.0), ECC_Visibility, QueryParams);
#else
	return false;
#endif
}

bool AGridLevelBuilderActor::PaintAtEditorCursor(EGridPaintAction Action, const FName& BlockType)
{
	if (Action == EGridPaintAction::None)
	{
		return false;
	}

	FHitResult HitResult;
	if (!TraceUnderEditorCursor(HitResult))
	{
		return false;
	}

	switch (Action)
	{
	case EGridPaintAction::Add:
		return AddBlockOnSurfaceFast(HitResult.Location, HitResult.ImpactNormal, BlockType);
	case EGridPaintAction::Replace:
		return ReplaceBlockAtHitFast(HitResult, BlockType);
	case EGridPaintAction::Delete:
		return RemoveBlockAtHitFast(HitResult);
	case EGridPaintAction::Eyedropper:
		return !GetBlockAtHit(HitResult.Location, HitResult.ImpactNormal).IsNone();
	default:
		return false;
	}
}

void AGridLevelBuilderActor::UpdateCursorGuide()
{
#if WITH_EDITOR
	if (CurrentPaintMode == EGridPaintAction::None)
	{
		HideCursorGuide();
		return;
	}

	FHitResult HitResult;
	if (!TraceUnderEditorCursor(HitResult))
	{
		HideCursorGuide();
		return;
	}

	const FIntVector TargetCoord = GetTargetGridCoord(HitResult.Location, HitResult.ImpactNormal, CurrentPaintMode);

	if (GuideBoxComponent)
	{
		const FVector CellCenter = GetCellCenterRelativeLocation(TargetCoord);
		GuideBoxComponent->SetRelativeLocation(CellCenter);
		GuideBoxComponent->SetBoxExtent(FVector(GridUnitSize * 0.5f));

		if (CurrentPaintMode == EGridPaintAction::Add)
		{
			GuideBoxComponent->ShapeColor = FColor(0, 255, 0);
		}
		else if (CurrentPaintMode == EGridPaintAction::Replace)
		{
			GuideBoxComponent->ShapeColor = FColor(255, 200, 0);
		}
		else if (CurrentPaintMode == EGridPaintAction::Delete)
		{
			GuideBoxComponent->ShapeColor = FColor(255, 0, 0);
		}

		GuideBoxComponent->MarkRenderStateDirty();
		GuideBoxComponent->SetVisibility(true);
	}

	if (CurrentPaintMode == EGridPaintAction::Add)
	{
		UpdateGhostPreview(HitResult.Location, HitResult.ImpactNormal, ActiveBlockType, CurrentPaintMode);
	}
	else
	{
		HideGhostPreview();
	}
#endif
}

void AGridLevelBuilderActor::HideCursorGuide()
{
	if (GuideBoxComponent)
	{
		GuideBoxComponent->SetVisibility(false);
	}
	HideGhostPreview();
}

void AGridLevelBuilderActor::EnableGlobalHotkeys(UUserWidget* InWidget)
{
#if WITH_EDITOR
	DisableGlobalHotkeys();

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	InputProcessor = MakeShared<FGridLevelBuilderInputProcessor>();
	InputProcessor->BuilderActor = this;
	InputProcessor->BoundWidget = InWidget;
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
#endif
}

void AGridLevelBuilderActor::DisableGlobalHotkeys()
{
#if WITH_EDITOR
	if (InputProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
	}
	HideCursorGuide();
#endif
}

FName AGridLevelBuilderActor::GetBlockAtHit(const FVector& HitLocation, const FVector& HitNormal) const
{
	const FIntVector TargetCoord = GetTargetGridCoord(HitLocation, HitNormal, EGridPaintAction::Eyedropper);
	if (const FName* Found = PlacedBlocks.Find(TargetCoord))
	{
		return *Found;
	}
	return NAME_None;
}

FIntVector AGridLevelBuilderActor::GetTargetGridCoord(const FVector& HitLocation, const FVector& HitNormal, EGridPaintAction Action) const
{
	const float OffsetDistance = GridUnitSize * 0.45f;
	const FVector AdjustedLocation = (Action == EGridPaintAction::Add)
		? (HitLocation + (HitNormal * OffsetDistance))
		: (HitLocation - (HitNormal * OffsetDistance));

	return WorldToGrid(AdjustedLocation);
}

void AGridLevelBuilderActor::UpdateGhostPreview(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType, EGridPaintAction Action)
{
	if (!PreviewComponent || !PaletteDataAsset)
	{
		return;
	}

	const FGridBlockEntry* Entry = PaletteDataAsset->FindBlockEntry(BlockType);
	if (!Entry || !Entry->StaticMesh)
	{
		PreviewComponent->SetVisibility(false);
		return;
	}

	const FIntVector TargetCoord = GetTargetGridCoord(HitLocation, HitNormal, Action);
	const FTransform RelativeTransform = GetBlockRelativeTransform(TargetCoord, BlockType);
	PreviewComponent->SetRelativeTransform(RelativeTransform);

	if (PreviewComponent->GetStaticMesh() != Entry->StaticMesh)
	{
		PreviewComponent->SetStaticMesh(Entry->StaticMesh);
	}
	if (Entry->MaterialOverride)
	{
		PreviewComponent->SetMaterial(0, Entry->MaterialOverride);
	}
	PreviewComponent->SetVisibility(true);
}

void AGridLevelBuilderActor::HideGhostPreview()
{
	if (PreviewComponent)
	{
		PreviewComponent->SetVisibility(false);
	}
}

void AGridLevelBuilderActor::RebuildAllInstances()
{
	ClearAllHISMInstances();

	if (!PaletteDataAsset || PlacedBlocks.Num() == 0)
	{
		return;
	}

	for (const auto& Pair : PlacedBlocks)
	{
		const FIntVector& GridCoord = Pair.Key;
		const FName& BlockType = Pair.Value;

		if (bEnableHiddenCubeCulling && IsBlockOccluded(GridCoord))
		{
			continue;
		}

		UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHISMComponent(BlockType);
		if (!HISM)
		{
			continue;
		}

		const FTransform LocalTransform = GetBlockRelativeTransform(GridCoord, BlockType);
		HISM->AddInstance(LocalTransform, false);
	}
}

void AGridLevelBuilderActor::RebuildSingleBlockHISM(const FName& BlockType)
{
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* FoundHISM = HISMComponents.Find(BlockType);
	if (!FoundHISM || !FoundHISM->Get())
	{
		return;
	}

	UHierarchicalInstancedStaticMeshComponent* HISM = FoundHISM->Get();
	HISM->ClearInstances();

	for (const auto& Pair : PlacedBlocks)
	{
		if (Pair.Value == BlockType)
		{
			const FTransform LocalTransform = GetBlockRelativeTransform(Pair.Key, BlockType);
			HISM->AddInstance(LocalTransform, false);
		}
	}
}

bool AGridLevelBuilderActor::IsBlockOccluded(const FIntVector& GridCoord) const
{
	static const FIntVector DirectionOffsets[6] = {
		FIntVector(1, 0, 0),
		FIntVector(-1, 0, 0),
		FIntVector(0, 1, 0),
		FIntVector(0, -1, 0),
		FIntVector(0, 0, 1),
		FIntVector(0, 0, -1)
	};

	for (const FIntVector& Offset : DirectionOffsets)
	{
		const FIntVector NeighborCoord = GridCoord + Offset;
		const FName* NeighborBlock = PlacedBlocks.Find(NeighborCoord);
		if (!NeighborBlock)
		{
			return false;
		}

		if (PaletteDataAsset)
		{
			const FGridBlockEntry* Entry = PaletteDataAsset->FindBlockEntry(*NeighborBlock);
			if (!Entry || !Entry->bIsOpaque)
			{
				return false;
			}
		}
	}

	return true;
}

FIntVector AGridLevelBuilderActor::WorldToGrid(const FVector& WorldLocation) const
{
	const FVector RelativePos = GetActorTransform().InverseTransformPosition(WorldLocation);
	return FIntVector(
		FMath::FloorToInt(RelativePos.X / GridUnitSize),
		FMath::FloorToInt(RelativePos.Y / GridUnitSize),
		FMath::FloorToInt(RelativePos.Z / GridUnitSize)
	);
}

FVector AGridLevelBuilderActor::GridToWorld(const FIntVector& GridCoord) const
{
	const FTransform RelativeTransform = GridToRelativeTransform(GridCoord);
	return GetActorTransform().TransformPosition(RelativeTransform.GetLocation());
}

FVector AGridLevelBuilderActor::GetCellCenterRelativeLocation(const FIntVector& GridCoord) const
{
	return FVector(
		(static_cast<float>(GridCoord.X) + 0.5f) * GridUnitSize,
		(static_cast<float>(GridCoord.Y) + 0.5f) * GridUnitSize,
		(static_cast<float>(GridCoord.Z) + 0.5f) * GridUnitSize
	);
}

FTransform AGridLevelBuilderActor::GetBlockRelativeTransform(const FIntVector& GridCoord, const FName& BlockType) const
{
	FVector MeshOffset = BlockPivotOffset;

	if (PaletteDataAsset)
	{
		if (const FGridBlockEntry* Entry = PaletteDataAsset->FindBlockEntry(BlockType))
		{
			if (Entry->StaticMesh)
			{
				const FBoxSphereBounds Bounds = Entry->StaticMesh->GetBounds();
				const FVector CellCenterOffset = FVector(GridUnitSize * 0.5f);
				MeshOffset = CellCenterOffset - Bounds.Origin;
			}
		}
	}

	const FVector Position = FVector(
		static_cast<float>(GridCoord.X) * GridUnitSize + MeshOffset.X,
		static_cast<float>(GridCoord.Y) * GridUnitSize + MeshOffset.Y,
		static_cast<float>(GridCoord.Z) * GridUnitSize + MeshOffset.Z
	);
	return FTransform(FRotator::ZeroRotator, Position);
}

FTransform AGridLevelBuilderActor::GridToRelativeTransform(const FIntVector& GridCoord) const
{
	return GetBlockRelativeTransform(GridCoord, ActiveBlockType);
}

UHierarchicalInstancedStaticMeshComponent* AGridLevelBuilderActor::GetOrCreateHISMComponent(const FName& BlockType)
{
	if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* ExistingComp = HISMComponents.Find(BlockType))
	{
		if (ExistingComp->Get())
		{
			return ExistingComp->Get();
		}
	}

	if (!PaletteDataAsset)
	{
		return nullptr;
	}

	const FGridBlockEntry* Entry = PaletteDataAsset->FindBlockEntry(BlockType);
	if (!Entry || !Entry->StaticMesh)
	{
		return nullptr;
	}

	const FString CompName = FString::Printf(TEXT("HISM_%s"), *BlockType.ToString());
	UHierarchicalInstancedStaticMeshComponent* NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, *CompName);
	if (!NewHISM)
	{
		return nullptr;
	}

	NewHISM->SetupAttachment(SceneRoot);
	NewHISM->RegisterComponent();
	NewHISM->SetStaticMesh(Entry->StaticMesh);

	if (Entry->MaterialOverride)
	{
		NewHISM->SetMaterial(0, Entry->MaterialOverride);
	}

	if (Entry->bHasCollision)
	{
		NewHISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		NewHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	HISMComponents.Add(BlockType, NewHISM);
	return NewHISM;
}

void AGridLevelBuilderActor::ClearAllHISMInstances()
{
	for (auto& Pair : HISMComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
}

void AGridLevelBuilderActor::UpdateBoxVisualizer()
{
	if (!BoxVisualizer)
	{
		return;
	}

	const int32 MinX = FMath::Min(BoxMinCoord.X, BoxMaxCoord.X);
	const int32 MaxX = FMath::Max(BoxMinCoord.X, BoxMaxCoord.X);
	const int32 MinY = FMath::Min(BoxMinCoord.Y, BoxMaxCoord.Y);
	const int32 MaxY = FMath::Max(BoxMinCoord.Y, BoxMaxCoord.Y);
	const int32 MinZ = FMath::Min(BoxMinCoord.Z, BoxMaxCoord.Z);
	const int32 MaxZ = FMath::Max(BoxMinCoord.Z, BoxMaxCoord.Z);

	const FVector MinPoint(MinX * GridUnitSize, MinY * GridUnitSize, MinZ * GridUnitSize);
	const FVector MaxPoint((MaxX + 1) * GridUnitSize, (MaxY + 1) * GridUnitSize, (MaxZ + 1) * GridUnitSize);

	const FVector Center = (MinPoint + MaxPoint) * 0.5f;
	const FVector Extent = (MaxPoint - MinPoint) * 0.5f;

	BoxVisualizer->SetRelativeLocation(Center);
	BoxVisualizer->SetBoxExtent(Extent);
}
