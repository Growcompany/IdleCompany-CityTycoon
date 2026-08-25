// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Level/CGLevelScriptBase.h"
#include "InGameLevel.generated.h"

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AInGameLevel : public ACGLevelScriptBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;
};
