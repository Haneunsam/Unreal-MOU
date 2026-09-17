#include "ServerContext/ServerContext.h"

namespace MOU::ServerRuntime
{
ServerContext& Context()
{
    static ServerContext Instance;
    return Instance;
}
}
