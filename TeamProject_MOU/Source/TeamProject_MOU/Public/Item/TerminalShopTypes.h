#pragma once

#include "CoreMinimal.h"
#include "TerminalShopTypes.generated.h"

USTRUCT(BlueprintType)
struct FTerminalCartItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terminal Shop")
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terminal Shop")
	int32 Quantity = 1;
};
