// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/TrafficRoute.h"

#include "Components/SplineComponent.h"

ATrafficRoute::ATrafficRoute()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Path = CreateDefaultSubobject<USplineComponent>(TEXT("Path"));
	Path->SetupAttachment(Root);
}
