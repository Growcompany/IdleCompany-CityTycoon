// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "CGGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ACGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	virtual void StartPlay() override;

public:
	ACGGameModeBase();

};
