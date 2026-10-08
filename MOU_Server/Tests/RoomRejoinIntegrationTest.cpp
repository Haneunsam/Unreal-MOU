#include "Accounts/Accounts.h"
#include "ServerApp/ServerApp.h"
#include "ServerContext/ServerContext.h"
#include "RelayRouteService/RelayRouteService.h"
#include "Rooms/Rooms.h"
#include "Framing.h"
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace MOU;
using namespace MOU::ServerRuntime;

// [REJOINTEST-003] 실제 TCP·UDP 통합 테스트의 실패를 보고한다.
static void Check(bool Value, const char* Label)
{
    if (!Value) { std::cerr << "FAIL: " << Label << '\n'; std::exit(1); }
}

// [REJOINTEST-004] 제한 시간 안에 소켓에서 정확한 바이트 수를 읽는다.
static void ReadExact(SocketHandle Socket, void* Data, size_t Size)
{
    auto* Bytes = static_cast<char*>(Data);
    while (Size)
    {
        const int Read = recv(Socket, Bytes, static_cast<int>(Size), 0);
        Check(Read > 0, "socket receive timeout or EOF");
        Bytes += Read;
        Size -= Read;
    }
}

struct Client
{
    SocketHandle Socket = kInvalidSocket;
    std::thread Worker;
    uint64_t UserId = 0;

    // [REJOINTEST-005] 루프백 TCP 연결을 실제 서버 ClientThread에 연결한다.
    Client()
    {
        SocketHandle Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in Address{};
        Address.sin_family = AF_INET;
        Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(Listener, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) == 0, "bind listener");
        Check(listen(Listener, 1) == 0, "listen");
#ifdef _WIN32
        int Length = sizeof(Address);
#else
        socklen_t Length = sizeof(Address);
#endif
        Check(getsockname(Listener, reinterpret_cast<sockaddr*>(&Address), &Length) == 0, "listener endpoint");
        Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(connect(Socket, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) == 0, "connect");
        const SocketHandle Accepted = accept(Listener, nullptr, nullptr);
        Check(Accepted != kInvalidSocket, "accept");
        CloseSocket(Listener);
        SetRecvTimeout(Socket, 5000);
        auto Session = Context().Sessions.Add(Accepted);
        Session->PeerAddress = "127.0.0.1";
        Worker = std::thread(ClientThread, Session);
    }

    // [REJOINTEST-006] FIN 수신에 따른 실제 서버 퇴장·계정 해제를 기다린다.
    ~Client()
    {
        ShutdownSend(Socket);
        Worker.join();
        CloseSocket(Socket);
    }

    // [REJOINTEST-007] 실제 TCP 프레임으로 요청을 송신한다.
    template<class T> void Send(EOpcode Op, const T& Body)
    {
        Check(SendPacket(Socket, Op, &Body, sizeof(Body)), "send packet");
    }
    // [REJOINTEST-008] 본문 없는 요청을 송신한다.
    void Send(EOpcode Op) { Check(SendPacket(Socket, Op, nullptr, 0), "send empty"); }

    // [REJOINTEST-009] 다른 알림은 소비하고 지정한 응답 프레임을 반환한다.
    std::vector<char> Receive(EOpcode Op)
    {
        for (int Attempt = 0; Attempt < 100; ++Attempt)
        {
            PacketHeader Header{};
            ReadExact(Socket, &Header, sizeof(Header));
            Check(Header.BodySize <= kMaxBodySize, "frame size");
            std::vector<char> Body(Header.BodySize);
            ReadExact(Socket, Body.data(), Body.size());
            if (Header.Opcode == static_cast<uint16_t>(Op)) return Body;
        }
        Check(false, "missing expected response");
        return {};
    }

    // [REJOINTEST-010] 고정 크기 응답의 길이를 검증하고 복사한다.
    template<class T> T Receive(EOpcode Op)
    {
        const auto Bytes = Receive(Op);
        Check(Bytes.size() == sizeof(T), "response body size");
        T Body{};
        std::memcpy(&Body, Bytes.data(), sizeof(Body));
        return Body;
    }

    // [REJOINTEST-011] 저장된 계정으로 인증하고 서버 확정 계정 번호를 기록한다.
    void Login(const char* Id)
    {
        LoginReqBody Request{};
        Request.Version = kProtocolVersion;
        CopyFixedString(Request.LoginId, kMaxLoginIdLen, Id);
        CopyFixedString(Request.Password, kMaxPasswordLen, "password123");
        Send(EOpcode::LoginReq, Request);
        const auto Ack = Receive<LoginAckBody>(EOpcode::LoginAck);
        Check(Ack.bSuccess && Ack.ServerVersion == kProtocolVersion, "login");
        UserId = Ack.UserId;
    }

    // [REJOINTEST-012] 방 입장 요청과 응답을 왕복한다.
    RoomJoinAckBody Join(uint32_t Room)
    {
        RoomJoinReqBody Request{};
        Request.RoomId = Room;
        Send(EOpcode::RoomJoinReq, Request);
        return Receive<RoomJoinAckBody>(EOpcode::RoomJoinAck);
    }
};

// [REJOINTEST-013] 역할별 capability 등록 후 실제 릴레이 양방향 전달을 검증한다.
static void VerifyRelay(const RelayHostRoute& Host, const RelayGuestRoute& Guest)
{
    Check(Host.RouteId && Host.RouteId == Guest.RouteId, "relay allocation");
    SocketHandle HostSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SocketHandle GuestSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SetRecvTimeout(HostSocket, 2000);
    SetRecvTimeout(GuestSocket, 2000);
    sockaddr_in HostAddress{}, GuestAddress{};
    HostAddress.sin_family = GuestAddress.sin_family = AF_INET;
    HostAddress.sin_addr.s_addr = GuestAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    HostAddress.sin_port = htons(Host.HostPort);
    GuestAddress.sin_port = htons(Guest.GuestPort);
    auto HostRegistration = MakeRelayRegistrationDatagram(Host.RouteId, ERelayPeerRole::Host, Host.HostToken);
    auto GuestRegistration = MakeRelayRegistrationDatagram(Guest.RouteId, ERelayPeerRole::Guest, Guest.GuestToken);
    Check(sendto(HostSocket, reinterpret_cast<const char*>(&HostRegistration), sizeof(HostRegistration), 0,
        reinterpret_cast<sockaddr*>(&HostAddress), sizeof(HostAddress)) > 0, "host register");
    Check(sendto(GuestSocket, reinterpret_cast<const char*>(&GuestRegistration), sizeof(GuestRegistration), 0,
        reinterpret_cast<sockaddr*>(&GuestAddress), sizeof(GuestAddress)) > 0, "guest register");
    FUdpRelayRouteStatus Status{};
    for (int Attempt = 0; Attempt < 100; ++Attempt)
    {
        if (Context().Relay->GetRouteStatus(Host.RouteId, Status) && Status.bHostRegistered && Status.bGuestRegistered) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Check(Status.bHostRegistered && Status.bGuestRegistered, "registered relay peers");
    const char Payload[] = "rejoin-game-packet";
    char Buffer[64]{};
    Check(sendto(GuestSocket, Payload, sizeof(Payload), 0, reinterpret_cast<sockaddr*>(&GuestAddress), sizeof(GuestAddress)) > 0, "guest payload");
    Check(recv(HostSocket, Buffer, sizeof(Buffer), 0) == sizeof(Payload) && std::memcmp(Buffer, Payload, sizeof(Payload)) == 0, "guest to host relay");
    Check(sendto(HostSocket, Payload, sizeof(Payload), 0, reinterpret_cast<sockaddr*>(&HostAddress), sizeof(HostAddress)) > 0, "host payload");
    Check(recv(GuestSocket, Buffer, sizeof(Buffer), 0) == sizeof(Payload) && std::memcmp(Buffer, Payload, sizeof(Payload)) == 0, "host to guest relay");
    CloseSocket(HostSocket); CloseSocket(GuestSocket);
}

// [REJOINTEST-014] 실제 서버 스레드에서 최초 시작·재입장·종료 후 재로그인·릴레이 보존을 검증한다.
int main()
{
    Check(NetInit(), "network init");
    Check(Accounts::Start(":memory:"), "isolated accounts DB");
    for (const char* Id : {"testhost", "testguest", "testlate"})
    {
        uint64_t UserId = 0;
        Check(Accounts::Create(Id, "password123", Id, UserId) == EAccountResult::Success, "create test account");
    }
    Context().Relay = std::make_unique<UdpRelay>();
    Context().RelayPublicIp = "127.0.0.1";
    FUdpRelayConfig Config;
    Config.BindAddress = "127.0.0.1";
    Config.FirstPort = 24000; Config.LastPort = 24063;
    Check(Context().Relay->Start(Config), "start relay");
    {
        Client Host;
        Host.Login("testhost");
        RoomCreateReqBody Create{};
        CopyFixedString(Create.Title, kMaxRoomTitleLen, "rejoin integration");
        Create.HostPort = 7777; Create.MaxPlayers = 4;
        Host.Send(EOpcode::RoomCreateReq, Create);
        const auto Created = Host.Receive<RoomCreateAckBody>(EOpcode::RoomCreateAck);
        Check(Created.bSuccess, "create room");
        const uint32_t Room = Created.RoomId;
        uint64_t GuestId = 0;
        RelayHostRoute InitialHostRoute{};
        {
            Client Guest;
            Guest.Login("testguest"); GuestId = Guest.UserId;
            Check(Guest.Join(Room).bSuccess, "initial join");
            RoomReadyReqBody Ready{}; Ready.bReady = 1;
            Guest.Send(EOpcode::RoomReadyReq, Ready);
            // 멤버 알림으로 준비 반영을 확인한 후 시작한다.
            for (;;)
            {
                const auto Bytes = Guest.Receive(EOpcode::RoomMemberList);
                RoomMemberListBody Members{}; std::memcpy(&Members, Bytes.data(), sizeof(Members));
                if (Members.bAllReady) break;
            }
            Host.Send(EOpcode::RoomStartReq);
            const auto Start = Host.Receive<RoomStartBody>(EOpcode::RoomStart);
            Check(Start.RelayRouteCount == 1, "initial relay count");
            InitialHostRoute = Start.RelayRoutes[0];
            Guest.Receive<RoomStartBody>(EOpcode::RoomStart);
            Host.Send(EOpcode::RoomHostReadyReq);
            const auto FirstReady = Guest.Receive<RoomHostReadyBody>(EOpcode::RoomHostReady);
            VerifyRelay(InitialHostRoute, FirstReady.Relay);

            Client Late;
            Late.Login("testlate");
            Late.Send(EOpcode::RoomListReq);
            const auto List = Late.Receive(EOpcode::RoomListAck);
            RoomListAckBody Head{}; std::memcpy(&Head, List.data(), sizeof(Head));
            Check(Head.Count == 1, "game absent from list");
            RoomInfo Info{}; std::memcpy(&Info, List.data() + sizeof(Head), sizeof(Info));
            Check(Info.State == static_cast<uint8_t>(ERoomState::InGame), "game state");
            const auto Join = Late.Join(Room);
            Check(Join.bSuccess && Join.ConnectRequestId, "new account midgame join");
            auto Prepare = Host.Receive<RoomGuestConnectPrepareBody>(EOpcode::RoomGuestConnectPrepare);
            Check(Prepare.GuestUserId == Late.UserId && Prepare.ConnectRequestId == Join.ConnectRequestId, "prepare identity");
            RoomGuestConnectAckBody Ack{Room, Late.UserId, Prepare.ConnectRequestId, 1};
            // 다른 참여자는 호스트를 대신해 승인할 수 없다.
            Guest.Send(EOpcode::RoomGuestConnectAck, Ack);
            Host.Send(EOpcode::RoomGuestConnectAck, Ack);
            auto LateReady = Late.Receive<RoomHostReadyBody>(EOpcode::RoomHostReady);
            Check(LateReady.ConnectRequestId == Join.ConnectRequestId, "ready identity");
            VerifyRelay(Prepare.Relay, LateReady.Relay);
            FUdpRelayRouteStatus Status{};
            Check(Context().Relay->GetRouteStatus(InitialHostRoute.RouteId, Status), "existing route overwritten");
            Guest.Send(EOpcode::RoomLeaveReq);
            for (int I = 0; I < 100 && Rooms::FindRoomOf(GuestId); ++I)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Check(Rooms::FindRoomOf(GuestId) == 0, "return-to-lobby leave");
            auto Rejoin = Guest.Join(Room);
            Check(Rejoin.bSuccess && Rejoin.ConnectRequestId, "logged-in rejoin");
            Prepare = Host.Receive<RoomGuestConnectPrepareBody>(EOpcode::RoomGuestConnectPrepare);
            Ack = {Room, GuestId, Prepare.ConnectRequestId, 1};
            Host.Send(EOpcode::RoomGuestConnectAck, Ack);
            auto RejoinReady = Guest.Receive<RoomHostReadyBody>(EOpcode::RoomHostReady);
            Check(RejoinReady.Relay.RouteId != InitialHostRoute.RouteId, "old relay reused");
            VerifyRelay(Prepare.Relay, RejoinReady.Relay);
        } // 실제 FIN -> ClientThread 퇴장 및 계정 해제
        Check(Rooms::FindRoomOf(GuestId) == 0, "quit did not remove guest");
        {
            Client Restarted;
            Restarted.Login("testguest");
            Check(Restarted.UserId == GuestId, "account identity changed");
            const auto Joined = Restarted.Join(Room);
            Check(Joined.bSuccess && Joined.ConnectRequestId, "restarted account rejoin");
            const auto Prepare = Host.Receive<RoomGuestConnectPrepareBody>(EOpcode::RoomGuestConnectPrepare);
            Host.Send(EOpcode::RoomGuestConnectAck, RoomGuestConnectAckBody{Room, GuestId, Prepare.ConnectRequestId, 1});
            const auto Ready = Restarted.Receive<RoomHostReadyBody>(EOpcode::RoomHostReady);
            VerifyRelay(Prepare.Relay, Ready.Relay);
        }
        {
            Client Failed;
            Failed.Login("testguest");
            const auto Joined = Failed.Join(Room);
            Check(Joined.bSuccess, "failure case join");
            const auto Prepare = Host.Receive<RoomGuestConnectPrepareBody>(EOpcode::RoomGuestConnectPrepare);
            Host.Send(EOpcode::RoomGuestConnectAck, RoomGuestConnectAckBody{Room, GuestId, Prepare.ConnectRequestId, 0});
            const auto Rejected = Failed.Receive<RoomHostReadyBody>(EOpcode::RoomHostReady);
            Check(Rejected.ConnectRequestId == Joined.ConnectRequestId && Rejected.CandidateCount == 0 &&
                Rejected.Relay.RouteId == 0, "prepare failure not delivered");
            Check(Rooms::FindRoomOf(GuestId) == 0, "failed preparation kept member");
            FUdpRelayRouteStatus Status{};
            Check(!Context().Relay->GetRouteStatus(Prepare.Relay.RouteId, Status), "failed preparation leaked route");
            // 실패 후 같은 연결로 다시 입장할 수 있어야 한다.
            Check(Failed.Join(Room).bSuccess, "retry after failed preparation");
        }
    }
    Check(Rooms::Count() == 0, "host quit kept room");
    Context().Relay->Stop();
    ClearRelayRoutes();
    Accounts::Stop();
    NetShutdown();
    std::cout << "RoomRejoinIntegrationTest passed: TCP login, room lifecycle, restart, UDP relay\n";
}
