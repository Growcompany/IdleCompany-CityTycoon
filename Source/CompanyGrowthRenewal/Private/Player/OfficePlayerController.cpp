// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/OfficePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Player/CGCheatManager.h"

AOfficePlayerController::AOfficePlayerController()
{
	// 기본 설정
	bShowMouseCursor = true; // 오피스에서는 기본적으로 마우스 커서 표시
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	CheatClass = UCGCheatManager::StaticClass();
}

void AOfficePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 시작 시 GameAndUI 모드로 설정 (카메라 이동 + UI 클릭 모두 가능)
	SetGameMode();

	UE_LOG(LogTemp, Log, TEXT("[OfficePlayerController] BeginPlay - GameAndUI mode activated"));
}

// ========== Input Mode Management ==========

void AOfficePlayerController::SetUIMode()
{
	bIsUIMode = true;

	// 마우스 커서 표시
	bShowMouseCursor = true;

	// UI 전용 입력 모드
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);

	UE_LOG(LogTemp, Log, TEXT("[OfficePlayerController] Switched to UI Mode"));
}

void AOfficePlayerController::SetGameMode()
{
	bIsUIMode = false;

	// 오피스에서는 항상 마우스 커서 표시 (UI 클릭 + 카메라 이동 동시 필요)
	bShowMouseCursor = true;

	// 게임 + UI 입력 모드 (카메라 이동 가능)
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false); // 카메라 이동 중에도 커서 표시
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	UE_LOG(LogTemp, Log, TEXT("[OfficePlayerController] Switched to GameAndUI Mode"));
}
