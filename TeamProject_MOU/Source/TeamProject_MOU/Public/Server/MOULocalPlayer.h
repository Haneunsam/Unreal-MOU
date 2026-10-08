#pragma once
#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "MOULocalPlayer.generated.h"
UCLASS()
class TEAMPROJECT_MOU_API UMOULocalPlayer : public ULocalPlayer
{
    GENERATED_BODY()
public:
    // [LATEJOIN-003] 일반 이동과 재접속을 구분하는 식별자를 모든 네트워크 로그인에 포함합니다.
    virtual FString GetGameLoginOptions() const override;
};
