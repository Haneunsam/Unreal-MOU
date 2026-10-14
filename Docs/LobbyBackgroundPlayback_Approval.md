# MainLobby 영상 배경 연결 — 적용 전 검토안

아래는 검토용 코드이며 아직 소스에 적용하지 않았다.

## 현재 역할과 변경 범위

- `Content/02_JSY/MainLobby/WBP_LobbyBackground.uasset`는 UUserWidget 기반이며 `BackgroundImage` 이미지가 있다.
- `BP_LobbyPlayerController`에는 BeginPlay, 해당 배경 위젯 생성, AddToViewport 연결이 있다. 이 기존 생성 흐름을 유지한다.
- 기존 C++ `Private/Server/Lobby/LobbyPlayerController.cpp`의 코드는 다음과 같다. 이 파일은 수정하지 않는다.

```cpp
void ALobbyPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ShowLoginWidgetIfNeeded();
}
```

현재 C++는 로그인/로비 UI를 관리하며, 배경 영상 디코더를 생성하거나 재생하지 않는다. 배경 위젯의 부모를 새 네이티브 클래스로 바꾸어 기존 생성 시점에 NativeConstruct가 영상 재생을 시작하게 한다. 컨트롤러·로그인·서버 통신 코드는 변경하지 않는다.

승인 요청 파일(모두 `TeamProject_MOU/Source/TeamProject_MOU/` 기준):

1. `TeamProject_MOU.Build.cs`: 기존 PrivateDependencyModuleNames에 MediaAssets 추가.
2. `Public/Server/Lobby/LobbyBackgroundWidgetBase.h`: 아래 클래스 신규 생성.
3. `Private/Server/Lobby/LobbyBackgroundWidgetBase.cpp`: 아래 함수 신규 생성.

함께 연결할 에셋: 기존 WBP_LobbyBackground 부모 클래스 변경, 영상 목록·UI 머티리얼 기본값 지정. 신규 FileMediaSource와 UI 머티리얼, MP4는 Content/Movies/LobbyBackgrounds에 배치한다. 배경 이미지의 배치·ZOrder와 로그인 UI는 유지한다. 영상은 무음으로 재생하여 기존 로비 BGM을 유지한다.

수정하지 않으면 영상 파일만 만들어지고 기존 검은 패널은 계속 검게 남는다. Item 폴더 안에서만 해결하려면 아이템 기능에 로비 UI/미디어 생명주기를 억지로 의존시켜야 하며, 로비에는 아이템이 없을 수 있어 적절한 대안이 아니다.

## 1. Build.cs — 기존 의존성 선언 위치

```diff
-        PrivateDependencyModuleNames.AddRange(new string[] { });
+        PrivateDependencyModuleNames.AddRange(new string[] { "MediaAssets" });
```

MediaPlayer, MediaSource, MediaTexture 구현을 사용하기 위한 모듈 연결이다. 공개 헤더는 전방 선언만 사용하므로 private 의존성으로 충분하다.

## 2. LobbyBackgroundWidgetBase.h — 신규 파일 전체

```cpp
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
```

## 3. LobbyBackgroundWidgetBase.cpp — 신규 파일 전체

```cpp
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
```

## 호출 흐름과 영향

기존 BP 컨트롤러가 WBP_LobbyBackground 생성 → 새 부모 NativeConstruct → 목록에서 하나 선택 → OpenSource → HandleMediaOpened → MediaTexture/UI 머티리얼을 기존 이미지에 표시 → 루프 재생. 위젯 제거 시 NativeDestruct에서 닫는다.

- 영상이 한 개면 그 영상만 재생한다. 여러 개면 위젯 생성 때마다 하나를 무작위 선택한다. 직전과 같은 영상이 다시 나올 수 있다.
- 경로는 FileMediaSource의 프로젝트 상대 경로를 사용한다. 촬영 맵을 MainLobby에서 로드하지 않는다.
- 파일이 없거나 열기에 실패하면 기존 배경으로 돌아간다. 로그인 기능에는 영향을 주지 않는다.
- 렌더 프리뷰에 관한 설정은 게임 코드와 별개이며 기존 Lv_TrashTown 맵을 수정하지 않는다.
- 새 UCLASS 때문에 에디터를 닫고 Editor 타깃을 빌드한 뒤 재시작하는 검증 단계가 필요할 수 있다. 저장되지 않은 관련 없는 사용자 에셋은 임의로 저장하지 않는다.

승인 후 위 세 소스 파일의 범위에서 구현·컴파일하고, 배경 에셋 연결 및 MainLobby PIE 재생을 검증한다. 추가 파일 변경이 필요하면 다시 검토를 요청한다.
