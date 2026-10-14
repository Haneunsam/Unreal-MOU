#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "LobbyBackgroundWidgetBase.generated.h"

class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMediaPlayer;
class UMediaSource;
class UMediaTexture;

UCLASS()
class TEAMPROJECT_MOU_API ULobbyBackgroundWidgetBase : public UUserWidget
{
    GENERATED_BODY()

protected:
    // [LOBBYBG-000] 기존 배경 이미지를 찾아 영상 하나를 무작위로 선택하고 비동기로 연다.
    virtual void NativeConstruct() override;

    // [LOBBYBG-001] 배경 제거 시 이벤트와 디코더를 해제하고 기존 브러시를 복원한다.
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, Category="MOU|Lobby Background")
    TArray<TObjectPtr<UMediaSource>> BackgroundVideos;

    // User Interface 도메인, VideoTexture 텍스처 파라미터를 가진 머티리얼.
    UPROPERTY(EditDefaultsOnly, Category="MOU|Lobby Background")
    TObjectPtr<UMaterialInterface> VideoMaterial;

private:
    // [LOBBYBG-002] 파일 열기가 완료되면 반복 재생을 시작한다.
    UFUNCTION()
    void HandleMediaOpened(FString OpenedUrl);

    // [LOBBYBG-003] 재생 준비 실패 시 기존 검은 배경을 유지하고 경고를 남긴다.
    UFUNCTION()
    void HandleMediaOpenFailed(FString FailedUrl);

    UPROPERTY(Transient) TObjectPtr<UImage> BackgroundImageWidget;
    UPROPERTY(Transient) TObjectPtr<UMediaPlayer> BackgroundPlayer;
    UPROPERTY(Transient) TObjectPtr<UMediaTexture> BackgroundTexture;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BackgroundMaterial;
    UPROPERTY(Transient) FSlateBrush OriginalBrush;
    FLinearColor OriginalTint = FLinearColor::White;
};
