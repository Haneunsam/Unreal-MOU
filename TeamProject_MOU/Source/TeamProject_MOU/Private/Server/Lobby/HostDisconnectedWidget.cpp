#include "Server/Lobby/HostDisconnectedWidget.h"
#include "Engine/GameInstance.h"
#include "Server/ServerSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// [HOSTLOST-005] 별도 BP 설정 없이 호스트 연결 종료 안내와 확인 버튼을 만듭니다.
TSharedRef<SWidget> UHostDisconnectedWidget::RebuildWidget()
{
    return SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.97f))
        .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(40.f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(12.f)
            [ SNew(STextBlock).Text(FText::FromString(TEXT("호스트의 연결이 끊겼습니다")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 24)) ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(12.f)
            [ SNew(SButton).ContentPadding(FMargin(40.f, 12.f))
                .Text(FText::FromString(TEXT("확인")))
                .OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, Confirm)) ]
        ];
}
// [HOSTLOST-006] 확인을 서브시스템으로 전달합니다.
FReply UHostDisconnectedWidget::Confirm()
{
    if (UGameInstance* GI = GetGameInstance())
        if (UServerSubsystem* Server = GI->GetSubsystem<UServerSubsystem>())
            Server->ConfirmHostDisconnected();
    return FReply::Handled();
}
