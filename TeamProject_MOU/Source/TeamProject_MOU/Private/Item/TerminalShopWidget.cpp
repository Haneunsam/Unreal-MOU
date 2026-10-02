#include "Item/TerminalShopWidget.h"

#include "Item/TerminalShop.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

// [TSHOP-011] 키보드 입력을 받을 수 있도록 상점 위젯을 설정한다.
UTerminalShopWidget::UTerminalShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
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
		// 이동 입력을 누른 채 상점을 열면 UI 전환 뒤 Key Up을 받지 못해 이동이 계속될 수 있다.
		// 현재 속도와 눌린 키 상태를 함께 비워 상점이 열리는 즉시 플레이어를 정지시킨다.
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UPawnMovementComponent* MovementComponent = Pawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}
		}
		PC->FlushPressedKeys();

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

// [TSHOP-008] Widget BP 콘텐츠를 기준 해상도에 맞춰 화면에 배치한다.
void UTerminalShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		return;
	}

	UWidget* BlueprintContent = WidgetTree->RootWidget;
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopLayers"));

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
