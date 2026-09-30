#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/Lobby/LobbyPageWidgetBase.h"
#include "Server/Lobby/LobbyBackend.h"
#include "Server/ServerSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

class FRoomTitleTestBackend : public ILobbyBackend
{
public:
    TArray<FServerClientEvent> Events;
    FString SentTitle;
    // [RTITLE-010] 외부 통신 없이 제목 전달 테스트의 GetBackendName 호출을 처리한다.
    virtual FString GetBackendName() const override { return TEXT("TitleTest"); }
    // [RTITLE-011] 외부 통신 없이 제목 전달 테스트의 SupportsChat 호출을 처리한다.
    virtual bool SupportsChat() const override { return false; }
    // [RTITLE-012] 외부 통신 없이 제목 전달 테스트의 Start 호출을 처리한다.
    virtual bool Start(const FString& Host, int32 Port) override { return false; }
    // [RTITLE-013] 외부 통신 없이 제목 전달 테스트의 Shutdown 호출을 처리한다.
    virtual void Shutdown() override {  }
    // [RTITLE-014] 외부 통신 없이 제목 전달 테스트의 IsRunning 호출을 처리한다.
    virtual bool IsRunning() const override { return false; }
    // [RTITLE-015] 외부 통신 없이 제목 전달 테스트의 SendLogin 호출을 처리한다.
    virtual void SendLogin(const FString& LoginId, const FString& Password, int32 TeamId) override {  }
    // [RTITLE-016] 외부 통신 없이 제목 전달 테스트의 SendCheckLoginId 호출을 처리한다.
    virtual bool SendCheckLoginId(uint32 RequestId, const FString& LoginId) override { return false; }
    // [RTITLE-017] 외부 통신 없이 제목 전달 테스트의 SendRegister 호출을 처리한다.
    virtual void SendRegister(const FString& LoginId, const FString& Password, const FString& Nickname) override {  }
    // [RTITLE-018] 외부 통신 없이 제목 전달 테스트의 SendChat 호출을 처리한다.
    virtual void SendChat(EChatChannelBP Channel, const FString& Text) override {  }
    // [RTITLE-019] 외부 통신 없이 제목 전달 테스트의 SendSetDead 호출을 처리한다.
    virtual void SendSetDead(int64 UserId, bool bDead) override {  }
    // [RTITLE-020] 외부 통신 없이 제목 전달 테스트의 CreateRoom 호출을 처리한다.
    virtual void CreateRoom(const FString& Title, const FString& RoomPassword, int32 HostPort,
	                        const FString& LanAddress) override { SentTitle = Title; }
    // [RTITLE-021] 외부 통신 없이 제목 전달 테스트의 RequestRoomList 호출을 처리한다.
    virtual void RequestRoomList() override {  }
    // [RTITLE-022] 외부 통신 없이 제목 전달 테스트의 JoinRoom 호출을 처리한다.
    virtual void JoinRoom(int32 RoomId, const FString& RoomPassword) override {  }
    // [RTITLE-023] 외부 통신 없이 제목 전달 테스트의 LeaveRoom 호출을 처리한다.
    virtual void LeaveRoom() override {  }
    // [RTITLE-024] 외부 통신 없이 제목 전달 테스트의 SetCustomization 호출을 처리한다.
    virtual bool SetCustomization(int32 RoomId, uint32 RequestId, const FCharacterCustomizationData& Data) { return false; }
	// [RTITLE-040] 테스트에서는 준비 상태 요청을 외부로 전송하지 않는다.
    virtual void SetReady(bool bReady) override {}
    // [RTITLE-025] 외부 통신 없이 제목 전달 테스트의 StartGame 호출을 처리한다.
    virtual void StartGame() override {  }
    // [RTITLE-026] 외부 통신 없이 제목 전달 테스트의 RequestFriendList 호출을 처리한다.
    virtual void RequestFriendList() override {  }
    // [RTITLE-027] 외부 통신 없이 제목 전달 테스트의 AddFriend 호출을 처리한다.
    virtual void AddFriend(const FString& Query) override {  }
    // [RTITLE-028] 외부 통신 없이 제목 전달 테스트의 RespondFriendRequest 호출을 처리한다.
    virtual void RespondFriendRequest(int64 FromUserId, bool bAccept) override {  }
    // [RTITLE-029] 외부 통신 없이 제목 전달 테스트의 RemoveFriend 호출을 처리한다.
    virtual void RemoveFriend(int64 TargetUserId) override {  }
    // [RTITLE-030] 외부 통신 없이 제목 전달 테스트의 SendDirectMessage 호출을 처리한다.
    virtual void SendDirectMessage(int64 TargetUserId, const FString& Text) override {  }
    // [RTITLE-031] 외부 통신 없이 제목 전달 테스트의 RequestDmHistory 호출을 처리한다.
    virtual void RequestDmHistory(int64 PeerUserId, int64 BeforeMessageId) override {  }
    // [RTITLE-032] 외부 통신 없이 제목 전달 테스트의 NotifyHostReady 호출을 처리한다.
    virtual void NotifyHostReady() override {  }
    // [RTITLE-033] 외부 통신 없이 제목 전달 테스트의 RequestHostProbe 호출을 처리한다.
    virtual void RequestHostProbe(int32 Port, uint32 Nonce) override {  }
    // [RTITLE-034] 외부 통신 없이 제목 전달 테스트의 ReportReachability 호출을 처리한다.
    virtual void ReportReachability(bool bReachable) override {  }
    // [RTITLE-035] 외부 통신 없이 제목 전달 테스트의 UpdateRoomState 호출을 처리한다.
    virtual void UpdateRoomState(int32 RoomId, int32 CurrentPlayers, bool bInGame) override {  }
    // [RTITLE-036] 외부 통신 없이 제목 전달 테스트의 DequeueEvent 호출을 처리한다.
    virtual bool DequeueEvent(FServerClientEvent& Out) override { if (Events.IsEmpty()) return false; Out = Events[0]; Events.RemoveAt(0); return true; }
    // [RTITLE-037] 외부 통신 없이 제목 전달 테스트의 DequeueMessage 호출을 처리한다.
    virtual bool DequeueMessage(FChatMessage& Out) override { return false; }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomLobbyTitleRegressionTest, "MOU.RoomLobby.TitleRegression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
// [RTITLE-006] 실제 WBP에 생성·참여 제목이 반영되고 실패·퇴장 시 이전 제목이 남지 않는지 검사한다.
bool FRoomLobbyTitleRegressionTest::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("방 생성 실패:"), EAutomationExpectedErrorFlags::Contains, 0);
    AddExpectedError(TEXT("방 참여 실패:"), EAutomationExpectedErrorFlags::Contains, 0);
    UGameInstance* GI = NewObject<UGameInstance>(GEngine); GI->InitializeStandalone();
    UWorld* World = GI->GetWorld();
    APlayerController* PC = World->SpawnActor<APlayerController>();
    ULocalPlayer* LP = NewObject<ULocalPlayer>(GEngine); LP->PlayerController=PC; PC->Player=LP;
    PC->SetAsLocalPlayerController(); World->AddController(PC);
    auto* Server=GI->GetSubsystem<UServerSubsystem>();
    auto Backend=MakeUnique<FRoomTitleTestBackend>();auto* Stub=Backend.Get();
    Server->Backend=MoveTemp(Backend);Server->ConnectionState=EChatConnectionState::LoggedIn;
    UClass* Class=LoadClass<URoomLobbyWidgetBase>(nullptr,TEXT("/Game/02_JSY/MainLobby/WBP_RoomLobbyWidget.WBP_RoomLobbyWidget_C"));
    if(!TestNotNull(TEXT("Room lobby WBP"),Class)){GI->Shutdown();return false;}
    auto* Page=CreateWidget<URoomLobbyWidgetBase>(PC,Class);auto Slate=Page->TakeWidget();
    auto* Title=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("TitleText")));
    if(!TestNotNull(TEXT("Actual TitleText binding"),Title)){GI->Shutdown();return false;}
    auto Pump=[&](const FServerClientEvent& E){Stub->Events.Add(E);Server->Tick(0.f);Page->Refresh(Server);};
    Page->Refresh(Server);TestEqual(TEXT("No title fallback"),Title->GetText().ToString(),FString(TEXT("방 대기실")));
    Server->CreateRoom(TEXT("함께 탐험할 사람!"),FString(),0);
    TestEqual(TEXT("Title sent unchanged"),Stub->SentTitle,FString(TEXT("함께 탐험할 사람!")));
    TestTrue(TEXT("Title unconfirmed before ack"),Server->GetCurrentRoomTitle().IsEmpty());
    FServerClientEvent E;E.Type=EServerClientEventType::RoomCreateAck;E.bRoomSuccess=true;E.RoomId=31;
    Pump(E);TestEqual(TEXT("Created title rendered"),Title->GetText().ToString(),Stub->SentTitle);
    TestTrue(TEXT("Create request title consumed"),Server->PendingCreatedRoomTitle.IsEmpty());
    Server->ClearRoomState();Page->Refresh(Server);
    TestEqual(TEXT("Exit clears display"),Title->GetText().ToString(),FString(TEXT("방 대기실")));
    Server->CreateRoom(TEXT("두 번째 방"),FString(),0);E.RoomId=32;Pump(E);
    TestEqual(TEXT("New room replaces title"),Title->GetText().ToString(),FString(TEXT("두 번째 방")));
    Server->ClearRoomState();Server->CreateRoom(TEXT("실패할 방"),FString(),0);E.bRoomSuccess=false;Pump(E);
    TestTrue(TEXT("Failed create not committed"),Server->GetCurrentRoomTitle().IsEmpty());
    TestTrue(TEXT("Failed create pending cleared"),Server->PendingCreatedRoomTitle.IsEmpty());
    E=FServerClientEvent();E.Type=EServerClientEventType::RoomListAck;
    FMOURoomInfo Room;Room.RoomId=42;Room.Title=TEXT("친구들이 만든 방");E.Rooms.Add(Room);Pump(E);
    Server->JoinRoom(42,FString());
    E.Rooms.Reset();Pump(E); // List changes while join is pending.
    E=FServerClientEvent();E.Type=EServerClientEventType::RoomJoinAck;E.Join.bSuccess=true;E.Join.RoomId=42;
    Pump(E);TestEqual(TEXT("Selected guest title survives list refresh"),Title->GetText().ToString(),Room.Title);
    Server->ClearRoomState();Server->JoinRoom(99,FString());E.Join.RoomId=99;Pump(E);
    TestEqual(TEXT("Unknown direct join never reuses old title"),Title->GetText().ToString(),FString(TEXT("방 대기실")));
    Server->ClearRoomState();Server->RoomTitlesById.Add(42,Room.Title);Server->JoinRoom(42,FString());
    E.Join.bSuccess=false;E.Join.RoomId=42;Pump(E);
    TestTrue(TEXT("Failed join not committed"),Server->GetCurrentRoomTitle().IsEmpty());
    TestTrue(TEXT("Failed join pending cleared"),Server->PendingJoinedRoomTitle.IsEmpty());
    Server->RoomTitlesById.Add(42,Room.Title);Server->JoinRoom(42,FString());
    E=FServerClientEvent();E.Type=EServerClientEventType::Disconnected;Pump(E);
    TestTrue(TEXT("Disconnect clears pending title"),Server->PendingJoinedRoomTitle.IsEmpty());
    TestTrue(TEXT("Disconnect clears cached names"),Server->RoomTitlesById.IsEmpty());
    Page->Refresh(nullptr);TestEqual(TEXT("Null server clears UI"),Title->GetText().ToString(),FString(TEXT("방 대기실")));
    Page->ReleaseSlateResources(true);GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    return true;
}
#endif


