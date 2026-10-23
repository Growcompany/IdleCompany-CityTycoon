// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/LegacyWorldMapPlayerController.h"
#include "Core/CGGameInstance.h"
#include "Player/CGCheatManager.h"

ALegacyWorldMapPlayerController::ALegacyWorldMapPlayerController()
	: Super()
{
	CheatClass = UCGCheatManager::StaticClass();
}

void ALegacyWorldMapPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 패닝 가능한 상태로 시작
	GoToNormalMode();
}

void ALegacyWorldMapPlayerController::ReturnToMainMap()
{
	UCGGameInstance* GameInstance = UCGGameInstance::GetInstance();
	if (GameInstance)
	{
		GameInstance->TransitionToLevel(TEXT("MainMap_City"));
	}
}
