// MOU 음성 - 마이크 상태 표시 위젯 구현.
// 대응하는 설계 문서: VOICE_INTEGRATION.md 15절(프라이버시), 7-1절(C 키)

#include "Voice/VoiceStatusWidget.h"

#include "Voice/VoiceSubsystem.h"
#include "Voice/VoiceTypes.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/InputComponent.h"
#include "Components/ProgressBar.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

// [VUI-003] 마이크 표시 위젯이 입력 포커스를 가져가지 않도록 설정한다.
UVoiceStatusWidget::UVoiceStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

// ---------------------------------------------------------------------------
// 수명 주기
// ---------------------------------------------------------------------------

// [VUI-004] 음소거 입력을 연결하고 아이콘과 음량 바를 초기화한다.
void UVoiceStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// ChatWidgetBase 와 같은 패턴: 별도 입력 에셋 없이 소유 플레이어의
	// InputComponent 에 직접 건다. EnhancedInput 이 붙어 있어도 레거시
	// InputComponent 는 함께 살아있으므로 충돌하지 않는다.
	if (bBindMuteKeyToOwningPlayer)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (PC->InputComponent != nullptr)
			{
				PC->InputComponent->BindKey(MuteToggleKey, IE_Pressed, this, &UVoiceStatusWidget::HandleMuteKeyPressed);
			}
		}
	}

	bMicStateApplied = false;
	DisplayLevel = 0.f;
	ApplyMicState(EvaluateMicState());
	UpdateLevelBar(0.f);
}

// [VUI-005] 위젯이 등록한 음소거 키 바인딩을 제거한다.
void UVoiceStatusWidget::NativeDestruct()
{
	// 위젯이 사라진 뒤에도 키 바인딩이 남아있으면 이미 파괴된 객체를 호출한다.
	// 이 위젯이 건 바인딩만 골라서 제거한다(ChatWidgetBase 와 동일한 이유).
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

// [VUI-006] 매 프레임 마이크 상태를 확인하고 음량 바를 갱신한다.
void UVoiceStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ApplyMicState(EvaluateMicState());
	UpdateLevelBar(InDeltaTime);
}

// ---------------------------------------------------------------------------
// 입력 처리
// ---------------------------------------------------------------------------

// [VUI-007] 음소거를 전환하고 아이콘을 즉시 갱신한다.
void UVoiceStatusWidget::HandleMuteKeyPressed()
{
	if (UVoiceSubsystem* Voice = GetVoiceSubsystem())
	{
		Voice->ToggleMute();

		// 입력 직후 아이콘과 음량 바에 반영한다.
		ApplyMicState(EvaluateMicState());
		UpdateLevelBar(0.f);
	}
}

// ---------------------------------------------------------------------------
// 상태 표시
// ---------------------------------------------------------------------------

namespace
{
	/**
	 * 음량 바가 **올라갈** 때의 보간 속도. 사실상 즉시다.
	 *
	 * ★ 올라가는 것을 늦추면 말을 시작하고 나서야 바가 따라온다 - 그러면
	 *   "내가 낸 소리" 와 "화면" 이 어긋나 보여서 게이지를 못 믿게 된다.
	 */
	constexpr float GLevelAttackSpeed = 30.f;

	/**
	 * 음량 바가 **내려갈** 때의 보간 속도. 일부러 느리다.
	 *
	 * ★ 사람 목소리는 음절 사이에서 순간순간 0 에 가까워진다. 내려가는 것도
	 *   빠르게 두면 말하는 내내 바가 발작하듯 떨려서 최고 음량을 읽을 수 없다.
	 *   천천히 내려오게 두면 방금 얼마나 크게 말했는지가 눈에 남는다.
	 */
	constexpr float GLevelReleaseSpeed = 8.f;

	// [VUI-012] 발화와 특수 상태에 사용할 기본 아이콘 색상을 반환한다.
	FLinearColor GetDefaultMicTint(EMicIconState State)
	{
		switch (State)
		{
		case EMicIconState::Dead:        return FLinearColor(0.9f,  0.25f, 0.25f);
		case EMicIconState::NoDevice:    return FLinearColor(0.5f,  0.5f,  0.5f);
		case EMicIconState::Calibrating: return FLinearColor(1.f,   0.85f, 0.3f);
		case EMicIconState::Muted:       return FLinearColor(0.6f,  0.6f,  0.6f);
		case EMicIconState::Speaking:    return FLinearColor(0.3f,  1.f,   0.3f);
		case EMicIconState::Idle:
		default:                         return FLinearColor(0.8f,  0.8f,  0.8f);
		}
	}
}

// ---------------------------------------------------------------------------
// 상태 판정
//
// 아이콘과 음량 바는 이 판정 결과를 함께 사용한다.
// ---------------------------------------------------------------------------

// [VUI-009] 장치와 플레이어의 음성 상태를 아이콘 상태 하나로 판정한다.
EMicIconState UVoiceStatusWidget::EvaluateMicState() const
{
	const UVoiceSubsystem* Voice = GetVoiceSubsystem();

	if (Voice == nullptr)
	{
		return EMicIconState::NoDevice;
	}

	// ★ 아래 순서가 곧 우선순위다. 위에 있을수록 "그 아래를 봐도 소용없는" 상태다.

	// 사망이 가장 먼저다. 마이크가 있든 없든, 음소거든 아니든 결과가 같다.
	// 이걸 먼저 보지 않으면 "왜 아무도 내 말을 안 듣지" 로 한참 헤맨다.
	if (Voice->IsVoiceDead())
	{
		return EMicIconState::Dead;
	}

	if (!Voice->IsCaptureReady())
	{
		return EMicIconState::NoDevice;
	}

	// 보정 중에는 다른 것을 보여줄 이유가 없다. "지금 조용히 해야 한다" 가 전부다.
	if (Voice->IsCalibrating())
	{
		return EMicIconState::Calibrating;
	}

	if (Voice->IsMuted())
	{
		return EMicIconState::Muted;
	}

	return Voice->IsSpeaking() ? EMicIconState::Speaking : EMicIconState::Idle;
}

// ---------------------------------------------------------------------------
// 상태 -> 아이콘
// ---------------------------------------------------------------------------

// [VUI-001] 마이크 상태에 따라 텍스처와 색상을 적용하고 상태 변경을 알린다.
void UVoiceStatusWidget::ApplyMicState(EMicIconState NewState)
{
	// ★ 안 바뀌었으면 아무것도 안 한다. 브러시를 매번 다시 넣으면 Slate 가
	//   매번 무효화되고, 무엇보다 OnMicStateChanged 가 매 주기 불려서 WBP
	//   애니메이션이 첫 프레임에서 계속 되감긴다.
	if (bMicStateApplied && NewState == CachedState)
	{
		return;
	}

	const EMicIconState OldState = CachedState;

	CachedState      = NewState;
	bMicStateApplied = true;

	if (MicIcon != nullptr)
	{
		UTexture2D* Texture = (NewState == EMicIconState::Muted)
			? MutedMicTexture.Get()
			: NormalMicTexture.Get();

		// 위젯 크기를 유지하며, 텍스처가 없으면 이전 상태의 이미지를 지운다.
		MicIcon->SetBrushFromTexture(Texture, false);
		MicIcon->SetBrushTintColor(FSlateColor(FLinearColor::White));

		const bool bOriginalColor = NewState == EMicIconState::Idle
			|| NewState == EMicIconState::Muted;
		MicIcon->SetColorAndOpacity(bOriginalColor
			? FLinearColor::White
			: GetDefaultMicTint(NewState));
	}

	OnMicStateChanged(NewState, OldState);
}

// ---------------------------------------------------------------------------
// 음량 바
// ---------------------------------------------------------------------------

// [VUI-002] 발화 기준을 넘은 음량을 게이지로 표시하고 비발화 상태에서는 0으로 초기화한다.
void UVoiceStatusWidget::UpdateLevelBar(float InDeltaTime)
{
	if (LevelBar == nullptr)
	{
		return;
	}

	const UVoiceSubsystem* Voice = GetVoiceSubsystem();

	LevelBar->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 아이콘이 초록색으로 표시되는 발화 상태에서만 게이지를 사용한다.
	if (Voice == nullptr || CachedState != EMicIconState::Speaking)
	{
		DisplayLevel = 0.f;
		LevelBar->SetPercent(0.f);
		return;
	}

	const float Threshold = FMath::Max(Voice->GetMicSensitivity(), 0.f);
	const float Ceiling = FMath::Max(
		Threshold * LevelBarHeadroom, Threshold + KINDA_SMALL_NUMBER);

	// 발화 기준 이하는 0, 상한 이상은 1로 환산한다.
	const float Target = FMath::Clamp(
		(Voice->GetLoudnessEnvelope() - Threshold) / (Ceiling - Threshold), 0.f, 1.f);

	// 비대칭 보간: 올라갈 땐 빠르게, 내려올 땐 천천히.
	const float Speed = (Target > DisplayLevel) ? GLevelAttackSpeed : GLevelReleaseSpeed;

	DisplayLevel = FMath::FInterpTo(DisplayLevel, Target, InDeltaTime, Speed);

	// 보간 끝의 작은 잔여값을 없애고 양 끝값에 정확히 도달시킨다.
	if (FMath::IsNearlyEqual(DisplayLevel, Target, 0.001f))
	{
		DisplayLevel = Target;
	}

	LevelBar->SetPercent(DisplayLevel);
}

// [VUI-008] 소유 로컬 플레이어의 음성 서브시스템을 조회한다.
UVoiceSubsystem* UVoiceStatusWidget::GetVoiceSubsystem() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UVoiceSubsystem>() : nullptr;
}

// ---------------------------------------------------------------------------
// 콘솔 명령 - ChatWidgetBase 의 MOU.Chat.ShowUI 와 정확히 같은 패턴.
//
// 실제 표시에는 MicIcon과 LevelBar가 배치된 WBP가 필요하다.
// C++ 기본 클래스만 생성하면 텍스트를 포함한 대체 UI는 표시하지 않는다.
// ---------------------------------------------------------------------------

namespace
{
	/** 월드마다 따로 기억한다. PIE 다중 창에서 창별로 독립적으로 검증하기 위함(ChatWidgetBase 와 동일 이유). */
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UVoiceStatusWidget>> GDebugVoiceStatusWidgets;

	void PruneDebugVoiceStatusWidgets()
	{
		for (auto It = GDebugVoiceStatusWidgets.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || !It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	UVoiceStatusWidget* FindDebugVoiceStatusWidget(UWorld* World)
	{
		PruneDebugVoiceStatusWidgets();

		if (const TWeakObjectPtr<UVoiceStatusWidget>* Found = GDebugVoiceStatusWidgets.Find(World))
		{
			if (UVoiceStatusWidget* Widget = Found->Get())
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
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, InViewport, UVoiceStatusWidget::StaticClass(), /*TopLevelOnly=*/false);

		return InViewport.Num() > 0 ? Cast<UVoiceStatusWidget>(InViewport[0]) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GVoiceShowUICommand(
		TEXT("MOU.Voice.ShowUI"),
		TEXT("마이크 상태 표시 위젯을 화면에 띄운다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World)
			{
				if (FindDebugVoiceStatusWidget(World) != nullptr)
				{
					return; // 이 창에는 이미 떠 있다
				}

				APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
				if (PC == nullptr)
				{
					return;
				}

				if (UVoiceStatusWidget* Widget = CreateWidget<UVoiceStatusWidget>(PC, UVoiceStatusWidget::StaticClass()))
				{
					Widget->AddToViewport();
					GDebugVoiceStatusWidgets.Add(World, Widget);
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GVoiceHideUICommand(
		TEXT("MOU.Voice.HideUI"),
		TEXT("마이크 상태 표시 위젯을 화면에서 제거한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World)
			{
				if (UVoiceStatusWidget* Widget = FindDebugVoiceStatusWidget(World))
				{
					Widget->RemoveFromParent();
				}
				GDebugVoiceStatusWidgets.Remove(World);
			}));
}
