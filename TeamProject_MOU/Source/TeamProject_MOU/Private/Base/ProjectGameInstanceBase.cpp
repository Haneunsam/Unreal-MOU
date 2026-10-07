// Fill out your copyright notice in the Description page of Project Settings.

#include "Base/ProjectGameInstanceBase.h"
#include "Server/MOUOnlineSession.h"

#include "Base/ProjectGameStateBase.h"
#include "Engine/World.h"
#include "Subsystems/WarehouseDataSubsystem.h"
#include "UI/MOU_GameUserSettings.h"
#include "TeamProject_MOUPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "UObject/UnrealType.h"

// [LOBBYLOAD-001] 최초 로비 입장의 참여 예정 명단과 전용 로딩을 시작합니다.
void UProjectGameInstanceBase::BeginLobbyEntryWait(const TArray<int64>& Members)
{
	if (UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("MainLobby")) return;
	LobbyEntryExpectedMembers.Reset();
	for (int64 Member : Members) if (Member > 0) LobbyEntryExpectedMembers.Add(Member);
	bLobbyEntryWaiting = true;
	ShowLobbyEntryLoading();
}

// [LOBBYLOAD-002] 현재 월드에 전용 로딩 위젯을 표시합니다.
void UProjectGameInstanceBase::ShowLobbyEntryLoading()
{
	if (!bLobbyEntryWaiting || !GetWorld() || IsDedicatedServerInstance()) return;
	if (!LobbyEntryLoadingWidget)
	{
		if (UClass* WidgetClass = LobbyEntryLoadingClass.LoadSynchronous())
			LobbyEntryLoadingWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	}
	if (LobbyEntryLoadingWidget && !LobbyEntryLoadingSlate.IsValid() && GetGameViewportClient())
	{
		LobbyEntryLoadingSlate = LobbyEntryLoadingWidget->TakeWidget();
		GetGameViewportClient()->AddViewportWidgetContent(LobbyEntryLoadingSlate.ToSharedRef(), 10000);
	}
	// 기존 BP가 생성한 일반 로딩만 내립니다. 에셋이나 일반 이동 시 로직은 변경하지 않습니다.
	if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("LoadingWidgetRef")))
		if (UUserWidget* Legacy = Cast<UUserWidget>(Property->GetObjectPropertyValue_InContainer(this)))
			if (Legacy != LobbyEntryLoadingWidget) Legacy->RemoveFromParent();
}

// [LOBBYLOAD-003] 전원 준비 또는 접속 취소 시 전용 로딩을 정리합니다.
void UProjectGameInstanceBase::FinishLobbyEntryWait()
{
	// 마지막 프레임에 기존 BP가 다시 표시한 일반 로딩도 최초 입장에 한해 정리합니다.
	if (bLobbyEntryWaiting)
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("LoadingWidgetRef")))
			if (UUserWidget* Legacy = Cast<UUserWidget>(Property->GetObjectPropertyValue_InContainer(this)))
				if (Legacy != LobbyEntryLoadingWidget) Legacy->RemoveFromParent();
	bLobbyEntryWaiting = false;
	LobbyEntryExpectedMembers.Reset();
	if (LobbyEntryLoadingSlate.IsValid() && GetGameViewportClient())
		GetGameViewportClient()->RemoveViewportWidgetContent(LobbyEntryLoadingSlate.ToSharedRef());
	LobbyEntryLoadingSlate.Reset();
	if (LobbyEntryLoadingWidget) LobbyEntryLoadingWidget->RemoveFromParent();
	LobbyEntryLoadingWidget = nullptr;
}

void UProjectGameInstanceBase::Init()
{
	Super::Init();

	if (UMOU_GameUserSettings* UserSettings = UMOU_GameUserSettings::GetMOUGameUserSettings())
	{
		UserSettings->ApplySettings(false);
		UserSettings->ApplyAudioSettings(this);
	}

	// ServerTravel / OpenLevel 등으로 새로운 맵을 읽기 직전에 호출되는 델리게이트 등록
	PreLoadMapHandle =
		FCoreUObjectDelegates::PreLoadMap.AddUObject(
			this,
			&UProjectGameInstanceBase::HandlePreLoadedMap
		);

	// 새로운 월드의 맵 로딩이 끝난 직후 호출되는 델리게이트 등록
	PostLoadMapHandle =
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
			this,
			&UProjectGameInstanceBase::HandlePostLoadMapWithWorld
		);
}

void UProjectGameInstanceBase::Shutdown()
{
	FinishLobbyEntryWait();
	// GameInstance가 종료될 때 등록했던 델리게이트를 반드시 해제
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	}

	Super::Shutdown();
}

void UProjectGameInstanceBase::SaveStoredItems(const TArray<FStoredItemData>& InStoredItems)
{
	SavedStoredItems = InStoredItems;
	bWarehouseInitialized = true;
	if (UWarehouseDataSubsystem* Warehouse = GetSubsystem<UWarehouseDataSubsystem>())
	{
		Warehouse->NotifyStoredWarehouseChanged();
	}
}

void UProjectGameInstanceBase::SaveStoredItemInstances(const TArray<FStoredItemInstanceData>& InStoredItemInstances)
{
	SavedStoredItemInstances = InStoredItemInstances;
	if (UWarehouseDataSubsystem* Warehouse = GetSubsystem<UWarehouseDataSubsystem>())
	{
		Warehouse->NotifyStoredWarehouseChanged();
	}
}

void UProjectGameInstanceBase::ClearStoredItems()
{
	SavedStoredItems.Reset();
	SavedStoredItemInstances.Reset();
	if (UWarehouseDataSubsystem* Warehouse = GetSubsystem<UWarehouseDataSubsystem>())
	{
		Warehouse->NotifyStoredWarehouseChanged();
	}
}

void UProjectGameInstanceBase::SavePendingDeliveryData(const FDeliveryData& InDeliveryData)
{
	PendingDeliveryData = InDeliveryData;
	if (UWarehouseDataSubsystem* Warehouse = GetSubsystem<UWarehouseDataSubsystem>())
	{
		Warehouse->NotifyPendingDeliveryChanged();
	}

	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			if (ATeamProject_MOUPlayerController* PC = Cast<ATeamProject_MOUPlayerController>(World->GetFirstPlayerController()))
			{
				PC->ServerSaveWarehouseDelivery(InDeliveryData.SelectedItems);
			}
		}
	}
}

void UProjectGameInstanceBase::ClearPendingDeliveryData()
{
	PendingDeliveryData.SelectedItems.Reset();
	PendingDeliveryData.SelectedItemInstances.Reset();
	if (UWarehouseDataSubsystem* Warehouse = GetSubsystem<UWarehouseDataSubsystem>())
	{
		Warehouse->NotifyPendingDeliveryChanged();
	}
}

void UProjectGameInstanceBase::HandlePreLoadedMap(const FString& MapName)
{
	// 표시 전용 위젯의 플레이어/월드 문맥을 분리하고 Slate 화면은 유지합니다.
	if (LobbyEntryLoadingWidget) LobbyEntryLoadingWidget->SetPlayerContext(FLocalPlayerContext());
	// 새로운 맵의 실제 파일 로딩이 시작됨
	MapLoaded = false;
}

void UProjectGameInstanceBase::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	// 실제 새로운 월드가 정상 생성되었으면 맵 파일 로딩 완료
	// 이것만으로 게임 플레이 준비가 전부 끝난 것은 아니므로,
	// 이후 Blueprint에서 Pawn / GameState / 데이터 복구 상태를 추가 확인
	MapLoaded = (LoadedWorld != nullptr);
	if (bLobbyEntryWaiting && LoadedWorld)
	{
		if (UGameplayStatics::GetCurrentLevelName(LoadedWorld, true) == TEXT("LobbyLevel"))
		{
			if (LobbyEntryLoadingWidget)
				LobbyEntryLoadingWidget->SetPlayerContext(FLocalPlayerContext(GetFirstGamePlayer()));
			ShowLobbyEntryLoading();
		}
		else
			FinishLobbyEntryWait();
	}
}

void UProjectGameInstanceBase::SaveEconomyData()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	AProjectGameStateBase* ProjectGameState = World->GetGameState<AProjectGameStateBase>();

	if (!ProjectGameState || !ProjectGameState->HasAuthority())
	{
		return;
	}

	// 게임 상태의 Gold / Reputation / Debt / DebtCycle / Economy HalfDay를
	// 레벨 이동 전에 GameInstance에 임시 보관
	SavedGold = ProjectGameState->Gold;
	SavedReputation = ProjectGameState->Reputation;
	SavedDebt = ProjectGameState->CurrentDebt;
	SavedDebtCycle = ProjectGameState->DebtCycle;
	SavedEconomyCurrentHalfDay = ProjectGameState->GetEconomyCurrentHalfDay();
	bHaveSavedEconomyData = true;
	SavedDebtCycleStartHalfDay = ProjectGameState->DebtCycleStartHalfDay;
}

void UProjectGameInstanceBase::LoadEconomyData()
{
	if (!bHaveSavedEconomyData)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AProjectGameStateBase* ProjectGameState = World->GetGameState<AProjectGameStateBase>();
	if (!ProjectGameState || !ProjectGameState->HasAuthority())
	{
		return;
	}

	// 저장해 둔 경제 정보를 새 레벨의 게임 상태에 다시 적용
	ProjectGameState->DebtCycleStartHalfDay = SavedDebtCycleStartHalfDay;
	ProjectGameState->SetGold(SavedGold);
	ProjectGameState->SetReputation(SavedReputation);
	ProjectGameState->SetCurrentDebt(SavedDebt);
	ProjectGameState->SetDebtCycle(SavedDebtCycle);
	ProjectGameState->SetEconomyCurrentHalfDay(SavedEconomyCurrentHalfDay);
}

void UProjectGameInstanceBase::ResetRunData()
{
	SavedGold = 0;
	SavedReputation = 0;
	SavedDebt = 0;
	SavedDebtCycle = 0;
	SavedEconomyCurrentHalfDay = 0;
	bHaveSavedEconomyData = false;
	SavedDebtCycleStartHalfDay = 0;

	ClearStoredItems();
	bWarehouseInitialized = false;
	ClearPendingDeliveryData();
	SavedPlayerInventories.Reset();
	if (UWarehouseDataSubsystem* WarehouseSubsystem = GetSubsystem<UWarehouseDataSubsystem>())
	{
		WarehouseSubsystem->InitializeWarehouseFromDataAsset();
	}
}


// [HOSTLOST-009] 확인 후 복귀를 지원하는 네트워크 세션을 생성합니다.
TSubclassOf<UOnlineSession> UProjectGameInstanceBase::GetOnlineSessionClass()
{
    return UMOUOnlineSession::StaticClass();
}
