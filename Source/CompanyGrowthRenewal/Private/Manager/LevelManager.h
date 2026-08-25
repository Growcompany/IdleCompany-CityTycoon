// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Manager/ManagerBase.h"
#include "LevelManager.generated.h"

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULevelManager : public UManagerBase
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	virtual void Prepare() override;
	virtual void Release() override;
	virtual void Destroy() override;

#pragma region SaveData
	/*virtual void OnSaveData(UInGameSaveData* SaveData)
	{
	}

	virtual void OnLoadData(UInGameSaveData* SaveData)
	{
	}*/
#pragma endregion SaveData
};
