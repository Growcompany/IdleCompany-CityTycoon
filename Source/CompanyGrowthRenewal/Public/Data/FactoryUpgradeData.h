// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Enum/CostCurveType.h"
#include "Data/FactorySaveData.h"
#include "Data/FactoryUpgradeConfig.h"
#include "FactoryUpgradeData.generated.h"

class UTexture2D;

/**
 * BrickFactory 강화 슬롯 정의 (DataTable Row)
 * 한 행 = 하나의 EFactoryUpgradeType = 하나의 슬롯.
 * RowName 은 enum 식별자와 일치 권장 (예: "HoldProductionSpeed").
 *
 * 밸런스/표시명/아이콘 변경은 CSV reimport 만으로 완결되어야 함 (DT 단일 진실 원천).
 * 복잡한 구간별 값 계산(FFactoryUpgradeConfig::CalculateUpgradeValue)은 코드에 유지.
 */
USTRUCT(BlueprintType)
struct FFactoryUpgradeDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	EFactoryUpgradeType UpgradeType = EFactoryUpgradeType::HoldProductionSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	FText SubDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	FString ValueUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	bool bIsInteger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	EResourceType CostResourceType = EResourceType::Money;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Balance")
	int32 MaxLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	int64 BaseCost = 100;

	// 비용 곡선 형태. 강화 슬롯 4종은 전부 Power — 초반 가파름 → 후반 완만이라
	// 효과 곡선이 요구하는 수백~수천 레벨이 실제로 도달 가능해진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	ECostCurveType CostCurveType = ECostCurveType::Power;

	// Geometric 전용 — 다음 레벨 비용 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	float CostGrowthRate = 1.15f;

	// Power 전용 — 초반 기울기 폭. 작을수록 가파르다. 현행 생산속도 7.5 / 생산량 34 / 자동생산 5 / 용량 53.5
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	float CostLevelScale = 11.0f;

	// Power 전용 — 곡선 세기. 현행 전 슬롯 3.0 (세제곱)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	float CostExponent = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EarlyCostMultiplier = 1.0f;

	// 현재 레벨이 이 값에 도달하면 초기 비용 보정이 끝난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost", meta = (ClampMin = "1"))
	int32 FullCostFromLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Unlock", meta = (ClampMin = "0"))
	int32 RequiredHQLevel = 0;

	// 비용 계산은 전부 이 곡선 하나를 거친다 — 단건/벌크/UI 표시가 갈라지지 않게.
	FFactoryCostCurve GetCostCurve() const
	{
		FFactoryCostCurve Curve;
		Curve.BaseCost = BaseCost;
		Curve.CurveType = CostCurveType;
		Curve.GrowthRate = CostGrowthRate;
		Curve.LevelScale = CostLevelScale;
		Curve.Exponent = CostExponent;
		Curve.EarlyCostMultiplier = EarlyCostMultiplier;
		Curve.FullCostFromLevel = FullCostFromLevel;
		return Curve;
	}

	int64 CalculateUpgradeCost(int32 CurrentLevel) const
	{
		return GetCostCurve().CostAtLevel(CurrentLevel);
	}
};

namespace FactoryUpgradeUnlockPolicy
{
	inline bool IsAutoUnlockType(EFactoryUpgradeType UpgradeType)
	{
		// 인부 추가는 공장의 기본 성장축이라 처음부터 열려 있다 — 자동(오프라인) 묶음은 상한 하나뿐이다.
		return UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity;
	}

	inline bool TryResolveRequiredHQLevel(
		const TArray<FFactoryUpgradeDefinition>& Definitions,
		int32& OutRequiredHQLevel)
	{
		OutRequiredHQLevel = 0;

		const FFactoryUpgradeDefinition* CapacityDefinition = Definitions.FindByPredicate(
			[](const FFactoryUpgradeDefinition& Definition)
			{
				return Definition.UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity;
			});

		if (!CapacityDefinition || CapacityDefinition->RequiredHQLevel <= 0)
		{
			return false;
		}

		OutRequiredHQLevel = CapacityDefinition->RequiredHQLevel;
		return true;
	}
}
