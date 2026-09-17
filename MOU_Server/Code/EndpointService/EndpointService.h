#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	bool IsPrivateAddress(const std::string& Address);
	std::string ResolveHostAddress(const std::string& PeerAddress, bool bVerbose = true);
	bool SendHostProbe(const std::string& Address, uint16_t Port, uint32_t Nonce);
	void HandleClientEndpointDatagram(const ClientEndpointDatagram& Datagram,
	                                  const sockaddr_in& From);
	void UdpReceiveLoop();
	bool PushCandidate(std::vector<HostCandidate>& Out, const std::string& Address,
	                   uint16_t Port, EHostAddrKind Kind);
	std::vector<HostCandidate> BuildHostCandidates(const std::string& PeerAddress,
	                                               const std::string& ReportedLanAddress,
	                                               uint16_t HostPort);
	uint8_t FillCandidates(HostCandidate (&Dest)[kMaxHostCandidates],
	                       const std::vector<HostCandidate>& Src);
	bool HandleHostProbeReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomReachabilityReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
}
