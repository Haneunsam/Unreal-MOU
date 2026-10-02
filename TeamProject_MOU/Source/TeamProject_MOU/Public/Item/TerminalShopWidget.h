#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerminalShopWidget.generated.h"

class ATerminalShop;

/** 상점 배경 위에 Widget BP 콘텐츠를 표시한다. */
UCLASS()
class TEAMPROJECT_MOU_API UTerminalShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// [TSHOP-011] 키보드 입력을 받을 수 있도록 상점 위젯을 설정한다.
	UTerminalShopWidget(const FObjectInitializer& ObjectInitializer);

	// [TSHOP-005] UI를 연 상점 액터를 기록한다.
	void InitializeShop(ATerminalShop* InShop);

	// [TSHOP-006] 상점 UI에 입력 초점과 마우스 커서를 준다.
	void ActivateShopInput();

	// [TSHOP-007] 상점 UI를 닫고 게임 입력으로 돌린다.
	UFUNCTION(BlueprintCallable, Category="Terminal Shop")
	void CloseShop();

protected:
	// [TSHOP-008] Widget BP 콘텐츠를 기준 해상도에 맞춰 화면에 배치한다.
	virtual void NativeOnInitialized() override;

	// [TSHOP-009] Esc 입력을 받아 상점 UI를 닫는다.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<ATerminalShop> OwningShop;
};
