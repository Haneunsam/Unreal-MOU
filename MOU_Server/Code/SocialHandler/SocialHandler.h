#pragma once
#include "Session/Session.h"
#include "DirectMessages/DirectMessages.h"

namespace MOU::ServerRuntime
{
	EPresence ComputePresence(uint64_t UserId);
	std::string ResolveNickname(uint64_t UserId);
	void SendFriendUpdate(uint64_t Recipient, uint64_t AboutId, const std::string& AboutNick,
	                      EFriendState State, bool bRemoved);
	void BroadcastPresence(const SessionPtr& Subject, EPresence NewPresence);
	void BroadcastPresenceFor(const std::vector<uint64_t>& UserIds, EPresence NewPresence);
	void SyncFriendCaches(uint64_t A, uint64_t B, bool bNowFriends);
	bool HandleFriendListReq(const SessionPtr& Session, const char*, uint32_t);
	bool HandleFriendAddReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleFriendRespondReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleFriendRemoveReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	void DeliverDirectMessage(uint64_t ToUserId, const DmRow& Row, uint64_t PeerToUserId);
	bool HandleDirectMessageSend(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	void DeliverPendingDirectMessages(const SessionPtr& Session);
	bool HandleDmHistoryReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
}
