// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/LoadingGameMode.h"

#include "UI/Panel/LoadingWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ALoadingGameMode::ALoadingGameMode()
{
	// 순수 UI 부팅 맵 — HUD 불필요. 기본 PlayerController/Pawn 은 위젯 owner 로만 사용.
	HUDClass = nullptr;
}

void ALoadingGameMode::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingGameMode] TableManager 없음 — 로딩 위젯 생성 실패"));
		return;
	}

	TSubclassOf<UUserWidget> LoadingClass = TableMgr->GetWidgetClass(EWidgetType::Loading);
	if (!LoadingClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingGameMode] EWidgetType::Loading 클래스 없음 — DT_WidgetClass 행 확인 필요"));
		return;
	}

	UWorld* LoadWorld = GetWorld();
	APlayerController* PC = LoadWorld ? LoadWorld->GetFirstPlayerController() : nullptr;
	LoadingWidgetInstance = CreateWidget<ULoadingWidget>(PC, LoadingClass);
	if (LoadingWidgetInstance)
	{
		LoadingWidgetInstance->AddToViewport(1000);
		UE_LOG(LogTemp, Log, TEXT("[LoadingGameMode] LoadingWidget 표시"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingGameMode] CreateWidget<ULoadingWidget> 실패 — UI_LoadingWidget 부모 클래스가 ULoadingWidget 인지 확인"));
	}
}
