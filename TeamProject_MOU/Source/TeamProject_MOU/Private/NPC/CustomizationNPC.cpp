#include "NPC/CustomizationNPC.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/CharacterCustomizationComponent.h"
#include "Player/MainCharacter.h"

ACustomizationNPC::ACustomizationNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// 상호작용 Trace 인식을 위해 Visibility 블록
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 플레이어 권장 대기 위치 스팟 컴포넌트 생성 (NPC 정면 150cm 지점)
	PlayerStandSpot = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerStandSpot"));
	PlayerStandSpot->SetupAttachment(RootComponent);
	PlayerStandSpot->SetRelativeLocation(FVector(150.0f, 0.0f, -88.0f));
	PlayerStandSpot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // NPC를 바라보도록 180도 회전

	// 프리뷰 카메라 스프링암 생성 (플레이어 스팟을 비추도록 구성)
	PreviewCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("PreviewCameraBoom"));
	PreviewCameraBoom->SetupAttachment(PlayerStandSpot);
	PreviewCameraBoom->TargetArmLength = 220.0f;
	PreviewCameraBoom->TargetOffset = FVector(0.0f, 0.0f, 90.0f);
	PreviewCameraBoom->bDoCollisionTest = false;
	PreviewCameraBoom->bUsePawnControlRotation = false;
	PreviewCameraBoom->bInheritPitch = false;
	PreviewCameraBoom->bInheritYaw = false;
	PreviewCameraBoom->bInheritRoll = false;
	PreviewCameraBoom->SetRelativeRotation(FRotator(-5.0f, 180.0f, 0.0f));

	// 프리뷰 카메라 컴포넌트 생성
	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(PreviewCameraBoom, USpringArmComponent::SocketName);
	PreviewCamera->bUsePawnControlRotation = false;
	PreviewCamera->SetFieldOfView(75.0f);

	// NPC 본인 외형 커스터마이징 컴포넌트
	CustomizationComponent = CreateDefaultSubobject<UCharacterCustomizationComponent>(TEXT("CustomizationComponent"));

	InteractPromptText = FText::FromString(TEXT("외형 변경 (대화)"));
	DialogueGreetingText = FText::FromString(TEXT("어서 오세요! 멋진 스타일로 외형을 꾸며보세요."));
}

void ACustomizationNPC::BeginPlay()
{
	Super::BeginPlay();
}

bool ACustomizationNPC::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}

	AMainCharacter* Character = Cast<AMainCharacter>(Interactor);
	return Character != nullptr && !Character->IsDead() && !Character->IsGroggy();
}

void ACustomizationNPC::Interact_Implementation(AActor* Interactor)
{
	if (AMainCharacter* Character = Cast<AMainCharacter>(Interactor))
	{
		Character->StartCustomization(this);
	}
}

FText ACustomizationNPC::GetInteractPrompt_Implementation() const
{
	return InteractPromptText;
}
