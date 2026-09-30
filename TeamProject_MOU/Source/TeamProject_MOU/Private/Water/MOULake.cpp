// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/MOULake.h"
#include "Water/MOUWaterBodyLakeComponent.h"
#include "Water/MOUWaterHazardComponent.h"

AMOULake::AMOULake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterBodyLakeComponentClass = UMOUWaterBodyLakeComponent::StaticClass();

	WaterHazardComponent = CreateDefaultSubobject<UMOUWaterHazardComponent>(TEXT("WaterHazardComponent"));
	WaterHazardComponent->SetAlwaysActive(true);
}

void AMOULake::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (WaterHazardComponent)
	{
		WaterHazardComponent->HandleActorEntered(OtherActor);
	}
}

void AMOULake::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (WaterHazardComponent)
	{
		WaterHazardComponent->HandleActorExited(OtherActor);
	}
}
