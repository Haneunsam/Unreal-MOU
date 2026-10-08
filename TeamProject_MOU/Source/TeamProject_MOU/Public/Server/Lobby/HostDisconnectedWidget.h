#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "HostDisconnectedWidget.generated.h"

UCLASS()
class TEAMPROJECT_MOU_API UHostDisconnectedWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    // [HOSTLOST-005] 별도 BP 설정 없이 호스트 연결 종료 안내와 확인 버튼을 만듭니다.
    virtual TSharedRef<SWidget> RebuildWidget() override;
    // [HOSTLOST-006] 확인을 서브시스템으로 전달합니다.
    FReply Confirm();
};
