// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/WorldMapPlayerController.h"
#include "Core/CGGameInstance.h"
#include "Player/CGCheatManager.h"

AWorldMapPlayerController::AWorldMapPlayerController()
	: Super()
{
	CheatClass = UCGCheatManager::StaticClass();
}

void AWorldMapPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 패닝 가능한 상태로 시작
	GoToNormalMode();
}

void AWorldMapPlayerController::ReturnToMainMap()
{
	UCGGameInstance* GameInstance = UCGGameInstance::GetInstance();
	if (GameInstance)
	{
		GameInstance->TransitionToLevel(TEXT("MainMap_TheRiverwalkCity"));
	}
}
