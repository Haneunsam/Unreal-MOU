#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/MOU_GameUserSettings.h"
#include "Voice/RadioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoicePlaybackGainTest, "MOU.Voice.PlaybackGain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// [VOICEVOLTEST-001] 200% 설정과 실제 부착된 무전기의 손/인벤토리/분리 상태별 출력을 검증한다.
bool FVoicePlaybackGainTest::RunTest(const FString& Parameters)
{
	UMOU_GameUserSettings* Settings = NewObject<UMOU_GameUserSettings>();
	TestEqual(TEXT("Default voice is 100 percent"), Settings->GetVoiceVolume(), 1.f);
	Settings->SetVoiceVolume(2.f);
	TestEqual(TEXT("200 percent is accepted"), Settings->GetVoiceVolume(), 2.f);
	Settings->SetVoiceVolume(3.f);
	TestEqual(TEXT("Above 200 percent is clamped"), Settings->GetVoiceVolume(), 2.f);
	Settings->SetVoiceVolume(0.f);
	TestEqual(TEXT("Zero mutes received voice"), Settings->GetVoiceVolume(), 0.f);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APawn* Holder = World->SpawnActor<APawn>();
	Holder->SetPlayerState(World->SpawnActor<APlayerState>());
	Holder->SetRootComponent(NewObject<USceneComponent>(Holder));
	Holder->GetRootComponent()->RegisterComponent();
	AActor* RadioActor = World->SpawnActor<AActor>();
	RadioActor->SetRootComponent(NewObject<USceneComponent>(RadioActor));
	RadioActor->GetRootComponent()->RegisterComponent();
	URadioComponent* Radio = NewObject<URadioComponent>(RadioActor);
	Radio->RegisterComponent();
	RadioActor->AttachToActor(Holder, FAttachmentTransformRules::KeepRelativeTransform);
	Radio->SetInHand(true);
	const float HeldRadius = Radio->GetEffectiveHearRadius();
	TestEqual(TEXT("Held radio uses full volume"), Radio->GetEffectiveSpeakerVolume(), 1.f);
	Radio->SetInHand(false);
	TestEqual(TEXT("Inventory radio uses half output"), Radio->GetEffectiveSpeakerVolume(), .5f);
	TestEqual(TEXT("Inventory radio retains reduced range"), Radio->GetEffectiveHearRadius(), HeldRadius * Radio->StowedRadiusScale);
	Radio->SetInHand(true);
	TestEqual(TEXT("Taking radio out restores output"), Radio->GetEffectiveSpeakerVolume(), 1.f);
	Radio->SetInHand(false);
	RadioActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	TestEqual(TEXT("Detached radio has no inventory volume penalty"), Radio->GetEffectiveSpeakerVolume(), 1.f);
	World->DestroyWorld(false);
	return true;
}
#endif
