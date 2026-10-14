#include "MiniGame/MiniGameTokenItem.h"

#include "MiniGame/MiniGameTokenReceiver.h"
#include "Kismet/GameplayStatics.h"

AMiniGameTokenItem::AMiniGameTokenItem()
{
	// ��ū ������ ��ü�� �� ���� ���
	MaxUseCount = 1;
	CurrentUseCount = 1;

	// ��ū �ϳ��� �̴ϰ��� �÷��� ���� Ƚ��
	GrantedPlayCount = 2;

	// �κ��丮�� ���� ������ ������
	bCanBeStoredInInventory = true;
}

bool AMiniGameTokenItem::CanUseToken() const
{
	return CurrentUseCount > 0;
}

AActor* AMiniGameTokenItem::ResolveTokenUser() const
{
	// �κ��丮 ���� ���¶�� �÷��̾�� Attach�Ǿ� �����Ƿ� �켱 Ȯ��
	if (AActor* AttachedActor = GetAttachParentActor())
	{
		return AttachedActor;
	}

	// ItemBase���� ���������� �������� �����ߴ� �÷��̾�
	if (LastOwner)
	{
		return LastOwner.Get();
	}

	// ������ Owner�� �����Ǿ� �ִ� ���
	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}

	return nullptr;
}

void AMiniGameTokenItem::OnUse_Implementation()
{
	if (HasAuthority())
	{
		UseTokenOnServer(ResolveTokenUser());
	}
	else
	{
		ServerUseToken();
	}
}

void AMiniGameTokenItem::ServerUseToken_Implementation()
{
	UseTokenOnServer(ResolveTokenUser());
}

void AMiniGameTokenItem::PickUp_Implementation(AActor* Picker)
{
	Super::PickUp_Implementation(Picker);

	if (HasAuthority() && IsValid(Picker))
	{
		SetOwner(Picker);
	}
}

void AMiniGameTokenItem::OnEquipped_Implementation(AActor* Equipper)
{
	Super::OnEquipped_Implementation(Equipper);

	if (HasAuthority() && IsValid(Equipper))
	{
		SetOwner(Equipper);
		LastOwner = Equipper;
	}
}

void AMiniGameTokenItem::Drop_Implementation(FVector DropLocation, AActor* Dropper)
{
	if (HasAuthority())
	{
		SetOwner(nullptr);
	}

	Super::Drop_Implementation(DropLocation, Dropper);
}
void AMiniGameTokenItem::UseTokenOnServer(AActor* UserActor)
{
	// ��ū �� �ӽ� ���� ������ ���������� ó��
	if (!HasAuthority())
	{
		return;
	}

	// �̹� �Һ�� ��ū
	if (!CanUseToken())
	{
		return;
	}


	if (!UserActor || !GetWorld())
	{
		return;
	}

	// ��ū�� ���� �� �ִ� ��� �̴ϰ��� �ӽ� �˻�
	TArray<AActor*> MiniGameMachines;

	UGameplayStatics::GetAllActorsWithInterface(
		this,
		UMiniGameTokenReceiver::StaticClass(),
		MiniGameMachines
	);

	for (AActor* Machine : MiniGameMachines)
	{
		if (!IsValid(Machine))
		{
			continue;
		}

		// ���� ��� ���� ���δ� �� �ӽ��� �Ǵ�
		// StockMachine������ UseArea �ȿ� �ִ����� ���⼭ �˻�
		const bool bAccepted =
			IMiniGameTokenReceiver::Execute_TryActivateMiniGameToken(
				Machine,
				UserActor,
				GrantedPlayCount
			);

		if (!bAccepted)
		{
			continue;
		}

		// �ӽ��� ��ū ����� �������� ���� ��ū �Һ�
		CurrentUseCount =
			FMath::Max(0, CurrentUseCount - 1);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[MiniGameToken] Token accepted. GrantedUses = %d"),
			GrantedPlayCount
		);

		// ���� Inventory ���� ó��
		OnTokenConsumed(UserActor);

		return;
	}

	// � �ӽſ����� �������� ���� ���
	// ��ū�� �Һ���� ����
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[MiniGameToken] No available machine.")
	);
}
