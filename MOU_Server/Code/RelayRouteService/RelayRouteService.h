#pragma once
#include "Session/Session.h"
#include "UdpRelay/UdpRelay.h"

namespace MOU::ServerRuntime
{
	bool MakeRelayToken(FUdpRelayToken& OutToken);
	std::vector<RelayHostRoute> GetHostRelayRoutes(uint32_t RoomId);
	bool GetGuestRelayRoute(uint32_t RoomId, uint64_t GuestUserId, RelayGuestRoute& OutRoute);
	void SetRelayAddress(RelayHostRoute& Route, const std::string& Address);
	void SetRelayAddress(RelayGuestRoute& Route, const std::string& Address);
	void ReleaseRelayRoutesForRoom(uint32_t RoomId);
	void ReleaseRelayRouteForGuest(uint32_t RoomId, uint64_t GuestUserId);
	bool AllocateRelayRoutes(uint32_t RoomId, uint64_t HostUserId,
	                         const std::vector<uint64_t>& Recipients);
	void ClearRelayRoutes();
}
