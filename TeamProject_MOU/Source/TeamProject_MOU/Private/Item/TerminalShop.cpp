#include "Item/TerminalShop.h"

#include "Item/TerminalShopWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

// [TSHOP-000] 상호작용 판정 상자와 임시 모니터 메시를 구성한다.
ATerminalShop::ATerminalShop()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetBoxExtent(FVector(70.0f, 20.0f, 65.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(InteractionBox);

	MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh"));
	MonitorMesh->SetupAttachment(InteractionBox);
	MonitorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MonitorMesh->SetRelativeScale3D(FVector(1.4f, 0.2f, 1.3f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		MonitorMesh->SetStaticMesh(Cube.Object);
	}

	ShopWidgetClass = UTerminalShopWidget::StaticClass();
}

// [TSHOP-001] 플레이어가 상점 사용 거리 안에 있는지 확인한다.
bool ATerminalShop::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) &&
		FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRadius);
}

// [TSHOP-002] 상호작용한 로컬 플레이어에게 상점 UI를 연다.
void ATerminalShop::Interact_Implementation(AActor* Interactor)
{
	if (!IInteractableInterface::Execute_CanInteract(this, Interactor) || OpenWidget.IsValid())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UTerminalShopWidget* Widget = CreateWidget<UTerminalShopWidget>(PC,
		ShopWidgetClass ? ShopWidgetClass.Get() : UTerminalShopWidget::StaticClass());
	if (!Widget)
	{
		return;
	}

	Widget->InitializeShop(this);
	Widget->AddToViewport(1000);
	OpenWidget = Widget;
	Widget->ActivateShopInput();
}

// [TSHOP-003] 상호작용 안내 문구를 반환한다.
FText ATerminalShop::GetInteractPrompt_Implementation() const
{
	return NSLOCTEXT("TerminalShop", "InteractPrompt", "상점 열기");
}

// [TSHOP-004] UI가 닫혔을 때 로컬 참조를 해제한다.
void ATerminalShop::NotifyWidgetClosed(UTerminalShopWidget* ClosedWidget)
{
	if (OpenWidget.Get() == ClosedWidget)
	{
		OpenWidget.Reset();
	}
}
