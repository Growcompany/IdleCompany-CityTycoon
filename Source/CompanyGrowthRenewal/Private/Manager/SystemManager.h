// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Manager/ManagerBase.h"
#include "SystemManager.generated.h"

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USystemManager : public UManagerBase
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	virtual void Prepare() override;
	virtual void Release() override;
	virtual void Destroy() override;

#pragma region Level Change
	virtual void OnLevelLoaded()
	{
	}

	virtual void OnLevelDestroy()
	{
	}
#pragma endregion

#pragma region SaveData
	/*virtual void OnSaveData(UOptionSaveData* SaveData)
	{
	}

	virtual void OnLoadData(UOptionSaveData* SaveData)
	{
	}*/
#pragma endregion SaveData
};