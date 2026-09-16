#pragma once

#include "CoreMinimal.h"
#include "TeamProject_MOUPlayerController.h"
#include "LobbyPlayerController.generated.h"

class ULoginWidgetBase;

/** 로비 진입 시 로그인 화면을 관리한다. 연결 상태는 ServerSubsystem이 유지한다. */
UCLASS()
class TEAMPROJECT_MOU_API ALobbyPlayerController : public ATeamProject_MOUPlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/**
	 * PIE/게임 시작 시 채팅 로그인 화면을 자동으로 띄울지.
	 *
	 * 이미 로그인되어 있으면(예: 방장이 방을 만들고 리슨서버로 여행해온 경우)
	 * 다시 묻지 않는다 — ShowLoginWidgetIfNeeded() 가 UServerSubsystem 의 연결 상태로 판단한다.
	 */
	UPROPERTY(EditAnywhere, Category = "MOU|Chat")
	bool bAutoShowLoginWidget = true;

	/** 자동으로 띄울 로그인 위젯 클래스. 비워두면 ULoginWidgetBase 의 C++ 기본 레이아웃을 쓴다. */
	UPROPERTY(EditAnywhere, Category = "MOU|Chat")
	TSubclassOf<ULoginWidgetBase> LoginWidgetClass;

	/**
	 * 이 컨트롤러만 다른 채팅 서버를 보게 할 때 쓰는 **예외용** 값. 평소에는 비워둔다.
	 *
	 * 비어 있으면 Config/DefaultGame.ini 의 팀 공유 주소(UMOUServerSettings)를 쓴다.
	 * 예전에는 여기에 127.0.0.1 이 박혀 있었는데, 그 값은 "이 게임이 돌고 있는 PC" 라는
	 * 뜻이라 서버를 켜지 않은 팀원은 자기 자신에게 접속하려다 항상 실패했다.
	 * 그래서 기본값을 없애고, 주소를 아는 곳을 설정 한 군데로 모았다.
	 */
	UPROPERTY(EditAnywhere, Category = "MOU|Chat")
	FString ServerHostOverride;

	/** 0 이면 ServerHostOverride 와 마찬가지로 설정값을 쓴다. */
	UPROPERTY(EditAnywhere, Category = "MOU|Chat")
	int32 ServerPortOverride = 0;

private:
	void ShowLoginWidgetIfNeeded();
};
