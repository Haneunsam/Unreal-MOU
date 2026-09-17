#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	void SendToUsers(const std::vector<uint64_t>& UserIds, EOpcode Op,
	                 const void* Head, uint32_t HeadSize,
	                 const void* Tail, uint32_t TailSize);
	SessionPtr FindAuthedSession(uint64_t UserId);
}
