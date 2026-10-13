// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MOU_GameUserSettings.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Voice/VoiceSubsystem.h"
#include "Voice/VoiceTypes.h"

// [MIC-007] 음성 출력과 마이크 인식 임계값의 기본 설정을 초기화한다.
UMOU_GameUserSettings::UMOU_GameUserSettings()
{
	MasterVolume = 0.5f;
	BGMVolume = 0.5f;
	SFXVolume = 0.5f;
	VoiceVolume = 1.0f;
	MicSensitivity = MOUVoice::SettingsMicThresholdDefault;

	MouseSensitivity = 1.0f;
	bInvertY = false;
	FieldOfView = 90.0f;
}

UMOU_GameUserSettings* UMOU_GameUserSettings::GetMOUGameUserSettings()
{
	return Cast<UMOU_GameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

// [MIC-008] 기본 설정 및 음성 설정 버전을 복원한다.
void UMOU_GameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	const FIntPoint DesktopRes = GetDesktopResolution();
	if (DesktopRes.X > 0 && DesktopRes.Y > 0)
	{
		SetScreenResolution(DesktopRes);
	}

	SetFullscreenMode(EWindowMode::Fullscreen);
	SetOverallScalabilityLevel(1);
	SetVSyncEnabled(false);
	SetFrameRateLimit(0.0f);

	MasterVolume = 0.5f;
	BGMVolume = 0.5f;
	SFXVolume = 0.5f;
	VoiceVolume = 1.0f;
	MicSensitivity = MOUVoice::SettingsMicThresholdDefault;

	MouseSensitivity = 1.0f;
	bInvertY = false;
	FieldOfView = 90.0f;

	CustomKeyBindings.Empty();
	MOUSettingsVersion = 2;
}

// [MIC-002] 기존 설정을 읽고 구버전 음성 볼륨과 마이크 임계값을 한 번 보정한다.
void UMOU_GameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);

	if (MOUSettingsVersion < 1)
	{
		SetToDefaults();
		SaveSettings();
		return;
	}

	if (MOUSettingsVersion < 2)
	{
		// 기존 음성 볼륨은 출력에 연결되지 않았으므로 새 기본값으로 시작한다.
		VoiceVolume = 1.0f;
		MicSensitivity = MOUVoice::SettingsMicThresholdDefault;
		MOUSettingsVersion = 2;
		SaveSettings();
	}
}

void UMOU_GameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	if (GEngine)
	{
		ApplyAudioSettings(GEngine->GetWorld());
	}

	OnAudioSettingsChanged.Broadcast();
	OnControlSettingsChanged.Broadcast();
}

void UMOU_GameUserSettings::SetMasterVolume(float InVolume)
{
	MasterVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	if (GEngine)
	{
		ApplyAudioSettings(GEngine->GetWorld());
	}
	OnAudioSettingsChanged.Broadcast();
}

// [BGMAUDIO-001] 배경음악 볼륨을 저장하고 공통 BGM 사운드 클래스에 즉시 반영한다.
void UMOU_GameUserSettings::SetBGMVolume(float InVolume)
{
	BGMVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	ApplyAudioSettings(GEngine ? GEngine->GetWorld() : nullptr);
	OnAudioSettingsChanged.Broadcast();
}

void UMOU_GameUserSettings::SetSFXVolume(float InVolume)
{
	SFXVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	OnAudioSettingsChanged.Broadcast();
}

// [VOICEVOL-001] 수신 음성 배율을 0~200% 범위로 저장하고 즉시 변경을 알린다.
void UMOU_GameUserSettings::SetVoiceVolume(float InVolume)
{
	VoiceVolume = FMath::Clamp(InVolume, 0.0f, 2.0f);
	OnAudioSettingsChanged.Broadcast();
}

// [MIC-003] 수동 마이크 임계값을 0~0.01로 제한하고 캡처에 적용한다.
void UMOU_GameUserSettings::SetMicSensitivity(float InSensitivity)
{
	MicSensitivity = FMath::Clamp(InSensitivity, 0.0f, MOUVoice::SettingsMicThresholdMax);
	// 각 로컬 플레이어의 VoiceSubsystem이 구독하여 실제 캡처에 즉시 반영한다.
	OnAudioSettingsChanged.Broadcast();
}

// [BGMAUDIO-002] 마스터 볼륨, 공통 BGM 볼륨과 마이크 감도를 적용한다.
void UMOU_GameUserSettings::ApplyAudioSettings(UObject* WorldContextObject)
{
	if (!BGMSoundClass)
	{
		BGMSoundClass = LoadObject<USoundClass>(
			nullptr,
			TEXT("/Game/02_JSY/BGM/SC_BGM.SC_BGM"));
	}

	if (BGMSoundClass)
	{
		BGMSoundClass->Properties.Volume = BGMVolume;
	}

	// 1. 게임 월드의 오디오 장치에 마스터 볼륨을 적용하고, 월드가 없으면 활성 장치를 사용한다.
	if (GEngine)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(
			WorldContextObject, EGetWorldErrorMode::ReturnNull);
		FAudioDeviceHandle Device = World
			? World->GetAudioDevice() : FAudioDeviceHandle();

		if (!Device.IsValid())
		{
			if (FAudioDeviceManager* Manager = GEngine->GetAudioDeviceManager())
			{
				Device = Manager->GetActiveAudioDevice();
			}
		}

		if (FAudioDevice* AudioDevice = Device.GetAudioDevice())
		{
			AudioDevice->SetTransientPrimaryVolume(MasterVolume);
		}
	}

	// 2. 보이스 서브시스템 마이크 감도 반영
	if (WorldContextObject)
	{
		if (UVoiceSubsystem* VoiceSubsystem = UVoiceSubsystem::Get(WorldContextObject))
		{
			VoiceSubsystem->SetMicSensitivity(MicSensitivity);
		}
	}
}

void UMOU_GameUserSettings::SetMouseSensitivity(float InSensitivity)
{
	MouseSensitivity = FMath::Clamp(InSensitivity, 0.1f, 3.0f);
	OnControlSettingsChanged.Broadcast();
}

void UMOU_GameUserSettings::SetInvertY(bool bInInvert)
{
	bInvertY = bInInvert;
	OnControlSettingsChanged.Broadcast();
}

void UMOU_GameUserSettings::SetFieldOfView(float InFOV)
{
	FieldOfView = FMath::Clamp(InFOV, 70.0f, 110.0f);
	OnControlSettingsChanged.Broadcast();
}

void UMOU_GameUserSettings::SetCustomKeyBinding(FName ActionName, const FKey& Key)
{
	if (!ActionName.IsNone())
	{
		CustomKeyBindings.FindOrAdd(ActionName) = Key;
		OnControlSettingsChanged.Broadcast();
	}
}

FKey UMOU_GameUserSettings::GetCustomKeyBinding(FName ActionName, const FKey& DefaultKey) const
{
	if (const FKey* Found = CustomKeyBindings.Find(ActionName))
	{
		return *Found;
	}
	return DefaultKey;
}

void UMOU_GameUserSettings::ClearAllCustomKeyBindings()
{
	CustomKeyBindings.Empty();
	OnControlSettingsChanged.Broadcast();
}
