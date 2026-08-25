// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/CostCurveType.h"
// 아래 switch 들이 EFactoryUpgradeType 의 '열거자'를 쓴다 — 전방 선언만으로는 부족하다.
// (지금까지는 모든 includer 가 우연히 이 헤더를 먼저 끌어와 컴파일됐을 뿐이다.)
#include "Data/FactorySaveData.h"

/**
 * 비용 곡선 파라미터 묶음 (DT 행 · 폴백 공용)
 *
 * 호출부가 BaseCost/GrowthRate 를 낱개로 들고 다니면 곡선 노브가 늘 때마다 인자가 번식하고,
 * DT 경로와 폴백 경로가 서로 다른 값을 쓰는 사고가 난다. 정수화 정책도 여기 한 곳에만 둔다.
 */
struct FFactoryCostCurve
{
	int64 BaseCost = 100;
	ECostCurveType CurveType = ECostCurveType::Power;
	float GrowthRate = 1.15f;   // Geometric 전용
	float LevelScale = 11.0f;   // Power 전용 — 작을수록 초반이 가파르다
	float Exponent = 3.0f;      // Power 전용

	int64 CostAtLevel(int32 Level) const
	{
		const int32 NormalizedLevel = FMath::Max(Level, 0);
		double Raw = 0.0;
		if (CurveType == ECostCurveType::Power)
		{
			// Scale 0 은 0 나눗셈 → Inf. 하한을 둬야 오염된 DT 행이 곡선을 죽이지 않는다.
			const double Scale = FMath::Max(static_cast<double>(LevelScale), 0.01);
			Raw = static_cast<double>(BaseCost)
				* FMath::Pow(1.0 + static_cast<double>(NormalizedLevel) / Scale, static_cast<double>(Exponent));
		}
		else
		{
			Raw = static_cast<double>(BaseCost)
				* FMath::Pow(static_cast<double>(GrowthRate), static_cast<double>(NormalizedLevel));
		}

		// double→int64 캐스트는 범위 초과 시 UB(음수 비용) — 캐스트 전에 유한성/상한을 검사한다
		if (!FMath::IsFinite(Raw) || Raw >= static_cast<double>(MAX_int64))
		{
			return MAX_int64;
		}
		return FMath::Clamp(FMath::RoundToInt64(Raw), 0LL, MAX_int64);
	}

	// 벌크 = 레벨별 단건 비용의 정확한 합. Count=1 이면 단건과 정확히 일치해야 한다(표시=청구 계약).
	int64 BulkCost(int32 StartLevel, int32 Count) const
	{
		if (Count <= 0)
		{
			return 0;
		}

		const int32 NormalizedStart = FMath::Max(StartLevel, 0);
		int64 Total = 0;
		for (int32 Offset = 0; Offset < Count; ++Offset)
		{
			const int64 TargetLevel = static_cast<int64>(NormalizedStart) + static_cast<int64>(Offset);
			if (TargetLevel > MAX_int32)
			{
				return MAX_int64;
			}

			const int64 LevelCost = CostAtLevel(static_cast<int32>(TargetLevel));
			if (LevelCost == MAX_int64 || Total > MAX_int64 - LevelCost)
			{
				return MAX_int64;
			}
			Total += LevelCost;
		}
		return Total;
	}
};

/**
 * Factory 업그레이드 계산 로직 관리
 * 모든 업그레이드 값 계산과 비용 계산을 담당
 */
struct COMPANYGROWTHRENEWAL_API FFactoryUpgradeConfig
{
public:
	/**
	 * 특정 레벨에서의 업그레이드 값을 계산
	 * @param Type 업그레이드 타입
	 * @param Level 레벨 (1부터 시작)
	 * @return 해당 레벨의 값
	 */
	static float CalculateUpgradeValue(EFactoryUpgradeType Type, int32 Level)
	{
		switch (Type)
		{
		case EFactoryUpgradeType::HoldProductionSpeed:
		{
			// 일반 생산 속도: 레벨1=1초 → 레벨351+=0.2초(바닥/만렙)
			if (Level == 1)
			{
				return 1.0f;
			}
			else if (Level <= 51)
			{
				// 레벨 2~51: 1초 → 0.5초 (0.01초씩 감소, 50단계)
				return 1.0f - ((Level - 1) * 0.01f);
			}
			else if (Level <= 351)
			{
				// 레벨 52~351: 0.5초 → 0.2초 (0.001초씩 감소, 300단계)
				return 0.5f - ((Level - 51) * 0.001f);
			}
			else
			{
				// 레벨 352+ : 0.2초 바닥 (만렙값). 트레일러 라이브 튜닝(FactoryMax -1 0.2)으로 확정.
				// 2026-08-13: MaxLevel 을 곡선 수렴점인 351 로 내렸으므로 이 분기는 오염 레벨 방어용 클램프다.
				// (구 등비 곡선에선 도달 불가라 방치됐지만, 멱함수 전환 후엔 "돈만 먹고 효과 0"인 구간이 됐다)
				return 0.2f;
			}
		}

		case EFactoryUpgradeType::HoldProductionAmount:
			// 생산량: 레벨 = 생산 개수 (1레벨=1개, 10000레벨=10000개)
			return static_cast<float>(Level);

		case EFactoryUpgradeType::AutoCollection:
		{
			// 곡선 정의역은 레벨1부터 — 0 이하(미초기화)는 레벨1 값으로 클램프 (아래 91 분기가 10.1초로 계산하는 것 방어)
			if (Level <= 0)
			{
				return 10.0f;
			}

			// 자동 생산 속도: 레벨1=10초 → 레벨1000=0.001초
			if (Level == 1)
			{
				return 10.0f;
			}
			else if (Level <= 91)
			{
				// 레벨 1~91: 10초 → 1초 (0.1초씩 감소, 90단계)
				return 10.0f - ((Level - 1) * 0.1f);
			}
			else if (Level <= 541)
			{
				// 레벨 92~541: 1초 → 0.1초 (0.002초씩 감소, 450단계)
				return 1.0f - ((Level - 91) * 0.002f);
			}
			else if (Level <= 1000)
			{
				// 레벨 542~1000: 0.1초 → 0.001초 (약 0.000216초씩 감소, 459단계)
				return 0.1f - ((Level - 541) * (0.099f / 459.0f));
			}
			else
			{
				// 만렙: 0.001초
				return 0.001f;
			}
		}

		case EFactoryUpgradeType::AutoCollectionCapacity:
			// 자동 수집 용량: 레벨 = 용량 (1레벨=1개, 10000레벨=10000개)
			return static_cast<float>(Level);
		}

		return 0.0f;
	}

	// DT(FFactoryUpgradeDefinition) 미로드 시 폴백 곡선. 정상 경로는 DT 행의 GetCostCurve().
	// ⚠ 값은 DT_FactoryUpgradeDefinition 과 일치시킬 것 — 어긋나면 DT 로드 실패 시에만 다른 가격이 나와 재현이 어렵다.
	static FFactoryCostCurve GetFallbackCostCurve(EFactoryUpgradeType Type)
	{
		FFactoryCostCurve Curve;
		Curve.CurveType = ECostCurveType::Power;
		Curve.Exponent = 3.0f;

		// LevelScale 은 "유효 만렙(효과 곡선이 수렴하는 레벨) 도달 누적"을 앵커로 역산한 값이다.
		// 생산속도 Lv351=10억 · 생산량 Lv10000=10조 · 자동생산 Lv1000=1조 · 용량 Lv10000=5조.
		switch (Type)
		{
		case EFactoryUpgradeType::HoldProductionSpeed:
			Curve.BaseCost = 100;
			Curve.LevelScale = 7.5f;
			break;
		case EFactoryUpgradeType::HoldProductionAmount:
			Curve.BaseCost = 150;
			Curve.LevelScale = 34.0f;
			break;
		case EFactoryUpgradeType::AutoCollection:
			Curve.BaseCost = 500;
			Curve.LevelScale = 5.0f;
			break;
		case EFactoryUpgradeType::AutoCollectionCapacity:
			Curve.BaseCost = 300;
			Curve.LevelScale = 53.5f;
			break;
		default:
			break;
		}
		return Curve;
	}

	static int64 CalculateUpgradeCost(EFactoryUpgradeType Type, int32 CurrentLevel)
	{
		return GetFallbackCostCurve(Type).CostAtLevel(CurrentLevel);
	}

	/**
	 * 초기 레벨 값 반환 (레벨 1의 값)
	 */
	static float GetInitialValue(EFactoryUpgradeType Type)
	{
		return CalculateUpgradeValue(Type, 1);
	}

	// DT(FFactoryUpgradeDefinition) 미로드 시 폴백. 정상 경로는 DT.MaxLevel.
	static int32 GetMaxLevel(EFactoryUpgradeType Type)
	{
		switch (Type)
		{
		case EFactoryUpgradeType::HoldProductionSpeed:
			return 351;  // 효과 곡선이 0.2초로 수렴하는 지점 = 상한
		case EFactoryUpgradeType::HoldProductionAmount:
			return 10000;  // 10000개까지
		case EFactoryUpgradeType::AutoCollection:
			return 1000;  // 0.001초까지
		case EFactoryUpgradeType::AutoCollectionCapacity:
			return 10000;  // 10000개까지
		}
		return 1;
	}
};
