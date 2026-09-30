#include "Server/Lobby/LoginWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Server/ServerSettings.h"
#include "Server/ServerSubsystem.h"
#include "Server/Chat/InGame/ChatWidgetBase.h"
#include "Server/Lobby/LobbyWidgetBase.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

ULoginWidgetBase::ULoginWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 로그인 화면은 마우스를 받아야 하므로 게임 입력을 막는 편이 자연스럽다.
	// 다만 입력 모드 전환은 게임 쪽 흐름과 충돌할 수 있어 여기서 강제하지 않는다.
	SetIsFocusable(true);
}

void ULoginWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WBP 가 아무 위젯도 바인딩해주지 않았다면 C++ 이 직접 기본 화면을 만든다.
	// ChatWidgetBase 와 같은 규약이다 — 디자이너 작업을 기다리지 않고 검증할 수 있다.
	if (LoginIdBox == nullptr && PasswordBox == nullptr && LoginButton == nullptr)
	{
		BuildDefaultLayout();
	}
}

// [AUTHUI-030] WBP가 없는 경우에도 로그인과 가입을 분리한 기본 화면을 구성한다.
void ULoginWidgetBase::BuildDefaultLayout()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Root;
    auto* Login = WidgetTree->ConstructWidget<UVerticalBox>(); Root->AddChild(Login); LoginPanel = Login;
    auto* Register = WidgetTree->ConstructWidget<UVerticalBox>(); Root->AddChild(Register); RegisterPanel = Register;
    auto Edit = [&](UVerticalBox* Parent, const TCHAR* Name, const TCHAR* Hint, bool Secret) {
        auto* W = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
        W->SetHintText(FText::FromString(Hint)); W->SetIsPassword(Secret); Parent->AddChild(W); return W;
    };
    auto Button = [&](UVerticalBox* Parent, const TCHAR* Name, const TCHAR* Label) {
        auto* W = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        auto* T = WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Label)); W->AddChild(T); Parent->AddChild(W); return W;
    };
    LoginIdBox = Edit(Login, TEXT("LoginIdBox"), TEXT("아이디"), false);
    PasswordBox = Edit(Login, TEXT("PasswordBox"), TEXT("비밀번호"), true);
    LoginPasswordEyeButton = Button(Login, TEXT("LoginPasswordEyeButton"), TEXT("비밀번호 표시/숨김"));
    LoginButton = Button(Login, TEXT("LoginButton"), TEXT("로그인"));
    RegisterButton = Button(Login, TEXT("RegisterButton"), TEXT("회원가입"));
    MessageText = WidgetTree->ConstructWidget<UTextBlock>(); Login->AddChild(MessageText);
    RegisterIdBox = Edit(Register, TEXT("RegisterIdBox"), TEXT("아이디"), false);
    CheckIdButton = Button(Register, TEXT("CheckIdButton"), TEXT("중복 확인"));
    RegisterPasswordBox = Edit(Register, TEXT("RegisterPasswordBox"), TEXT("비밀번호"), true);
    RegisterPasswordEyeButton = Button(Register, TEXT("RegisterPasswordEyeButton"), TEXT("비밀번호 표시/숨김"));
    ConfirmPasswordBox = Edit(Register, TEXT("ConfirmPasswordBox"), TEXT("비밀번호 확인"), true);
    SubmitRegisterButton = Button(Register, TEXT("SubmitRegisterButton"), TEXT("회원가입 완료"));
    CloseRegisterButton = Button(Register, TEXT("CloseRegisterButton"), TEXT("닫기"));
    RegisterMessageText = WidgetTree->ConstructWidget<UTextBlock>(); Register->AddChild(RegisterMessageText);
    Register->SetVisibility(ESlateVisibility::Collapsed);
}

// [AUTHUI-031] 입력과 서버 이벤트를 연결하고 로그인 화면을 초기화한다.
void ULoginWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton != nullptr)
	{
		LoginButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::HandleLoginClicked);
	}
	if (RegisterButton != nullptr)
	{
		RegisterButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::HandleRegisterClicked);
	}

    if (CheckIdButton) CheckIdButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::HandleCheckIdClicked);
    if (SubmitRegisterButton) SubmitRegisterButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::TryRegister);
    if (CloseRegisterButton) CloseRegisterButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::CloseRegisterPanel);
    if (LoginPasswordEyeButton) LoginPasswordEyeButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::ToggleLoginPassword);
    if (RegisterPasswordEyeButton) RegisterPasswordEyeButton->OnClicked.AddUniqueDynamic(this, &ULoginWidgetBase::ToggleRegisterPassword);
    if (RegisterIdBox) RegisterIdBox->OnTextChanged.AddUniqueDynamic(this, &ULoginWidgetBase::HandleRegisterIdChanged);
    if (PasswordBox) PasswordBox->OnTextCommitted.AddUniqueDynamic(this, &ULoginWidgetBase::HandlePasswordCommitted);
    if (ConfirmPasswordBox) ConfirmPasswordBox->OnTextCommitted.AddUniqueDynamic(this, &ULoginWidgetBase::HandlePasswordCommitted);
    ClearAllPasswordFields();
    CloseRegisterPanel();
	// NativeConstruct 는 뷰포트에 다시 붙을 때마다 불릴 수 있어 중복 구독을 막는다.
	if (!bSubscribed)
	{
		if (UServerSubsystem* Chat = GetServerSubsystem())
		{
			Chat->OnChatLoginCompleted.AddDynamic(this, &ULoginWidgetBase::HandleLoginCompleted);
			Chat->OnLoginIdChecked.AddDynamic(this, &ULoginWidgetBase::HandleIdChecked);
			Chat->OnChatRegisterCompleted.AddDynamic(this, &ULoginWidgetBase::HandleRegisterCompleted);
			Chat->OnChatStateChanged.AddDynamic(this, &ULoginWidgetBase::HandleStateChanged);
			bSubscribed = true;
		}
	}

	// 화면이 뜨자마자 서버에 붙어둔다. 사용자가 입력하는 동안 연결이 끝나 있게 된다.
	EnsureConnected();
	SetMessage(TEXT("아이디와 비밀번호를 입력하세요."), false);
}

// [AUTHUI-032] 서버 이벤트와 미확정 요청을 정리하고 비밀번호를 지운다.
void ULoginWidgetBase::NativeDestruct()
{
	// 구독 해제를 여기서 반드시 해야 파괴된 위젯으로 델리게이트가 날아오지 않는다.
	if (bSubscribed)
	{
		if (UServerSubsystem* Chat = GetServerSubsystem())
		{
			Chat->OnChatLoginCompleted.RemoveDynamic(this, &ULoginWidgetBase::HandleLoginCompleted);
			Chat->CancelLoginIdCheck(ActiveCheckRequestId);
            if (bRegisterRequestPending) Chat->CancelPendingRegistration();
            Chat->OnLoginIdChecked.RemoveDynamic(this, &ULoginWidgetBase::HandleIdChecked);
			Chat->OnChatRegisterCompleted.RemoveDynamic(this, &ULoginWidgetBase::HandleRegisterCompleted);
			Chat->OnChatStateChanged.RemoveDynamic(this, &ULoginWidgetBase::HandleStateChanged);
		}
		bSubscribed = false;
	}

	ClearAllPasswordFields();
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// 동작
// ---------------------------------------------------------------------------

UServerSubsystem* ULoginWidgetBase::GetServerSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UServerSubsystem>();
	}
	return nullptr;
}

void ULoginWidgetBase::EnsureConnected()
{
	UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat == nullptr)
	{
		SetMessage(TEXT("채팅 시스템을 찾을 수 없습니다."), true);
		return;
	}

	if (Chat->GetConnectionState() == EChatConnectionState::Disconnected)
	{
		// 비워두면 ConnectToChatServer 가 설정에서 주소를 읽는다. 위젯이 굳이
		// 기본 주소를 알 필요는 없다 — 아는 곳이 여러 군데면 다시 어긋난다.
		Chat->ConnectToChatServer(ServerHost, ServerPort);

		// 어디에 붙는 중인지 화면에 보여준다. 접속이 안 될 때 "내가 어느 서버를
		// 보고 있었는지" 를 사용자가 바로 알 수 있어야 한다.
		const FString Target = (ServerHost.IsEmpty() || ServerPort <= 0)
			? UMOUServerSettings::GetResolvedEndpointText()
			: FString::Printf(TEXT("%s:%d"), *ServerHost, ServerPort);
		UE_LOG(LogMOUServer, Log, TEXT("로그인 화면이 접속을 시작한다: %s"), *Target);
	}
}

bool ULoginWidgetBase::ReadAndValidateInput(FString& OutId, FString& OutPassword)
{
	OutId       = LoginIdBox  ? LoginIdBox->GetText().ToString()  : FString();
	OutPassword = PasswordBox ? PasswordBox->GetText().ToString() : FString();

	// 앞뒤 공백은 사용자가 의도한 것이 아닌 경우가 대부분이라 아이디에서만 걷어낸다.
	// 비밀번호는 공백도 유효한 문자이므로 절대 건드리지 않는다.
	OutId.TrimStartAndEndInline();

	FString Reason;
	if (!UServerSubsystem::ValidateCredentials(OutId, OutPassword, Reason))
	{
		SetMessage(Reason, true);
		return false;
	}
	return true;
}

// [AUTHUI-033] 로그인 패널의 검증된 자격증명으로 수동 로그인을 요청한다.
void ULoginWidgetBase::TryLogin()
{
	if (bBusy || bRegisterPanelOpen)
	{
		return;
	}

	FString Id, Password;
	if (!ReadAndValidateInput(Id, Password))
	{
		return;
	}

	UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat == nullptr)
	{
		SetMessage(TEXT("채팅 시스템을 찾을 수 없습니다."), true);
		return;
	}

	SetBusy(true);
	SetMessage(TEXT("로그인 중..."), false);

	EnsureConnected();
	// 아직 연결 전이면 서브시스템이 요청을 보관했다가 연결되는 순간 보낸다.
	Chat->Login(Id, Password, TeamId);
}

// [AUTHUI-034] 가입 검증이 완료된 입력을 서버에 제출한다.
void ULoginWidgetBase::TryRegister()
{
    if (bBusy || !bRegisterPanelOpen) return;
    FString Id, Password;
    if (!ReadAndValidateRegisterInput(Id, Password)) return;
    auto* Chat = GetServerSubsystem();
    if (!Chat || Chat->GetConnectionState() != EChatConnectionState::Connected) {
        EnsureConnected(); SetMessage(TEXT("서버 연결 후 다시 시도해 주세요."), true); return;
    }
    SubmittedRegisterId = Id; bRegisterRequestPending = true;
    SetBusy(true); SetMessage(TEXT("계정을 만드는 중..."), false);
    Chat->RegisterAccount(Id, Password, FString());
}

void ULoginWidgetBase::HandleLoginClicked()    { TryLogin(); }
// [AUTHUI-035] 가입 실행 대신 가입 화면을 연다.
void ULoginWidgetBase::HandleRegisterClicked() {
    OpenRegisterPanel();
}

// [AUTHUI-036] 가입 성공 시 자동 로그인 없이 로그인 화면으로 복귀한다.
void ULoginWidgetBase::HandleRegisterCompleted(bool bSuccess, EChatLoginResultBP Result)
{
    if (!bRegisterPanelOpen || !bRegisterRequestPending) return;
    bRegisterRequestPending = false; SetBusy(false);
    if (!bSuccess) {
        if (Result == EChatLoginResultBP::DuplicateId) { bIdAvailable = false; CheckedLoginId.Empty(); SetCheckStatus(2); }
        SetMessage(Result == EChatLoginResultBP::ServerError
            ? TEXT("가입 결과를 확인하지 못했습니다. 로그인하거나 중복 확인을 다시 해주세요.")
            : UServerSubsystem::GetLoginResultText(Result), true);
        return;
    }
    if (LoginIdBox) LoginIdBox->SetText(FText::FromString(SubmittedRegisterId));
    CloseRegisterPanel();
    SetMessage(TEXT("가입이 완료되었습니다. 로그인해 주세요."), false);
}

void ULoginWidgetBase::HandleLoginCompleted(const FChatLoginResult& Result)
{
	SetBusy(false);

	if (!Result.bSuccess)
	{
		SetMessage(UServerSubsystem::GetLoginResultText(Result.Result), true);
		// 비밀번호는 화면에 남겨두지 않는다. 아이디는 고칠 일이 적으니 남긴다.
		if (PasswordBox != nullptr)
		{
			ClearAllPasswordFields();
		}
		return;
	}

	SetMessage(FString::Printf(TEXT("%s 님, 환영합니다."), *Result.Name), false);

	// 성공한 뒤에는 입력칸에 자격증명을 남기지 않는다.
	if (PasswordBox != nullptr)
	{
		ClearAllPasswordFields();
	}

	OnLoginSucceeded(Result);

	// 채팅을 먼저 띄우고 로비를 나중에 띄운다. 나중에 붙은 쪽이 위에 오므로
	// 로비 버튼이 채팅창에 가리지 않는다.
	if (bShowChatWidgetOnSuccess)
	{
		ShowChatWidget();
	}
	if (bShowLobbyWidgetOnSuccess)
	{
		ShowLobbyWidget();
	}
	if (bRemoveOnSuccess)
	{
		RemoveFromParent();
	}
}

// [AUTHUI-037] 연결 종료 시 가입 검증과 대기를 해제한다.
void ULoginWidgetBase::HandleStateChanged(EChatConnectionState NewState, const FString& Detail)
{
    if (NewState == EChatConnectionState::Disconnected) {
        const bool Pending = bRegisterRequestPending;
        bRegisterRequestPending = false;
        HandleRegisterIdChanged(FText::GetEmpty()); SetBusy(false);
        SetMessage(Pending ? TEXT("가입 결과를 확인하지 못했습니다. 로그인하거나 중복 확인을 다시 해주세요.")
            : TEXT("서버에 연결할 수 없습니다. 잠시 후 다시 시도해 주세요."), true);
    }
}

void ULoginWidgetBase::ShowChatWidget()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	// 디자이너 WBP 가 지정돼 있으면 그것을, 없으면 C++ 기본 위젯을 쓴다.
	UClass* WidgetClass = ChatWidgetClass ? ChatWidgetClass.Get() : UChatWidgetBase::StaticClass();
	if (UUserWidget* ChatWidget = CreateWidget<UUserWidget>(PC, WidgetClass))
	{
		ChatWidget->AddToViewport();
	}
}

void ULoginWidgetBase::ShowLobbyWidget()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	UClass* WidgetClass = LobbyWidgetClass ? LobbyWidgetClass.Get() : ULobbyWidgetBase::StaticClass();
	if (UUserWidget* LobbyWidget = CreateWidget<UUserWidget>(PC, WidgetClass))
	{
		LobbyWidget->AddToViewport();
	}
}

// [AUTHUI-038] 요청 중 입력과 버튼을 함께 잠가 중복 제출을 방지한다.
void ULoginWidgetBase::SetBusy(bool bInBusy)
{
    bBusy = bInBusy;
    UWidget* Controls[] = {LoginIdBox, PasswordBox, LoginButton, RegisterButton,
        RegisterIdBox, RegisterPasswordBox, ConfirmPasswordBox, CheckIdButton,
        SubmitRegisterButton, CloseRegisterButton, LoginPasswordEyeButton, RegisterPasswordEyeButton};
    for (UWidget* Control : Controls) if (Control) Control->SetIsEnabled(!bBusy);
    if (CheckIdButton) CheckIdButton->SetIsEnabled(!bBusy && !ActiveCheckRequestId);
}

// [AUTHUI-039] 활성 패널에 진행 또는 오류 안내를 표시한다.
void ULoginWidgetBase::SetMessage(const FString& Text, bool bIsError)
{
    UTextBlock* Target = bRegisterPanelOpen ? RegisterMessageText.Get() : MessageText.Get();
    if (!Target) return;
    Target->SetText(FText::FromString(Text));
    Target->SetColorAndOpacity(FSlateColor(bIsError ? FLinearColor(1.f,.35f,.3f) : FLinearColor(.75f,.9f,1.f)));
}

// ---------------------------------------------------------------------------
// 콘솔 명령 - 게임 플로우에 로그인 화면을 붙이기 전에 UI 를 검증하기 위한 것.
//
//   MOU.Chat.ShowLogin      로그인 위젯을 띄운다
//
// 게임에서 정식으로 쓸 때는 이 명령이 아니라, 타이틀 화면에서
// CreateWidget<ULoginWidgetBase>() -> AddToViewport() 를 직접 호출하면 된다.
// ---------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GShowLoginCommand(
	TEXT("MOU.Chat.ShowLogin"),
	TEXT("로그인 UI 를 띄운다. 사용법: MOU.Chat.ShowLogin [호스트] [포트]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (PC == nullptr)
			{
				return;
			}

			ULoginWidgetBase* Widget = CreateWidget<ULoginWidgetBase>(PC, ULoginWidgetBase::StaticClass());
			if (Widget == nullptr)
			{
				return;
			}

			// NativeConstruct 에서 접속하므로 뷰포트에 붙이기 전에 설정을 끝내야 한다.
			if (Args.IsValidIndex(0)) { Widget->ServerHost = Args[0]; }
			if (Args.IsValidIndex(1)) { Widget->ServerPort = FCString::Atoi(*Args[1]); }

			Widget->AddToViewport();

			// 로그인 화면은 마우스로 조작하므로 커서를 켜준다.
			PC->SetShowMouseCursor(true);
		}));
#endif

// [AUTHUI-010] 로그인 입력을 지우고 별도 회원가입 패널을 연다.
void ULoginWidgetBase::OpenRegisterPanel()
{
    if (bBusy) return;
    ClearAllPasswordFields(); bRegisterPanelOpen = true;
    if (LoginPanel) LoginPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (RegisterPanel) RegisterPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (RegisterIdBox) RegisterIdBox->SetText(FText::GetEmpty());
    HandleRegisterIdChanged(FText::GetEmpty());
    SetMessage(TEXT("아이디 중복 확인 후 비밀번호를 입력하세요."), false);
}

// [AUTHUI-011] 가입 조회와 비밀번호를 지우고 로그인 화면으로 돌아간다.
void ULoginWidgetBase::CloseRegisterPanel()
{
    if (bBusy) return;
    HandleRegisterIdChanged(FText::GetEmpty()); ClearAllPasswordFields(); bRegisterPanelOpen = false;
    if (RegisterPanel) RegisterPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (LoginPanel) LoginPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// [AUTHUI-012] 아이디 편집 시 이전 중복 확인과 진행 중 조회를 무효화한다.
void ULoginWidgetBase::HandleRegisterIdChanged(const FText& Text)
{
    if (auto* Chat = GetServerSubsystem()) Chat->CancelLoginIdCheck(ActiveCheckRequestId);
    ActiveCheckRequestId = 0; bIdAvailable = false; CheckedLoginId.Empty(); SetCheckStatus(0);
    if (CheckIdButton) CheckIdButton->SetIsEnabled(!bBusy);
}

// [AUTHUI-013] 아이디 형식을 검사하고 서버에 중복 확인을 요청한다.
void ULoginWidgetBase::HandleCheckIdClicked()
{
    if (bBusy || ActiveCheckRequestId || !bRegisterPanelOpen || !RegisterIdBox) return;
    const FString Id = RegisterIdBox->GetText().ToString().TrimStartAndEnd();
    FString Reason;
    if (!UServerSubsystem::ValidateLoginId(Id, Reason)) { SetMessage(Reason, true); return; }
    HandleRegisterIdChanged(FText::GetEmpty()); CheckingLoginId = Id;
    EnsureConnected();
    if (auto* Chat = GetServerSubsystem()) ActiveCheckRequestId = Chat->CheckLoginId(Id);
    if (CheckIdButton) CheckIdButton->SetIsEnabled(!ActiveCheckRequestId);
    SetMessage(ActiveCheckRequestId ? TEXT("중복 확인 중...") : TEXT("서버 연결 후 다시 시도해 주세요."), !ActiveCheckRequestId);
}

// [AUTHUI-014] 요청 번호와 현재 아이디가 일치하는 조회 결과만 반영한다.
void ULoginWidgetBase::HandleIdChecked(int64 RequestId, EChatLoginResultBP Result)
{
    if (!bRegisterPanelOpen || !ActiveCheckRequestId || RequestId != ActiveCheckRequestId) return;
    ActiveCheckRequestId = 0;
    if (CheckIdButton) CheckIdButton->SetIsEnabled(!bBusy);
    if (!RegisterIdBox || RegisterIdBox->GetText().ToString().TrimStartAndEnd() != CheckingLoginId) return;
    bIdAvailable = Result == EChatLoginResultBP::Success;
    CheckedLoginId = bIdAvailable ? CheckingLoginId : FString();
    SetCheckStatus(bIdAvailable ? 1 : Result == EChatLoginResultBP::DuplicateId ? 2 : 0);
    SetMessage(bIdAvailable ? TEXT("비밀번호를 입력하고 회원가입을 완료하세요.") : Result == EChatLoginResultBP::DuplicateId ? TEXT("다른 아이디로 중복 확인해 주세요.") : UServerSubsystem::GetLoginResultText(Result), !bIdAvailable);
}

// [AUTHUI-015] 로그인 비밀번호의 마스킹과 눈 아이콘을 함께 전환한다.
void ULoginWidgetBase::ToggleLoginPassword()
{
    bLoginPasswordVisible = !bLoginPasswordVisible;
    if (PasswordBox) PasswordBox->SetIsPassword(!bLoginPasswordVisible);
    if (LoginEyeSlash) LoginEyeSlash->SetVisibility(bLoginPasswordVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

// [AUTHUI-016] 가입 비밀번호의 마스킹과 눈 아이콘을 함께 전환한다.
void ULoginWidgetBase::ToggleRegisterPassword()
{
    bRegisterPasswordVisible = !bRegisterPasswordVisible;
    if (RegisterPasswordBox) RegisterPasswordBox->SetIsPassword(!bRegisterPasswordVisible);
    if (RegisterEyeSlash) RegisterEyeSlash->SetVisibility(bRegisterPasswordVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

// [AUTHUI-017] 계정 길이 정책·비밀번호 일치·아이디 중복 확인을 검증한다.
bool ULoginWidgetBase::ReadAndValidateRegisterInput(FString& Id, FString& Password)
{
    Id = RegisterIdBox ? RegisterIdBox->GetText().ToString().TrimStartAndEnd() : FString();
    Password = RegisterPasswordBox ? RegisterPasswordBox->GetText().ToString() : FString();
    FString Reason;
    if (!UServerSubsystem::ValidateCredentials(Id, Password, Reason)) { SetMessage(Reason, true); return false; }
    if (!ConfirmPasswordBox || Password != ConfirmPasswordBox->GetText().ToString()) { SetMessage(TEXT("비밀번호가 일치하지 않습니다."), true); return false; }
    if (!bIdAvailable || CheckedLoginId != Id) { SetMessage(TEXT("아이디 중복 확인을 완료해 주세요."), true); return false; }
    return true;
}

// [AUTHUI-018] 모든 비밀번호를 지우고 숨김 상태로 되돌린다.
void ULoginWidgetBase::ClearAllPasswordFields()
{
    UEditableTextBox* Boxes[] = {PasswordBox, RegisterPasswordBox, ConfirmPasswordBox};
    for (auto* Box : Boxes) if (Box) { Box->SetText(FText::GetEmpty()); Box->SetIsPassword(true); }
    bLoginPasswordVisible = bRegisterPasswordVisible = false;
    if (LoginEyeSlash) LoginEyeSlash->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (RegisterEyeSlash) RegisterEyeSlash->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// [AUTHUI-019] 중복 확인 안내·사용 가능·중복 이미지를 배타적으로 표시한다.
void ULoginWidgetBase::SetCheckStatus(int32 Status)
{
    UWidget* Images[] = {CheckIdHint, CheckIdAvailable, CheckIdDuplicate};
    for (int32 I=0; I<3; ++I) if (Images[I]) Images[I]->SetVisibility(I == Status ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

// [AUTHUI-020] Enter 입력을 현재 패널의 로그인 또는 가입 제출에 연결한다.
void ULoginWidgetBase::HandlePasswordCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (Method != ETextCommit::OnEnter) return;
    if (bRegisterPanelOpen) TryRegister(); else TryLogin();
}
