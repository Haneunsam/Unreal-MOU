#include "ServerConfig/ServerConfig.h"
#include "NatPortMapping/NatPortMapping.h"
#include "RelayRouteService/RelayRouteService.h"
#include "EndpointService/EndpointService.h"
#include "ServerApp/ServerApp.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"
#include "SocialHandler/SocialHandler.h"
#include "RoomHandler/RoomHandler.h"
#include "PacketDispatcher/PacketDispatcher.h"
#include "Accounts/Accounts.h"
#include "ChatLog/ChatLog.h"
#include "DirectMessages/DirectMessages.h"
#include "Friends/Friends.h"
#include "Framing.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


namespace MOU::ServerRuntime
{

	void OnInterrupt(int)
	{
		Context().Running = false;

		// ★ 공유기에 열어둔 포트를 먼저 지운다. 영구 매핑이라 여기서 안 지우면
		//   프로세스가 사라져도 공유기에는 그대로 남는다.
		//   네트워크 왕복이라 몇 백 ms 걸릴 수 있는데, 그 대기가 곧 "확실히 지웠다" 는 보장이다.
		//   Start 를 안 했거나 실패했으면 아무 일도 하지 않는다.
		Nat::Stop();

		ChatLog::Stop();
		Accounts::Stop();
		// v7. 둘 다 동기 커밋이라 큐에 남은 것이 없지만, 커넥션을 닫아야
		// WAL 이 정리된다. Start 가 실패했어도 부르는 것이 안전하다.
		Friends::Stop();
		DirectMessages::Stop();
		std::_Exit(0);   // 소켓과 메모리 회수는 OS 에 맡긴다
	}


	void ClientThread(SessionPtr Session)
	{
		char Temp[1024];
		PacketHeader Header{};
		std::vector<char> Body;

		for (;;)
		{
			const int Received = ::recv(Session->Sock, Temp, sizeof(Temp), 0);

			// 0 이면 정상 종료, 음수면 에러. 기존 코드는 != 0 만 봐서
			// 에러(-1) 일 때 send(sock, buf, -1, 0) 이 호출됐다.
			if (Received <= 0)
			{
				break;
			}

			Session->RecvBuf.insert(Session->RecvBuf.end(), Temp, Temp + Received);

			// 한 번의 recv 에 여러 패킷이 붙어 왔을 수 있으므로 다 꺼낼 때까지 돈다.
			bool bDisconnect = false;
			for (;;)
			{
				const EFrameResult Result = TryExtractPacket(Session->RecvBuf, Header, Body);

				if (Result == EFrameResult::NeedMore)
				{
					break;
				}
				if (Result == EFrameResult::Malformed)
				{
					ServerLog::Print("[차단] 비정상 패킷 크기. 연결을 끊는다. (UserId=%llu)\n",
					            static_cast<unsigned long long>(Session->UserId));
					bDisconnect = true;
					break;
				}
				if (!HandlePacket(Session, Header, Body))
				{
					bDisconnect = true;
					break;
				}
			}

			if (bDisconnect)
			{
				break;
			}
		}

		// 방장이 나가면 그 방은 이미 들어갈 수 없는 곳이 된다(리슨서버가 죽었으므로).
		// 목록에 유령 방이 남지 않도록 여기서 반드시 정리한다.
		// 정상 종료든 랜선이 뽑혔든 이 자리를 지나가므로 한 곳에서 처리된다.
		//
		// 남은 멤버에게 통보하는 것까지 같은 함수가 처리한다. 이게 없으면
		// 게스트는 방장이 사라진 줄도 모르고 대기실에 영영 앉아있게 된다.
		if (Session->bAuthed)
		{
			LeaveRoomAndNotify(Session);

			// ★ 친구에게 오프라인을 알린다 (M4). **Remove 보다 먼저** 해야 한다 -
			//   세션이 목록에서 빠진 뒤에는 FriendIds 캐시를 읽을 수 없고,
			//   그러면 친구들 화면에 이 사람이 영원히 온라인으로 남는다.
			//
			//   정상 종료든 랜선이 뽑혔든 이 자리를 지나가므로 한 곳에서 끝난다.
			BroadcastPresence(Session, EPresence::Offline);
		}

		ServerLog::Print("[종료] %s (UserId=%llu) 연결 해제\n",
		            Session->Name.empty() ? "(미로그인)" : Session->Name.c_str(),
		            static_cast<unsigned long long>(Session->UserId));

		Context().Sessions.Remove(Session);
		ServerLog::Print("       현재 접속자 %zu명\n", Context().Sessions.Count());
	}

int RunServer(int argc, char** argv)
{
    ServerLog::Initialize();
    ServerConfig Config;
    if (!ParseServerConfig(argc, argv, Config)) return 1;
    Context().PublicIp = Config.PublicIp;
    Context().RelayPublicIp = Config.RelayPublicIp;
    Context().RelayLanIp = Config.RelayLanIp;

	if (!NetInit())
	{
		ServerLog::Print("NetInit() 실패\n");
		return 1;
	}

	const SocketHandle ListenSock = ::socket(PF_INET, SOCK_STREAM, 0);
	if (ListenSock == kInvalidSocket)
	{
		ServerLog::Print("socket() 실패: %d\n", LastNetError());
		return 1;
	}

	// 도달성 프로브용 UDP 소켓. (v9)
	// bind 하지 않는다 — 보내기만 하므로 OS 가 알아서 임시 포트를 잡는다.
	// 실패해도 서버는 그대로 돈다. 프로브만 못 하게 될 뿐이고, 그때는 방이
	// "모름(=예전처럼 동작)" 으로 남는다.
	Context().ProbeSock = ::socket(PF_INET, SOCK_DGRAM, 0);
	if (Context().ProbeSock == kInvalidSocket)
	{
		ServerLog::Print("[경고] 프로브용 UDP 소켓을 못 만들었다(%d). 도달성 확인 없이 진행한다.\n",
		            LastNetError());
	}
	else
	{
		// ★ v10 부터는 bind 한다. 보내기만 하던 소켓이 이제 **받기도** 해야 한다 —
		//   참여자가 자기 게임 포트에서 등록 데이터그램을 쏘고, 서버는 그 출발지를
		//   관측해 방장에게 알려준다. 그것이 홀펀칭의 유일한 단서다.
		//
		//   TCP 와 같은 번호를 쓴다. 프로토콜이 다르므로 충돌하지 않고,
		//   팀이 외울 포트가 하나로 유지된다.
		//
		// ★★ 공유기에 **외부 UDP <port> -> 서버 PC** 포워딩이 필요하다.
		//    TCP 만 열려 있으면 등록 데이터그램이 서버까지 오지 못하고,
		//    그러면 홀펀칭이 통째로 동작하지 않는다.
		sockaddr_in ProbeAddr{};
		ProbeAddr.sin_family      = AF_INET;
		ProbeAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);
		ProbeAddr.sin_port        = ::htons(static_cast<uint16_t>(std::atoi(Config.PortArg)));

		if (::bind(Context().ProbeSock, reinterpret_cast<sockaddr*>(&ProbeAddr), sizeof(ProbeAddr)) != 0)
		{
			ServerLog::Print("[경고] UDP %s 를 열지 못했다(%d). 홀펀칭 없이 진행한다.\n",
			            Config.PortArg, LastNetError());
		}
		else
		{
			// 타임아웃이 있어야 종료할 때 recvfrom 에 갇히지 않는다.
			SetRecvTimeout(Context().ProbeSock, 500);
			ServerLog::Print("[UDP] %s 에서 엔드포인트 등록을 받는다.\n", Config.PortArg);
			ServerLog::Print("      * 공유기에 '외부 UDP %s -> 이 PC' 포워딩이 있어야 한다.\n", Config.PortArg);
		}
	}

	sockaddr_in ServerAddr{};
	ServerAddr.sin_family      = AF_INET;
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	ServerAddr.sin_port        = htons(static_cast<uint16_t>(std::atoi(Config.PortArg)));

	if (::bind(ListenSock, reinterpret_cast<sockaddr*>(&ServerAddr), sizeof(ServerAddr)) != 0)
	{
		ServerLog::Print("bind() 실패: %d\n", LastNetError());
		return 1;
	}
	if (::listen(ListenSock, SOMAXCONN) != 0)
	{
		ServerLog::Print("listen() 실패: %d\n", LastNetError());
		return 1;
	}

	std::signal(SIGINT,  OnInterrupt);
	std::signal(SIGTERM, OnInterrupt);

	// 채팅 로그 DB. 두 번째 인자로 경로를 바꿀 수 있다 (테스트용으로 분리할 때 편하다).
	// 열기에 실패해도 서버는 계속 돈다. 로그가 안 남는 것보다 채팅이 끊기는 게 나쁘다.
	const char* DbPath = (Config.DbArg != nullptr) ? Config.DbArg : "chat_log.db";
	ChatLog::Start(DbPath);

	// 계정도 같은 파일에 둔다(테이블이 다르므로 섞이지 않는다).
	// 커넥션은 별개다 — ChatLog 쪽은 라이터 스레드 전용이라 남이 끼면 안 된다.
	if (!Accounts::Start(DbPath))
	{
		ServerLog::Print("[치명] 계정 DB 를 열지 못했다. 아무도 로그인할 수 없다.\n");
		return 1;
	}

	// v7 친구 + 메신저. 같은 파일, 각자 별도 커넥션(Accounts 와 같은 이유).
	//
	// ★ 실패해도 서버를 죽이지 않는다. 계정과 심각도가 다르다 - 로그인은
	//   못 하면 아무것도 안 되지만, 친구 목록이 안 뜨는 것은 게임과 채팅을
	//   막지 않는다. 대신 왜 안 되는지는 분명히 말해준다.
	if (!Friends::Start(DbPath))
	{
		ServerLog::Print("[경고] 친구 DB 를 열지 못했다. 친구 기능만 동작하지 않는다.\n");
	}
	if (!DirectMessages::Start(DbPath))
	{
		ServerLog::Print("[경고] 메신저 DB 를 열지 못했다. DM 만 동작하지 않는다.\n");
	}

	// 공유기에 이 포트를 열어달라고 요청한다. 블로킹이라 accept 루프 전에 끝낸다.
	//
	// ★ 실패해도 서버는 그대로 뜬다. 같은 네트워크에서는 어차피 접속되고,
	//   UPnP 가 안 되는 것은 "이 경로로는 못 간다" 는 뜻이지 서버 오류가 아니다.
	if (Config.bUseUpnp)
	{
		const uint16_t ListenPort = static_cast<uint16_t>(std::atoi(Config.PortArg));
		const Nat::EResult Result = Nat::Start(ListenPort, /*bTcp=*/true);

		if (Result == Nat::EResult::Success)
		{
			ServerLog::Print("[NAT] 외부에서는 %s:%u 로 접속하면 된다.\n",
				Nat::ExternalIp().empty() ? "<외부IP>" : Nat::ExternalIp().c_str(),
				static_cast<unsigned>(Nat::MappedExternalPort()));

			// 공유기가 다른 외부 포트를 열어준 경우, 클라이언트는 그 포트로 붙어야 한다.
			if (Nat::MappedExternalPort() != ListenPort)
			{
				ServerLog::Print("[NAT] ★ 내부 포트와 외부 포트가 다르다. 클라이언트의 서버 주소를\n");
				ServerLog::Print("        외부 포트(%u)로 설정해야 한다.\n",
					static_cast<unsigned>(Nat::MappedExternalPort()));
			}

			// UPnP 가 알아낸 외부 IP 를 방 호스트 주소 치환에도 쓴다.
			// 명시적으로 --public-ip 를 준 쪽이 이긴다 — 사람이 준 값이
			// 더 정확한 상황(공유기가 외부 IP 를 잘못 보고하는 경우)이 있다.
			if (Context().PublicIp.empty() && !Nat::ExternalIp().empty()
			    && !IsPrivateAddress(Nat::ExternalIp()))
			{
				Context().PublicIp = Nat::ExternalIp();
				ServerLog::Print("[NAT] 공인 IP %s 를 방 호스트 주소 치환에 쓴다.\n",
					Context().PublicIp.c_str());
			}
		}
		else
		{
			ServerLog::Print("[NAT] 포트를 열지 못했다: %s\n", Nat::ResultText(Result));
			ServerLog::Print("[NAT] 같은 네트워크에서만 접속할 수 있다. 서버는 그대로 계속한다.\n");
		}
	}

	// relay 는 공인 주소와 **별도 UDP 포트 범위**가 있어야 한다. 기존 9000/UDP 는
	// 엔드포인트 등록용 단일 소켓이라 여러 UE 참여자를 투명하게 중계할 수 없다.
	if (Config.bUseRelay)
	{
		if (Context().RelayPublicIp.empty())
		{
			Context().RelayPublicIp = Context().PublicIp;  // 보통은 로그인 서버와 같은 공인 IP 다.
		}

		if (Context().RelayPublicIp.empty() || IsPrivateAddress(Context().RelayPublicIp))
		{
			ServerLog::Print("[릴레이] 비활성: --relay-public-ip 또는 --public-ip 로 공인 주소를 줘야 한다.\n");
		}
		else
		{
			FUdpRelayConfig RelayConfig;
			RelayConfig.FirstPort = Config.RelayFirstPort;
			RelayConfig.LastPort  = Config.RelayLastPort;
			Context().Relay = std::make_unique<UdpRelay>();
			if (!Context().Relay->Start(RelayConfig))
			{
				ServerLog::Print("[릴레이] UDP %u-%u 를 열지 못했다. 직접 연결만 계속한다.\n",
				            static_cast<unsigned>(Config.RelayFirstPort), static_cast<unsigned>(Config.RelayLastPort));
				Context().Relay.reset();
			}
			else
			{
				ServerLog::Print("[릴레이] 활성: %s:%u-%u/UDP (참여자당 포트 2개)\n",
				            Context().RelayPublicIp.c_str(), static_cast<unsigned>(Config.RelayFirstPort),
				            static_cast<unsigned>(Config.RelayLastPort));
				ServerLog::Print("[릴레이] ★ 공유기에 외부 UDP %u-%u -> 이 PC 를 수동 포트포워딩해야 한다.\n",
				            static_cast<unsigned>(Config.RelayFirstPort), static_cast<unsigned>(Config.RelayLastPort));
				if (!Context().RelayLanIp.empty())
				{
					ServerLog::Print("[릴레이] 서버와 같은 LAN 참가자에게는 %s 를 내려준다(헤어핀 회피).\n",
					            Context().RelayLanIp.c_str());
				}
			}
		}
	}

	// 방장이 서버와 같은 공유기 안에 있을 때 무엇으로 바꿔 기록할지.
	// 이게 비어 있으면 예전 동작 그대로이고, 그 조합에서만 외부 참가자가
	// 사설 주소를 받아 무한 로딩에 걸린다. 켤 때 분명히 보이게 찍어둔다.
	if (!Context().PublicIp.empty())
	{
		ServerLog::Print("[방 주소] 방장이 이 서버와 같은 네트워크에 있으면 호스트 주소를 %s 로 기록한다.\n",
			Context().PublicIp.c_str());
		ServerLog::Print("[방 주소] 그 방장의 리슨서버 포트(보통 7777/UDP)도 공유기에 열려 있어야 한다.\n");
	}
	else
	{
		ServerLog::Print("[방 주소] --public-ip 가 없다. 방장이 이 서버와 같은 공유기 안이면\n");
		ServerLog::Print("          외부 참가자에게 사설 주소가 전달되어 접속하지 못한다.\n");
	}

	ServerLog::Print("=== MOU 서버 시작 (port %s) ===\n", Config.PortArg);

	// 엔드포인트 등록 수신. accept 루프와 독립적이라 별도 스레드가 맞다. (v10)
	// Context().Running 이 꺼지면 recvfrom 타임아웃(500ms) 다음 바퀴에 스스로 빠져나온다.
	std::thread UdpThread;
	if (Context().ProbeSock != kInvalidSocket)
	{
		UdpThread = std::thread(UdpReceiveLoop);
	}

	while (Context().Running)
	{
		sockaddr_in ClientAddr{};
		int AddrSize = sizeof(ClientAddr);

		const SocketHandle ClientSock =
			::accept(ListenSock, reinterpret_cast<sockaddr*>(&ClientAddr),
#ifdef _WIN32
			         &AddrSize);
#else
			         reinterpret_cast<socklen_t*>(&AddrSize));
#endif
		if (ClientSock == kInvalidSocket)
		{
			ServerLog::Print("accept() 실패: %d\n", LastNetError());
			continue;
		}

		char AddrText[INET_ADDRSTRLEN] = {};
		::inet_ntop(AF_INET, &ClientAddr.sin_addr, AddrText, sizeof(AddrText));
		ServerLog::Print("[접속] %s\n", AddrText);

		// 고정 배열이 아니므로 접속자 수 상한이 없다.
		SessionPtr Session = Context().Sessions.Add(ClientSock);
		// 방을 만들 때 호스트 주소로 쓸 값이다. 여기서 한 번만 확정해둔다.
		Session->PeerAddress = AddrText;
		std::thread(ClientThread, Session).detach();
	}

	// UDP 스레드를 먼저 거둔다. 소켓을 닫기 전에 빠져나와야 이미 닫힌 핸들로
	// recvfrom 을 부르는 일이 없다.
	if (UdpThread.joinable())
	{
		UdpThread.join();
	}
	if (Context().ProbeSock != kInvalidSocket)
	{
		CloseSocket(Context().ProbeSock);
		Context().ProbeSock = kInvalidSocket;
	}
	if (Context().Relay)
	{
		Context().Relay->Stop();
		Context().Relay.reset();
	}
	ClearRelayRoutes();

	ChatLog::Stop();
	Accounts::Stop();
	Friends::Stop();
	DirectMessages::Stop();
	CloseSocket(ListenSock);
	NetShutdown();
	return 0;
}


}
