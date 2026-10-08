#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/ServerSubsystem.h"
#include "Server/MOULocalPlayer.h"
#include "Server/MOUOnlineSession.h"
#include "Base/ProjectGameInstanceBase.h"
#include "Server/Lobby/HostDisconnectedWidget.h"
#include "TeamProject_MOUGameMode.h"
#include "TeamProject_MOUPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Player/MainCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLateJoinRegressionTest, "MOU.Rejoin.SafeLobbyAndHostLoss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
// [LATEJOIN-013] 안전구역 승인, 재접속 격리, 관전 스폰 차단과 호스트 종료 확인 흐름을 검증합니다.
bool FLateJoinRegressionTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> AllowActorEvents(GAllowActorScriptExecutionInEditor, true);
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    UServerSubsystem* Server = GI->GetSubsystem<UServerSubsystem>();
    const FString First = Server->GetPlaySessionId();
    TestEqual(TEXT("Session survives ordinary map travel"), Server->GetPlaySessionId(), First);
    TestFalse(TEXT("New raid join waits"), Server->AdmitPlaySession(First, false));
    TestTrue(TEXT("Safe lobby activates player"), Server->AdmitPlaySession(First, true));
    TestTrue(TEXT("Existing participant stays active on raid travel"), Server->AdmitPlaySession(First, false));
    Server->ClearRoomState();
    const FString Rejoined = Server->GetPlaySessionId();
    TestNotEqual(TEXT("Leaving room rotates session"), Rejoined, First);
    TestFalse(TEXT("Rejoin during mission waits"), Server->AdmitPlaySession(Rejoined, false));
    TestFalse(TEXT("Room reset clears admissions"), Server->AdmitPlaySession(First, false));
    TestTrue(TEXT("Safe return activates rejoined player"), Server->AdmitPlaySession(Rejoined, true));
    TestFalse(TEXT("Missing session cannot bypass mission restriction"), Server->AdmitPlaySession(FString(), false));
    TestTrue(TEXT("Configured local player supplies session option"), GEngine->LocalPlayerClass->IsChildOf(UMOULocalPlayer::StaticClass()));

    UWorld* World = GI->GetWorld();
    UClass* ModeClass = LoadClass<ATeamProject_MOUGameMode>(nullptr,
        TEXT("/Game/01_LDJ/GameSystem/BP/BP_Gamemode.BP_Gamemode_C"));
    if (!TestNotNull(TEXT("Production game mode class"), ModeClass)) { GI->Shutdown(); return false; }
    auto* Mode = World->SpawnActor<ATeamProject_MOUGameMode>(ModeClass);
    auto* PC = World->SpawnActor<ATeamProject_MOUPlayerController>(Mode->PlayerControllerClass);
    if (!TestNotNull(TEXT("Production player controller"), PC)) { GI->Shutdown(); return false; }
    auto* LP = NewObject<UMOULocalPlayer>(GEngine);
    LP->PlayerController = PC; PC->Player = LP;
    PC->SetAsLocalPlayerController();
    if (!PC->PlayerState) PC->PlayerState = World->SpawnActor<APlayerState>();
    if (!PC->PlayerCameraManager)
    {
        PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
        PC->PlayerCameraManager->InitializeFor(PC);
    }
    PC->bWaitForSafeLobby = true;
    PC->PlaySessionId = Rejoined;
    Mode->LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Test/LobbyLevel.LobbyLevel")));
    Mode->GenericPlayerInitialization(PC);
    TestTrue(TEXT("Mission join is spectator only on authority"), PC->PlayerState->IsOnlyASpectator());
    Mode->RestartPlayer(PC);
    TestNull(TEXT("Mission spectator cannot spawn gameplay pawn"), PC->GetPawn());
    PC->StartSpectating();
    TestTrue(TEXT("No pawn and no targets still enters spectator mode"), PC->IsSpectating());
    PC->SpectateNextPlayer();
    PC->SpectatePrevPlayer();
    TestTrue(TEXT("Missing replicated targets retains spectator mode"), PC->IsSpectating());
    PC->StopSpectating();
    auto* TeammateController = World->SpawnActor<APlayerController>();
    auto* Teammate = World->SpawnActor<AMainCharacter>();
    if (!TeammateController->PlayerState) TeammateController->PlayerState = World->SpawnActor<APlayerState>();
    TeammateController->Possess(Teammate);
    TestTrue(TEXT("Fixture teammate has human player state"), Teammate->IsPlayerControlled());
    Teammate->SetActorLocation(FVector(100000.f, 0.f, 0.f));
    TestEqual(TEXT("Fixture exposes one living teammate"), PC->GetAliveTeammates().Num(), 1);
    PC->ServerCycleLateJoinTarget(1);
    TestEqual(TEXT("Server follows distant teammate for network relevancy"), PC->GetViewTarget(), static_cast<AActor*>(Teammate));
    auto* OtherController = World->SpawnActor<APlayerController>();
    auto* Other = World->SpawnActor<AMainCharacter>();
    if (!OtherController->PlayerState) OtherController->PlayerState = World->SpawnActor<APlayerState>();
    OtherController->Possess(Other);
    Teammate->bIsDead = true;
    PC->ServerCycleLateJoinTarget(1);
    TestEqual(TEXT("Dead observer target is replaced with living teammate"), PC->GetViewTarget(), static_cast<AActor*>(Other));
    PC->bWaitForSafeLobby = false;
    Teammate->bIsDead = false;
    PC->ServerCycleLateJoinTarget(1);
    TestEqual(TEXT("Active players cannot use late-join camera RPC"), PC->GetViewTarget(), static_cast<AActor*>(Other));
    PC->bWaitForSafeLobby = true;
    TeammateController->Destroy(); OtherController->Destroy(); Teammate->Destroy(); Other->Destroy();
    auto* Next = World->SpawnActor<ATeamProject_MOUPlayerController>(Mode->PlayerControllerClass);
    PC->SeamlessTravelTo(Next);
    TestTrue(TEXT("Seamless replacement preserves restriction"), Next->bWaitForSafeLobby);
    TestEqual(TEXT("Seamless replacement preserves session"), Next->PlaySessionId, Rejoined);
    const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
    Mode->LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Test/") + Map + TEXT(".") + Map));
    TestTrue(TEXT("Configured safe map detected"), Mode->IsLobbyLevel());
    Mode->GenericPlayerInitialization(PC);
    TestFalse(TEXT("Safe return removes wait flag"), PC->bWaitForSafeLobby);
    TestFalse(TEXT("Safe return removes spectator-only spawn gate"), PC->PlayerState->IsOnlyASpectator());
    TestTrue(TEXT("Safe return permits player restart"), Mode->PlayerCanRestart(PC));

    Server->ConnectionState = EChatConnectionState::LoggedIn;
    Server->CurrentRoomId = 42;
    Server->NotifyHostDisconnected();
    TestTrue(TEXT("Host loss waits for confirmation"), Server->IsHostDisconnectPending());
    TestFalse(TEXT("No automatic lobby request before confirmation"), Server->bReturnToLobbyAfterFailure);
    TestEqual(TEXT("Closed room is cleared even without backend"), Server->GetCurrentRoomId(), 0);
    TestEqual(TEXT("Host loss preserves login"), Server->GetConnectionState(), EChatConnectionState::LoggedIn);
    Server->NotifyHostDisconnected();
    TestFalse(TEXT("Duplicate host loss cannot bypass confirmation"), Server->bReturnToLobbyAfterFailure);
    auto* Modal = CreateWidget<UHostDisconnectedWidget>(GI, UHostDisconnectedWidget::StaticClass());
    TestTrue(TEXT("Native host loss modal builds without BP"), Modal->TakeWidget()->GetChildren()->Num() > 0);
    TestEqual(TEXT("Production game instance uses deferred disconnect session"),
        GetMutableDefault<UProjectGameInstanceBase>()->GetOnlineSessionClass().Get(), UMOUOnlineSession::StaticClass());
    auto* Session = NewObject<UMOUOnlineSession>(GI);
    FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
    const FString TravelBefore = Context->TravelURL;
    Session->HandleDisconnect(World, nullptr);
    TestEqual(TEXT("Engine disconnect does not queue default map before confirmation"), Context->TravelURL, TravelBefore);
    Server->ConfirmHostDisconnected();
    TestFalse(TEXT("Confirmation dismisses alert"), Server->IsHostDisconnectPending());
    TestTrue(TEXT("Confirmation schedules MainLobby travel"), Server->bReturnToLobbyAfterFailure);
    Server->bReturnToLobbyAfterFailure = false;
    PC->Destroy(); Next->Destroy(); Mode->Destroy();
    GI->Shutdown();
    return true;
}
#endif
