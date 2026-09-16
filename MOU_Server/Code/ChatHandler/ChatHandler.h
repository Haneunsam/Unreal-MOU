#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	const char* ChannelName(EChatChannel Channel);
	void RouteChat(const SessionPtr& Sender, EChatChannel Channel,
	               const char* Text, uint16_t TextLen);
	bool HandleChatSend(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleSetDead(const SessionPtr& Session, const char* Body, uint32_t BodySize);
}
