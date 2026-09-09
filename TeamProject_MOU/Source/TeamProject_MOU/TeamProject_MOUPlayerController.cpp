// Copyright Epic Games, Inc. All Rights Reserved.


#include "TeamProject_MOUPlayerController.h"
#include "Subsystems/WarehouseDataSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TeamProject_MOU.h"
#include "Widgets/Input/SVirtualJoystick.h"

#include "Engine/GameInstance.h"

// 음성 RPC 창구. 컨트롤러는 음성 시스템의 내부를 몰라도 되지만,
// "모든 컨트롤러가 음성 창구를 하나씩 갖는다" 는 것은 컨트롤러의 책임이다
#include "Voice/VoiceComponent.h"

// 마이크/무전기 상태 표시. 로그인 위젯과 같은 이유로 여기서만 의존한다 -
// 위젯은 누가 자기를 띄우는지 몰라야 하고, 띄우는 정책은 컨트롤러 몫이다.
#include "Voice/RadioStatusWidget.h"
#include "Voice/VoiceStatusWidget.h"

ATeamProject_MOUPlayerController::ATeamProject_MOUPlayerController()
{
	// ★ 생성자에서 만들어야 서버와 클라이언트가 같은 컴포넌트를 갖는다.
	//   이유는 헤더의 VoiceComponent 주석 참고.
	VoiceComponent = CreateDefaultSubobject<UVoiceComponent>(TEXT("MOUVoiceComponent"));
}

void ATeamProject_MOUPlayerController::ServerSaveWarehouseDelivery_Implementation(
	const TArray<FStoredItemData>& RequestedItems)
{
	UWarehouseDataSubsystem* Warehouse = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UWarehouseDataSubsystem>() : nullptr;
	const bool bSucceeded = Warehouse && Warehouse->SavePendingDeliveryDataFromRequest(RequestedItems);
	ClientWarehouseDeliverySaveCompleted(bSucceeded);
}

void ATeamProject_MOUPlayerController::ClientWarehouseDeliverySaveCompleted_Implementation(bool bSucceeded)
{
	OnWarehouseDeliverySaveCompleted.Broadcast(bSucceeded);
}

void ATeamProject_MOUPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogTeamProject_MOU, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	ShowVoiceWidgetsIfNeeded();
}

void ATeamProject_MOUPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ATeamProject_MOUPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

// ---------------------------------------------------------------------------
// 마이크 / 무전기 상태 표시
//
// ★ ZOrder 를 로그인 위젯보다 낮게 둔다(기본 0). 로그인 화면이 떠 있는 동안
//   마이크 아이콘이 그 위를 덮으면 안 되기 때문이다.
//
// ★ 두 위젯 모두 "지금 상태" 를 스스로 폴링한다. 마이크가 없든 무전기가 없든
//   위젯 쪽에서 알아서 처리하므로(무전기는 스스로 접힌다), 여기서 조건을
//   따져 띄울지 말지 고르지 않는다 - 그 판단이 두 군데로 갈라지면 어긋난다.
// ---------------------------------------------------------------------------

void ATeamProject_MOUPlayerController::ShowVoiceWidgetsIfNeeded()
{
	// ★ 로컬 컨트롤러가 아니면 만들지 않는다. 서버가 남의 컨트롤러에도 만들면
	//   화면에는 안 보이는데 NativeTick 만 도는 위젯이 사람 수만큼 생긴다.
	if (!bAutoShowVoiceWidgets || !IsLocalPlayerController())
	{
		return;
	}

	// --- 마이크 -------------------------------------------------------------
	//
	// 이건 끌 수 있는 장식이 아니라 프라이버시 표시다(15절). 클래스를 안 넣어도
	// C++ 기본 레이아웃으로라도 반드시 뜬다.
	if (VoiceStatusWidget == nullptr)
	{
		UClass* WidgetClass = VoiceStatusWidgetClass
			? VoiceStatusWidgetClass.Get()
			: UVoiceStatusWidget::StaticClass();

		VoiceStatusWidget = CreateWidget<UVoiceStatusWidget>(this, WidgetClass);

		if (VoiceStatusWidget != nullptr)
		{
			VoiceStatusWidget->AddToViewport();
		}
		else
		{
			UE_LOG(LogTeamProject_MOU, Error,
				TEXT("마이크 상태 위젯을 만들지 못했다. VoiceStatusWidgetClass 가 UVoiceStatusWidget 을 상속하는지 확인할 것."));
		}
	}

	// --- 무전기 -------------------------------------------------------------
	//
	// 무전기가 없어도 띄운다. 위젯이 bHideWhenNoRadio 로 스스로 접히고,
	// 무전기를 줍는 순간 알아서 다시 나타난다 - 아이템을 줍고 버리는 시점마다
	// 여기서 만들고 부수면 그 타이밍을 놓치는 경로가 반드시 생긴다.
	if (RadioStatusWidget == nullptr)
	{
		UClass* WidgetClass = RadioStatusWidgetClass
			? RadioStatusWidgetClass.Get()
			: URadioStatusWidget::StaticClass();

		RadioStatusWidget = CreateWidget<URadioStatusWidget>(this, WidgetClass);

		if (RadioStatusWidget != nullptr)
		{
			RadioStatusWidget->AddToViewport();
		}
		else
		{
			UE_LOG(LogTeamProject_MOU, Error,
				TEXT("무전기 상태 위젯을 만들지 못했다. RadioStatusWidgetClass 가 URadioStatusWidget 을 상속하는지 확인할 것."));
		}
	}
}
