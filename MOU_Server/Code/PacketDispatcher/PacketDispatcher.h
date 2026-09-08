#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	bool HandlePacket(const SessionPtr& Session, const PacketHeader& Header,
	                  const std::vector<char>& Body);
}
