#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/Lobby/LoginWidgetBase.h"
#include "Server/ServerSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoginUIRegressionTest, "MOU.LoginUI.WidgetRegression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
// [AUTHUI-022] 실제 로그인 WBP의 검증·가입 후 복귀·비밀번호 표시와 렌더 결과를 외부 접속 없이 검사한다.
bool FLoginUIRegressionTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>(GEngine); GI->InitializeStandalone();
    UWorld* World = GI->GetWorld();
    APlayerController* PC = World->SpawnActor<APlayerController>();
    ULocalPlayer* LP = NewObject<ULocalPlayer>(GEngine); LP->PlayerController = PC; PC->Player = LP;
    PC->SetAsLocalPlayerController(); World->AddController(PC);
    UServerSubsystem* Server = GI->GetSubsystem<UServerSubsystem>();
    FEnumProperty* State = FindFProperty<FEnumProperty>(Server->GetClass(), TEXT("ConnectionState"));
    State->GetUnderlyingProperty()->SetIntPropertyValue(State->ContainerPtrToValuePtr<void>(Server), static_cast<int64>(EChatConnectionState::Connected));
    UClass* Class = LoadClass<ULoginWidgetBase>(nullptr, TEXT("/Game/02_JSY/MainLobby/WBP_LoginWidget.WBP_LoginWidget_C"));
    if (!TestNotNull(TEXT("Login WBP"), Class)) { GI->Shutdown(); return false; }
    ULoginWidgetBase* W = CreateWidget<ULoginWidgetBase>(PC, Class);
    auto Slate = W->TakeWidget();
    if (!TestNotNull(TEXT("Register ID binding"), W->RegisterIdBox.Get())) { GI->Shutdown(); return false; }
    TestFalse(TEXT("Initial registration hidden"), W->bRegisterPanelOpen);
    TestFalse(TEXT("Register panel collapsed"), W->RegisterPanel->IsVisible());
    TestTrue(TEXT("Login uses masking"), W->PasswordBox->GetIsPassword());
    TestNotNull(TEXT("Main title reused"), W->GetWidgetFromName(TEXT("GameTitleImage")));
    FString Preview; FParse::Value(FCommandLine::Get(), TEXT("LoginUIPreviewDir="), Preview);
    auto Render = [&](const TCHAR* File, int32 Width=1280, int32 Height=800) {
        if (Preview.IsEmpty()) return;
#if WITH_EDITOR
        FAssetCompilingManager::Get().FinishAllCompilation();
#endif
        FWidgetRenderer Renderer(false); W->ForceLayoutPrepass();
        for (int I=0; I<3; ++I) Renderer.DrawWidget(Slate, FVector2D(Width,Height));
        auto* Target=Renderer.DrawWidget(Slate,FVector2D(Width,Height));
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)) {
            for (FColor& Pixel:Pixels) Pixel=Pixel.ReinterpretAsLinear().ToFColorSRGB();
            TArray64<uint8> Bytes; FImageUtils::PNGCompressImageArray(Width,Height,Pixels,Bytes);
            TestTrue(TEXT("Preview written"),FFileHelper::SaveArrayToFile(Bytes,*(Preview/File)));
        } else AddError(TEXT("Login render failed"));
    };
    Render(TEXT("Login_Initial.png"));
    W->RegisterButton->OnClicked.Broadcast();
    TestTrue(TEXT("Register opens its panel"),W->bRegisterPanelOpen);
    TestFalse(TEXT("Login hidden behind signup"),W->LoginPanel->IsVisible());
    TestFalse(TEXT("Open does not register"),W->bRegisterRequestPending);
    W->RegisterIdBox->SetText(FText::FromString(TEXT("test_user")));
    W->RegisterPasswordBox->SetText(FText::FromString(TEXT("example123")));
    W->ConfirmPasswordBox->SetText(FText::FromString(TEXT("different")));
    FString Id, Password;
    TestFalse(TEXT("Mismatch rejected"),W->ReadAndValidateRegisterInput(Id,Password));
    W->ConfirmPasswordBox->SetText(W->RegisterPasswordBox->GetText());
    TestFalse(TEXT("Unchecked ID rejected"),W->ReadAndValidateRegisterInput(Id,Password));
    W->ActiveCheckRequestId=8;W->CheckingLoginId=TEXT("test_user");
    W->HandleIdChecked(7,EChatLoginResultBP::Success);
    TestFalse(TEXT("Stale response ignored"),W->bIdAvailable);
    W->HandleIdChecked(8,EChatLoginResultBP::Success);
    TestTrue(TEXT("Matching availability accepted"),W->ReadAndValidateRegisterInput(Id,Password));
    Render(TEXT("Signup_Available.png"));
    W->RegisterPasswordEyeButton->OnClicked.Broadcast();
    TestFalse(TEXT("Password reveal"),W->RegisterPasswordBox->GetIsPassword());
    TestFalse(TEXT("Eye slash hidden while visible"),W->RegisterEyeSlash->IsVisible());
    TestTrue(TEXT("Confirmation stays masked"),W->ConfirmPasswordBox->GetIsPassword());
    W->RegisterPasswordEyeButton->OnClicked.Broadcast();
    TestTrue(TEXT("Eye slash returns"),W->RegisterEyeSlash->IsVisible());
    W->RegisterIdBox->OnTextChanged.Broadcast(W->RegisterIdBox->GetText());
    TestFalse(TEXT("Any ID edit invalidates check"),W->bIdAvailable);
    W->ActiveCheckRequestId=9;W->CheckingLoginId=TEXT("test_user");
    W->HandleIdChecked(9,EChatLoginResultBP::DuplicateId);
    TestTrue(TEXT("Duplicate indicator visible"),W->CheckIdDuplicate->IsVisible());
    Render(TEXT("Signup_Duplicate.png"));
    W->bRegisterRequestPending=true;W->SetBusy(true);W->SubmittedRegisterId=TEXT("test_user");
    TestFalse(TEXT("Busy locks ID"),W->RegisterIdBox->GetIsEnabled());
    W->CloseRegisterPanel();TestTrue(TEXT("Busy cannot close"),W->bRegisterPanelOpen);
    W->HandleRegisterCompleted(true,EChatLoginResultBP::Success);
    TestFalse(TEXT("Signup success returns to login"),W->bRegisterPanelOpen);
    TestFalse(TEXT("Signup success never starts login busy state"),W->bBusy);
    TestFalse(TEXT("Signup success never queues Login packet"),Server->bHasPendingLogin);
    TestEqual(TEXT("Signup success preserves connected unauthenticated state"),Server->GetConnectionState(),EChatConnectionState::Connected);
    TestEqual(TEXT("Only ID is copied"),W->LoginIdBox->GetText().ToString(),FString(TEXT("test_user")));
    TestTrue(TEXT("All passwords erased"),W->PasswordBox->GetText().IsEmpty() && W->RegisterPasswordBox->GetText().IsEmpty() && W->ConfirmPasswordBox->GetText().IsEmpty());
    Render(TEXT("Login_AfterSignup.png"));
    W->RegisterButton->OnClicked.Broadcast();
    W->ActiveCheckRequestId=10;W->CheckingLoginId=TEXT("test_user");
    W->CloseRegisterButton->OnClicked.Broadcast();
    W->HandleIdChecked(10,EChatLoginResultBP::Success);
    TestFalse(TEXT("Late result after close ignored"),W->bIdAvailable);
    W->RegisterButton->OnClicked.Broadcast();Render(TEXT("Signup_960x540.png"),960,540);
    W->HandleStateChanged(EChatConnectionState::Disconnected,FString());
    TestFalse(TEXT("Disconnect resets availability"),W->bIdAvailable);
    W->ActiveCheckRequestId=11; W->CheckingLoginId=TEXT("test_user");
    Server->PendingCheckRequestId=11; Server->CheckRequestTime=0;
    Server->Tick(0.f);
    TestEqual(TEXT("Query timeout resets correlation"),W->ActiveCheckRequestId,0u);
    TestFalse(TEXT("Query timeout does not accept ID"),W->bIdAvailable);
    Server->bHasPendingRegister=true;Server->PendingRegisterPassword=TEXT("transient");Server->RegisterRequestTime=0;
    W->bRegisterRequestPending=true;W->SetBusy(true);
    Server->Tick(0.f);
    TestFalse(TEXT("Uncertain registration not replayed"),Server->bHasPendingRegister);
    TestTrue(TEXT("Pending password erased on timeout"),Server->PendingRegisterPassword.IsEmpty());
    TestFalse(TEXT("Registration timeout unlocks controls"),W->bBusy);
    W->NativeDestruct();W->ReleaseSlateResources(true);GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    return true;
}
#endif
