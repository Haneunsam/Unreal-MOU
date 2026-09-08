#include "RelayRouteService/RelayRouteService.h"
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
	std::atomic<uint64_t> GNextRelayRouteId{ 1 };

	/** 한 참여자에게 줄 host/guest capability 는 반드시 분리한다. */
	struct FRelayRouteAssignment
	{
		uint64_t        GuestUserId = 0;
		RelayHostRoute  Host;
		RelayGuestRoute Guest;
	};

	std::mutex GRelayRoutesMutex;
	std::unordered_map<uint32_t, std::vector<FRelayRouteAssignment>> GRelayRoutesByRoom;

	bool MakeRelayToken(FUdpRelayToken& OutToken)
	{
		try
		{
			std::random_device Random;
			bool bAnyNonZero = false;
			for (std::uint8_t& Byte : OutToken)
			{
				Byte = static_cast<std::uint8_t>(Random());
				bAnyNonZero = bAnyNonZero || Byte != 0;
			}
			return bAnyNonZero;
		}
		catch (...)
		{
			return false;
		}
	}


	std::vector<RelayHostRoute> GetHostRelayRoutes(uint32_t RoomId)
	{
		std::vector<RelayHostRoute> Result;
		std::lock_guard<std::mutex> Lock(GRelayRoutesMutex);
		const auto It = GRelayRoutesByRoom.find(RoomId);
		if (It == GRelayRoutesByRoom.end())
		{
			return Result;
		}

		Result.reserve(It->second.size());
		for (const FRelayRouteAssignment& Assignment : It->second)
		{
			Result.push_back(Assignment.Host);
		}
		return Result;
	}


	bool GetGuestRelayRoute(uint32_t RoomId, uint64_t GuestUserId, RelayGuestRoute& OutRoute)
	{
		OutRoute = {};
		std::lock_guard<std::mutex> Lock(GRelayRoutesMutex);
		const auto It = GRelayRoutesByRoom.find(RoomId);
		if (It == GRelayRoutesByRoom.end())
		{
			return false;
		}

		for (const FRelayRouteAssignment& Assignment : It->second)
		{
			if (Assignment.GuestUserId == GuestUserId)
			{
				OutRoute = Assignment.Guest;
				return true;
			}
		}
		return false;
	}


	void SetRelayAddress(RelayHostRoute& Route, const std::string& Address)
	{
		CopyFixedString(Route.Address, kMaxAddressLen, Address);
	}


	void SetRelayAddress(RelayGuestRoute& Route, const std::string& Address)
	{
		CopyFixedString(Route.Address, kMaxAddressLen, Address);
	}


	void ReleaseRelayRoutesForRoom(uint32_t RoomId)
	{
		if (RoomId == 0)
		{
			return;
		}
		{
			std::lock_guard<std::mutex> Lock(GRelayRoutesMutex);
			GRelayRoutesByRoom.erase(RoomId);
		}
		if (Context().Relay)
		{
			Context().Relay->RemoveRoutesForRoom(RoomId);
		}
	}


	void ReleaseRelayRouteForGuest(uint32_t RoomId, uint64_t GuestUserId)
	{
		uint64_t RouteId = 0;
		{
			std::lock_guard<std::mutex> Lock(GRelayRoutesMutex);
			const auto It = GRelayRoutesByRoom.find(RoomId);
			if (It == GRelayRoutesByRoom.end())
			{
				return;
			}
			for (auto RouteIt = It->second.begin(); RouteIt != It->second.end(); ++RouteIt)
			{
				if (RouteIt->GuestUserId == GuestUserId)
				{
					RouteId = RouteIt->Host.RouteId;
					It->second.erase(RouteIt);
					break;
				}
			}
			if (It->second.empty())
			{
				GRelayRoutesByRoom.erase(It);
			}
		}
		if (RouteId != 0 && Context().Relay)
		{
			Context().Relay->RemoveRoute(RouteId);
		}
	}


	bool AllocateRelayRoutes(uint32_t RoomId, uint64_t HostUserId,
	                         const std::vector<uint64_t>& Recipients)
	{
		if (!Context().Relay || !Context().Relay->IsRunning() || Context().RelayPublicIp.empty())
		{
			return false;
		}

		std::vector<FRelayRouteAssignment> Created;
		Created.reserve(Recipients.size());
		for (const uint64_t UserId : Recipients)
		{
			if (UserId == HostUserId)
			{
				continue;
			}

			FUdpRelayToken HostToken{};
			FUdpRelayToken GuestToken{};
			FUdpRelayPortPair Ports{};
			const uint64_t RouteId = GNextRelayRouteId.fetch_add(1);
			if (RouteId == 0 || !MakeRelayToken(HostToken) || !MakeRelayToken(GuestToken) ||
				HostToken == GuestToken || !Context().Relay->CreateRoute(RouteId, HostToken, GuestToken, RoomId, Ports))
			{
				for (const FRelayRouteAssignment& Assignment : Created)
				{
					Context().Relay->RemoveRoute(Assignment.Host.RouteId);
				}
				return false;
			}

			FRelayRouteAssignment Assignment{};
			Assignment.GuestUserId = UserId;
			CopyFixedString(Assignment.Host.Address, kMaxAddressLen, Context().RelayPublicIp);
			Assignment.Host.HostPort = Ports.HostPort;
			Assignment.Host.RouteId = RouteId;
			std::copy(HostToken.begin(), HostToken.end(), Assignment.Host.HostToken);

			CopyFixedString(Assignment.Guest.Address, kMaxAddressLen, Context().RelayPublicIp);
			Assignment.Guest.GuestPort = Ports.GuestPort;
			Assignment.Guest.RouteId = RouteId;
			std::copy(GuestToken.begin(), GuestToken.end(), Assignment.Guest.GuestToken);
			Created.push_back(Assignment);
		}

		if (Created.empty())
		{
			return true;  // 혼자 시작한 방은 relay 경로가 필요 없다.
		}

		std::lock_guard<std::mutex> Lock(GRelayRoutesMutex);
		GRelayRoutesByRoom[RoomId] = std::move(Created);
		return true;
	}

	void ClearRelayRoutes()
	{
		std::lock_guard<std::mutex> Lock(GRelayRoutesMutex);
		GRelayRoutesByRoom.clear();
	}

}
