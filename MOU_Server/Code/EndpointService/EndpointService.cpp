#include "EndpointService/EndpointService.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"

#include "Rooms/Rooms.h"
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

	bool IsPrivateAddress(const std::string& Address)
	{
		unsigned A = 0, B = 0, C = 0, D = 0;
		if (std::sscanf(Address.c_str(), "%u.%u.%u.%u", &A, &B, &C, &D) != 4)
		{
			return false;   // 파싱 실패. 함부로 바꾸지 않는다
		}

		if (A == 10)  return true;                        // 10.0.0.0/8
		if (A == 127) return true;                        // 루프백
		if (A == 172 && B >= 16 && B <= 31) return true;  // 172.16.0.0/12
		if (A == 192 && B == 168) return true;            // 192.168.0.0/16
		if (A == 169 && B == 254) return true;            // 링크로컬
		if (A == 100 && B >= 64 && B <= 127) return true; // CGNAT 100.64.0.0/10
		return false;
	}


	std::string ResolveHostAddress(const std::string& PeerAddress, bool bVerbose)
	{
		if (Context().PublicIp.empty() || !IsPrivateAddress(PeerAddress))
		{
			return PeerAddress;
		}

		if (bVerbose)
		{
			ServerLog::Print("[방 주소] 호스트가 서버와 같은 네트워크에 있다(%s). "
			            "외부 참가자를 위해 %s 로 바꿔 기록한다.\n",
			            PeerAddress.c_str(), Context().PublicIp.c_str());
		}
		return Context().PublicIp;
	}


	bool SendHostProbe(const std::string& Address, uint16_t Port, uint32_t Nonce)
	{
		if (Context().ProbeSock == kInvalidSocket || Address.empty() || Port == 0)
		{
			return false;
		}

		sockaddr_in Dest{};
		Dest.sin_family = AF_INET;
		Dest.sin_port   = ::htons(Port);
		if (::inet_pton(AF_INET, Address.c_str(), &Dest.sin_addr) != 1)
		{
			return false;
		}

		HostProbeDatagram Datagram{};
		Datagram.Magic = kHostProbeMagic;
		Datagram.Nonce = Nonce;

		const int Sent = ::sendto(Context().ProbeSock, reinterpret_cast<const char*>(&Datagram),
		                          static_cast<int>(sizeof(Datagram)), 0,
		                          reinterpret_cast<const sockaddr*>(&Dest), sizeof(Dest));
		return Sent == static_cast<int>(sizeof(Datagram));
	}


	void HandleClientEndpointDatagram(const ClientEndpointDatagram& Datagram,
	                                  const sockaddr_in& From)
	{
		char AddrText[INET_ADDRSTRLEN] = {};
		if (::inet_ntop(AF_INET, &From.sin_addr, AddrText, sizeof(AddrText)) == nullptr)
		{
			return;
		}
		// ★ 방 주소와 같은 규칙을 여기에도 건다.
		//   서버와 같은 공유기 안에 있는 사람은 출발지가 사설 주소(대개 게이트웨이)로
		//   보인다. 그 값을 그대로 punch 대상으로 넘기면 방장은 자기 LAN 의 엉뚱한
		//   기기로 쏘게 되고, 구멍은 아무 데도 안 뚫린다.
		//   실제로 Player1 의 엔드포인트가 192.168.35.1 로 관측되고 있었다.
		// 방 주소와 같은 규칙. 조용히 한다 — 등록은 자주 오고, 방 생성용 문구를
		// 여기서 찍으면 엉뚱한 맥락에서 도배된다.
		const std::string SourceAddress = ResolveHostAddress(AddrText, /*bVerbose=*/false);
		const uint16_t    SourcePort    = ::ntohs(From.sin_port);

		SessionPtr Target;
		Context().Sessions.ForEach([&](const SessionPtr& Session)
		{
			if (Session->bAuthed && Session->UserId == Datagram.UserId)
			{
				Target = Session;
			}
		});

		if (!Target)
		{
			ServerLog::Print("[엔드포인트] 모르는 UserId %llu 의 등록을 버린다 (%s:%u)\n",
			            static_cast<unsigned long long>(Datagram.UserId),
			            SourceAddress.c_str(), SourcePort);
			return;
		}

		// ★ 위조 차단. 남의 UserId 를 적어 보내면 여기서 걸린다 —
		//   그 사람의 TCP 연결은 다른 IP 에서 오고 있기 때문이다.
		if (ResolveHostAddress(Target->PeerAddress, /*bVerbose=*/false) != SourceAddress)
		{
			ServerLog::Print("[거부] 엔드포인트 등록의 출발지가 세션과 다르다: %s (세션은 %s)\n",
			            SourceAddress.c_str(), Target->PeerAddress.c_str());
			return;
		}

		Target->GameEndpointAddress = SourceAddress;
		Target->GameEndpointPort    = SourcePort;

		// ★ 이 사람이 방장이면 방의 공인 후보를 관측값으로 덮는다. (v10)
		//
		//   방을 만들 때 적은 공인 후보는 UPnP 가 알려준 포트다. 그런데 홀펀칭으로
		//   실제 뚫리는 구멍은 **동적 바인딩의 포트**이고, UPnP 정적 매핑이 그 번호를
		//   점유하고 있으면 둘이 달라진다(실측: UPnP 있을 때 1035, 없을 때 7777).
		//   참여자가 붙어야 하는 곳은 뚫린 쪽이다.
		Rooms::UpdateHostEndpoint(Target->UserId, SourceAddress, SourcePort);

		ServerLog::Print("[엔드포인트] %s(%llu) 의 게임 주소를 %s:%u 로 관측했다\n",
		            Target->Name.c_str(),
		            static_cast<unsigned long long>(Target->UserId),
		            SourceAddress.c_str(), SourcePort);

		ClientEndpointAckBody Ack{};
		Ack.Nonce     = Datagram.Nonce;
		Ack.Port      = SourcePort;
		Ack.bObserved = 1;
		CopyFixedString(Ack.Address, kMaxAddressLen, SourceAddress);
		SendPacket(Target->Sock, EOpcode::ClientEndpointAck, &Ack, sizeof(Ack));
	}


	void UdpReceiveLoop()
	{
		ServerLog::Print("[UDP] 엔드포인트 수신 스레드 시작\n");

		while (Context().Running)
		{
			char Buffer[512];
			sockaddr_in From{};
			socklen_t   FromLen = sizeof(From);

			const int Read = ::recvfrom(Context().ProbeSock, Buffer, static_cast<int>(sizeof(Buffer)), 0,
			                            reinterpret_cast<sockaddr*>(&From), &FromLen);
			if (Read < 0)
			{
				if (IsRecvTimeout(LastNetError()))
				{
					continue;   // 타임아웃은 정상이다. Context().Running 을 다시 보라는 뜻일 뿐
				}
				if (!Context().Running)
				{
					break;
				}
				continue;
			}

			if (Read < static_cast<int>(sizeof(ClientEndpointDatagram)))
			{
				continue;   // 우리 것이 아니다
			}

			ClientEndpointDatagram Datagram{};
			std::memcpy(&Datagram, Buffer, sizeof(Datagram));
			if (Datagram.Magic != kClientEndpointMagic)
			{
				continue;
			}

			HandleClientEndpointDatagram(Datagram, From);
		}

		ServerLog::Print("[UDP] 엔드포인트 수신 스레드 종료\n");
	}


	bool PushCandidate(std::vector<HostCandidate>& Out, const std::string& Address,
	                   uint16_t Port, EHostAddrKind Kind)
	{
		if (Address.empty() || Address.size() >= kMaxAddressLen || Port == 0)
		{
			return false;
		}
		if (Out.size() >= kMaxHostCandidates)
		{
			return false;
		}
		// 같은 주소가 두 번 들어가는 것을 막는다. 방장이 서버와 같은 LAN 이면
		// 공인 후보와 사설 후보가 같은 값이 될 수 있다.
		for (const HostCandidate& C : Out)
		{
			if (Address == C.Address && Port == C.Port)
			{
				return false;
			}
		}

		HostCandidate C{};
		CopyFixedString(C.Address, kMaxAddressLen, Address);
		C.Port = Port;
		C.Kind = static_cast<uint8_t>(Kind);
		Out.push_back(C);
		return true;
	}


	std::vector<HostCandidate> BuildHostCandidates(const std::string& PeerAddress,
	                                               const std::string& ReportedLanAddress,
	                                               uint16_t HostPort)
	{
		std::vector<HostCandidate> Candidates;

		PushCandidate(Candidates, ResolveHostAddress(PeerAddress), HostPort, EHostAddrKind::Public);

		if (!ReportedLanAddress.empty())
		{
			if (IsPrivateAddress(ReportedLanAddress))
			{
				PushCandidate(Candidates, ReportedLanAddress, HostPort, EHostAddrKind::Lan);
			}
			else
			{
				// 조용히 버리지 않고 남긴다. 공인 주소를 사설 자리에 넣어 보내는 것은
				// 버그이거나 장난이고, 둘 다 알아야 한다.
				ServerLog::Print("[거부] 사설이 아닌 주소를 LAN 후보로 신고했다: %s\n",
				            ReportedLanAddress.c_str());
			}
		}

		return Candidates;
	}


	uint8_t FillCandidates(HostCandidate (&Dest)[kMaxHostCandidates],
	                       const std::vector<HostCandidate>& Src)
	{
		uint8_t Count = 0;
		for (const HostCandidate& C : Src)
		{
			if (Count >= kMaxHostCandidates)
			{
				break;
			}
			Dest[Count++] = C;
		}
		return Count;
	}


	bool HandleHostProbeReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(HostProbeReqBody))
		{
			return true;
		}

		HostProbeReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const bool bSent = SendHostProbe(Session->PeerAddress, Req.Port, Req.Nonce);

		ServerLog::Print("[프로브] %s(%llu) 의 %s:%u 로 UDP 발사 %s\n",
		            Session->Name.c_str(),
		            static_cast<unsigned long long>(Session->UserId),
		            Session->PeerAddress.c_str(), Req.Port,
		            bSent ? "성공" : "실패");

		HostProbeSentBody Ack{};
		Ack.Nonce = Req.Nonce;
		Ack.bSent = bSent ? 1 : 0;
		return SendPacket(Session->Sock, EOpcode::HostProbeSent, &Ack, sizeof(Ack));
	}


	bool HandleRoomReachabilityReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(RoomReachabilityReqBody))
		{
			return true;
		}

		RoomReachabilityReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const bool bReachable = (Req.bReachable != 0);

		// ★ 세션에 먼저 적는다. 이것이 진실의 원본이다.
		//
		//   프로브는 **방을 만들기 전에** 돈다 — 그때만 게임 포트가 비어 있기 때문이다.
		//   그래서 이 신고가 RoomCreateReq 보다 먼저 오는 것이 오히려 정상이다.
		//   방에만 적으려 하면 "아직 방이 없다"(NotInRoom)로 버려지고, 정작 방에는
		//   표시가 안 붙는다. 실제로 그렇게 조용히 버려지고 있었다.
		Session->bLanOnly               = !bReachable;
		Session->bHasReachabilityReport = true;

		ServerLog::Print("[도달성] %s(%llu) 는 %s\n", Session->Name.c_str(),
		            static_cast<unsigned long long>(Session->UserId),
		            bReachable ? "외부에서 들어올 수 있다"
		                       : "**같은 LAN 에서만** 들어올 수 있다 (공유기가 포워딩을 안 한다)");

		// 이미 방이 있으면 그 방도 같이 갱신한다. 방을 만든 뒤 다시 확인한 경우다.
		// 방이 없으면 위에 적어둔 값이 RoomCreateReq 에서 얹힌다 — 오류가 아니다.
		uint32_t RoomId = 0;
		if (Rooms::SetReachability(Session->UserId, bReachable, RoomId) == ERoomResult::Success)
		{
			ServerLog::Print("[도달성]   방 #%u 에 반영했다.\n", RoomId);
		}
		return true;
	}

}
