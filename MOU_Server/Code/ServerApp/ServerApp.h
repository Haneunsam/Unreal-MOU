#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	void OnInterrupt(int);
	void ClientThread(SessionPtr Session);
	int RunServer(int argc, char** argv);
}
