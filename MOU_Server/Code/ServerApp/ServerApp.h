#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	void OnInterrupt(int);
	// [NETLIVE-003] 패킷을 처리하고 연결 종료 원인과 계정 해제 완료를 기록한다.
	void ClientThread(SessionPtr Session);
	// [NETLIVE-002] 서버를 초기화하고 종료 감지 설정을 적용한 연결을 세션으로 등록한다.
	int RunServer(int argc, char** argv);
}
