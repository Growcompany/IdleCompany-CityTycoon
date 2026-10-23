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

	UpgradeData.Add(EFactoryUpgradeType::LaborerCount,
		FFactoryUpgradeData(1, FFactoryUpgradeConfig::GetInitialValue(EFactoryUpgradeType::LaborerCount)));

	UpgradeData.Add(EFactoryUpgradeType::AutoCollectionCapacity,
		FFactoryUpgradeData(1, FFactoryUpgradeConfig::GetInitialValue(EFactoryUpgradeType::AutoCollectionCapacity)));

	// 증축의 생산 배율은 런타임에 외형 테이블의 해당 레벨에서 계산한다.
	UpgradeData.Add(EFactoryUpgradeType::FactoryExpansion, FFactoryUpgradeData(1, 0.f));
}
