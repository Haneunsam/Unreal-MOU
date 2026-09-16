#pragma once
#include "Session/Session.h"
#include "UdpRelay/UdpRelay.h"
#include <atomic>
#include <memory>

namespace MOU::ServerRuntime
{
// Process-wide state. Existing session/room lock ordering is preserved.
// Public addresses are initialized before client worker threads start.
struct ServerContext
{
    SessionManager Sessions;
    std::atomic<bool> Running{true};
    std::string PublicIp;
    std::unique_ptr<UdpRelay> Relay;
    std::string RelayPublicIp;
    std::string RelayLanIp;
    SocketHandle ProbeSock = kInvalidSocket;
};
ServerContext& Context();
}
