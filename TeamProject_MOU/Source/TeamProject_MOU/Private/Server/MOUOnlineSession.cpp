#include "Server/MOUOnlineSession.h"
#include "Server/ServerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
// [HOSTLOST-008] 입장 완료 후 호스트가 끊기면 자동 맵 이동 대신 확인을 기다립니다.
void UMOUOnlineSession::HandleDisconnect(UWorld* World, UNetDriver* NetDriver)
{
    if (World)
        if (UGameInstance* GI = World->GetGameInstance())
            if (UServerSubsystem* Server = GI->GetSubsystem<UServerSubsystem>())
                if (Server->DeferHostDisconnect(World)) return;
    Super::HandleDisconnect(World, NetDriver);
}
