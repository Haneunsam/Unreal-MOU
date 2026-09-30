// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/MOURiver.h"
#include "Water/MOUWaterBodyRiverComponent.h"
#include "Water/MOUWaterHazardComponent.h"

AMOURiver::AMOURiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterBodyRiverComponentClass = UMOUWaterBodyRiverComponent::StaticClass();

	WaterHazardComponent = CreateDefaultSubobject<UMOUWaterHazardComponent>(TEXT("WaterHazardComponent"));
	WaterHazardComponent->SetAlwaysActive(false);
	WaterHazardComponent->SetDeactivationZThreshold(-50.0f);
}

void AMOURiver::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (WaterHazardComponent)
	{
		WaterHazardComponent->HandleActorEntered(OtherActor);
	}
}

void AMOURiver::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (WaterHazardComponent)
	{
		WaterHazardComponent->HandleActorExited(OtherActor);
	}
}
