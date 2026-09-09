#include "Server/Lobby/LobbyPlayerController.h"

#include "Engine/GameInstance.h"
#include "Server/Lobby/LoginWidgetBase.h"
#include "Server/ServerSubsystem.h"
#include "TeamProject_MOU.h"

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ShowLoginWidgetIfNeeded();
}

void ALobbyPlayerController::ShowLoginWidgetIfNeeded()
{
	if (!bAutoShowLoginWidget || !IsLocalPlayerController())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UServerSubsystem* Chat = GameInstance ? GameInstance->GetSubsystem<UServerSubsystem>() : nullptr;
	if (Chat == nullptr)
	{
		UE_LOG(LogTeamProject_MOU, Warning, TEXT("채팅 서브시스템을 찾지 못해 로그인 화면을 띄우지 못했다."));
		return;
	}

	// 이미 로그인되어 있으면 다시 묻지 않는다.
	// (방장이 방을 만들고 리슨서버로 여행해온 경우 ServerSubsystem 은 GameInstance 소유라
	//  레벨을 넘어가도 로그인 상태가 그대로 살아있다.)
	if (Chat->GetConnectionState() == EChatConnectionState::LoggedIn)
	{
		return;
	}

	UClass* WidgetClass = LoginWidgetClass ? LoginWidgetClass.Get() : ULoginWidgetBase::StaticClass();
	ULoginWidgetBase* LoginWidget = CreateWidget<ULoginWidgetBase>(this, WidgetClass);
	if (LoginWidget == nullptr)
	{
		return;
	}

	// 비워두면 위젯이 설정(Config/DefaultGame.ini)에서 읽는다. 컨트롤러가 굳이
	// 기본 주소를 알 필요는 없으므로, 예외적으로 지정했을 때만 덮어쓴다.
	LoginWidget->ServerHost = ServerHostOverride;
	LoginWidget->ServerPort = ServerPortOverride;
	LoginWidget->AddToViewport();

	// 로그인 화면은 마우스로 조작하므로 커서를 켜준다. NativeConstruct 가 입력 모드까지
	// 바꾸지는 않으므로(위젯은 게임 흐름을 몰라도 되게 만들었다) 여기서 챙긴다.
	SetShowMouseCursor(true);
}
