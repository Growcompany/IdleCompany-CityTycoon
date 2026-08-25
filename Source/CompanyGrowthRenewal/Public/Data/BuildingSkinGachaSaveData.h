// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingSkinGachaSaveData.generated.h"

// 스킨 가챠 천장 카운터 (티켓 종류별 1개씩). 특성 FBuildingTraitPityData 와 동일 모델.
USTRUCT(BlueprintType)
struct FBuildingSkinPityData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 PullsSinceLastEpic = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 PullsSinceLastLegendary = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 TotalPulls = 0;

	void IncrementPull()
	{
		PullsSinceLastEpic++;
		PullsSinceLastLegendary++;
		TotalPulls++;
	}

	void OnEpicObtained() { PullsSinceLastEpic = 0; }
	void OnLegendaryObtained() { PullsSinceLastEpic = 0; PullsSinceLastLegendary = 0; }

	// 특성과 동일 천장 상수
	static constexpr int32 SoftPityStart = 40;
	static constexpr int32 HardPity = 60;
	static constexpr int32 GrandPity = 150;
	static constexpr float SoftPityBonusPerPull = 0.02f;
};

// 스킨 가챠 글로벌 데이터 (게임 전체 1개). 일반/고급 천장 독립, 마일리지 합산.
USTRUCT(BlueprintType)
struct FGachaSkinData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FBuildingSkinPityData NormalPity;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FBuildingSkinPityData AdvancedPity;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Mileage")
	int32 MileagePoints = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	int32 TotalPullsAllTime = 0;

	static constexpr int32 MileageEpicBox = 50;
	static constexpr int32 MileageLegendaryBox = 150;
};

// 스킨 가챠 인벤토리 (게임 전체 1개, FGameSaveData 에 포함).
// 소유 스킨 목록은 별도 저장소(FGameSaveData.OwnedBuildingSkins) — 여기엔 천장/마일리지만.
USTRUCT(BlueprintType)
struct FBuildingSkinGachaInventory
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FGachaSkinData GachaData;
};

// 스킨 가챠 결과 (런타임 전용, 세이브 X)
USTRUCT(BlueprintType)
struct FBuildingSkinGachaResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Result")
	int32 ResultSkinID = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Result")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 천장 보장으로 떴는지 (Epic 하드 천장 or Legendary 대천장)
	UPROPERTY(BlueprintReadWrite, Category = "Result")
	bool bWasPityGuaranteed = false;

	// 마일리지 상자 교환 결과인지
	UPROPERTY(BlueprintReadWrite, Category = "Result")
	bool bFromMileageBox = false;

	// 해당 등급 스킨을 모두 보유 중이라 중복으로 처리(마일리지 환산)된 결과인지.
	// true 면 ResultSkinID 는 표시용(미신규), MileageGained 만큼 마일리지 적립됨.
	UPROPERTY(BlueprintReadWrite, Category = "Result")
	bool bDuplicate = false;

	UPROPERTY(BlueprintReadWrite, Category = "Result")
	int32 MileageGained = 0;
};
