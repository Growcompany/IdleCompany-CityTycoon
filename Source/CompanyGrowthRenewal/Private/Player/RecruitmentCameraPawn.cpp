// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/RecruitmentCameraPawn.h"
#include "Camera/CameraComponent.h"

ARecruitmentCameraPawn::ARecruitmentCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootScene);
}

void ARecruitmentCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// 카메라 앵글 적용 (양수 Pitch = 위를 올려봄)
	Camera->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));
	Camera->SetFieldOfView(CameraFOV);
}
