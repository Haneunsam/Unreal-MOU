// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/RunTypes.h"
#include "Game/LevelSettlementState.h"
#include "TeamProject_MOUGameMode.generated.h"

class APlayerController;
class APlayerState;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ATeamProject_MOUGameMode : public AGameModeBase
{
	GENERATED_BODY()
    friend class FLateJoinRegressionTest;
	friend class FLobbyEntryReadyRegressionTest;

public:
	// [LOBBYLOAD-006] 예상 참여자의 소유 Pawn과 준비 보고를 검증하고 전원 준비를 집계합니다.
	void ReportLobbyEntryReady(class ATeamProject_MOUPlayerController* PC, int64 UserId);
	TMap<int64, TWeakObjectPtr<class ATeamProject_MOUPlayerController>> LobbyEntryReadyMembers;
    // [LATEJOIN-005] 새 접속의 식별자로 안전구역 합류 여부를 결정합니다.
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
    // [LATEJOIN-006] 일반 및 심리스 이동 후 관전 제한을 적용하거나 안전구역에서 해제합니다.
    virtual void GenericPlayerInitialization(AController* C) override;
	
	/** Constructor */
	ATeamProject_MOUGameMode();
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	// [SETTLEMENT-005] 접속 종료된 플레이어를 확인 대상에서 제거하고 남은 인원을 다시 검사합니다.
	virtual void Logout(AController* Exiting) override;

	// Server-side preparation checks use the configured lobby, not a physical warehouse actor.
	// [LATEJOIN-012] 지정된 안전구역을 판별하며 미설정 시 LobbyLevel을 사용합니다.
	bool IsLobbyLevel() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Run|Time")
	void AdvanceHalfDay();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Run|Players")
	void NotifyPlayerDeath();

	// 상환 UI/시스템이 실패를 확정한 뒤 호출합니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Run|Debt")
	void NotifyDebtPaymentFailed();

	// 배달/약탈 시스템이 성공 정산 데이터를 확정할 때 호출합니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Run|Settlement")
	bool NotifyLevelSettlement(const FLevelSettlementData& Result);

	// 타임아웃 UI/정산 연출이 끝난 뒤 서버에서 호출하여 로비로 이동합니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Run|Level Timer")
	void CompleteLevelTimeoutSequence();

	// [SETTLEMENT-001] 서버에서 플레이어별 정산 확인 상태를 갱신합니다.
	void SetSettlementConfirmation(APlayerController* PlayerController, bool bConfirmed);

protected:
	// 전멸 후 즉시 이동할 로비입니다. 사용하는 GameMode BP에서 지정해야 합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run|Travel")
	TSoftObjectPtr<UWorld> LobbyMap;

	// 목록에 현재 맵이 없으면 레벨 타이머가 시작되지 않습니다(로비 등).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run|Level Timer")
	TObjectPtr<class ULevelTimerConfigDataAsset> LevelTimerConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run|GameOver", meta = (ClampMin = "0.0", Units = "s"))
	float GameOverResetDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run|Level Timer", meta = (ClampMin = "0.05", Units = "s"))
	float LevelTimerUpdateInterval = 0.25f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Run|GameOver")
	void OnRunGameOver(ERunEndReason Reason);

	// [SETTLEMENT-006] 전원 확인 후 BP의 기존 저장 및 RequestTravel 흐름을 시작합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Run|Settlement")
	void OnAllPlayersConfirmedSettlement(APlayerController* TravelRequester);

private:
	UPROPERTY()
	TObjectPtr<class ARunState> RunState;

	UPROPERTY()
	TObjectPtr<class AGameCycleState> GameCycleState;

	UPROPERTY()
	TObjectPtr<class ALevelTimerState> LevelTimerState;

	UPROPERTY()
	TObjectPtr<class ALevelSettlementState> LevelSettlementState;

	UPROPERTY()
	TObjectPtr<class ADeliveryManager> DeliveryManager;

	FTimerHandle LevelTimerUpdateHandle;
	FTimerHandle ResetTimerHandle;
	bool bKillingPlayersForLevelTimeout = false;
	bool bTimeoutTravelStarted = false;
	bool bSettlementTravelStarted = false;
	TSet<TWeakObjectPtr<APlayerState>> ConfirmedSettlementPlayers;

	void TryStartLevelTimer();
	void UpdateLevelTimer();
	void KillAllPlayersByTimeLimit();
	void BeginLevelTimeoutSequence();
	void FinalizeFailedSettlement(ELevelSettlementReason Reason);
	void CheckAllPlayersDead();
	void FinishRun(ERunEndReason Reason);
	void DestroyPlayerOwnedItems();
	void ResetRunToDayOne();
	// [RUN-000] 플레이어 전멸 시 영속 런 데이터를 보존하고 로비로 즉시 이동합니다.
	void TravelToLobbyAfterWipe();
	void TravelToLobbyAfterTimeout();
	// [SETTLEMENT-002] 현재 접속 인원이 모두 확인했는지 검사합니다.
	void CheckAllPlayersConfirmedSettlement(APlayerController* PreferredRequester = nullptr);
	// [SETTLEMENT-003] 정상 정산이 끝난 뒤 BP의 기존 이동 흐름을 한 번만 시작합니다.
	void CompleteSettlementSequence(APlayerController* TravelRequester);
	void EnrichSettlementData(FLevelSettlementData& Result) const;
};




