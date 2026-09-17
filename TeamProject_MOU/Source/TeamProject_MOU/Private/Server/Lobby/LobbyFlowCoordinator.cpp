#include "Server/Lobby/LobbyFlowCoordinator.h"

#include "Engine/GameInstance.h"
#include "Server/ServerSubsystem.h"

void ULobbyFlowCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UServerSubsystem>();

	if (UServerSubsystem* Server = GetServerSubsystem())
	{
		Server->OnChatStateChanged.AddDynamic(this, &ULobbyFlowCoordinator::HandleChatStateChanged);
		Server->OnRoomCreated.AddDynamic(this, &ULobbyFlowCoordinator::HandleRoomCreated);
		Server->OnRoomListReceived.AddDynamic(this, &ULobbyFlowCoordinator::HandleRoomListReceived);
		Server->OnRoomJoinCompleted.AddDynamic(this, &ULobbyFlowCoordinator::HandleRoomJoinCompleted);
		Server->OnReachabilityChecked.AddDynamic(this, &ULobbyFlowCoordinator::HandleReachabilityChecked);
		Server->OnChatLoginCompleted.AddDynamic(this, &ULobbyFlowCoordinator::HandleLoginCompleted);
		Server->OnRoomMembersChanged.AddDynamic(this, &ULobbyFlowCoordinator::HandleRoomMembersChanged);
		Server->OnRoomClosed.AddDynamic(this, &ULobbyFlowCoordinator::HandleRoomClosed);
		Server->OnRoomGameStarted.AddDynamic(this, &ULobbyFlowCoordinator::HandleGameStarted);
		Server->OnRoomHostReady.AddDynamic(this, &ULobbyFlowCoordinator::HandleHostReady);
		TravelFailedHandle = Server->OnTravelFailed.AddUObject(this, &ULobbyFlowCoordinator::HandleTravelFailed);
	}
}

void ULobbyFlowCoordinator::Deinitialize()
{
	if (UServerSubsystem* Server = GetServerSubsystem())
	{
		Server->OnChatStateChanged.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleChatStateChanged);
		Server->OnRoomCreated.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleRoomCreated);
		Server->OnRoomListReceived.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleRoomListReceived);
		Server->OnRoomJoinCompleted.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleRoomJoinCompleted);
		Server->OnReachabilityChecked.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleReachabilityChecked);
		Server->OnChatLoginCompleted.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleLoginCompleted);
		Server->OnRoomMembersChanged.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleRoomMembersChanged);
		Server->OnRoomClosed.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleRoomClosed);
		Server->OnRoomGameStarted.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleGameStarted);
		Server->OnRoomHostReady.RemoveDynamic(this, &ULobbyFlowCoordinator::HandleHostReady);
		if (TravelFailedHandle.IsValid())
		{
			Server->OnTravelFailed.Remove(TravelFailedHandle);
			TravelFailedHandle.Reset();
		}
	}

	ClearPendingRequest();
	Super::Deinitialize();
}

bool ULobbyFlowCoordinator::CreateRoom(const FString& Title, const FString& RoomPassword, int32 HostPort)
{
	UServerSubsystem* Server = GetServerSubsystem();
	if (Server == nullptr || Operation != EMOULobbyFlowOperation::Idle)
	{
		return false;
	}

	Operation = EMOULobbyFlowOperation::CreatingRoom;
	PendingRoomPassword = RoomPassword;
	Server->CreateRoom(Title, RoomPassword, HostPort);
	return true;
}

bool ULobbyFlowCoordinator::JoinRoom(int32 RoomId, const FString& RoomPassword)
{
	UServerSubsystem* Server = GetServerSubsystem();
	if (Server == nullptr || Operation != EMOULobbyFlowOperation::Idle)
	{
		return false;
	}

	Operation = EMOULobbyFlowOperation::JoiningRoom;
	PendingRoomPassword = RoomPassword;
	Server->JoinRoom(RoomId, RoomPassword);
	return true;
}

bool ULobbyFlowCoordinator::RequestRoomList()
{
	if (UServerSubsystem* Server = GetServerSubsystem())
	{
		Server->RequestRoomList();
		return true;
	}
	return false;
}

void ULobbyFlowCoordinator::HandleChatStateChanged(EChatConnectionState NewState, const FString& Detail)
{
	if (NewState == EChatConnectionState::Disconnected && Operation != EMOULobbyFlowOperation::Idle)
	{
		const EMOULobbyFlowOperation InterruptedOperation = Operation;
		const FString UsedPassword = PendingRoomPassword;
		ClearPendingRequest();

		if (InterruptedOperation == EMOULobbyFlowOperation::CreatingRoom)
		{
			OnRoomCreateCompleted.Broadcast(false, 0, EMOURoomResultBP::NotAuthed, UsedPassword);
		}
		else if (InterruptedOperation == EMOULobbyFlowOperation::JoiningRoom)
		{
			FMOURoomJoinResult Failed;
			Failed.Result = EMOURoomResultBP::NotAuthed;
			OnRoomJoinCompleted.Broadcast(Failed, UsedPassword);
		}
	}

	OnConnectionStateChanged.Broadcast(NewState, Detail);
}

void ULobbyFlowCoordinator::HandleRoomCreated(bool bSuccess, int32 RoomId, EMOURoomResultBP Result)
{
	if (Operation != EMOULobbyFlowOperation::CreatingRoom)
	{
		return;
	}

	const FString UsedPassword = PendingRoomPassword;
	ClearPendingRequest();

	if (bSuccess)
	{
		if (UServerSubsystem* Server = GetServerSubsystem())
		{
			Server->SetRoomPassword(UsedPassword);
		}
	}

	OnRoomCreateCompleted.Broadcast(bSuccess, RoomId, Result, UsedPassword);
	if (bSuccess)
	{
		OnRoomEntered.Broadcast(RoomId, true, UsedPassword);
	}
}

void ULobbyFlowCoordinator::HandleRoomListReceived(const TArray<FMOURoomInfo>& Rooms)
{
	OnRoomListReceived.Broadcast(Rooms);
}

void ULobbyFlowCoordinator::HandleRoomJoinCompleted(const FMOURoomJoinResult& Result)
{
	if (Operation != EMOULobbyFlowOperation::JoiningRoom)
	{
		return;
	}

	const FString UsedPassword = PendingRoomPassword;
	ClearPendingRequest();

	if (Result.bSuccess)
	{
		if (UServerSubsystem* Server = GetServerSubsystem())
		{
			Server->SetRoomPassword(UsedPassword);
		}
	}

	OnRoomJoinCompleted.Broadcast(Result, UsedPassword);
	if (Result.bSuccess)
	{
		OnRoomEntered.Broadcast(Result.RoomId, false, UsedPassword);
	}
}

void ULobbyFlowCoordinator::HandleReachabilityChecked(bool bReachable, const FString& Detail)
{
	OnReachabilityChecked.Broadcast(bReachable, Detail);
}

void ULobbyFlowCoordinator::HandleLoginCompleted(const FChatLoginResult& Result)
{
	OnLoginCompleted.Broadcast(Result);
}

void ULobbyFlowCoordinator::HandleRoomMembersChanged(
	int32 RoomId,
	const TArray<FMOURoomMember>& Members,
	bool bAllReady)
{
	OnRoomMembersChanged.Broadcast(RoomId, Members, bAllReady);
}

void ULobbyFlowCoordinator::HandleRoomClosed(int32 RoomId, EMOURoomCloseReasonBP Reason)
{
	OnRoomClosed.Broadcast(RoomId, Reason);
}

void ULobbyFlowCoordinator::HandleGameStarted(const FMOURoomJoinResult& Host, bool bIsHost)
{
	OnGameStarted.Broadcast(Host, bIsHost);
}

void ULobbyFlowCoordinator::HandleHostReady(const FMOURoomJoinResult& Host)
{
	OnHostReady.Broadcast(Host);
}

void ULobbyFlowCoordinator::HandleTravelFailed(const FString& Reason)
{
	OnTravelFailed.Broadcast(Reason);
}

UServerSubsystem* ULobbyFlowCoordinator::GetServerSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UServerSubsystem>();
	}
	return nullptr;
}

void ULobbyFlowCoordinator::ClearPendingRequest()
{
	Operation = EMOULobbyFlowOperation::Idle;
	PendingRoomPassword.Empty();
}
