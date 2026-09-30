#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridLevelBuilderActor.generated.h"

class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UGridBlockPaletteDataAsset;
class UStaticMeshComponent;
class UUserWidget;
class UWrapBox;

UENUM(BlueprintType)
enum class EGridPaintAction : uint8
{
	None,
	Add,
	Replace,
	Delete,
	Eyedropper
};

UCLASS()
class UGridPaletteButtonBinder : public UObject
{
	GENERATED_BODY()

public:
	FName BoundBlockName = NAME_None;
	TWeakObjectPtr<class AGridLevelBuilderActor> BuilderActor;

	UFUNCTION()
	void OnClicked();
};

UCLASS()
class TEAMPROJECT_MOU_API AGridLevelBuilderActor : public AActor
{
	GENERATED_BODY()

public:
	AGridLevelBuilderActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid | Settings")
	float GridUnitSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid | Settings")
	FVector BlockPivotOffset = FVector(50.0f, 50.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid | Settings")
	TObjectPtr<UGridBlockPaletteDataAsset> PaletteDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid | Settings")
	bool bEnableHiddenCubeCulling = true;

	UPROPERTY(EditAnywhere, Category = "Grid | Box Action")
	FIntVector BoxMinCoord = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, Category = "Grid | Box Action")
	FIntVector BoxMaxCoord = FIntVector(4, 4, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid | Mode")
	EGridPaintAction CurrentPaintMode = EGridPaintAction::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid | Mode")
	FName ActiveBlockType = FName("Grass");

	UPROPERTY(VisibleAnywhere, SaveGame, Category = "Grid | Data")
	TMap<FIntVector, FName> PlacedBlocks;

	UFUNCTION(CallInEditor, Category = "Grid | Box Action")
	void ExecuteBoxFill();

	UFUNCTION(CallInEditor, Category = "Grid | Box Action")
	void ExecuteBoxClear();

	UFUNCTION(CallInEditor, Category = "Grid | Maintenance")
	void RebuildAllInstances();

	UFUNCTION(CallInEditor, Category = "Grid | Maintenance")
	void ClearAllBlocks();

	UFUNCTION(BlueprintCallable, Category = "Grid | Mode")
	void SetPaintMode(EGridPaintAction NewMode);

	UFUNCTION(BlueprintCallable, Category = "Grid | Mode")
	void SelectActiveBlock(FName NewBlockName);

	UFUNCTION(BlueprintCallable, Category = "Grid | UI")
	void PopulatePaletteWrapBox(UWrapBox* InWrapBox, FVector2D ButtonSize = FVector2D(64.0f, 64.0f));

	UFUNCTION(BlueprintCallable, Category = "Grid | Interaction")
	bool SetBlock(const FIntVector& GridCoord, const FName& BlockType, bool bAutoRebuild = false);

	UFUNCTION(BlueprintCallable, Category = "Grid | Interaction")
	bool RemoveBlock(const FIntVector& GridCoord, bool bAutoRebuild = false);

	UFUNCTION(BlueprintCallable, Category = "Grid | Interaction")
	void FillBoxRange(const FIntVector& MinCoord, const FIntVector& MaxCoord, const FName& BlockType, bool bAutoRebuild = true);

	UFUNCTION(BlueprintCallable, Category = "Grid | Interaction")
	void ClearBoxRange(const FIntVector& MinCoord, const FIntVector& MaxCoord, bool bAutoRebuild = true);

	UFUNCTION(BlueprintCallable, Category = "Grid | Paint")
	bool AddBlockOnSurface(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType, bool bAutoRebuild = true);

	UFUNCTION(BlueprintCallable, Category = "Grid | Paint")
	bool ReplaceBlockAtHit(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType, bool bAutoRebuild = true);

	UFUNCTION(BlueprintCallable, Category = "Grid | Paint")
	bool RemoveBlockAtHit(const FVector& HitLocation, const FVector& HitNormal, bool bAutoRebuild = true);

	UFUNCTION(BlueprintCallable, Category = "Grid | Fast Paint")
	bool AddBlockOnSurfaceFast(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType);

	UFUNCTION(BlueprintCallable, Category = "Grid | Fast Paint")
	bool ReplaceBlockAtHitFast(const FHitResult& HitResult, const FName& BlockType);

	UFUNCTION(BlueprintCallable, Category = "Grid | Fast Paint")
	bool RemoveBlockAtHitFast(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Grid | Editor")
	bool TraceUnderEditorCursor(FHitResult& OutHit);

	UFUNCTION(BlueprintCallable, Category = "Grid | Editor")
	bool PaintAtEditorCursor(EGridPaintAction Action, const FName& BlockType);

	UFUNCTION(BlueprintCallable, Category = "Grid | Guide")
	void UpdateCursorGuide();

	UFUNCTION(BlueprintCallable, Category = "Grid | Guide")
	void HideCursorGuide();

	UFUNCTION(BlueprintCallable, Category = "Grid | Hotkeys")
	void EnableGlobalHotkeys(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = "Grid | Hotkeys")
	void DisableGlobalHotkeys();

	UFUNCTION(BlueprintPure, Category = "Grid | Paint")
	FName GetBlockAtHit(const FVector& HitLocation, const FVector& HitNormal) const;

	UFUNCTION(BlueprintPure, Category = "Grid | Paint")
	FIntVector GetTargetGridCoord(const FVector& HitLocation, const FVector& HitNormal, EGridPaintAction Action) const;

	UFUNCTION(BlueprintCallable, Category = "Grid | Preview")
	void UpdateGhostPreview(const FVector& HitLocation, const FVector& HitNormal, const FName& BlockType, EGridPaintAction Action);

	UFUNCTION(BlueprintCallable, Category = "Grid | Preview")
	void HideGhostPreview();

	UFUNCTION(BlueprintPure, Category = "Grid | Utility", meta = (WorldContext = "WorldContextObject"))
	static AGridLevelBuilderActor* GetOrFindBuilderActor(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Grid | Utility")
	FIntVector WorldToGrid(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Grid | Utility")
	FVector GridToWorld(const FIntVector& GridCoord) const;

	UFUNCTION(BlueprintPure, Category = "Grid | Utility")
	FTransform GridToRelativeTransform(const FIntVector& GridCoord) const;

	UFUNCTION(BlueprintPure, Category = "Grid | Utility")
	FTransform GetBlockRelativeTransform(const FIntVector& GridCoord, const FName& BlockType) const;

	UFUNCTION(BlueprintPure, Category = "Grid | Utility")
	FVector GetCellCenterRelativeLocation(const FIntVector& GridCoord) const;

	bool IsCursorInsideEditorViewport() const;

	UFUNCTION(BlueprintPure, Category = "Grid | Utility")
	bool IsBlockOccluded(const FIntVector& GridCoord) const;

	void RebuildSingleBlockHISM(const FName& BlockType);

private:
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxVisualizer;

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	TObjectPtr<UBoxComponent> GuideBoxComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PreviewComponent;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGridPaletteButtonBinder>> ButtonBinders;

#if WITH_EDITOR
	TSharedPtr<class FGridLevelBuilderInputProcessor> InputProcessor;
#endif

	UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISMComponent(const FName& BlockType);
	void ClearAllHISMInstances();
	void UpdateBoxVisualizer();
};
