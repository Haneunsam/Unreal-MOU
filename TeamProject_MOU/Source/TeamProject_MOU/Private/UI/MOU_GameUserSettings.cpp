// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MOU_GameUserSettings.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Voice/VoiceSubsystem.h"

UMOU_GameUserSettings::UMOU_GameUserSettings()
{
	MasterVolume = 0.5f;
	BGMVolume = 0.5f;
	SFXVolume = 0.5f;
	VoiceVolume = 0.5f;
	MicSensitivity = 0.05f;

	MouseSensitivity = 1.0f;
	bInvertY = false;
	FieldOfView = 90.0f;
}

UMOU_GameUserSettings* UMOU_GameUserSettings::GetMOUGameUserSettings()
{
	return Cast<UMOU_GameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

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
	VoiceVolume = 0.5f;
	MicSensitivity = 0.05f;

	MouseSensitivity = 1.0f;
	bInvertY = false;
	FieldOfView = 90.0f;

	CustomKeyBindings.Empty();
	MOUSettingsVersion = 1;
}

void UMOU_GameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);

	if (MOUSettingsVersion < 1)
	{
		SetToDefaults();
		MOUSettingsVersion = 1;
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

void UMOU_GameUserSettings::SetVoiceVolume(float InVolume)
{
	VoiceVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
	OnAudioSettingsChanged.Broadcast();
}

void UMOU_GameUserSettings::SetMicSensitivity(float InSensitivity)
{
	MicSensitivity = FMath::Clamp(InSensitivity, 0.0f, 1.0f);
	if (GEngine)
	{
		if (UVoiceSubsystem* VoiceSubsystem = UVoiceSubsystem::Get(GEngine->GetWorld()))
		{
			VoiceSubsystem->SetMicSensitivity(MicSensitivity);
		}
	}
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
