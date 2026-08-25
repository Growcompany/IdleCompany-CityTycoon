// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LevelScriptActor.h"
#include "CGLevelScriptBase.generated.h"

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ACGLevelScriptBase : public ALevelScriptActor
{
	GENERATED_BODY()
	
public:
	ACGLevelScriptBase();

	virtual void Tick(float DeltaSeconds) override;
	
protected:
	virtual void PostInitializeComponents() override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	virtual void BeginDestroy() override;

protected:
	virtual void Destroyed() override;
};