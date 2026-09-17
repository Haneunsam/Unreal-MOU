#include "ServerConfig/ServerConfig.h"
#include "EndpointService/EndpointService.h"
#include "ServerLog/ServerLog.h"
#include <cstdio>
#include <cstring>
namespace MOU::ServerRuntime
{
bool ParseServerConfig(int argc, char** argv, ServerConfig& Config)
{
	// 인자 파싱. --upnp / --public-ip / --relay 는 어디에 와도 되고,
	// 나머지는 순서대로 <port> [db경로] 다.

	for (int Index = 1; Index < argc; ++Index)
	{
		if (std::strcmp(argv[Index], "--upnp") == 0)
		{
			Config.bUseUpnp = true;
		}
		else if (std::strcmp(argv[Index], "--relay") == 0)
		{
			Config.bUseRelay = true;
		}
		else if (std::strncmp(argv[Index], "--relay-public-ip=", 18) == 0)
		{
			Config.RelayPublicIp = argv[Index] + 18;
			Config.bUseRelay = true;
		}
		else if (std::strcmp(argv[Index], "--relay-public-ip") == 0)
		{
			if (Index + 1 >= argc)
			{
				ServerLog::Print("[오류] --relay-public-ip 뒤에 주소가 없다.\n");
				return false;
			}
			Config.RelayPublicIp = argv[++Index];
			Config.bUseRelay = true;
		}
		else if (std::strncmp(argv[Index], "--relay-lan-ip=", 15) == 0)
		{
			Config.RelayLanIp = argv[Index] + 15;
			Config.bUseRelay = true;
		}
		else if (std::strcmp(argv[Index], "--relay-lan-ip") == 0)
		{
			if (Index + 1 >= argc)
			{
				ServerLog::Print("[오류] --relay-lan-ip 뒤에 주소가 없다.\n");
				return false;
			}
			Config.RelayLanIp = argv[++Index];
			Config.bUseRelay = true;
		}
		else if (std::strncmp(argv[Index], "--relay-ports=", 14) == 0 ||
		         std::strcmp(argv[Index], "--relay-ports") == 0)
		{
			const char* Range = nullptr;
			if (std::strncmp(argv[Index], "--relay-ports=", 14) == 0)
			{
				Range = argv[Index] + 14;
			}
			else if (Index + 1 < argc)
			{
				Range = argv[++Index];
			}
			if (Range == nullptr)
			{
				ServerLog::Print("[오류] --relay-ports 뒤에 10000-10127 형태의 범위가 필요하다.\n");
				return false;
			}
			unsigned First = 0, Last = 0;
			char Trailing = '\0';
			if (std::sscanf(Range, "%u-%u%c", &First, &Last, &Trailing) != 2 || First == 0 || Last > 65535 ||
				First > Last || ((Last - First + 1) % 2) != 0)
			{
				ServerLog::Print("[오류] relay UDP 포트 범위 '%s' 가 잘못됐다. 짝수 개의 1-65535 범위를 써야 한다.\n", Range);
				return false;
			}
			Config.RelayFirstPort = static_cast<uint16_t>(First);
			Config.RelayLastPort  = static_cast<uint16_t>(Last);
			Config.bUseRelay = true;
		}
		// --public-ip=1.2.3.4 와 --public-ip 1.2.3.4 를 둘 다 받는다.
		// 붙여 쓰는 쪽만 지원하면 공백을 넣었을 때 그 값이 db경로로 먹혀
		// 엉뚱한 파일에 계정이 생기는 사고가 난다 (--upnp 때 겪은 그것과 같다).
		else if (std::strncmp(argv[Index], "--public-ip=", 12) == 0)
		{
			Config.PublicIp = argv[Index] + 12;
		}
		else if (std::strcmp(argv[Index], "--public-ip") == 0)
		{
			if (Index + 1 >= argc)
			{
				ServerLog::Print("[오류] --public-ip 뒤에 주소가 없다.\n");
				return false;
			}
			Config.PublicIp = argv[++Index];
		}
		else if (Config.PortArg == nullptr)
		{
			Config.PortArg = argv[Index];
		}
		else if (Config.DbArg == nullptr)
		{
			Config.DbArg = argv[Index];
		}
		else
		{
			Config.PortArg = nullptr;   // 인자가 너무 많다. 사용법을 보여준다
			break;
		}
	}

	// 사설 주소를 공인 IP 라고 우기면 치환이 오히려 상황을 악화시킨다.
	// 조용히 무시하지 말고 여기서 멈춰서 알려준다.
	if (!Config.PublicIp.empty() && IsPrivateAddress(Config.PublicIp))
	{
		ServerLog::Print("[오류] --public-ip 에 사설 주소(%s)를 줬다. 공인 IP 를 줘야 한다.\n",
		            Config.PublicIp.c_str());
		return false;
	}
	if (!Config.RelayPublicIp.empty() && IsPrivateAddress(Config.RelayPublicIp))
	{
		ServerLog::Print("[오류] --relay-public-ip 에 사설 주소(%s)를 줬다. 공인 IP 를 줘야 한다.\n",
		            Config.RelayPublicIp.c_str());
		return false;
	}
	if (!Config.RelayLanIp.empty() && !IsPrivateAddress(Config.RelayLanIp))
	{
		ServerLog::Print("[오류] --relay-lan-ip 는 서버 PC의 사설 LAN IPv4여야 한다: %s\n", Config.RelayLanIp.c_str());
		return false;
	}

	if (Config.PortArg == nullptr)
	{
		ServerLog::Print("사용법: %s <port> [db경로] [--upnp] [--public-ip <주소>] [--relay]\n", argv[0]);
		ServerLog::Print("  db경로를 생략하면 현재 디렉터리의 chat_log.db 를 쓴다.\n");
		ServerLog::Print("  --upnp : 공유기(UPnP)에 이 포트를 자동으로 열어달라고 요청한다.\n");
		ServerLog::Print("           다른 네트워크에서 접속시킬 때만 필요하다. 같은 공유기\n");
		ServerLog::Print("           안에서만 쓸 거라면 켤 이유가 없다.\n");
		ServerLog::Print("  --public-ip : 이 서버의 공인 IP. 방장이 서버와 같은 공유기 안에\n");
		ServerLog::Print("           있을 때, 방의 호스트 주소로 사설 IP 대신 이 값을 기록한다.\n");
		ServerLog::Print("           안 주면 --upnp 성공 시 알아낸 값을 자동으로 쓴다.\n");
		ServerLog::Print("  --relay : 직접 UDP 연결 실패 시 사용할 자체 UDP relay 를 켠다.\n");
		ServerLog::Print("           기본 범위는 10000-10127/UDP 이며, 공유기에 같은 범위를\n");
		ServerLog::Print("           이 PC로 수동 포트포워딩해야 한다. --public-ip 를 재사용한다.\n");
		ServerLog::Print("  --relay-public-ip <주소> : relay 전용 공인 IP 를 명시한다.\n");
		ServerLog::Print("  --relay-lan-ip <주소> : 서버와 같은 LAN 참가자에게 줄 사설 relay 주소.\n");
		ServerLog::Print("  --relay-ports <처음-끝> : 예: --relay-ports 10000-10127\n");
		return false;
	}

    return true;
}
}
