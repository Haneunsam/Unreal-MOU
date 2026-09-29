#include "Server/Lobby/LobbyPageWidgetBase.h"
#include "Server/Lobby/RoomPlayerSlotWidgetBase.h"
#include "Server/Lobby/LobbyCustomizationComponent.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Server/ServerSubsystem.h"
#include "Components/CharacterCustomizationComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

namespace
{
	UVerticalBox* BuildPagePanel(UWidgetTree* Tree, const TCHAR* RootName, const FVector2D& Size)
	{
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), RootName);
		Tree->RootWidget = Canvas;

		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.94f));
		Border->SetPadding(FMargin(20.f));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Border);
		Slot->SetAnchors(FAnchors(0.5f));
		Slot->SetAlignment(FVector2D(0.5f));
		Slot->SetSize(Size);

		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Border->AddChild(Box);
		return Box;
	}

	void AddRow(UVerticalBox* Box, UWidget* Widget, float BottomPadding = 8.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
	}

	UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const TCHAR* Text)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		AddRow(Box, Label);
		return Label;
	}

	UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const TCHAR* Text, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(Name) + TEXT("Label")));
		Label->SetText(FText::FromString(Text));
		Button->AddChild(Label);
		AddRow(Box, Button);
		if (OutLabel != nullptr)
		{
			*OutLabel = Label;
		}
		return Button;
	}

	void SetMessageText(UTextBlock* Target, const FString& Text, bool bIsError)
	{
		if (Target == nullptr)
		{
			return;
		}
		Target->SetText(FText::FromString(Text));
		Target->SetColorAndOpacity(FSlateColor(bIsError
			? FLinearColor(1.f, 0.45f, 0.45f)
			: FLinearColor(0.75f, 0.75f, 0.75f)));
	}
}

void ULobbyMainWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbyMainWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (CreateRoomButton) { CreateRoomButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleCreateRoomClicked); }
	if (JoinRoomButton) { JoinRoomButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleJoinRoomClicked); }
	if (SettingsButton) { SettingsButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleSettingsClicked); }
	if (QuitGameButton) { QuitGameButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleQuitGameClicked); }
}

void ULobbyMainWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbyMainRoot"), FVector2D(380.f, 440.f));
	TitleText = AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("MOU 로비"));
	StatusText = AddText(WidgetTree, Box, TEXT("StatusText"), TEXT("서버 상태 확인 중..."));
	CreateRoomButton = AddButton(WidgetTree, Box, TEXT("CreateRoomButton"), TEXT("방 만들기"));
	JoinRoomButton = AddButton(WidgetTree, Box, TEXT("JoinRoomButton"), TEXT("방 참여하기"));
	SettingsButton = AddButton(WidgetTree, Box, TEXT("SettingsButton"), TEXT("환경설정"));
	QuitGameButton = AddButton(WidgetTree, Box, TEXT("QuitGameButton"), TEXT("게임 종료"));
	MessageText = AddText(WidgetTree, Box, TEXT("MessageText"), TEXT(""));
}

void ULobbyMainWidgetBase::Refresh(const UServerSubsystem* Server)
{
	const bool bLoggedIn = Server != nullptr && Server->GetConnectionState() == EChatConnectionState::LoggedIn;
	if (CreateRoomButton) { CreateRoomButton->SetIsEnabled(bLoggedIn); }
	if (JoinRoomButton) { JoinRoomButton->SetIsEnabled(bLoggedIn); }
	if (StatusText)
	{
		FString Status = TEXT("서버에 연결되어 있지 않습니다.");
		if (Server != nullptr)
		{
			switch (Server->GetConnectionState())
			{
			case EChatConnectionState::LoggedIn: Status = FString::Printf(TEXT("%s 님으로 접속 중"), *Server->GetLoginResult().Name); break;
			case EChatConnectionState::Connected: Status = TEXT("서버에 연결됨. 로그인 대기 중..."); break;
			case EChatConnectionState::Connecting: Status = TEXT("서버에 연결하는 중..."); break;
			default: break;
			}
		}
		StatusText->SetText(FText::FromString(Status));
	}
}

void ULobbyMainWidgetBase::SetMessage(const FString& Text, bool bIsError) { SetMessageText(MessageText, Text, bIsError); }
void ULobbyMainWidgetBase::HandleCreateRoomClicked() { OnCreateRoom.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleJoinRoomClicked() { OnJoinRoom.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleSettingsClicked() { OnOpenSettings.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleQuitGameClicked() { OnQuitGame.ExecuteIfBound(); }

void URoomLobbyWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void URoomLobbyWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	EnsurePlayerSlots();
	if (ReadyButton) { ReadyButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleReadyClicked); }
	if (StartGameButton) { StartGameButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleStartClicked); }
	if (CustomizeButton) { CustomizeButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleCustomizeClicked); }
	if (LeaveRoomButton) { LeaveRoomButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleLeaveClicked); }
}

void URoomLobbyWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("RoomLobbyRoot"), FVector2D(560.f, 660.f));
	TitleText = AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("방 대기실"));
	StatusText = AddText(WidgetTree, Box, TEXT("StatusText"), TEXT(""));
	PlayerSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PlayerSlotGrid"));
	AddRow(Box, PlayerSlotGrid, 12.f);
	CastChecked<UVerticalBoxSlot>(PlayerSlotGrid->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* ReadyLabel = nullptr;
	UTextBlock* StartLabel = nullptr;
	ReadyButton = AddButton(WidgetTree, Box, TEXT("ReadyButton"), TEXT("준비하기"), &ReadyLabel);
	StartGameButton = AddButton(WidgetTree, Box, TEXT("StartGameButton"), TEXT("게임 시작"), &StartLabel);
	ReadyButtonLabel = ReadyLabel;
	StartGameButtonLabel = StartLabel;
	CustomizeButton = AddButton(WidgetTree, Box, TEXT("CustomizeButton"), TEXT("커스터마이징"));
	LeaveRoomButton = AddButton(WidgetTree, Box, TEXT("LeaveRoomButton"), TEXT("방 나가기"));
	MessageText = AddText(WidgetTree, Box, TEXT("MessageText"), TEXT(""));
}

// [RTITLE-005] 현재 방 제목과 준비 상태를 대기실 위젯에 반영한다.
void URoomLobbyWidgetBase::Refresh(const UServerSubsystem* Server)
{
	if (TitleText)
	{
		const FString RoomTitle = Server ? Server->GetCurrentRoomTitle() : FString();
		TitleText->SetText(FText::FromString(RoomTitle.IsEmpty() ? TEXT("방 대기실") : RoomTitle));
	}
	if (Server == nullptr)
	{
		return;
	}
	const bool bIsHost = Server->IsRoomHost();
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("방 #%d — %s"),
			Server->GetCurrentRoomId(), bIsHost ? TEXT("방장") : TEXT("참여자"))));
	}
	if (ReadyButton)
	{
		ReadyButton->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ReadyButton->SetIsEnabled(!bIsHost);
	}
	if (ReadyButtonLabel)
	{
		ReadyButtonLabel->SetText(FText::FromString(Server->IsSelfReady() ? TEXT("준비 해제") : TEXT("준비하기")));
	}
	if (StartGameButton)
	{
		StartGameButton->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StartGameButton->SetIsEnabled(bIsHost && Server->AreAllMembersReady());
	}
	if (StartGameButtonLabel)
	{
		StartGameButtonLabel->SetText(FText::FromString(Server->AreAllMembersReady()
			? TEXT("게임 시작") : TEXT("게임 시작 (준비 대기 중)")));
	}
	RebuildMemberList(Server);
}

void URoomLobbyWidgetBase::EnsurePlayerSlots()
{
	if (!PlayerSlotGrid && MemberListBox)
	{
		PlayerSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
		MemberListBox->ClearChildren();
		UVerticalBoxSlot* GridSlot = MemberListBox->AddChildToVerticalBox(PlayerSlotGrid);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (!PlayerSlotGrid) { return; }
	if (PlayerSlots.Num() < 4)
	{
		PlayerSlotGrid->SetSlotPadding(FMargin(6.f));
		UClass* SlotClass = PlayerSlotWidgetClass ? PlayerSlotWidgetClass.Get() : URoomPlayerSlotWidgetBase::StaticClass();
		for (int32 Index = PlayerSlots.Num(); Index < 4; ++Index)
		{
			auto* PlayerSlot = CreateWidget<URoomPlayerSlotWidgetBase>(GetOwningPlayer(), SlotClass);
			if (!PlayerSlot) { return; }
			PlayerSlots.Add(PlayerSlot);
			UUniformGridSlot* GridSlot = PlayerSlotGrid->AddChildToUniformGrid(
				PlayerSlot, Index / FMath::Clamp(SlotColumns, 1, 4), Index % FMath::Clamp(SlotColumns, 1, 4));
			// Canvas-based WBP cards must receive the whole cell, not only their desired size.
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void URoomLobbyWidgetBase::RebuildMemberList(const UServerSubsystem* Server)
{
	if (!Server) { return; }
	EnsurePlayerSlots();
	const auto Members = Server->GetRoomMembers();
	for (int32 Index = 0; Index < PlayerSlots.Num(); ++Index)
	{
		const FMOURoomMember* Found = Members.FindByPredicate(
			[Index](const FMOURoomMember& Member) { return Member.SlotIndex == Index; });
		if (Found) { PlayerSlots[Index]->SetMember(*Found, Found->UserId == Server->GetLoginResult().UserId); }
		else { PlayerSlots[Index]->ClearMember(); }
	}
}

void URoomLobbyWidgetBase::SetMessage(const FString& Text, bool bIsError) { SetMessageText(MessageText, Text, bIsError); }
void URoomLobbyWidgetBase::HandleReadyClicked() { OnToggleReady.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleStartClicked() { OnStartGame.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleCustomizeClicked() { OnCustomize.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleLeaveClicked() { OnLeaveRoom.ExecuteIfBound(); }

void ULobbySettingsWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbySettingsWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (BackButton) { BackButton->OnClicked.AddUniqueDynamic(this, &ULobbySettingsWidgetBase::HandleBackClicked); }
}

void ULobbySettingsWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbySettingsRoot"), FVector2D(420.f, 320.f));
	AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("환경설정"));
	AddText(WidgetTree, Box, TEXT("DescriptionText"), TEXT("WBP에서 설정 항목을 배치하세요."));
	BackButton = AddButton(WidgetTree, Box, TEXT("BackButton"), TEXT("뒤로가기"));
}

void ULobbySettingsWidgetBase::HandleBackClicked() { OnBack.ExecuteIfBound(); }

// [LCUI-003] 디자이너 루트가 없으면 기본 편집 화면을 생성한다.
void ULobbyCustomizeWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

// [LCUI-004] 편집 상태를 초기화하고 본인 미리보기와 버튼 및 서버 이벤트를 연결한다.
void ULobbyCustomizeWidgetBase::NativeConstruct()
{
	ReleaseLocalPreview();
	bWaitingForConfirmation = false;
	SetIsEnabled(true);
	Super::NativeConstruct();
	ConnectOwnSlotPreview();
	if (BackButton) { BackButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleBackClicked); }
	if (ConfirmButton) { ConfirmButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleConfirmClicked); }
	if (ResetButton) { ResetButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleResetClicked); }
	if (auto* Server = UServerSubsystem::Get(this))
	{
		Server->OnLobbyCustomizationResult.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleCustomizationResult);
		Server->OnRoomMembersChanged.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandlePreviewRoomMembersChanged);
	}
}

// [LCUI-001] 본인 슬롯을 원본으로 창 전용 미리보기를 생성한다.
bool ULobbyCustomizeWidgetBase::CreateLocalPreview(AActor* SourceActor)
{
	USkeletalMeshComponent* SourceMesh = SourceActor ? SourceActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	USceneCaptureComponent2D* SourceCapture = SourceActor ? SourceActor->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
	UTextureRenderTarget2D* SourceTarget = SourceCapture ? SourceCapture->TextureTarget.Get() : nullptr;
	if (!GetWorld() || !SourceMesh || !SourceMesh->GetSkeletalMeshAsset() || !SourceTarget) return false;

	ReleaseLocalPreview();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// BP 액터를 복제하지 않아 슬롯 갱신 Tick과 기존 커스터마이징 컴포넌트가 유입되지 않는다.
	AActor* Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SourceActor->GetActorTransform(), SpawnParameters);
	if (!Actor) return false;
	LocalPreviewActor = Actor;
	Actor->SetReplicates(false);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	USceneComponent* Root = NewObject<USceneComponent>(Actor);
	Actor->AddInstanceComponent(Root);
	Actor->SetRootComponent(Root);
	Root->SetWorldTransform(SourceActor->GetActorTransform());
	Root->RegisterComponent();

	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Actor);
	Actor->AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(Root);
	Mesh->SetRelativeTransform(SourceMesh->GetComponentTransform().GetRelativeTransform(SourceActor->GetActorTransform()));
	Mesh->SetSkeletalMeshAsset(SourceMesh->GetSkeletalMeshAsset());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetVisibleInSceneCaptureOnly(true);
	for (int32 Index = 0; Index < SourceMesh->GetNumMaterials(); ++Index)
		Mesh->SetMaterial(Index, SourceMesh->GetMaterial(Index));
	// 포즈만 원본을 따라가며 메시, 회전, 머티리얼은 편집창이 별도로 소유한다.
	Mesh->SetLeaderPoseComponent(SourceMesh);
	Mesh->RegisterComponent();
	LocalPreviewMesh = Mesh;

	ULobbyCustomizationComponent* Component = NewObject<ULobbyCustomizationComponent>(Actor);
	Actor->AddInstanceComponent(Component);
	Component->RegisterComponent();
	if (!Component->InitializeLobbyPreview(Mesh, EditingDataAsset, CurrentData))
	{
		ReleaseLocalPreview();
		return false;
	}
	PreviewComponent = Component;

	// 로봇의 눈/입/유리처럼 별도 StaticMesh인 부속물도 보존한다.
	TInlineComponentArray<UStaticMeshComponent*> SourceParts(SourceActor);
	TMap<USceneComponent*, USceneComponent*> CopiedParts;
	CopiedParts.Add(SourceMesh, Mesh);
	for (UStaticMeshComponent* SourcePart : SourceParts)
	{
		if (!SourcePart->GetStaticMesh() || !SourcePart->IsVisible() || SourcePart->bHiddenInGame) continue;
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Actor);
		Actor->AddInstanceComponent(Part);
		Part->SetupAttachment(Mesh);
		Part->SetRelativeTransform(SourcePart->GetComponentTransform().GetRelativeTransform(SourceMesh->GetComponentTransform()));
		Part->SetStaticMesh(SourcePart->GetStaticMesh());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetVisibleInSceneCaptureOnly(true);
		for (int32 Index = 0; Index < SourcePart->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Material = SourcePart->GetMaterial(Index);
			UMaterialInstanceDynamic* SourceDMI = Cast<UMaterialInstanceDynamic>(Material);
			if (SourceDMI) Material = SourceDMI->Parent;
			if (!Material) continue;
			UMaterialInstanceDynamic* PartDMI = UMaterialInstanceDynamic::Create(Material, this);
			if (SourceDMI) PartDMI->CopyInterpParameters(SourceDMI);
			Part->SetMaterial(Index, PartDMI);
		}
		Part->RegisterComponent();
		CopiedParts.Add(SourcePart, Part);
	}
	for (UStaticMeshComponent* SourcePart : SourceParts)
	{
		USceneComponent** Part = CopiedParts.Find(SourcePart);
		USceneComponent** Parent = CopiedParts.Find(SourcePart->GetAttachParent());
		if (Part && Parent)
		{
			(*Part)->AttachToComponent(*Parent, FAttachmentTransformRules::KeepRelativeTransform, SourcePart->GetAttachSocketName());
			(*Part)->SetRelativeTransform(SourcePart->GetRelativeTransform());
		}
	}

	PreviewRenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	PreviewRenderTarget->RenderTargetFormat = SourceTarget->RenderTargetFormat;
	PreviewRenderTarget->ClearColor = SourceTarget->ClearColor;
	PreviewRenderTarget->TargetGamma = SourceTarget->TargetGamma;
	PreviewRenderTarget->InitCustomFormat(FMath::Max(1, SourceTarget->SizeX), FMath::Max(1, SourceTarget->SizeY),
		SourceTarget->GetFormat(), SourceTarget->bForceLinearGamma);
	PreviewRenderTarget->UpdateResourceImmediate(true);

	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Actor);
	Actor->AddInstanceComponent(Capture);
	Capture->SetupAttachment(Root);
	Capture->SetRelativeTransform(SourceCapture->GetComponentTransform().GetRelativeTransform(SourceActor->GetActorTransform()));
	Capture->ProjectionType = SourceCapture->ProjectionType;
	Capture->FOVAngle = SourceCapture->FOVAngle;
	Capture->OrthoWidth = SourceCapture->OrthoWidth;
	Capture->CaptureSource = SourceCapture->CaptureSource;
	Capture->PostProcessSettings = SourceCapture->PostProcessSettings;
	Capture->PostProcessBlendWeight = SourceCapture->PostProcessBlendWeight;
	Capture->SetShowFlagSettings(SourceCapture->GetShowFlagSettings());
	Capture->ShowFlags = SourceCapture->ShowFlags;
	Capture->TextureTarget = PreviewRenderTarget;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Add(Actor);
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = false;
	Capture->RegisterComponent();
	PreviewCapture = Capture;

	// 같은 위치의 조명은 유지하되 기존 캡처에는 전용 액터가 겹쳐 보이지 않게 한다.
	for (TObjectIterator<USceneCaptureComponent> It; It; ++It)
	{
		USceneCaptureComponent* OtherCapture = *It;
		if (OtherCapture == Capture || OtherCapture->GetWorld() != GetWorld() || !OtherCapture->IsRegistered()) continue;
		OtherCapture->HiddenActors.Add(Actor);
		OtherPreviewCaptures.Add(OtherCapture);
	}
	SourcePreviewActor = SourceActor;
	return true;
}

// [LCUI-002] 창 전용 미리보기 자원을 해제한다.
void ULobbyCustomizeWidgetBase::ReleaseLocalPreview()
{
	if (PreviewImage)
	{
		PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		PreviewImage->SetBrushFromTexture(nullptr);
	}
	if (PreviewCapture.IsValid())
	{
		PreviewCapture->bCaptureEveryFrame = false;
		PreviewCapture->Deactivate();
		PreviewCapture->TextureTarget = nullptr;
	}
	if (AActor* Actor = LocalPreviewActor.Get())
	{
		for (const TWeakObjectPtr<USceneCaptureComponent>& Capture : OtherPreviewCaptures)
			if (Capture.IsValid()) Capture->HiddenActors.Remove(Actor);
		Actor->Destroy();
	}
	OtherPreviewCaptures.Reset();
	PreviewComponent.Reset();
	LocalPreviewMesh.Reset();
	LocalPreviewActor.Reset();
	SourcePreviewActor.Reset();
	PreviewCapture.Reset();
	PreviewRenderTarget = nullptr;
	PreviewUIMaterial = nullptr;
	LocalSlotIndex = INDEX_NONE;
	PreviewRoomId = 0;
}

// [LCUI-019] 로그인한 본인 슬롯을 찾아 독립 미리보기를 PreviewImage에 연결한다.
void ULobbyCustomizeWidgetBase::ConnectOwnSlotPreview()
{
	// 본인 UserId로 슬롯을 찾으며 정보가 없을 때 호스트 슬롯을 대신 사용하지 않는다.
	const UServerSubsystem* Server = UServerSubsystem::Get(this);
	if (!GetWorld() || !Server || Server->GetCurrentRoomId() == 0)
	{
		ReleaseLocalPreview();
		return;
	}
	const int64 SelfUserId = Server->GetLoginResult().UserId;
	const TArray<FMOURoomMember> Members = Server->GetRoomMembers();
	const FMOURoomMember* Self = Members.FindByPredicate(
		[SelfUserId](const FMOURoomMember& Member) { return Member.UserId == SelfUserId; });
	if (!Self || Self->SlotIndex < 0 || Self->SlotIndex > 3)
	{
		ReleaseLocalPreview();
		ShowStatus(FText::FromString(TEXT("내 슬롯 정보 수신을 기다리는 중...")), false);
		return;
	}

	// A designer-placed PreviewImage controls placement; a Canvas-only WBP gets a fallback.
	if (!PreviewImage)
	{
		if (auto* Canvas = Cast<UCanvasPanel>(WidgetTree ? WidgetTree->RootWidget : nullptr))
		{
			PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
			if (auto* CanvasSlot = Canvas->AddChildToCanvas(PreviewImage))
			{
				CanvasSlot->SetAnchors(FAnchors(0.02f, 0.12f, 0.38f, 0.92f));
				CanvasSlot->SetOffsets(FMargin(0));
				CanvasSlot->SetZOrder(-1);
			}
		}
	}
	if (!PreviewImage)
	{
		ShowStatus(FText::FromString(TEXT("PreviewImage가 없습니다. WBP에 Image를 추가하세요.")), false);
		return;
	}
	PreviewImage->SetVisibility(ESlateVisibility::Collapsed);

	const int32 SlotIndex = Self->SlotIndex;
	AActor* SlotActor = URoomPlayerSlotWidgetBase::FindLobbyPreviewActor(this, SlotIndex);
	if (PreviewComponent.IsValid() && SourcePreviewActor.Get() == SlotActor && PreviewRoomId == Server->GetCurrentRoomId())
	{
		PreviewImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/02_JSY/MainLobby/LobbyCharacter/M_UI_LobbyCharacter.M_UI_LobbyCharacter"));
	if (!Material || !CreateLocalPreview(SlotActor))
	{
		ReleaseLocalPreview();
		ShowStatus(FText::FromString(FString::Printf(
			TEXT("슬롯 %d의 편집용 미리보기를 생성하지 못했습니다. 메시와 SceneCapture/RenderTarget을 확인하세요."), SlotIndex)), false);
		return;
	}
	LocalSlotIndex = SlotIndex;
	PreviewRoomId = Server->GetCurrentRoomId();
	PreviewUIMaterial = UMaterialInstanceDynamic::Create(Material, this);
	PreviewUIMaterial->SetTextureParameterValue(TEXT("PortraitRT"), PreviewRenderTarget);
	PreviewImage->SetBrushFromMaterial(PreviewUIMaterial);
	PreviewImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UpdatePreview();
	ShowStatus(FText::GetEmpty(), true);
}

// [LCUI-017] 멤버 목록 갱신 시 본인 슬롯 연결만 확인하고 편집값은 보존한다.
void ULobbyCustomizeWidgetBase::HandlePreviewRoomMembersChanged(
	int32 RoomId, const TArray<FMOURoomMember>& /*Members*/, bool /*bAllReady*/)
{
	if (const UServerSubsystem* Server = UServerSubsystem::Get(this))
		if (RoomId == Server->GetCurrentRoomId()) ConnectOwnSlotPreview();
}

// [LCUI-021] BP 레이아웃이 없는 경우의 기본 버튼과 상태 표시를 만든다.
void ULobbyCustomizeWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbyCustomizeRoot"), FVector2D(520.f, 420.f));
	AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("커스터마이징"));
	AddText(WidgetTree, Box, TEXT("DescriptionText"), TEXT("몸 색상 / 금속성 / 거칠기 A·B / 데칼 / 데칼 색상 / 반복 X·Y"));
	ConfirmButton = AddButton(WidgetTree, Box, TEXT("ConfirmButton"), TEXT("적용 및 저장"));
	ResetButton = AddButton(WidgetTree, Box, TEXT("ResetButton"), TEXT("기본값"));
	CustomizationStatusText = AddText(WidgetTree, Box, TEXT("CustomizationStatusText"), TEXT(""));
	BackButton = AddButton(WidgetTree, Box, TEXT("BackButton"), TEXT("뒤로가기"));
}

// [LCUI-020] 뒤로가기 버튼을 편집 취소 처리에 연결한다.
void ULobbyCustomizeWidgetBase::HandleBackClicked() { CancelAndExit(); }
// [LCUI-014] 확인 버튼을 서버 전송 처리에 연결한다.
void ULobbyCustomizeWidgetBase::HandleConfirmClicked() { ConfirmAndSave(); }
// [LCUI-015] 전송 대기 중이 아닐 때 기본 외형을 미리본다.
void ULobbyCustomizeWidgetBase::HandleResetClicked() { if (!bWaitingForConfirmation) ResetToDefault(); }

// [LCUI-012] 본인의 확정된 외형으로 편집값과 UI를 초기화한다.
void ULobbyCustomizeWidgetBase::InitializeCustomization()
{
	CachedCharacter.Reset();
	CachedCustomizationComp.Reset();
	if (auto* Server = UServerSubsystem::Get(this))
	{
		CurrentData = Server->GetLocalCustomization();
		const int64 SelfUserId = Server->GetLoginResult().UserId;
		const TArray<FMOURoomMember> Members = Server->GetRoomMembers();
		if (const FMOURoomMember* Self = Members.FindByPredicate(
			[SelfUserId](const FMOURoomMember& Member) { return Member.UserId == SelfUserId; }))
		{
			CurrentData = Self->Customization;
			if (!EditingDataAsset)
			{
				if (AActor* Source = URoomPlayerSlotWidgetBase::FindLobbyPreviewActor(this, Self->SlotIndex))
					if (auto* Component = Source->FindComponentByClass<UCharacterCustomizationComponent>())
						EditingDataAsset = Component->GetCustomizationDataAsset();
			}
		}
	}
	if (!EditingDataAsset) EditingDataAsset = NewObject<UCustomizationDataAsset>(this);
	OriginalData = CurrentData;
	OnCustomizationDataInitialized(CurrentData);
	UpdatePreview();
}

// [LCUI-009] 기존 BP 연결을 받되 실제 편집 대상은 본인 슬롯에서 복사한 전용 메시로 제한한다.
void ULobbyCustomizeWidgetBase::SetPreviewComponent(UCharacterCustomizationComponent* Component)
{
	if (!Component)
	{
		ReleaseLocalPreview();
		return;
	}
	// 전달된 슬롯 컴포넌트를 직접 편집하지 않고 본인 UserId를 다시 확인한다.
	ConnectOwnSlotPreview();
}

// [LCUI-013] 색상과 문양 편집을 창 전용 메시 및 RenderTarget에만 반영한다.
void ULobbyCustomizeWidgetBase::UpdatePreview()
{
	if (bWaitingForConfirmation) return;
	if (PreviewComponent.IsValid()) PreviewComponent->ApplyLobbyPreview(CurrentData);
	if (PreviewCapture.IsValid() && !PreviewCapture->bCaptureEveryFrame) PreviewCapture->CaptureScene();
	OnCustomizationPreviewChanged(CurrentData);
}

// [LCUI-008] 대기실 캐릭터와 카메라는 유지하고 편집용 메시만 회전한다.
void ULobbyCustomizeWidgetBase::RotateCharacter(float DeltaX)
{
	if (bWaitingForConfirmation) return;
	if (LocalPreviewMesh.IsValid())
		LocalPreviewMesh->AddLocalRotation(FRotator(0, DeltaX * DragRotationSpeed, 0));
}

// [LCUI-018] 편집 상태 메시지를 텍스트와 BP 이벤트에 전달한다.
void ULobbyCustomizeWidgetBase::ShowStatus(const FText& Message, bool bSuccess)
{
	if (CustomizationStatusText) CustomizationStatusText->SetText(Message);
	OnCustomizationStatus(Message, bSuccess);
}

// [LCUI-006] 확인 버튼에서만 편집값을 서버로 전송하고 응답을 기다린다.
void ULobbyCustomizeWidgetBase::ConfirmAndSave()
{
	if (bWaitingForConfirmation) return;
	auto* Server = UServerSubsystem::Get(this);
	if (!Server || PreviewRoomId == 0 || PreviewRoomId != Server->GetCurrentRoomId() ||
		LocalSlotIndex == INDEX_NONE || !PreviewComponent.IsValid() || !PreviewRenderTarget)
	{
		ShowStatus(FText::FromString(TEXT("내 슬롯 미리보기가 준비되지 않았습니다. 슬롯 정보와 RenderTarget을 확인하세요.")), false);
		return;
	}
	CloseColorPickers();
	CloseColorPicker();
	if (!Server->SubmitCustomization(CurrentData))
	{
		ShowStatus(FText::FromString(TEXT("외형을 전송할 수 없습니다. 연결/입장 상태 또는 진행 중인 요청을 확인하세요.")), false);
		return;
	}
	bWaitingForConfirmation = true;
	SetIsEnabled(false);
	ShowStatus(FText::FromString(TEXT("외형 적용 중...")), false);
}

// [LCUI-016] 서버 승인 결과를 반영하고 실패 시 다시 편집할 수 있게 한다.
void ULobbyCustomizeWidgetBase::HandleCustomizationResult(bool bSuccess, bool bSavedToDisk)
{
	if (!bWaitingForConfirmation) return;
	bWaitingForConfirmation = false;
	SetIsEnabled(true);
	if (!bSuccess)
	{
		ShowStatus(FText::FromString(TEXT("외형 적용 실패. 방 상태/서버 연결을 확인한 뒤 다시 시도하세요.")), false);
		return;
	}
	if (auto* Server = UServerSubsystem::Get(this)) CurrentData = Server->GetLocalCustomization();
	OriginalData = CurrentData;
	UpdatePreview();
	if (!bSavedToDisk)
	{
		ShowStatus(FText::FromString(TEXT("외형은 적용되었습니다. 디스크 저장에 실패하여 다음 실행에는 유지되지 않을 수 있습니다.")), true);
		return;
	}
	OnBack.ExecuteIfBound();
}

// [LCUI-007] 편집값과 전용 미리보기를 폐기한 뒤 이전 화면으로 돌아간다.
void ULobbyCustomizeWidgetBase::CancelAndExit()
{
	if (bWaitingForConfirmation) return;
	CloseColorPickers();
	CloseColorPicker();
	CurrentData = OriginalData;
	ReleaseLocalPreview();
	OnBack.ExecuteIfBound();
}

// [LCUI-005] 이벤트 연결과 창 전용 미리보기를 해제한다.
void ULobbyCustomizeWidgetBase::NativeDestruct()
{
	CloseColorPickers();
	if (auto* Server = UServerSubsystem::Get(this))
	{
		Server->OnLobbyCustomizationResult.RemoveDynamic(this, &ULobbyCustomizeWidgetBase::HandleCustomizationResult);
		Server->OnRoomMembersChanged.RemoveDynamic(this, &ULobbyCustomizeWidgetBase::HandlePreviewRoomMembersChanged);
	}
	ReleaseLocalPreview();
	bWaitingForConfirmation = false;
	Super::NativeDestruct();
}
