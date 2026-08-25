// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/MainMapHUD.h"
#include "Manager/UIManagerSubsystem.h"

void AMainMapHUD::BeginPlay()
{
    Super::BeginPlay();

    if (UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->CreateInGameLayerWidget();
        UE_LOG(LogTemp, Log, TEXT("[MainMapHUD] CreateInGameLayerWidget called"));
    }
}
