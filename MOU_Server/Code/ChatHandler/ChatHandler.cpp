#include "ChatHandler/ChatHandler.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"

#include "ChatLog/ChatLog.h"
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

	const char* ChannelName(EChatChannel Channel)
	{
		switch (Channel)
		{
		case EChatChannel::All:     return "전체";
		case EChatChannel::Team:    return "팀";
		case EChatChannel::Dead:    return "사망";
		case EChatChannel::Whisper: return "귓속말";
		case EChatChannel::System:  return "시스템";
		default:                    return "알수없음";
		}
	}


	void RouteChat(const SessionPtr& Sender, EChatChannel Channel,
	               const char* Text, uint16_t TextLen)
	{
		// 발화 자격 검증. 클라이언트가 보낸 채널 값을 그대로 믿지 않는다.
		if (Channel == EChatChannel::Dead && !Sender->bDead)
		{
			ServerLog::Print("[거부] %s(%llu) 가 살아있는 상태로 사망 채널에 발화 시도\n",
			            Sender->Name.c_str(), static_cast<unsigned long long>(Sender->UserId));
			return;
		}
		if (Channel == EChatChannel::System)
		{
			return;   // 시스템 메시지는 서버만 만든다
		}

		ChatBroadcastBody Out{};
		Out.SenderUserId = Sender->UserId;                        // 서버 보관값
		Out.Timestamp    = static_cast<int64_t>(std::time(nullptr));
		Out.TextLen      = TextLen;
		Out.Channel      = static_cast<uint8_t>(Channel);
		CopyFixedString(Out.SenderName, kMaxNameLen, Sender->Name);   // 서버 보관값

		int DeliverCount = 0;
		Context().Sessions.ForEach([&](const SessionPtr& Target)
		{
			if (!Target->bAuthed)
			{
				return;
			}

			bool bDeliver = false;
			switch (Channel)
			{
			case EChatChannel::All:
				bDeliver = true;
				break;
			case EChatChannel::Team:
				bDeliver = (Target->TeamId == Sender->TeamId);
				break;
			case EChatChannel::Dead:
				bDeliver = Target->bDead;          // 죽은 사람에게만
				break;
			case EChatChannel::Whisper:
				bDeliver = false;                  // 9단계
				break;
			default:
				break;
			}

			if (bDeliver)
			{
				SendPacket2(Target->Sock, EOpcode::ChatBroadcast,
				            &Out, sizeof(Out), Text, TextLen);
				++DeliverCount;
			}
		});

		ServerLog::Print("[%s] %s: %.*s  (수신 %d명)\n",
		            ChannelName(Channel), Sender->Name.c_str(),
		            static_cast<int>(TextLen), Text, DeliverCount);

		// 수신자가 0명이어도 기록은 남긴다.
		// "아무도 못 들었지만 말한 것은 사실" 이므로 나중에 신고/관전 조회 때 필요하다.
		// Enqueue 는 큐에 넣기만 하고 바로 돌아오므로 여기서 디스크를 기다리지 않는다.
		ChatLog::Enqueue(Out.Timestamp, Sender->UserId, Sender->Name,
		                 static_cast<uint8_t>(Channel), Sender->TeamId, Text, TextLen);
	}


	bool HandleChatSend(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed)
		{
			return true;   // 로그인 전 채팅은 무시하되 연결은 유지
		}
		if (BodySize < sizeof(ChatSendBody))
		{
			return false;
		}

		ChatSendBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		// 선언한 TextLen 과 실제 도착한 바이트 수가 맞는지 확인한다.
		// 이 검사가 없으면 TextLen 을 크게 속여 버퍼 밖을 읽게 만들 수 있다.
		if (Req.TextLen > kMaxTextLen)
		{
			return false;
		}
		if (sizeof(ChatSendBody) + Req.TextLen != BodySize)
		{
			return false;
		}

		const char* Text = Body + sizeof(ChatSendBody);
		RouteChat(Session, static_cast<EChatChannel>(Req.Channel), Text, Req.TextLen);
		return true;
	}


	bool HandleSetDead(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (BodySize < sizeof(SetDeadBody))
		{
			return false;
		}

		SetDeadBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		// 지금은 테스트 편의를 위해 클라이언트가 자기 상태를 바꾸도록 열어둔다.
		// 8단계에서 리슨서버 전용 인증 연결에서만 받도록 잠근다.
		Session->bDead = (Req.bDead != 0);
		ServerLog::Print("[상태] %s -> %s\n", Session->Name.c_str(),
		            Session->bDead ? "사망" : "생존");
		return true;
	}

}
