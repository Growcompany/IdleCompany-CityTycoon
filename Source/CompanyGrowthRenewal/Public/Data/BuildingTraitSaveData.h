// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingTraitSaveData.generated.h"

// 건물 한 채의 특성 슬롯 (FOfficeSaveData 에 포함)
// TraitID = FBuildingTraitTableRow.TraitID (FName)
// NAME_None = 빈 슬롯. 용량 3칸 고정, 무료 해금 수 = min(footprint 칸수, MaxFreeTraitSlots).
USTRUCT(BlueprintType)
struct FBuildingTraitSlotData
{
	GENERATED_BODY()

	// 슬롯 용량 (모든 빌딩 공통). 세트 보너스가 2/3세트까지만 존재해 4칸 이상은 세트에 기여하지 못한다.
	static constexpr int32 MaxBuildingTraitSlots = 3;

	// 무료 해금 상한 — 마지막 칸을 건물 크기와 무관하게 유료로 남겨 "3번째 칸은 어느 건물이든 같은 값" 규칙을 세운다.
	static constexpr int32 MaxFreeTraitSlots = 2;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Trait|Slot")
	TArray<FName> EquippedTraits;

	// 다이아로 영구 해금한 슬롯 비트마스크 (bit i set = 슬롯 i 구매됨). 무료 슬롯과 별개로 누적.
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Trait|Slot")
	int32 DiamondUnlockedSlotMask = 0;

	FBuildingTraitSlotData()
	{
		EquippedTraits.Init(NAME_None, MaxBuildingTraitSlots);
	}

	// 빈 슬롯 인덱스 반환 (-1이면 가득 참)
	int32 GetEmptySlotIndex() const
	{
		for (int32 i = 0; i < EquippedTraits.Num(); ++i)
		{
			if (EquippedTraits[i].IsNone()) return i;
		}
		return INDEX_NONE;
	}

	bool IsSlotEmpty(int32 SlotIndex) const
	{
		return EquippedTraits.IsValidIndex(SlotIndex) && EquippedTraits[SlotIndex].IsNone();
	}
};

// 가챠 천장 카운터 (티켓 종류별 1개씩 보유)
// 채용 시스템 FGachaPityData 를 본 시스템 맞춤으로 재정의 (대천장 150회 추가)
USTRUCT(BlueprintType)
struct FBuildingTraitPityData
{
	GENERATED_BODY()

	// 마지막 Epic 이상 획득 이후 뽑기 횟수 (하드 천장 60 용)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 PullsSinceLastEpic = 0;

	// 마지막 Legendary 획득 이후 뽑기 횟수 (대천장 150 용)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Pity")
	int32 PullsSinceLastLegendary = 0;

	// 해당 티켓 총 뽑기 횟수 (통계용)
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

	// 천장 상수 (BUILDING_TRAIT_SYSTEM §9.5)
	static constexpr int32 SoftPityStart = 40;
	static constexpr int32 HardPity = 60;
	static constexpr int32 GrandPity = 150;
	static constexpr float SoftPityBonusPerPull = 0.02f;
};

// 가챠 글로벌 데이터 (게임 전체 1개)
// 일반/고급 티켓 천장 독립, 마일리지는 합산
USTRUCT(BlueprintType)
struct FGachaTraitData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FBuildingTraitPityData NormalPity;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FBuildingTraitPityData AdvancedPity;

	// 합산 마일리지 (50pt = Epic 선택 상자, 150pt = Legendary 선택 상자)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha|Mileage")
	int32 MileagePoints = 0;

	// 도감/통계용 전체 뽑기 횟수
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	int32 TotalPullsAllTime = 0;

	static constexpr int32 MileageEpicBox = 50;
	static constexpr int32 MileageLegendaryBox = 150;
};

// 글로벌 특성 인벤토리 (게임 전체 1개, FGameSaveData 에 포함)
USTRUCT(BlueprintType)
struct FBuildingTraitInventory
{
	GENERATED_BODY()

	// 보유 특성 (FName TraitID → 보유 수량). 장착 시 -1, 분해/합성 재료 사용 시 -1.
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Inventory")
	TMap<FName, int32> OwnedTraits;

	// 도감 (한 번이라도 보유했던 특성 ID 집합)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Encyclopedia")
	TSet<FName> EncyclopediaUnlocked;

	// 도감 마일스톤 보상 수령 여부 (10/25/50/75/100 = 5개 비트 플래그)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Encyclopedia")
	int32 EncyclopediaClaimedMask = 0;

	// 분해 포인트 (§4.2 dust)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Dismantle")
	int32 DismantleDust = 0;

	// 가챠 데이터 (천장/마일리지)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Gacha")
	FGachaTraitData GachaData;
};

// 가챠 결과 (런타임 전용, 세이브 X)
USTRUCT(BlueprintType)
struct FBuildingTraitGachaResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Result")
	FName ResultTraitID = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "Result")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 천장 보장으로 떴는지 (Epic 하드 천장 or Legendary 대천장)
	UPROPERTY(BlueprintReadWrite, Category = "Result")
	bool bWasPityGuaranteed = false;

	// 마일리지 상자 교환 결과인지
	UPROPERTY(BlueprintReadWrite, Category = "Result")
	bool bFromMileageBox = false;
};

// 분해 상점 교환 항목 enum (UI 빌드 + 매니저 분기 키)
UENUM(BlueprintType)
enum class EBuildingTraitDustExchange : uint8
{
	NormalTicket            UMETA(DisplayName = "일반 뽑기권 (30 dust)"),
	AdvancedTicket          UMETA(DisplayName = "고급 뽑기권 (150 dust, 7장/주)"),
	CommonChoice            UMETA(DisplayName = "Common 지정 (10 dust)"),
	UnusualChoice           UMETA(DisplayName = "Unusual 지정 (40 dust, 5/주)"),
	RareChoice              UMETA(DisplayName = "Rare 지정 (200 dust, 1/주)"),
	EpicBox                 UMETA(DisplayName = "Epic 선택 상자 (1500 dust, 시즌 1회)"),
	LegendaryBox            UMETA(DisplayName = "Legendary 선택 상자 (8000 dust, 시즌 1회)")
};
