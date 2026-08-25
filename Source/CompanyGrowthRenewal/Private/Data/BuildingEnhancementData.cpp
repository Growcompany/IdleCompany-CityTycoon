// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/BuildingEnhancementData.h"
#include "Manager/TableManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Math/UnrealMathUtility.h"

namespace
{
	// BlueprintFunctionLibrary static 함수에서 GameInstance/Subsystem 접근 헬퍼.
	// PIE/Game world 우선 — Editor world 는 무시 (TableManager 미초기화 상태일 수 있음).
	UTableManagerSubsystem* GetTableMgr()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.WorldType != EWorldType::Game && Ctx.WorldType != EWorldType::PIE) continue;
			if (UWorld* World = Ctx.World())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					if (UTableManagerSubsystem* Mgr = GI->GetSubsystem<UTableManagerSubsystem>())
					{
						return Mgr;
					}
				}
			}
		}
		return nullptr;
	}
	bool TryGetDef(EBuildingEnhancementType Type, FBuildingEnhancementDefinition& Out)
	{
		if (UTableManagerSubsystem* Mgr = GetTableMgr())
		{
			return Mgr->GetEnhancementDefinition(Type, Out);
		}
		return false;
	}

	// 행 부재 스팸 방지 — KeystoneAuraPower 등 폴백 운용 타입
	static TSet<uint8> GLoggedMissingBaseCostTypes;
	static bool GLoggedMissingVaultK = false;

	constexpr float VaultBaseSeconds = 300.0f;
	constexpr float VaultMaxSeconds = 12.0f * 60.0f * 60.0f;
	constexpr float VaultFallbackK = 0.000625f;
}

int64 UBuildingEnhancementHelper::GetBaseCost(EBuildingEnhancementType EnhancementType)
{
	if (UTableManagerSubsystem* Mgr = GetTableMgr())
	{
		FBuildingEnhancementDefinition Def;
		if (Mgr->GetEnhancementDefinition(EnhancementType, Def))
		{
			return Def.BaseCost;
		}
	}
	const uint8 TypeKey = static_cast<uint8>(EnhancementType);
	if (!GLoggedMissingBaseCostTypes.Contains(TypeKey))
	{
		GLoggedMissingBaseCostTypes.Add(TypeKey);
		UE_LOG(LogTemp, Warning, TEXT("[Enhancement] BaseCost lookup 실패 (Type=%d) — DT_BuildingEnhancementDefinition 미초기화 또는 row 누락. 폴백 1000."),
			static_cast<int32>(EnhancementType));
	}
	return 1000;
}

float UBuildingEnhancementHelper::GetCostGrowthRate(EBuildingEnhancementType EnhancementType)
{
	if (UTableManagerSubsystem* Mgr = GetTableMgr())
	{
		FBuildingEnhancementDefinition Def;
		if (Mgr->GetEnhancementDefinition(EnhancementType, Def))
		{
			return Def.CostGrowthRate;
		}
	}
	return 1.08f;
}

float UBuildingEnhancementHelper::GetEffectPerLevel(EBuildingEnhancementType EnhancementType)
{
	if (UTableManagerSubsystem* Mgr = GetTableMgr())
	{
		FBuildingEnhancementDefinition Def;
		if (Mgr->GetEnhancementDefinition(EnhancementType, Def))
		{
			return Def.EffectPerLevel;
		}
	}
	return 0.0008f;
}

EResourceType UBuildingEnhancementHelper::GetCostResourceType(EBuildingEnhancementType EnhancementType)
{
	if (UTableManagerSubsystem* Mgr = GetTableMgr())
	{
		FBuildingEnhancementDefinition Def;
		if (Mgr->GetEnhancementDefinition(EnhancementType, Def))
		{
			return Def.CostResourceType;
		}
	}
	return EResourceType::Money;
}

namespace
{
	// double→int64 캐스트는 int64 범위 초과 시 UB(음수 비용) — 캐스트 전에 유한성/상한을 검사한다
	int64 SafeCostCast(double Cost)
	{
		if (!FMath::IsFinite(Cost) || Cost >= static_cast<double>(MAX_int64))
		{
			return MAX_int64;
		}
		return FMath::Clamp(static_cast<int64>(Cost), 0LL, MAX_int64);
	}
}

int64 UBuildingEnhancementHelper::CalculateUpgradeCost(EBuildingEnhancementType EnhancementType, int32 CurrentLevel, int32 FootprintCells)
{
	const int32 Level = FMath::Max(CurrentLevel, 0);

	FBuildingEnhancementDefinition Def;
	if (!TryGetDef(EnhancementType, Def))
	{
		// 나머지 노브는 USTRUCT 기본값(Power / Scale 11 / Exponent 3). GetBaseCost 가 폴백 경고를 남긴다.
		Def.BaseCost = GetBaseCost(EnhancementType);
	}

	// 증축은 층당 인원이 칸수 비례로 늘므로 비용도 같이 비례시킨다 — 안 그러면 큰 건물이 공짜로 4~9배 이득.
	const int64 CellMultiplier = Def.bScaleCostByFootprint ? FMath::Max(FootprintCells, 1) : 1;
	const double Base = static_cast<double>(Def.BaseCost) * static_cast<double>(CellMultiplier);
	double Cost = 0.0;
	if (Def.CostCurveType == ECostCurveType::Power)
	{
		// Scale 0 이면 0 나눗셈으로 Inf → SafeCostCast 가 MAX_int64 로 흡수하지만 곡선이 죽는다. 하한을 둔다.
		const double Scale = FMath::Max(static_cast<double>(Def.CostLevelScale), 0.01);
		Cost = Base * FMath::Pow(1.0 + static_cast<double>(Level) / Scale, static_cast<double>(Def.CostExponent));
	}
	else
	{
		Cost = Base * FMath::Pow(static_cast<double>(Def.CostGrowthRate), static_cast<double>(Level));
	}
	return SafeCostCast(Cost);
}

int64 UBuildingEnhancementHelper::CalculateBulkUpgradeCost(EBuildingEnhancementType EnhancementType, int32 CurrentLevel, int32 UpgradeCount, int32 FootprintCells)
{
	if (UpgradeCount <= 0)
	{
		return 0;
	}

	const int32 NormalizedCurrentLevel = FMath::Max(CurrentLevel, 0);
	int64 TotalCost = 0;
	for (int32 Offset = 0; Offset < UpgradeCount; ++Offset)
	{
		const int64 TargetLevel = static_cast<int64>(NormalizedCurrentLevel) + static_cast<int64>(Offset);
		if (TargetLevel > MAX_int32)
		{
			return MAX_int64;
		}

		const int64 LevelCost = CalculateUpgradeCost(EnhancementType, static_cast<int32>(TargetLevel), FootprintCells);
		if (LevelCost == MAX_int64 || TotalCost > MAX_int64 - LevelCost)
		{
			return MAX_int64;
		}
		TotalCost += LevelCost;
	}
	return TotalCost;
}

float UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType EnhancementType, int32 Level)
{
	if (Level <= 0)
	{
		return 1.0f;
	}

	const float EffectPerLevel = GetEffectPerLevel(EnhancementType);

	return 1.0f + (Level * EffectPerLevel);
}

float UBuildingEnhancementHelper::CalculateVaultSeconds(int32 Level)
{
	if (Level <= 0)
	{
		return VaultBaseSeconds;
	}

	// k = VaultCapacity 행 EffectPerLevel(곡선 형태 파라미터). 하드코딩 이원화 금지 — DT 단일 진실.
	float K = VaultFallbackK;
	bool bFound = false;
	if (UTableManagerSubsystem* Mgr = GetTableMgr())
	{
		FBuildingEnhancementDefinition Def;
		if (Mgr->GetEnhancementDefinition(EBuildingEnhancementType::VaultCapacity, Def))
		{
			K = Def.EffectPerLevel;
			bFound = true;
		}
	}
	if (!bFound && !GLoggedMissingVaultK)
	{
		GLoggedMissingVaultK = true;
		UE_LOG(LogTemp, Warning, TEXT("[Enhancement] VaultCapacity EffectPerLevel(k) lookup 실패 — DT 미초기화/row 누락. 폴백 0.000625."));
	}

	const float Curve = 1.0f - FMath::Exp(-static_cast<float>(Level) * K);
	return VaultBaseSeconds + (VaultMaxSeconds - VaultBaseSeconds) * Curve;
}

bool UBuildingEnhancementHelper::IsEnhancementVisibleForCompanyType(EEnhancementCategory Category, ECompanyType CompanyType)
{
	switch (Category)
	{
	case EEnhancementCategory::Common:  return true;
	case EEnhancementCategory::Project: return IsProjectType(CompanyType);
	default:                            return false;
	}
}
