// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EmployeeTypes.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/GachaTier.h"
#include "GachaRecruitmentData.generated.h"

/**
 * 천장(Pity) 카운터
 * - 고급/프리미엄 티어별 독립 관리
 * - 보장 등급(하드피티): 고급/프리미엄 모두 Legendary (2026-06-10 정합)
 * - Soft Pity: 40회부터 매 회 +2% 보장등급 확률
 * - Hard Pity: 60회째 보장등급 확정
 */
USTRUCT(BlueprintType)
struct FGachaPityData
{
	GENERATED_BODY()

	// 마지막 보장등급 획득 이후 뽑기 횟수
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 PullsSinceLastGuaranteed = 0;

	// 해당 티어 총 뽑기 횟수
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 TotalPulls = 0;

	void IncrementPull()
	{
		PullsSinceLastGuaranteed++;
		TotalPulls++;
	}

	void ResetPity()
	{
		PullsSinceLastGuaranteed = 0;
	}

	// Soft Pity 시작 회차
	static constexpr int32 SoftPityStart = 40;
	// Hard Pity 확정 회차
	static constexpr int32 HardPity = 60;
	// Soft Pity 구간에서 매 회 추가되는 보장등급 확률
	static constexpr float SoftPityBonusPerPull = 0.02f;
};

/**
 * 마일리지 데이터 (프리미엄 뽑기 전용)
 * - 프리미엄 1회 뽑기 = 1pt 적립
 * - 200pt = Legendary 선택권 교환
 */
USTRUCT(BlueprintType)
struct FGachaMileageData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Mileage")
	int32 CurrentPoints = 0;

	// 교환 비용 (200pt)
	static constexpr int32 ExchangeCost = 200;
};

/**
 * 글로벌 가챠 데이터 (게임 전체 1개, 세이브 대상)
 * - 고급/프리미엄 각각 독립 Pity 카운터
 * - 프리미엄 전용 마일리지
 */
USTRUCT(BlueprintType)
struct FGachaRecruitmentData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FGachaPityData AdvancedPity;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FGachaPityData PremiumPity;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FGachaMileageData Mileage;
};

/**
 * UI 표시용 등급별 확률 1행 (Blueprint 친화 — TPair 대체)
 */
USTRUCT(BlueprintType)
struct FGachaRarityChance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gacha|Probability")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 0~100 (퍼센트)
	UPROPERTY(BlueprintReadOnly, Category = "Gacha|Probability")
	float Percent = 0.f;
};

/**
 * 뽑기 결과 (런타임 전용)
 * - 레벨 전환 시 GameInstance에 저장하여 연출 맵에서 표시
 * - 세이브 대상 아님
 */
USTRUCT(BlueprintType)
struct FGachaResultData
{
	GENERATED_BODY()

	// 생성된 직원 인스턴스
	UPROPERTY(BlueprintReadWrite, Category = "Gacha|Result")
	FEmployeeInstance ResultEmployee;

	// 잠재능력 등급 (가챠의 핵심 가치)
	UPROPERTY(BlueprintReadWrite, Category = "Gacha|Result")
	ELootBoxRarity PotentialRarity = ELootBoxRarity::Common;

	// 사용한 뽑기 티어
	UPROPERTY(BlueprintReadWrite, Category = "Gacha|Result")
	EGachaTier UsedTier = EGachaTier::Normal;

	// 천장에 의한 확정 여부
	UPROPERTY(BlueprintReadWrite, Category = "Gacha|Result")
	bool bWasPityGuaranteed = false;

	// 배치할 건물 인덱스
	UPROPERTY(BlueprintReadWrite, Category = "Gacha|Result")
	int32 TargetBuildingIndex = INDEX_NONE;
};
