#include "Server/MOULocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Server/ServerSubsystem.h"
// [LATEJOIN-003] 일반 이동과 재접속을 구분하는 식별자를 모든 네트워크 로그인에 포함합니다.
FString UMOULocalPlayer::GetGameLoginOptions() const
{
    FString Options = Super::GetGameLoginOptions();
    if (const UGameInstance* GI = GetGameInstance())
        if (UServerSubsystem* Server = GI->GetSubsystem<UServerSubsystem>())
            Options += TEXT("?MOUPlaySession=") + Server->GetPlaySessionId();
    return Options;
}
