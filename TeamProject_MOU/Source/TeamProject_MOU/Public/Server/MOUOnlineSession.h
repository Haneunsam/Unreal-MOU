#pragma once
#include "CoreMinimal.h"
#include "GameFramework/OnlineSession.h"
#include "MOUOnlineSession.generated.h"
UCLASS()
class TEAMPROJECT_MOU_API UMOUOnlineSession : public UOnlineSession
{
    GENERATED_BODY()
public:
    // [HOSTLOST-008] 입장 완료 후 호스트가 끊기면 자동 맵 이동 대신 확인을 기다립니다.
    virtual void HandleDisconnect(UWorld* World, UNetDriver* NetDriver) override;
};
