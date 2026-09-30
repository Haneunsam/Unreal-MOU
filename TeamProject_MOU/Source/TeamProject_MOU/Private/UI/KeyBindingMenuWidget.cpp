// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/KeyBindingMenuWidget.h"
#include "UI/MOU_GameUserSettings.h"
#include "Components/Button.h"
#include "Components/InputKeySelector.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "Input/Events.h"

void UKeyBindingMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 포커스 가능하도록 설정 (ESC 키 이벤트 수신을 위함)
	SetIsFocusable(true);

	// 네비게이션 버튼 바인딩
	if (Button_Back)
	{
		Button_Back->OnClicked.AddDynamic(this, &UKeyBindingMenuWidget::OnBackClicked);
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.AddDynamic(this, &UKeyBindingMenuWidget::OnBackClicked);
	}
	if (Button_ResetDefaults)
	{
		Button_ResetDefaults->OnClicked.AddDynamic(this, &UKeyBindingMenuWidget::OnResetDefaultsClicked);
	}

	// 키 중복 경고 모달 버튼 바인딩
	if (Button_ConflictConfirm)
	{
		Button_ConflictConfirm->OnClicked.AddDynamic(this, &UKeyBindingMenuWidget::OnConflictConfirmClicked);
	}
	if (Button_ConflictCancel)
	{
		Button_ConflictCancel->OnClicked.AddDynamic(this, &UKeyBindingMenuWidget::OnConflictCancelClicked);
	}

	// 키 메타데이터 초기화 및 모달 숨김
	InitializeActionBindingInfos();
	HideConflictModal();

	// 키 선택기 바인딩: 1. 이동 (WASD)
	if (KeySelector_MoveForward)
	{
		KeySelector_MoveForward->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnMoveForwardKeySelected);
	}
	if (KeySelector_MoveBackward)
	{
		KeySelector_MoveBackward->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnMoveBackwardKeySelected);
	}
	if (KeySelector_MoveLeft)
	{
		KeySelector_MoveLeft->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnMoveLeftKeySelected);
	}
	if (KeySelector_MoveRight)
	{
		KeySelector_MoveRight->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnMoveRightKeySelected);
	}

	// 키 선택기 바인딩: 2. 기본 행동
	if (KeySelector_Jump)
	{
		KeySelector_Jump->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnJumpKeySelected);
	}
	if (KeySelector_Sprint)
	{
		KeySelector_Sprint->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnSprintKeySelected);
	}
	if (KeySelector_Interact)
	{
		KeySelector_Interact->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnInteractKeySelected);
	}
	if (KeySelector_GrabDrop)
	{
		KeySelector_GrabDrop->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnGrabDropKeySelected);
	}
	if (KeySelector_Use)
	{
		KeySelector_Use->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnUseKeySelected);
	}
	if (KeySelector_Throw)
	{
		KeySelector_Throw->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnThrowKeySelected);
	}
	if (KeySelector_Slap)
	{
		KeySelector_Slap->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnSlapKeySelected);
	}
	if (KeySelector_Light)
	{
		KeySelector_Light->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnLightKeySelected);
	}
	if (KeySelector_LightColor)
	{
		KeySelector_LightColor->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnLightColorKeySelected);
	}
	if (KeySelector_Slot1)
	{
		KeySelector_Slot1->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnSlot1KeySelected);
	}
	if (KeySelector_Slot2)
	{
		KeySelector_Slot2->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnSlot2KeySelected);
	}
	if (KeySelector_Slot3)
	{
		KeySelector_Slot3->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnSlot3KeySelected);
	}
	if (KeySelector_Quest)
	{
		KeySelector_Quest->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnQuestKeySelected);
	}
	if (KeySelector_ViewEconomy)
	{
		KeySelector_ViewEconomy->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnViewEconomyKeySelected);
	}
	if (KeySelector_Emote)
	{
		KeySelector_Emote->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnEmoteKeySelected);
	}
	if (KeySelector_RadioPower)
	{
		KeySelector_RadioPower->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnRadioPowerKeySelected);
	}
	if (KeySelector_RadioTransmit)
	{
		KeySelector_RadioTransmit->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnRadioTransmitKeySelected);
	}
	if (KeySelector_VoiceMute)
	{
		KeySelector_VoiceMute->OnKeySelected.AddDynamic(this, &UKeyBindingMenuWidget::OnVoiceMuteKeySelected);
	}

	// 현재 설정된 키 로드
	RefreshUIFromSettings();

	bIsClosing = false;
	if (Anim_Open)
	{
		PlayAnimation(Anim_Open);
	}
	BP_OnMenuOpen();
}

FReply UKeyBindingMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC 키를 눌렀을 때
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// 중복 경고 모달이 열려 있다면 모달 취소 먼저 처리
		if (bIsConflictModalOpen)
		{
			OnConflictCancelClicked();
			return FReply::Handled();
		}

		// 키 설정 팝업 창을 닫고 부모 메뉴로 복귀
		CloseMenu();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UMOU_GameUserSettings* UKeyBindingMenuWidget::GetUserSettings() const
{
	return UMOU_GameUserSettings::GetMOUGameUserSettings();
}

void UKeyBindingMenuWidget::RefreshUIFromSettings()
{
	UMOU_GameUserSettings* UserSettings = GetUserSettings();
	if (!UserSettings)
	{
		return;
	}

	// 1. 이동 (WASD)
	if (KeySelector_MoveForward)
	{
		KeySelector_MoveForward->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Move_Forward"), EKeys::W)));
	}
	if (KeySelector_MoveBackward)
	{
		KeySelector_MoveBackward->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Move_Backward"), EKeys::S)));
	}
	if (KeySelector_MoveLeft)
	{
		KeySelector_MoveLeft->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Move_Left"), EKeys::A)));
	}
	if (KeySelector_MoveRight)
	{
		KeySelector_MoveRight->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Move_Right"), EKeys::D)));
	}

	// 2. 기본 행동
	if (KeySelector_Jump)
	{
		KeySelector_Jump->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Jump"), EKeys::SpaceBar)));
	}
	if (KeySelector_Sprint)
	{
		KeySelector_Sprint->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Sprint"), EKeys::LeftShift)));
	}

	// 3. 상호작용 및 상호동작
	if (KeySelector_Interact)
	{
		KeySelector_Interact->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Interact"), EKeys::F)));
	}
	if (KeySelector_GrabDrop)
	{
		KeySelector_GrabDrop->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Grab_Drop"), EKeys::E)));
	}

	// 4. 전투 및 도구 사용
	if (KeySelector_Use)
	{
		KeySelector_Use->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Use"), EKeys::LeftMouseButton)));
	}
	if (KeySelector_Throw)
	{
		KeySelector_Throw->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Throw"), EKeys::Q)));
	}
	if (KeySelector_Slap)
	{
		KeySelector_Slap->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Slap"), EKeys::T)));
	}

	// 5. 손전등 및 조명
	if (KeySelector_Light)
	{
		KeySelector_Light->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Light"), EKeys::Four)));
	}
	if (KeySelector_LightColor)
	{
		KeySelector_LightColor->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_LightColor"), EKeys::Five)));
	}

	// 6. 인벤토리 퀵슬롯
	if (KeySelector_Slot1)
	{
		KeySelector_Slot1->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Slot1"), EKeys::One)));
	}
	if (KeySelector_Slot2)
	{
		KeySelector_Slot2->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Slot2"), EKeys::Two)));
	}
	if (KeySelector_Slot3)
	{
		KeySelector_Slot3->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_Slot3"), EKeys::Three)));
	}

	// 7. 퀘스트, 경제, 메뉴
	if (KeySelector_Quest)
	{
		KeySelector_Quest->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_CapsLock"), EKeys::CapsLock)));
	}
	if (KeySelector_ViewEconomy)
	{
		KeySelector_ViewEconomy->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_ViewEcnomoy"), EKeys::V)));
	}
	if (KeySelector_Emote)
	{
		KeySelector_Emote->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("IA_EmoteToggle"), EKeys::Tab)));
	}

	// 8. 무전 및 음성 대화
	if (KeySelector_RadioPower)
	{
		KeySelector_RadioPower->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Radio_Power"), EKeys::Z)));
	}
	if (KeySelector_RadioTransmit)
	{
		KeySelector_RadioTransmit->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Radio_Transmit"), EKeys::X)));
	}
	if (KeySelector_VoiceMute)
	{
		KeySelector_VoiceMute->SetSelectedKey(FInputChord(UserSettings->GetCustomKeyBinding(FName("Voice_Mute"), EKeys::C)));
	}
}

void UKeyBindingMenuWidget::ResetToDefaults()
{
	if (UMOU_GameUserSettings* UserSettings = GetUserSettings())
	{
		UserSettings->ClearAllCustomKeyBindings();
		UserSettings->SaveSettings();
		RefreshUIFromSettings();
	}
}

void UKeyBindingMenuWidget::CloseMenu()
{
	if (bIsClosing)
	{
		return;
	}
	bIsClosing = true;

	BP_OnMenuClose();

	if (Anim_Close && Anim_Close->GetEndTime() > 0.05f)
	{
		PlayAnimation(Anim_Close);
		const float AnimLength = FMath::Min(Anim_Close->GetEndTime(), 0.5f);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CloseTimerHandle, this, &UKeyBindingMenuWidget::FinishCloseMenu, AnimLength, false);
		}
		else
		{
			FinishCloseMenu();
		}
	}
	else
	{
		FinishCloseMenu();
	}
}

void UKeyBindingMenuWidget::FinishCloseMenu()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
	}
	OnKeyBindingMenuClosed.Broadcast();
	RemoveFromParent();
}

void UKeyBindingMenuWidget::RebindActionKey(FName ActionName, const FKey& NewKey)
{
	if (UMOU_GameUserSettings* UserSettings = GetUserSettings())
	{
		UserSettings->SetCustomKeyBinding(ActionName, NewKey);
		UserSettings->SaveSettings();
	}
}

// -----------------------------------------------------------------------------
// 키 중복 검사 및 바인딩 메타데이터 제어
// -----------------------------------------------------------------------------

void UKeyBindingMenuWidget::InitializeActionBindingInfos()
{
	if (ActionBindingInfos.Num() > 0)
	{
		return;
	}

	ActionBindingInfos = {
		{ FName("Move_Forward"), EKeys::W, FText::FromString(TEXT("앞으로 이동")) },
		{ FName("Move_Backward"), EKeys::S, FText::FromString(TEXT("뒤로 이동")) },
		{ FName("Move_Left"), EKeys::A, FText::FromString(TEXT("왼쪽으로 이동")) },
		{ FName("Move_Right"), EKeys::D, FText::FromString(TEXT("오른쪽으로 이동")) },
		{ FName("IA_Jump"), EKeys::SpaceBar, FText::FromString(TEXT("점프")) },
		{ FName("IA_Sprint"), EKeys::LeftShift, FText::FromString(TEXT("달리기")) },
		{ FName("IA_Interact"), EKeys::F, FText::FromString(TEXT("상호작용")) },
		{ FName("IA_Grab_Drop"), EKeys::E, FText::FromString(TEXT("물건 잡기/놓기")) },
		{ FName("IA_Use"), EKeys::LeftMouseButton, FText::FromString(TEXT("사용/공격")) },
		{ FName("IA_Throw"), EKeys::Q, FText::FromString(TEXT("던지기")) },
		{ FName("IA_Slap"), EKeys::T, FText::FromString(TEXT("뺨때리기")) },
		{ FName("IA_Light"), EKeys::Four, FText::FromString(TEXT("손전등")) },
		{ FName("IA_LightColor"), EKeys::Five, FText::FromString(TEXT("색상 변경")) },
		{ FName("IA_Slot1"), EKeys::One, FText::FromString(TEXT("아이템 슬롯 1")) },
		{ FName("IA_Slot2"), EKeys::Two, FText::FromString(TEXT("아이템 슬롯 2")) },
		{ FName("IA_Slot3"), EKeys::Three, FText::FromString(TEXT("아이템 슬롯 3")) },
		{ FName("IA_CapsLock"), EKeys::CapsLock, FText::FromString(TEXT("퀘스트")), FName("IA_QuestAsk") },
		{ FName("IA_QuestAsk"), EKeys::CapsLock, FText::FromString(TEXT("퀘스트")), FName("IA_CapsLock") },
		{ FName("IA_ViewEcnomoy"), EKeys::V, FText::FromString(TEXT("정산/경제현황")) },
		{ FName("IA_EmoteToggle"), EKeys::Tab, FText::FromString(TEXT("이모트")) },
		{ FName("Radio_Power"), EKeys::Z, FText::FromString(TEXT("무전기 전원")) },
		{ FName("Radio_Transmit"), EKeys::X, FText::FromString(TEXT("무전기 송신")) },
		{ FName("Voice_Mute"), EKeys::C, FText::FromString(TEXT("음성 음소거")) }
	};
}

FKey UKeyBindingMenuWidget::GetDefaultKeyForAction(FName ActionName) const
{
	for (const FMOUActionBindingInfo& Info : ActionBindingInfos)
	{
		if (Info.ActionName == ActionName)
		{
			return Info.DefaultKey;
		}
	}
	return EKeys::Invalid;
}

FKey UKeyBindingMenuWidget::GetCurrentKeyForAction(FName ActionName) const
{
	if (UMOU_GameUserSettings* UserSettings = GetUserSettings())
	{
		const FKey DefaultKey = GetDefaultKeyForAction(ActionName);
		return UserSettings->GetCustomKeyBinding(ActionName, DefaultKey);
	}
	return GetDefaultKeyForAction(ActionName);
}

FText UKeyBindingMenuWidget::GetActionDisplayName(FName ActionName) const
{
	for (const FMOUActionBindingInfo& Info : ActionBindingInfos)
	{
		if (Info.ActionName == ActionName)
		{
			return Info.DisplayName;
		}
	}
	return FText::FromName(ActionName);
}

FName UKeyBindingMenuWidget::GetLinkedAction(FName ActionName) const
{
	for (const FMOUActionBindingInfo& Info : ActionBindingInfos)
	{
		if (Info.ActionName == ActionName)
		{
			return Info.LinkedActionName;
		}
	}
	return NAME_None;
}

FName UKeyBindingMenuWidget::FindConflictingAction(FName TargetAction, const FKey& NewKey) const
{
	if (!NewKey.IsValid() || NewKey == EKeys::Invalid)
	{
		return NAME_None;
	}

	const FName LinkedTarget = GetLinkedAction(TargetAction);

	for (const FMOUActionBindingInfo& Info : ActionBindingInfos)
	{
		// 대상 액션 본인이거나 링크된 연계 액션이면 충돌 검사에서 제외
		if (Info.ActionName == TargetAction || Info.ActionName == LinkedTarget)
		{
			continue;
		}

		// 현재 설정된 키 조회
		const FKey AssignedKey = GetCurrentKeyForAction(Info.ActionName);
		if (AssignedKey == NewKey)
		{
			return Info.ActionName;
		}
	}

	return NAME_None;
}

void UKeyBindingMenuWidget::HandleKeySelected(FName ActionName, const FKey& NewKey)
{
	// 유효하지 않은 키는 바로 비우기 처리
	if (!NewKey.IsValid() || NewKey == EKeys::Invalid)
	{
		RebindActionKey(ActionName, EKeys::Invalid);
		const FName LinkedAction = GetLinkedAction(ActionName);
		if (LinkedAction != NAME_None)
		{
			RebindActionKey(LinkedAction, EKeys::Invalid);
		}
		RefreshUIFromSettings();
		return;
	}

	// 현재 이미 같은 키가 할당되어 있다면 무시
	const FKey CurrentKey = GetCurrentKeyForAction(ActionName);
	if (CurrentKey == NewKey)
	{
		return;
	}

	// 다른 조작키와의 충돌 여부 검사
	const FName ConflictingAction = FindConflictingAction(ActionName, NewKey);
	if (ConflictingAction != NAME_None)
	{
		// 중복 발생 -> 모달 팝업 표시
		ShowConflictModal(ActionName, NewKey, ConflictingAction);
	}
	else
	{
		// 중복 없음 -> 즉시 바인딩
		RebindActionKey(ActionName, NewKey);
		const FName LinkedAction = GetLinkedAction(ActionName);
		if (LinkedAction != NAME_None)
		{
			RebindActionKey(LinkedAction, NewKey);
		}
	}
}

void UKeyBindingMenuWidget::ShowConflictModal(FName TargetAction, const FKey& NewKey, FName ConflictingAction)
{
	PendingActionName = TargetAction;
	PendingNewKey = NewKey;
	PendingConflictingAction = ConflictingAction;
	bIsConflictModalOpen = true;

	if (Text_ConflictWarningMessage)
	{
		const FText ConflictDisplayName = GetActionDisplayName(ConflictingAction);
		const FText TargetDisplayName = GetActionDisplayName(TargetAction);
		const FText KeyDisplayName = NewKey.GetDisplayName();

		const FText Message = FText::Format(
			FText::FromString(TEXT("'{0}' 키는 이미 [{1}]에 사용 중입니다.\n기존 설정을 해제하고 [{2}]에 할당하시겠습니까?")),
			KeyDisplayName,
			ConflictDisplayName,
			TargetDisplayName
		);
		Text_ConflictWarningMessage->SetText(Message);
	}

	if (Panel_ConflictModal)
	{
		Panel_ConflictModal->SetVisibility(ESlateVisibility::Visible);
	}
}

void UKeyBindingMenuWidget::HideConflictModal()
{
	bIsConflictModalOpen = false;
	PendingActionName = NAME_None;
	PendingNewKey = EKeys::Invalid;
	PendingConflictingAction = NAME_None;

	if (Panel_ConflictModal)
	{
		Panel_ConflictModal->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UKeyBindingMenuWidget::OnConflictConfirmClicked()
{
	if (PendingActionName != NAME_None && PendingNewKey.IsValid())
	{
		// 1. 기존 충돌 액션의 키 해제
		if (PendingConflictingAction != NAME_None)
		{
			RebindActionKey(PendingConflictingAction, EKeys::Invalid);
			const FName LinkedConflict = GetLinkedAction(PendingConflictingAction);
			if (LinkedConflict != NAME_None)
			{
				RebindActionKey(LinkedConflict, EKeys::Invalid);
			}
		}

		// 2. 새 액션에 키 할당
		RebindActionKey(PendingActionName, PendingNewKey);
		const FName LinkedTarget = GetLinkedAction(PendingActionName);
		if (LinkedTarget != NAME_None)
		{
			RebindActionKey(LinkedTarget, PendingNewKey);
		}
	}

	HideConflictModal();
	RefreshUIFromSettings();
}

void UKeyBindingMenuWidget::OnConflictCancelClicked()
{
	HideConflictModal();
	// 방금 UI에 임시 표시된 키를 원래 저장된 키로 원복
	RefreshUIFromSettings();
}

// -----------------------------------------------------------------------------
// 키 선택 이벤트 핸들러들
// -----------------------------------------------------------------------------

void UKeyBindingMenuWidget::OnMoveForwardKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Move_Forward"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnMoveBackwardKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Move_Backward"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnMoveLeftKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Move_Left"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnMoveRightKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Move_Right"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnJumpKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Jump"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnSprintKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Sprint"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnInteractKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Interact"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnGrabDropKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Grab_Drop"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnUseKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Use"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnThrowKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Throw"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnSlapKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Slap"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnLightKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Light"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnLightColorKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_LightColor"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnSlot1KeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Slot1"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnSlot2KeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Slot2"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnSlot3KeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_Slot3"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnQuestKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_CapsLock"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnViewEconomyKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_ViewEcnomoy"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnEmoteKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("IA_EmoteToggle"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnRadioPowerKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Radio_Power"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnRadioTransmitKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Radio_Transmit"), SelectedKey.Key);
}

void UKeyBindingMenuWidget::OnVoiceMuteKeySelected(FInputChord SelectedKey)
{
	HandleKeySelected(FName("Voice_Mute"), SelectedKey.Key);
}

// -----------------------------------------------------------------------------
// 버튼 핸들러들
// -----------------------------------------------------------------------------

void UKeyBindingMenuWidget::OnBackClicked()
{
	CloseMenu();
}

void UKeyBindingMenuWidget::OnResetDefaultsClicked()
{
	ResetToDefaults();
}
