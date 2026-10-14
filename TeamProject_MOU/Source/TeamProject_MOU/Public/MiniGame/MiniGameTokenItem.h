#pragma once

#include "CoreMinimal.h"
#include "Base/ItemBase.h"
#include "MiniGameTokenItem.generated.h"

UCLASS()
class TEAMPROJECT_MOU_API AMiniGameTokenItem : public AItemBase
{
	GENERATED_BODY()

public:
	AMiniGameTokenItem();

	// ���� ��ū ��� ���� ����
	UFUNCTION(BlueprintPure, Category = "MiniGame|Token")
	bool CanUseToken() const;

	// �� ��ū 1���� �ӽſ� �����ϴ� �÷��� Ƚ��
	UFUNCTION(BlueprintPure, Category = "MiniGame|Token")
	int32 GetGrantedPlayCount() const
	{
		return GrantedPlayCount;
	}

protected:
	// ��Ŭ�� ��ū ���
	virtual void OnUse_Implementation() override;
	UFUNCTION(Server, Reliable)
	void ServerUseToken();

	virtual void PickUp_Implementation(AActor* Picker) override;
	virtual void OnEquipped_Implementation(AActor* Equipper) override;
	virtual void Drop_Implementation(FVector DropLocation, AActor* Dropper) override;

	UFUNCTION(BlueprintCallable, Category = "MiniGame|Token")
	void UseTokenOnServer(AActor* UserActor);

	// ��ū 1�� ��� ���� �� �ӽſ� ������ �÷��� Ƚ��
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniGame|Token",
		meta = (ClampMin = "1"))
	int32 GrantedPlayCount = 2;

	// ��ū �Һ� ���� �� ȣ��
	// ���� Inventory���� ��ū ���� ó�� �����
	UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame|Token")
	void OnTokenConsumed(AActor* UserActor);

private:
	// ���� ��ū�� ������ �ִ� �÷��̾� ã��
	AActor* ResolveTokenUser() const;
};
