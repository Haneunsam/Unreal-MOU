#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	// [REJOIN-017] 인증된 제어 요청과 중도 입장 준비 응답을 핸들러에 전달한다.
	bool HandlePacket(const SessionPtr& Session, const PacketHeader& Header,
	                  const std::vector<char>& Body);
}
