// MOU 음성 - 무전기 상태 표시 위젯 구현.
// 대응하는 설계 문서: VOICE_INTEGRATION.md 7-3절, 14절 V6 / Radio.h 의 규칙표

#include "Voice/RadioStatusWidget.h"

#include "Voice/Radio.h"
#include "Voice/RadioComponent.h"
#include "Voice/VoiceComponent.h"
#include "Voice/VoiceSubsystem.h"
#include "Voice/VoiceTypes.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/InputComponent.h"
#include "Components/ProgressBar.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

// [RUI-010] 포커스를 가져가지 않는 무전기 위젯을 초기화한다.
URadioStatusWidget::URadioStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UI는 마우스나 키보드 포커스를 가져가지 않는다.
	SetIsFocusable(false);
}

// ---------------------------------------------------------------------------
// 수명 주기
// ---------------------------------------------------------------------------

// [RUI-002] 입력을 연결하고 상태 갱신 타이머를 시작한다.
void URadioStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UVoiceStatusWidget 과 같은 패턴: 별도 입력 에셋 없이 소유 플레이어의
	// InputComponent 에 직접 건다. EnhancedInput 이 붙어 있어도 레거시
	// InputComponent 는 함께 살아 있으므로 충돌하지 않는다.
	if (bBindKeysToOwningPlayer)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (PC->InputComponent != nullptr)
			{
				PC->InputComponent->BindKey(PowerToggleKey, IE_Pressed, this, &URadioStatusWidget::HandlePowerKeyPressed);

				// ★ PTT 는 누름과 뗌이 **둘 다** 필요하다. 뗌을 안 걸면 한 번
				//   누른 순간부터 영원히 송신 상태로 남는다.
				PC->InputComponent->BindKey(TransmitKey, IE_Pressed, this, &URadioStatusWidget::HandleTransmitKeyPressed);
				PC->InputComponent->BindKey(TransmitKey, IE_Released, this, &URadioStatusWidget::HandleTransmitKeyReleased);
			}
		}
	}

	bRadioStateApplied = false;
	RefreshRadioStatus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle, this, &URadioStatusWidget::RefreshRadioStatus,
			FMath::Max(RefreshInterval, 0.01f), true);
	}
}

// [RUI-003] 타이머와 입력을 해제하고 송신을 종료한다.
void URadioStatusWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	// ★ 사라지기 전에 송신을 끊는다. 위젯이 없어지면 X 를 뗀 것을 받을 수 없어서
	//   송신이 켜진 채로 영영 남는다.
	if (UVoiceSubsystem* Voice = GetVoiceSubsystem())
	{
		Voice->SetRadioTransmitting(false);
	}

	// 위젯이 사라진 뒤에도 키 바인딩이 남아 있으면 이미 파괴된 객체를 호출한다.
	// 이 위젯이 건 바인딩만 골라서 제거한다(UVoiceStatusWidget 과 동일한 이유).
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (PC->InputComponent != nullptr)
		{
			PC->InputComponent->KeyBindings.RemoveAll([this](const FInputKeyBinding& Binding)
			{
				return Binding.KeyDelegate.GetUObject() == this;
			});
		}
	}

	Super::NativeDestruct();
}

// [RUI-001] 숨겨진 상태에서도 무전기 상태와 배터리를 갱신한다.
void URadioStatusWidget::RefreshRadioStatus()
{
	ApplyRadioState(EvaluateRadioState());
	UpdateBatteryBar();
}

// ---------------------------------------------------------------------------
// 무전기 찾기
//
// ★ 매번 새로 찾는다. 캐시하면 무전기를 떨구거나 뺏긴 순간 죽은 포인터를
//   들고 있게 되고, 무엇보다 "지금 이 순간 무전기가 있는가" 자체가 이 UI 가
//   보여줘야 하는 정보다. 부착 액터 몇 개를 훑는 비용은 0.1초에 한 번이다.
// ---------------------------------------------------------------------------

APawn* URadioStatusWidget::GetLocalPawn() const
{
	APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPawn() : nullptr;
}

URadioComponent* URadioStatusWidget::FindLocalRadioComponent() const
{
	const APawn* Pawn = GetLocalPawn();

	if (!IsValid(Pawn))
	{
		return nullptr;
	}

	// 재귀로 훑는다. 손에 든 무전기는 캐릭터 메시의 소켓에 붙어 한 단계 더
	// 들어가 있을 수 있다 - 몇 단계인지 가정하지 않는다(ARadio::FindCarriedBy 와 동일).
	//
	// 인벤토리에 있는 것도 같이 잡힌다. AItemBase::OnUnequipped 가 플레이어에
	// 붙여두기 때문이다("투명한 주머니"). 그래서 인벤토리 구현이 바뀌어도
	// 여기가 안 깨진다.
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);

	for (AActor* Actor : Attached)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		// ★ 클래스가 아니라 컴포넌트로 찾는다. 진짜 아이템(ARadio)과 테스트
		//   무전기(AVoiceDebugRadio)의 공통분모가 이것뿐이라서다(헤더 상단 ★).
		if (URadioComponent* Comp = Actor->FindComponentByClass<URadioComponent>())
		{
			return Comp;
		}
	}

	return nullptr;
}

ARadio* URadioStatusWidget::FindLocalRadioItem() const
{
	URadioComponent* Comp = FindLocalRadioComponent();
	return Comp ? Cast<ARadio>(Comp->GetOwner()) : nullptr;
}

UVoiceSubsystem* URadioStatusWidget::GetVoiceSubsystem() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UVoiceSubsystem>() : nullptr;
}

// ---------------------------------------------------------------------------
// 입력 처리
// ---------------------------------------------------------------------------

// [RUI-006] 손에 든 무전기에만 전원 변경을 요청한다.
void URadioStatusWidget::HandlePowerKeyPressed()
{
	// ★ 캐시하지 않고 지금 다시 찾는다. 마지막 갱신(최대 0.1초 전) 이후에
	//   무전기를 떨궜을 수 있다.
	URadioComponent* Comp = FindLocalRadioComponent();

	if (Comp == nullptr || !Comp->IsInHand())
	{
		return; // 손에 들고 있을 때만 전원을 조작한다.
	}

	// 진짜 아이템은 자기 힘으로 서버까지 간다(ARadio::SetPowered 가 Server RPC 를 탄다).
	if (ARadio* Item = Cast<ARadio>(Comp->GetOwner()))
	{
		Item->TogglePower();

		// 다음 주기까지 기다리지 않고 즉시 반영. 키를 눌렀는데 최대 0.1초 동안
		// 화면이 그대로면 "안 눌렸나" 하고 한 번 더 누르게 된다.
		ApplyRadioState(EvaluateRadioState());
		return;
	}

	// 테스트 무전기(AVoiceDebugRadio)에는 그 경로가 없다. URadioComponent::SetPowered
	// 는 서버 전용이라 클라에서 부르면 아무 일도 안 일어나므로, 콘솔 명령이 쓰는
	// 것과 같은 디버그 RPC 로 돌린다.
	if (UVoiceSubsystem* Voice = GetVoiceSubsystem())
	{
		if (UVoiceComponent* VoiceComp = Voice->GetVoiceComponent())
		{
			VoiceComp->ServerDebugSetRadioPower(!Comp->IsPoweredOn());
		}
	}

	ApplyRadioState(EvaluateRadioState());
}

// [RUI-007] 송신 시작을 요청하고 상태 이미지를 갱신한다.
void URadioStatusWidget::HandleTransmitKeyPressed()
{
	// 손에 들었는지 / 켜져 있는지는 **여기서 막지 않는다.** 서버가
	// UVoiceRouter::FindUsableRadioFor 에서 다시 확인하므로, 클라에서 미리
	// 거르면 판정이 두 군데로 갈라져 어긋나기만 한다(ARadio::StartTransmit 주석).
	//
	// 상태 아이콘은 EvaluateRadioState에서 장착 여부까지 확인한다.
	if (ARadio* Item = FindLocalRadioItem())
	{
		Item->StartTransmit();
	}
	else if (FindLocalRadioComponent() != nullptr)
	{
		// 테스트 무전기. 송신은 어차피 로컬 상태라 서브시스템을 직접 켠다
		// (ARadio::StartTransmit 이 하는 일과 똑같다).
		if (UVoiceSubsystem* Voice = GetVoiceSubsystem())
		{
			Voice->SetRadioTransmitting(true);
		}
	}
	else
	{
		return; // 무전기가 아예 없다
	}

	ApplyRadioState(EvaluateRadioState());
}

// [RUI-008] 송신을 종료하고 상태 이미지를 갱신한다.
void URadioStatusWidget::HandleTransmitKeyReleased()
{
	// ★ 무전기를 못 찾아도 무조건 끊는다(헤더 주석). 누른 사이에 떨어뜨렸으면
	//   ARadio 를 못 찾는데, 그렇다고 안 끊으면 송신이 켜진 채로 남는다.
	if (UVoiceSubsystem* Voice = GetVoiceSubsystem())
	{
		Voice->SetRadioTransmitting(false);
	}

	ApplyRadioState(EvaluateRadioState());
}

// ---------------------------------------------------------------------------
// 상태 표시
// ---------------------------------------------------------------------------

namespace
{
	// 배터리 잔량이 20% 이하이면 경고 색으로 표시한다.
	constexpr float GBatteryLowThreshold = 0.2f;
}

// ---------------------------------------------------------------------------
// 상태 판정
//
// 전원과 장착, 송수신 상태를 판정하고 표현은 ApplyRadioState에서 처리한다.
// ---------------------------------------------------------------------------

// [RUI-005] 전원과 장착 여부를 확인하여 송수신 상태를 판정한다.
ERadioIconState URadioStatusWidget::EvaluateRadioState() const
{
	const URadioComponent* Comp = FindLocalRadioComponent();

	if (Comp == nullptr)
	{
		return ERadioIconState::None;
	}

	if (!Comp->IsPoweredOn())
	{
		// ★ 꺼져 있으면 송신도 수신도 없다. 전원을 먼저 보는 이유다 -
		//   꺼진 무전기에 "수신 중" 이 뜨면 그 자체로 거짓말이다.
		return ERadioIconState::Off;
	}

	const UVoiceSubsystem* Voice = GetVoiceSubsystem();

	if (Voice == nullptr)
	{
		return ERadioIconState::On;
	}

	// ★ 송신이 수신을 이긴다(VoiceTypes.h 의 ERadioIconState 주석).
	//   둘 다 성립할 때 놓치면 안 되는 쪽은 송신이다 - 이 게임에서 송신은
	//   곧 내 위치가 새는 것이라, 켜진 줄 모르는 편이 훨씬 위험하다.
	//
	// 수납 중 송신 요청은 서버에서 거절되므로 송신 아이콘도 표시하지 않는다.
	if (Comp->IsInHand() && Voice->IsRadioTransmitting())
	{
		return ERadioIconState::Transmitting;
	}

	if (Voice->IsReceivingRadio())
	{
		return ERadioIconState::Receiving;
	}

	return ERadioIconState::On;
}

// ---------------------------------------------------------------------------
// 상태 -> 아이콘
// ---------------------------------------------------------------------------

// [RUI-004] 본체 전원 색과 상태 이미지, 위젯 가시성을 적용한다.
void URadioStatusWidget::ApplyRadioState(ERadioIconState NewState)
{
	// ★ 안 바뀌었으면 아무것도 안 한다. 매 주기 다시 넣으면 OnRadioStateChanged
	//   가 계속 불려서 WBP 애니메이션이 첫 프레임에서 되감긴다.
	if (bRadioStateApplied && NewState == CachedState)
	{
		return;
	}

	const ERadioIconState OldState = CachedState;

	CachedState        = NewState;
	bRadioStateApplied = true;

	// ★ 무전기가 없으면 위젯째로 접는다. 마이크와 다르다 - 무전기를 안 가진
	//   것은 정상 상태라 화면을 차지할 이유가 없다.
	//   숨겨져도 월드 타이머가 소지 여부를 확인한다.
	if (bHideWhenNoRadio)
	{
		SetVisibility(NewState == ERadioIconState::None
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	const bool bOff = NewState == ERadioIconState::Off;
	const FLinearColor PowerTint = bOff
		? FLinearColor(0.55f, 0.55f, 0.55f) : FLinearColor::White;

	if (RadioIcon != nullptr)
	{
		RadioIcon->SetColorAndOpacity(PowerTint);
	}

	if (StatusIcon != nullptr)
	{
		const ERadioIconState IconState = bOff ? ERadioIconState::On : NewState;
		const FSlateBrush* Brush = IconBrushes.Find(IconState);
		StatusIcon->SetBrush(Brush ? *Brush : FSlateBrush());
		const FLinearColor* Tint = IconTints.Find(NewState);
		StatusIcon->SetColorAndOpacity(!bOff && Tint ? *Tint : PowerTint);
	}

	OnRadioStateChanged(NewState, OldState);
}

// ---------------------------------------------------------------------------
// 배터리 바
// ---------------------------------------------------------------------------

// [RUI-009] 실제 배터리 잔량과 부족 경고 색을 표시한다.
void URadioStatusWidget::UpdateBatteryBar()
{
	// ★ 배터리는 ARadio 에만 있다. CurrentDurability 가 AItemBase 의 것이라
	//   테스트 무전기(AVoiceDebugRadio)에는 아예 없다.
	const ARadio* Item = FindLocalRadioItem();

	bBatteryLow = (Item != nullptr) && (Item->GetBatteryPercent() <= GBatteryLowThreshold);

	if (BatteryBar == nullptr)
	{
		return; // BatteryBar가 없는 WBP에서는 게이지를 표시하지 않는다.
	}

	if (Item == nullptr)
	{
		// 테스트 무전기는 배터리 개념 자체가 없다. 0% 로 그리면 "방전됨" 으로
		// 잘못 읽히므로 바를 아예 접는다.
		BatteryBar->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 보간하지 않는다. 배터리는 초당 몇 퍼센트씩 천천히 줄어드는 값이라
	// 0.1초 간격으로 그대로 넣어도 이미 부드럽다 - 보간을 걸면 오히려 실제
	// 잔량보다 늦게 따라와서 "곧 꺼진다" 를 늦게 알리게 된다.
	BatteryBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	BatteryBar->SetPercent(Item->GetBatteryPercent());

	// 색만 여기서 바꾼다. 깜빡임 같은 연출은 WBP 가 IsBatteryLow 로 건다.
	BatteryBar->SetFillColorAndOpacity(bBatteryLow
		? FLinearColor(0.95f, 0.3f, 0.3f)
		: FLinearColor(0.3f, 1.f, 0.3f));
}

// ---------------------------------------------------------------------------
// 콘솔 명령 - UVoiceStatusWidget 의 MOU.Voice.ShowUI 와 같은 패턴.
//
// 실제 게임에서는 무전기를 처음 주웠을 때 등에서
// CreateWidget<URadioStatusWidget>(PC, ...) -> AddToViewport() 하면 된다.
// 일반 게임에서는 PlayerController가 생성하며, 콘솔은 진단용으로 사용한다.
// ---------------------------------------------------------------------------

namespace
{
	/** 월드마다 따로 기억한다. PIE 다중 창에서 창별로 독립 검증하기 위함. */
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<URadioStatusWidget>> GDebugRadioStatusWidgets;

	void PruneDebugRadioStatusWidgets()
	{
		for (auto It = GDebugRadioStatusWidgets.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || !It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	URadioStatusWidget* FindDebugRadioStatusWidget(UWorld* World)
	{
		PruneDebugRadioStatusWidgets();

		if (const TWeakObjectPtr<URadioStatusWidget>* Found = GDebugRadioStatusWidgets.Find(World))
		{
			if (URadioStatusWidget* Widget = Found->Get())
			{
				return Widget;
			}
		}

		// ★ 이 맵만 보면 **콘솔이 만든 것밖에 못 찾는다.**
		//   지금은 ATeamProject_MOUPlayerController::BeginPlay 가 시작할 때
		//   이미 하나 띄우므로, 그것을 못 보면 ShowUI 를 칠 때마다 위젯이 하나씩
		//   더 쌓여 화면에 글자가 겹쳐 보인다. 뷰포트에 있는 것까지 훑는다.
		if (World == nullptr)
		{
			return nullptr;
		}

		TArray<UUserWidget*> InViewport;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, InViewport, URadioStatusWidget::StaticClass(), /*TopLevelOnly=*/false);

		return InViewport.Num() > 0 ? Cast<URadioStatusWidget>(InViewport[0]) : nullptr;
	}

	/**
	 * ★ 하나의 토글 명령으로 둔다(MOU.Voice.Mute / Codec 과 같은 방식).
	 *
	 *   Show/Hide 두 개로 나눌 수도 있지만, 이 위젯은 Z/X 를 **바인딩까지 하므로**
	 *   껐다 켜는 일이 잦다 - 다른 키 테스트를 하려면 잠깐 꺼야 한다. 토글이 낫다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GRadioUICommand(
		TEXT("MOU.Voice.RadioUI"),
		TEXT("무전기 상태 위젯을 켜고 끈다(이미지·배터리 표시 + Z/X 조작). ")
		TEXT("인자 없으면 토글. 사용법: [0|1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World)
			{
				URadioStatusWidget* Existing = FindDebugRadioStatusWidget(World);

				const bool bWantOn = Args.IsValidIndex(0)
					? (FCString::Atoi(*Args[0]) != 0)
					: (Existing == nullptr);

				if (!bWantOn)
				{
					if (Existing != nullptr)
					{
						Existing->RemoveFromParent(); // NativeDestruct 가 송신을 끊고 키를 푼다
					}
					GDebugRadioStatusWidgets.Remove(World);
					return;
				}

				if (Existing != nullptr)
				{
					return; // 이 창에는 이미 떠 있다
				}

				APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
				if (PC == nullptr)
				{
					return;
				}

				if (URadioStatusWidget* Widget = CreateWidget<URadioStatusWidget>(PC, URadioStatusWidget::StaticClass()))
				{
					Widget->AddToViewport();
					GDebugRadioStatusWidgets.Add(World, Widget);

					UE_LOG(LogMOUVoice, Log,
						TEXT("무전기 UI 를 띄웠다. Z = 전원, X 홀드 = 송신. ")
						TEXT("무전기가 없으면 MOU.Voice.Radio.Spawn 으로 먼저 들 것."));
				}
			}));
}
