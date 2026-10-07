#include "Server/Lobby/LobbyPlayerController.h"

#include "Engine/GameInstance.h"
#include "Server/Lobby/LoginWidgetBase.h"
#include "Server/Lobby/LobbyWidgetBase.h"
#include "Server/ServerSubsystem.h"
#include "TeamProject_MOU.h"

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ShowLoginWidgetIfNeeded();
}

// [LOBBYRETURN-002] 로그인 여부와 방 상태에 따라 로그인 화면 또는 메인로비 화면을 표시한다.
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

	// 로그인된 사용자는 방에 속해 있지 않을 때 메인로비 화면을 연다.
	if (Chat->GetConnectionState() == EChatConnectionState::LoggedIn)
	{
		// 방에 들어간 상태의 여행에서는 로비 화면을 추가로 열지 않는다.
		if (Chat->GetCurrentRoomId() != 0)
		{
			return;
		}

		UClass* LobbyClass = LoadClass<ULobbyWidgetBase>(
			nullptr,
			TEXT("/Game/02_JSY/MainLobby/WBP_LobbyWidget.WBP_LobbyWidget_C"));

		if (LobbyClass == nullptr)
		{
			UE_LOG(LogTeamProject_MOU, Error,
				TEXT("메인로비 위젯 클래스를 불러오지 못했습니다."));
			return;
		}

		if (ULobbyWidgetBase* LobbyWidget =
			CreateWidget<ULobbyWidgetBase>(this, LobbyClass))
		{
			LobbyWidget->AddToViewport();
		}
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
