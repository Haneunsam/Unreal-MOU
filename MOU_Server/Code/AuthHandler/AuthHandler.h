#pragma once
#include "Session/Session.h"
#include "Accounts/Accounts.h"

namespace MOU::ServerRuntime
{
	ELoginResult ToLoginResult(EAccountResult R);
	const char* AccountResultName(EAccountResult R);
	void SendLoginFailure(const SessionPtr& Session, ELoginResult Reason);
	bool HandleLoginReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRegisterReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
}
