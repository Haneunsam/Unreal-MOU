#include "SocialHandler/SocialHandler.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"
#include "SessionMessaging/SessionMessaging.h"
#include "Accounts/Accounts.h"
#include "DirectMessages/DirectMessages.h"
#include "Friends/Friends.h"
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

	EPresence ComputePresence(uint64_t UserId)
	{
		bool bOnline = false;

		Context().Sessions.ForEach([&](const SessionPtr& S)
		{
			if (S->bAuthed && S->UserId == UserId)
			{
				bOnline = true;
			}
		});

		if (!bOnline)
		{
			return EPresence::Offline;
		}

		// ★ "대기중"(방에 앉아 있음)은 따로 두지 않는다. 친구 입장에서 온라인과
		//   대기중은 둘 다 "지금 말 걸어도 된다" 라서 행동이 안 바뀐다.
		ERoomState State = ERoomState::Waiting;
		if (Rooms::GetRoomStateOf(UserId, State) && State == ERoomState::InGame)
		{
			return EPresence::InGame;
		}

		return EPresence::Online;
	}


	std::string ResolveNickname(uint64_t UserId)
	{
		if (const SessionPtr S = FindAuthedSession(UserId))
		{
			return S->Name;
		}

		std::string Nick;
		Accounts::GetNickname(UserId, Nick);
		return Nick;
	}


	void SendFriendUpdate(uint64_t Recipient, uint64_t AboutId, const std::string& AboutNick,
	                      EFriendState State, bool bRemoved)
	{
		const SessionPtr Target = FindAuthedSession(Recipient);
		if (!Target)
		{
			return;
		}

		FriendUpdateBody Body{};
		Body.UserId   = AboutId;
		Body.State    = static_cast<uint8_t>(State);
		Body.bRemoved = bRemoved ? 1 : 0;
		// 지워진 상대의 상태는 의미가 없다. 굳이 세션을 뒤지지 않는다.
		Body.Presence = bRemoved ? static_cast<uint8_t>(EPresence::Offline)
		                         : static_cast<uint8_t>(ComputePresence(AboutId));
		CopyFixedString(Body.Nickname, kMaxNameLen, AboutNick);

		SendPacket(Target->Sock, EOpcode::FriendUpdate, &Body, sizeof(Body));
	}


	void BroadcastPresence(const SessionPtr& Subject, EPresence NewPresence)
	{
		if (!Subject || !Subject->bAuthed)
		{
			return;
		}

		// 복사본을 받는다 — 아래에서 세션을 순회하는 동안 남이 목록을 고칠 수
		// 있고, 그러면 반복자가 깨진다(Session.h 의 CopyFriendIds 주석).
		const std::vector<uint64_t> FriendIds = Subject->CopyFriendIds();

		if (FriendIds.empty())
		{
			return;
		}

		FriendPresenceBody Body{};
		Body.UserId   = Subject->UserId;
		Body.Presence = static_cast<uint8_t>(NewPresence);

		SendToUsers(FriendIds, EOpcode::FriendPresence, &Body, sizeof(Body), nullptr, 0);
	}


	void BroadcastPresenceFor(const std::vector<uint64_t>& UserIds, EPresence NewPresence)
	{
		for (const uint64_t Id : UserIds)
		{
			if (const SessionPtr S = FindAuthedSession(Id))
			{
				BroadcastPresence(S, NewPresence);
			}
		}
	}


	void SyncFriendCaches(uint64_t A, uint64_t B, bool bNowFriends)
	{
		if (const SessionPtr SA = FindAuthedSession(A))
		{
			bNowFriends ? SA->AddFriendId(B) : SA->RemoveFriendId(B);
		}
		if (const SessionPtr SB = FindAuthedSession(B))
		{
			bNowFriends ? SB->AddFriendId(A) : SB->RemoveFriendId(A);
		}
	}


	bool HandleFriendListReq(const SessionPtr& Session, const char*, uint32_t)
	{
		if (!Session->bAuthed)
		{
			return true;   // 로그인 전에는 조용히 무시한다
		}

		std::vector<FriendRow> Rows;
		if (!Friends::GetList(Session->UserId, Rows))
		{
			FriendListAckBody Empty{};
			return SendPacket(Session->Sock, EOpcode::FriendListAck, &Empty, sizeof(Empty));
		}

		// 안 읽은 개수는 DM 쪽이 안다. 친구마다 COUNT 를 돌리면 친구 수만큼
		// 질의가 나가므로 한 번에 받아 맞춰 넣는다.
		std::vector<UnreadCount> Unread;
		DirectMessages::GetUnreadCounts(Session->UserId, Unread);

		std::vector<FriendEntry> Entries;
		Entries.reserve(Rows.size());

		for (const FriendRow& Row : Rows)
		{
			FriendEntry E{};
			E.UserId = Row.UserId;
			E.State  = static_cast<uint8_t>(Row.State);
			CopyFixedString(E.Nickname, kMaxNameLen, Row.Nickname);

			// ★ 아직 친구가 아닌 상대의 접속 상태는 알려주지 않는다.
			//   신청만 걸어두면 남의 온/오프라인을 훔쳐볼 수 있게 되는 것을 막는다.
			E.Presence = (Row.State == EFriendState::Friend)
				? static_cast<uint8_t>(ComputePresence(Row.UserId))
				: static_cast<uint8_t>(EPresence::Offline);

			for (const UnreadCount& U : Unread)
			{
				if (U.PeerUserId == Row.UserId)
				{
					// 65535 를 넘을 일은 없지만, 넘으면 잘라서 보낸다.
					E.UnreadCount = static_cast<uint16_t>(
						U.Count > 0xFFFFu ? 0xFFFFu : U.Count);
					break;
				}
			}

			Entries.push_back(E);
		}

		FriendListAckBody Head{};
		Head.Count = static_cast<uint16_t>(Entries.size());

		return SendPacket2(Session->Sock, EOpcode::FriendListAck,
		                   &Head, sizeof(Head),
		                   Entries.empty() ? nullptr : Entries.data(),
		                   static_cast<uint32_t>(Entries.size() * sizeof(FriendEntry)));
	}


	bool HandleFriendAddReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		auto SendAck = [&](EFriendResult R, uint64_t TargetId)
		{
			FriendAddAckBody Ack{};
			Ack.TargetUserId = TargetId;
			Ack.bSuccess     = (R == EFriendResult::Success) ? 1 : 0;
			Ack.Result       = static_cast<uint8_t>(R);
			return SendPacket(Session->Sock, EOpcode::FriendAddAck, &Ack, sizeof(Ack));
		};

		if (!Session->bAuthed)
		{
			return SendAck(EFriendResult::NotAuthed, 0);
		}
		if (BodySize < sizeof(FriendAddReqBody))
		{
			return SendAck(EFriendResult::InvalidFormat, 0);
		}

		FriendAddReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const std::string Query = ReadFixedString(Req.Query, kMaxFriendQueryLen);

		uint64_t    TargetId = 0;
		std::string TargetNick;
		bool        bBecameFriends = false;

		const EFriendResult R =
			Friends::Add(Session->UserId, Query, TargetId, TargetNick, bBecameFriends);

		if (R != EFriendResult::Success)
		{
			ServerLog::Print("[친구] %s 의 신청 실패: \"%s\" 사유=%u\n",
			            Session->Name.c_str(), Query.c_str(), static_cast<unsigned>(R));
			return SendAck(R, 0);
		}

		if (bBecameFriends)
		{
			// 맞신청이라 그 자리에서 친구가 됐다. 양쪽 다 갱신해야 한다.
			SyncFriendCaches(Session->UserId, TargetId, /*bNowFriends=*/true);
			SendFriendUpdate(TargetId, Session->UserId, Session->Name,
			                 EFriendState::Friend, /*bRemoved=*/false);
			SendFriendUpdate(Session->UserId, TargetId, TargetNick,
			                 EFriendState::Friend, /*bRemoved=*/false);

			ServerLog::Print("[친구] %s <-> %s 맞신청으로 친구 성립\n",
			            Session->Name.c_str(), TargetNick.c_str());
		}
		else
		{
			// 대기 상태. 상대가 접속해 있으면 지금 알려준다.
			// 오프라인이면 다음 로그인 때 FriendListAck 에 PendingIncoming 으로 들어간다.
			if (const SessionPtr Target = FindAuthedSession(TargetId))
			{
				FriendRequestIncomingBody Note{};
				Note.FromUserId = Session->UserId;
				CopyFixedString(Note.FromNickname, kMaxNameLen, Session->Name);
				SendPacket(Target->Sock, EOpcode::FriendRequestIncoming, &Note, sizeof(Note));
			}

			ServerLog::Print("[친구] %s -> %s 신청\n",
			            Session->Name.c_str(), TargetNick.c_str());
		}

		return SendAck(EFriendResult::Success, TargetId);
	}


	bool HandleFriendRespondReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(FriendRespondReqBody))
		{
			return true;
		}

		FriendRespondReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const bool bAccept = (Req.bAccept != 0);

		const EFriendResult R = Friends::Respond(Session->UserId, Req.FromUserId, bAccept);
		if (R != EFriendResult::Success)
		{
			ServerLog::Print("[친구] %s 의 응답 실패: from=%llu 사유=%u\n",
			            Session->Name.c_str(),
			            static_cast<unsigned long long>(Req.FromUserId),
			            static_cast<unsigned>(R));
			return true;
		}

		// ★ 상대가 오프라인이어도 이름이 필요하다 — 이 이름은 **나에게 가는**
		//   FriendUpdate 에 실린다(ResolveNickname 주석).
		const std::string FromNick = ResolveNickname(Req.FromUserId);

		if (bAccept)
		{
			SyncFriendCaches(Session->UserId, Req.FromUserId, /*bNowFriends=*/true);

			// 양쪽에 보낸다. 수락한 쪽도 자기 화면을 직접 고치지 않고 이 신호로
			// 갱신하게 해야 두 클라가 같은 그림을 본다.
			SendFriendUpdate(Req.FromUserId, Session->UserId, Session->Name,
			                 EFriendState::Friend, /*bRemoved=*/false);
			SendFriendUpdate(Session->UserId, Req.FromUserId, FromNick,
			                 EFriendState::Friend, /*bRemoved=*/false);

			ServerLog::Print("[친구] %s 가 %llu 의 신청을 수락\n",
			            Session->Name.c_str(),
			            static_cast<unsigned long long>(Req.FromUserId));
		}
		else
		{
			// 거절은 줄이 사라진 것이라 양쪽 목록에서 지워야 한다.
			SendFriendUpdate(Req.FromUserId, Session->UserId, Session->Name,
			                 EFriendState::Friend, /*bRemoved=*/true);
			SendFriendUpdate(Session->UserId, Req.FromUserId, FromNick,
			                 EFriendState::Friend, /*bRemoved=*/true);

			ServerLog::Print("[친구] %s 가 %llu 의 신청을 거절\n",
			            Session->Name.c_str(),
			            static_cast<unsigned long long>(Req.FromUserId));
		}

		return true;
	}


	bool HandleFriendRemoveReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(FriendRemoveReqBody))
		{
			return true;
		}

		FriendRemoveReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const EFriendResult R = Friends::Remove(Session->UserId, Req.TargetUserId);
		if (R != EFriendResult::Success)
		{
			return true;
		}

		SyncFriendCaches(Session->UserId, Req.TargetUserId, /*bNowFriends=*/false);

		// 오프라인 상대여도 이름이 필요하다(ResolveNickname 주석).
		const std::string TargetNick = ResolveNickname(Req.TargetUserId);

		SendFriendUpdate(Req.TargetUserId, Session->UserId, Session->Name,
		                 EFriendState::Friend, /*bRemoved=*/true);
		SendFriendUpdate(Session->UserId, Req.TargetUserId, TargetNick,
		                 EFriendState::Friend, /*bRemoved=*/true);

		ServerLog::Print("[친구] %s 가 %llu 를 삭제\n",
		            Session->Name.c_str(),
		            static_cast<unsigned long long>(Req.TargetUserId));
		return true;
	}


	void DeliverDirectMessage(uint64_t ToUserId, const DmRow& Row, uint64_t PeerToUserId)
	{
		const SessionPtr Target = FindAuthedSession(ToUserId);
		if (!Target)
		{
			return;
		}

		DirectMessageBody Head{};
		Head.MessageId  = Row.MessageId;
		Head.FromUserId = Row.FromUserId;
		Head.ToUserId   = PeerToUserId;
		Head.Timestamp  = Row.Timestamp;
		Head.TextLen    = static_cast<uint16_t>(Row.Text.size());

		SendPacket2(Target->Sock, EOpcode::DirectMessage,
		            &Head, sizeof(Head),
		            Row.Text.empty() ? nullptr : Row.Text.data(),
		            static_cast<uint32_t>(Row.Text.size()));
	}


	bool HandleDirectMessageSend(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(DirectMessageSendBody))
		{
			return true;
		}

		DirectMessageSendBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		// 본문은 구조체 뒤에 이어붙어 온다. 길이가 실제로 도착했는지 확인한다 —
		// 이걸 안 보면 위조된 TextLen 으로 남의 메모리를 읽게 된다.
		if (BodySize < sizeof(Req) + Req.TextLen || Req.TextLen == 0)
		{
			return true;
		}
		if (Req.TextLen > kMaxTextLen)
		{
			return true;
		}

		const char* Text = Body + sizeof(Req);

		// ★★ 친구가 아니면 보낼 수 없다. 클라가 아무 UserId 에게나 쏘는 것을
		//   서버가 막아야 한다 — UI 가 친구만 보여준다는 것은 방어가 아니다.
		if (!Friends::AreFriends(Session->UserId, Req.TargetUserId))
		{
			ServerLog::Print("[거부] %s -> %llu DM: 친구가 아니다\n",
			            Session->Name.c_str(),
			            static_cast<unsigned long long>(Req.TargetUserId));
			return true;
		}

		uint64_t MessageId = 0;
		int64_t  Timestamp = 0;

		if (!DirectMessages::Send(Session->UserId, Req.TargetUserId,
		                          Text, Req.TextLen, MessageId, Timestamp))
		{
			ServerLog::Print("[오류] DM 저장 실패: %s -> %llu\n",
			            Session->Name.c_str(),
			            static_cast<unsigned long long>(Req.TargetUserId));
			return true;
		}

		DmRow Row;
		Row.MessageId  = MessageId;
		Row.FromUserId = Session->UserId;
		Row.Timestamp  = Timestamp;
		Row.Text.assign(Text, Req.TextLen);

		// 받는 사람에게. 오프라인이면 조용히 넘어가고, 다음 로그인 때
		// 밀린 메시지로 받는다(저장은 위에서 이미 끝났다).
		DeliverDirectMessage(Req.TargetUserId, Row, Req.TargetUserId);

		// 보낸 사람에게도 되돌려준다(위 함수 주석).
		DeliverDirectMessage(Session->UserId, Row, Req.TargetUserId);

		ServerLog::Print("[DM] %s -> %llu (%u바이트)%s\n",
		            Session->Name.c_str(),
		            static_cast<unsigned long long>(Req.TargetUserId),
		            Req.TextLen,
		            FindAuthedSession(Req.TargetUserId) ? "" : " [오프라인 - 보관]");
		return true;
	}


	void DeliverPendingDirectMessages(const SessionPtr& Session)
	{
		std::vector<DmRow> Pending;

		// 상한을 둔다. 오래 안 들어온 계정에 수천 통이 쌓여 있으면 로그인
		// 직후 그것을 한꺼번에 밀어넣게 되고, 그동안 다른 처리가 밀린다.
		// 넘친 것은 대화창을 열 때 기록 조회로 따라온다.
		if (!DirectMessages::GetPending(Session->UserId, kDmPageSize * 4, Pending))
		{
			return;
		}

		for (const DmRow& Row : Pending)
		{
			DeliverDirectMessage(Session->UserId, Row, Session->UserId);
		}

		if (!Pending.empty())
		{
			ServerLog::Print("[DM] %s 에게 밀린 메시지 %zu통 전달\n",
			            Session->Name.c_str(), Pending.size());
		}
	}


	bool HandleDmHistoryReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(DmHistoryReqBody))
		{
			return true;
		}

		DmHistoryReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		// ★ 친구가 아니면 기록도 볼 수 없다. 보내는 쪽만 막고 조회를 열어두면
		//   과거에 친구였던 사람의 대화를 계속 들여다볼 수 있게 된다.
		if (!Friends::AreFriends(Session->UserId, Req.PeerUserId))
		{
			DmHistoryAckBody Empty{};
			Empty.PeerUserId = Req.PeerUserId;
			return SendPacket(Session->Sock, EOpcode::DmHistoryAck, &Empty, sizeof(Empty));
		}

		std::vector<DmRow> Rows;
		bool bHasMore = false;

		if (!DirectMessages::GetHistory(Session->UserId, Req.PeerUserId,
		                                Req.BeforeMessageId, kDmPageSize, Rows, bHasMore))
		{
			DmHistoryAckBody Empty{};
			Empty.PeerUserId = Req.PeerUserId;
			return SendPacket(Session->Sock, EOpcode::DmHistoryAck, &Empty, sizeof(Empty));
		}

		// 최신 페이지 = 대화창을 연 것 = 읽음(위 ★★).
		if (Req.BeforeMessageId == 0)
		{
			DirectMessages::MarkRead(Session->UserId, Req.PeerUserId);
		}

		// --- 가변 길이 본문을 이어붙인다 ---
		//
		// ★ DmEntry 는 고정 크기가 아니다. 뒤에 TextLen 바이트가 따라오므로
		//   받는 쪽도 배열 인덱싱이 아니라 순회로 읽어야 한다(ChatProtocol.h).
		std::vector<char> Tail;

		for (const DmRow& Row : Rows)
		{
			DmEntry E{};
			E.MessageId  = Row.MessageId;
			E.FromUserId = Row.FromUserId;
			E.Timestamp  = Row.Timestamp;
			E.TextLen    = static_cast<uint16_t>(Row.Text.size());

			const char* Head = reinterpret_cast<const char*>(&E);
			Tail.insert(Tail.end(), Head, Head + sizeof(E));
			Tail.insert(Tail.end(), Row.Text.begin(), Row.Text.end());
		}

		DmHistoryAckBody Ack{};
		Ack.PeerUserId = Req.PeerUserId;
		Ack.Count      = static_cast<uint16_t>(Rows.size());
		Ack.bHasMore   = bHasMore ? 1 : 0;

		return SendPacket2(Session->Sock, EOpcode::DmHistoryAck,
		                   &Ack, sizeof(Ack),
		                   Tail.empty() ? nullptr : Tail.data(),
		                   static_cast<uint32_t>(Tail.size()));
	}

}
