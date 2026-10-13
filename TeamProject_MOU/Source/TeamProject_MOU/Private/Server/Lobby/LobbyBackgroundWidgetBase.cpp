#include "Server/Lobby/LobbyBackgroundWidgetBase.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "TeamProject_MOU.h"

// [LOBBYBG-000] 기존 배경 이미지를 찾아 영상 하나를 무작위로 선택하고 비동기로 연다.
void ULobbyBackgroundWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (BackgroundPlayer) return;

    BackgroundImageWidget = Cast<UImage>(GetWidgetFromName(TEXT("BackgroundImage")));
    if (!BackgroundImageWidget || !VideoMaterial) return;

    TArray<UMediaSource*> ValidVideos;
    for (UMediaSource* Source : BackgroundVideos)
    {
        if (IsValid(Source)) ValidVideos.Add(Source);
    }
    if (ValidVideos.IsEmpty()) return;

    OriginalBrush = BackgroundImageWidget->GetBrush();
    OriginalTint = BackgroundImageWidget->GetColorAndOpacity();
    BackgroundPlayer = NewObject<UMediaPlayer>(this);
    BackgroundPlayer->PlayOnOpen = false;
    BackgroundPlayer->OnMediaOpened.AddDynamic(this, &ThisClass::HandleMediaOpened);
    BackgroundPlayer->OnMediaOpenFailed.AddDynamic(this, &ThisClass::HandleMediaOpenFailed);

    BackgroundTexture = NewObject<UMediaTexture>(this);
    BackgroundTexture->NewStyleOutput = true;
    BackgroundTexture->SetMediaPlayer(BackgroundPlayer);
    BackgroundTexture->UpdateResource();
    BackgroundMaterial = UMaterialInstanceDynamic::Create(VideoMaterial, this);
    BackgroundMaterial->SetTextureParameterValue(TEXT("VideoTexture"), BackgroundTexture);

    UMediaSource* Selected = ValidVideos[FMath::RandRange(0, ValidVideos.Num() - 1)];
    if (!BackgroundPlayer->OpenSource(Selected))
    {
        HandleMediaOpenFailed(Selected->GetUrl());
    }
}

// [LOBBYBG-001] 배경 제거 시 이벤트와 디코더를 해제하고 기존 브러시를 복원한다.
void ULobbyBackgroundWidgetBase::NativeDestruct()
{
    if (BackgroundPlayer)
    {
        BackgroundPlayer->OnMediaOpened.RemoveAll(this);
        BackgroundPlayer->OnMediaOpenFailed.RemoveAll(this);
        BackgroundPlayer->Close();
        if (BackgroundImageWidget)
        {
            BackgroundImageWidget->SetBrush(OriginalBrush);
            BackgroundImageWidget->SetColorAndOpacity(OriginalTint);
        }
    }
    if (BackgroundTexture) BackgroundTexture->SetMediaPlayer(nullptr);
    BackgroundMaterial = nullptr;
    BackgroundTexture = nullptr;
    BackgroundPlayer = nullptr;
    BackgroundImageWidget = nullptr;
    Super::NativeDestruct();
}

// [LOBBYBG-002] 파일 열기가 완료되면 반복 재생을 시작한다.
void ULobbyBackgroundWidgetBase::HandleMediaOpened(FString OpenedUrl)
{
    if (!BackgroundPlayer || !BackgroundImageWidget || !BackgroundMaterial) return;
    BackgroundPlayer->SetLooping(true);
    if (!BackgroundPlayer->Play())
    {
        HandleMediaOpenFailed(OpenedUrl);
        return;
    }
    BackgroundImageWidget->SetBrushFromMaterial(BackgroundMaterial);
    BackgroundImageWidget->SetColorAndOpacity(FLinearColor::White);
}

// [LOBBYBG-003] 재생 준비 실패 시 기존 검은 배경을 유지하고 경고를 남긴다.
void ULobbyBackgroundWidgetBase::HandleMediaOpenFailed(FString FailedUrl)
{
    if (BackgroundImageWidget)
    {
        BackgroundImageWidget->SetBrush(OriginalBrush);
        BackgroundImageWidget->SetColorAndOpacity(OriginalTint);
    }
    UE_LOG(LogTeamProject_MOU, Warning, TEXT("로비 배경 영상 재생 실패: %s"), *FailedUrl);
}
