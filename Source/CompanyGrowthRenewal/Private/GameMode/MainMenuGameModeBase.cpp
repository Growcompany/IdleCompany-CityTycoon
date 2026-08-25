// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/MainMenuGameModeBase.h"
#include "AsyncLoadingScreenLibrary.h"

void AMainMenuGameModeBase::StartPlay()
{
	Super::StartPlay();

	// 로딩 화면 종료
	UAsyncLoadingScreenLibrary::StopLoadingScreen();
}
