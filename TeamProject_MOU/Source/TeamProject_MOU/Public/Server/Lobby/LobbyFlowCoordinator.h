// MOU 로비 UI 흐름 관리자.
//
// 방 생성/목록/참여 응답을 개별 화면이 직접 받지 않게 한다. 화면은 스택에서
// 언제든 빠질 수 있지만 이 객체는 GameInstance 수명이라 늦게 도착한 서버 응답을
// 놓치지 않는다. UServerSubsystem 은 네트워크와 방 상태의 원본이고, 이 클래스는
// 그 결과를 UI 흐름에 맞는 이벤트로 바꾸는 얇은 계층이다.

#pragma once

#include "CoreMinimal.h"
#include "Server/Chat/ChatTypes.h"
#include "Server/Lobby/LobbyTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LobbyFlowCoordinator.generated.h"

class UServerSubsystem;

UENUM(BlueprintType)
enum class EMOULobbyFlowOperation : uint8
{
	Idle,
	CreatingRoom,
	JoiningRoom
};

DECLARE_MULTICAST_DELEGATE_FourParams(FOnLobbyFlowRoomCreateCompleted,
	bool /*bSuccess*/, int32 /*RoomId*/, EMOURoomResultBP /*Result*/, const FString& /*RoomPassword*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLobbyFlowRoomListReceived, const TArray<FMOURoomInfo>& /*Rooms*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyFlowRoomJoinCompleted,
	const FMOURoomJoinResult& /*Result*/, const FString& /*RoomPassword*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnLobbyFlowRoomEntered,
	int32 /*RoomId*/, bool /*bIsHost*/, const FString& /*RoomPassword*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyFlowReachabilityChecked,
	bool /*bReachable*/, const FString& /*Detail*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyFlowConnectionStateChanged,
	EChatConnectionState /*NewState*/, const FString& /*Detail*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLobbyFlowLoginCompleted, const FChatLoginResult& /*Result*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnLobbyFlowRoomMembersChanged,
	int32 /*RoomId*/, const TArray<FMOURoomMember>& /*Members*/, bool /*bAllReady*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyFlowRoomClosed,
	int32 /*RoomId*/, EMOURoomCloseReasonBP /*Reason*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLobbyFlowGameStarted,
	const FMOURoomJoinResult& /*Host*/, bool /*bIsHost*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLobbyFlowHostReady, const FMOURoomJoinResult& /*Host*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLobbyFlowTravelFailed, const FString& /*Reason*/);

/** 로비 서버 응답을 수신하고 화면 독립적인 흐름 이벤트로 중계한다. */
UCLASS()
class TEAMPROJECT_MOU_API ULobbyFlowCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** false 면 다른 방 생성/참여 요청이 이미 진행 중이거나 서버를 찾지 못한 것이다. */
	bool CreateRoom(const FString& Title, const FString& RoomPassword, int32 HostPort);
	bool JoinRoom(int32 RoomId, const FString& RoomPassword);
	bool RequestRoomList();

	UFUNCTION(BlueprintPure, Category = "MOU|Lobby|Flow")
	EMOULobbyFlowOperation GetOperation() const { return Operation; }

	UFUNCTION(BlueprintPure, Category = "MOU|Lobby|Flow")
	bool IsRoomRequestInFlight() const { return Operation != EMOULobbyFlowOperation::Idle; }

	FOnLobbyFlowRoomCreateCompleted OnRoomCreateCompleted;
	FOnLobbyFlowRoomListReceived OnRoomListReceived;
	FOnLobbyFlowRoomJoinCompleted OnRoomJoinCompleted;
	FOnLobbyFlowRoomEntered OnRoomEntered;
	FOnLobbyFlowReachabilityChecked OnReachabilityChecked;
	FOnLobbyFlowConnectionStateChanged OnConnectionStateChanged;
	FOnLobbyFlowLoginCompleted OnLoginCompleted;
	FOnLobbyFlowRoomMembersChanged OnRoomMembersChanged;
	FOnLobbyFlowRoomClosed OnRoomClosed;
	FOnLobbyFlowGameStarted OnGameStarted;
	FOnLobbyFlowHostReady OnHostReady;
	FOnLobbyFlowTravelFailed OnTravelFailed;

private:
	UFUNCTION()
	void HandleChatStateChanged(EChatConnectionState NewState, const FString& Detail);

	UFUNCTION()
	void HandleRoomCreated(bool bSuccess, int32 RoomId, EMOURoomResultBP Result);

	UFUNCTION()
	void HandleRoomListReceived(const TArray<FMOURoomInfo>& Rooms);

	UFUNCTION()
	void HandleRoomJoinCompleted(const FMOURoomJoinResult& Result);

	UFUNCTION()
	void HandleReachabilityChecked(bool bReachable, const FString& Detail);

	UFUNCTION()
	void HandleLoginCompleted(const FChatLoginResult& Result);

	UFUNCTION()
	void HandleRoomMembersChanged(int32 RoomId, const TArray<FMOURoomMember>& Members, bool bAllReady);

	UFUNCTION()
	void HandleRoomClosed(int32 RoomId, EMOURoomCloseReasonBP Reason);

	UFUNCTION()
	void HandleGameStarted(const FMOURoomJoinResult& Host, bool bIsHost);

	UFUNCTION()
	void HandleHostReady(const FMOURoomJoinResult& Host);

	void HandleTravelFailed(const FString& Reason);

	UServerSubsystem* GetServerSubsystem() const;
	void ClearPendingRequest();

	EMOULobbyFlowOperation Operation = EMOULobbyFlowOperation::Idle;
	FString PendingRoomPassword;
	FDelegateHandle TravelFailedHandle;
};
