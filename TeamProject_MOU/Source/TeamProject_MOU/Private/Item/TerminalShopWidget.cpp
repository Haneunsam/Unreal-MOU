#include "Item/TerminalShopWidget.h"

#include "Item/TerminalShop.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

// [TSHOP-011] 상점 배경 텍스처를 로드한다.
UTerminalShopWidget::UTerminalShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	BackgroundTexture = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/04_JJO/TerminalShop/UI/T_ShopBackground.T_ShopBackground"));
}

// [TSHOP-005] UI를 연 상점 액터를 기록한다.
void UTerminalShopWidget::InitializeShop(ATerminalShop* InShop)
{
	OwningShop = InShop;
}

// [TSHOP-006] 상점 UI에 입력 초점과 마우스 커서를 준다.
void UTerminalShopWidget::ActivateShopInput()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		SetKeyboardFocus();
	}
}

// [TSHOP-007] 상점 UI를 닫고 게임 입력으로 돌린다.
void UTerminalShopWidget::CloseShop()
{
	if (ATerminalShop* Shop = OwningShop.Get())
	{
		Shop->NotifyWidgetClosed(this);
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	RemoveFromParent();
}

// [TSHOP-008] 배경을 0.8 불투명도로 화면에 채우고 그 위에 Widget BP 콘텐츠를 배치한다.
void UTerminalShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		return;
	}

	UWidget* BlueprintContent = WidgetTree->RootWidget;
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopLayers"));
	if (BackgroundTexture)
	{
		UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopBackground"));
		Background->SetBrushFromTexture(BackgroundTexture, true);
		Background->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f));
		Background->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UOverlaySlot* BackgroundSlot = Layers->AddChildToOverlay(Background);
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ShopScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	UOverlaySlot* ScaleSlot = Layers->AddChildToOverlay(Scale);
	ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
	ScaleSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* Fixed = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopSize"));
	Fixed->SetWidthOverride(1920.0f);
	Fixed->SetHeightOverride(1080.0f);
	Scale->AddChild(Fixed);
	if (BlueprintContent)
	{
		Fixed->AddChild(BlueprintContent);
	}
	WidgetTree->RootWidget = Layers;
}

// [TSHOP-009] Esc 입력을 받아 상점 UI를 닫는다.
FReply UTerminalShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseShop();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
