// 플랫폼별 소켓 API 차이를 흡수하는 얇은 래퍼.
#pragma once

#ifdef _WIN32

	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <mstcpip.h>
	#pragma comment(lib, "ws2_32.lib")

	namespace MOU
	{
		using SocketHandle = SOCKET;
		inline constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

		inline bool NetInit()     { WSADATA Data; return ::WSAStartup(MAKEWORD(2, 2), &Data) == 0; }
		inline void NetShutdown() { ::WSACleanup(); }
		inline void CloseSocket(SocketHandle Sock) { ::closesocket(Sock); }
		inline int  LastNetError() { return ::WSAGetLastError(); }

		// 보낼 데이터를 다 내보낸 뒤 FIN 을 보낸다. 수신은 계속 열어둔다.
		// 다른 스레드가 recv() 에 블록된 소켓을 곧바로 closesocket() 하면
		// 아직 나가지 않은 송신 데이터가 버려질 수 있다.
		inline void ShutdownSend(SocketHandle Sock) { ::shutdown(Sock, SD_SEND); }

		inline bool SetRecvTimeout(SocketHandle Sock, int Milliseconds)
		{
			const DWORD Timeout = static_cast<DWORD>(Milliseconds);
			return ::setsockopt(Sock, SOL_SOCKET, SO_RCVTIMEO,
			                    reinterpret_cast<const char*>(&Timeout), sizeof(Timeout)) == 0;
		}

		// [NETLIVE-001] TCP 생존 확인과 송신 대기 제한을 설정하여 종료 정리 지연을 줄인다.
		inline bool ConfigureSessionSocket(SocketHandle Sock)
		{
			tcp_keepalive KeepAlive{};
			KeepAlive.onoff = 1;
			KeepAlive.keepalivetime = 15000;
			KeepAlive.keepaliveinterval = 3000;
			DWORD BytesReturned = 0;
			if (::WSAIoctl(Sock, SIO_KEEPALIVE_VALS,
			              &KeepAlive, sizeof(KeepAlive), nullptr, 0,
			              &BytesReturned, nullptr, nullptr) != 0)
			{
				return false;
			}
			const DWORD SendTimeout = 3000;
			return ::setsockopt(Sock, SOL_SOCKET, SO_SNDTIMEO,
			                    reinterpret_cast<const char*>(&SendTimeout), sizeof(SendTimeout)) == 0;
		}

		inline bool IsRecvTimeout(int ErrorCode) { return ErrorCode == WSAETIMEDOUT; }
	}

#else

	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <cerrno>

	namespace MOU
	{
		using SocketHandle = int;
		inline constexpr SocketHandle kInvalidSocket = -1;

		inline bool NetInit()     { return true; }
		inline void NetShutdown() {}
		inline void CloseSocket(SocketHandle Sock) { ::close(Sock); }
		inline int  LastNetError() { return errno; }

		// 보낼 데이터를 다 내보낸 뒤 FIN 을 보낸다. 수신은 계속 열어둔다.
		inline void ShutdownSend(SocketHandle Sock) { ::shutdown(Sock, SHUT_WR); }

		inline bool SetRecvTimeout(SocketHandle Sock, int Milliseconds)
		{
			timeval Timeout{};
			Timeout.tv_sec  = Milliseconds / 1000;
			Timeout.tv_usec = (Milliseconds % 1000) * 1000;
			return ::setsockopt(Sock, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof(Timeout)) == 0;
		}

		// [NETLIVE-001] TCP 생존 확인과 송신 대기 제한을 설정하여 종료 정리 지연을 줄인다.
		inline bool ConfigureSessionSocket(SocketHandle Sock)
		{
			const int Enabled = 1;
			const int IdleSeconds = 15;
			const int IntervalSeconds = 3;
			const int ProbeCount = 5;
			if (::setsockopt(Sock, SOL_SOCKET, SO_KEEPALIVE, &Enabled, sizeof(Enabled)) != 0 ||
			    ::setsockopt(Sock, IPPROTO_TCP, TCP_KEEPIDLE, &IdleSeconds, sizeof(IdleSeconds)) != 0 ||
			    ::setsockopt(Sock, IPPROTO_TCP, TCP_KEEPINTVL, &IntervalSeconds, sizeof(IntervalSeconds)) != 0 ||
			    ::setsockopt(Sock, IPPROTO_TCP, TCP_KEEPCNT, &ProbeCount, sizeof(ProbeCount)) != 0)
			{
				return false;
			}
			timeval SendTimeout{};
			SendTimeout.tv_sec = 3;
			return ::setsockopt(Sock, SOL_SOCKET, SO_SNDTIMEO, &SendTimeout, sizeof(SendTimeout)) == 0;
		}

		inline bool IsRecvTimeout(int ErrorCode)
		{
			return ErrorCode == EAGAIN || ErrorCode == EWOULDBLOCK;
		}
	}

#endif
