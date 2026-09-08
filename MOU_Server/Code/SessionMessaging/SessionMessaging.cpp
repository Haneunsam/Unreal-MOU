#include "SessionMessaging/SessionMessaging.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"

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

	void SendToUsers(const std::vector<uint64_t>& UserIds, EOpcode Op,
	                 const void* Head, uint32_t HeadSize,
	                 const void* Tail, uint32_t TailSize)
	{
		if (UserIds.empty())
		{
			return;
		}

		Context().Sessions.ForEach([&](const SessionPtr& Target)
		{
			if (!Target->bAuthed)
			{
				return;
			}
			if (std::find(UserIds.begin(), UserIds.end(), Target->UserId) == UserIds.end())
			{
				return;
			}
			SendPacket2(Target->Sock, Op, Head, HeadSize, Tail, TailSize);
		});
	}


	SessionPtr FindAuthedSession(uint64_t UserId)
	{
		SessionPtr Found;

		Context().Sessions.ForEach([&](const SessionPtr& S)
		{
			if (S->bAuthed && S->UserId == UserId)
			{
				Found = S;
			}
		});

		return Found;
	}

}
