#pragma once
#include <cstdint>
#include <string>
namespace MOU::ServerRuntime
{
struct ServerConfig
{
    bool bUseUpnp = false;
    bool bUseRelay = false;
    uint16_t RelayFirstPort = 10000;
    uint16_t RelayLastPort = 10127;
    // Borrow argv storage for the lifetime of RunServer.
    const char* PortArg = nullptr;
    const char* DbArg = nullptr;
    std::string PublicIp, RelayPublicIp, RelayLanIp;
};
bool ParseServerConfig(int argc, char** argv, ServerConfig& Config);
}
