// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/FactorySaveData.h"
#include "Data/FactoryUpgradeConfig.h"

void FFactorySaveData::InitializeUpgradeData()
{
	// 기본값 초기화 (레벨 1부터 시작)
	UpgradeData.Add(EFactoryUpgradeType::HoldProductionSpeed,
		FFactoryUpgradeData(1, FFactoryUpgradeConfig::GetInitialValue(EFactoryUpgradeType::HoldProductionSpeed)));

	UpgradeData.Add(EFactoryUpgradeType::HoldProductionAmount,
		FFactoryUpgradeData(1, FFactoryUpgradeConfig::GetInitialValue(EFactoryUpgradeType::HoldProductionAmount)));

	UpgradeData.Add(EFactoryUpgradeType::AutoCollection,
		FFactoryUpgradeData(1, FFactoryUpgradeConfig::GetInitialValue(EFactoryUpgradeType::AutoCollection)));

	UpgradeData.Add(EFactoryUpgradeType::AutoCollectionCapacity,
		FFactoryUpgradeData(1, FFactoryUpgradeConfig::GetInitialValue(EFactoryUpgradeType::AutoCollectionCapacity)));
}
