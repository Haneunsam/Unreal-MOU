#pragma once
#include "Session/Session.h"
#include "Accounts/Accounts.h"

namespace MOU::ServerRuntime
{
	ELoginResult ToLoginResult(EAccountResult R);
	const char* AccountResultName(EAccountResult R);
	void SendLoginFailure(const SessionPtr& Session, ELoginResult Reason);
	bool HandleLoginReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	// [AUTHUI-002] 가입 전 중복 확인 요청의 형식과 버전을 검사하고 조회 결과를 반환한다.
	bool HandleCheckLoginIdReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRegisterReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
}
