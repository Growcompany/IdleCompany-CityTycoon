// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/LootBoxCategory.h"
#include "Enum/LootBoxRarity.h"
#include "LootBoxInventoryData.generated.h"

/**
 * 카테고리별 룩박스 인벤토리
 * - 특정 카테고리(BuildingSkin, BuildingItem 등)의 룩박스 보유 현황
 */
USTRUCT(BlueprintType)
struct FLootBoxCategoryInventory
{
	GENERATED_BODY()

	// 보유 중인 룩박스 목록 (LootBoxID -> Count)
	// 예: {"Square_Common": 5, "Sphere_Epic": 2}
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "LootBox")
	TMap<FName, int32> OwnedLootBoxes;

	// 획득한 아이템은 FGameSaveData의 카테고리별 배열에 저장됨
	// - BuildingSkin → OwnedBuildingSkins
	// - BuildingItem → OwnedBuildingItems (향후 구현)

	FLootBoxCategoryInventory()
	{
	}

	// 룩박스 추가
	void AddLootBox(FName LootBoxID, int32 Count = 1)
	{
		if (OwnedLootBoxes.Contains(LootBoxID))
		{
			OwnedLootBoxes[LootBoxID] += Count;
		}
		else
		{
			OwnedLootBoxes.Add(LootBoxID, Count);
		}
	}

	// 룩박스 사용 (개수 감소)
	bool UseLootBox(FName LootBoxID)
	{
		if (OwnedLootBoxes.Contains(LootBoxID) && OwnedLootBoxes[LootBoxID] > 0)
		{
			OwnedLootBoxes[LootBoxID]--;
			if (OwnedLootBoxes[LootBoxID] <= 0)
			{
				OwnedLootBoxes.Remove(LootBoxID);
			}
			return true;
		}
		return false;
	}

	// 룩박스 보유 개수 확인
	int32 GetLootBoxCount(FName LootBoxID) const
	{
		return OwnedLootBoxes.Contains(LootBoxID) ? OwnedLootBoxes[LootBoxID] : 0;
	}
};

/**
 * 전체 룩박스 인벤토리 데이터
 * - 모든 카테고리의 룩박스 보유 현황
 */
USTRUCT(BlueprintType)
struct FLootBoxInventoryData
{
	GENERATED_BODY()

	// 카테고리별 룩박스 인벤토리
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "LootBox")
	TMap<ELootBoxCategory, FLootBoxCategoryInventory> CategoryInventories;

	FLootBoxInventoryData()
	{
	}

	// 특정 카테고리의 인벤토리 가져오기 (없으면 생성)
	FLootBoxCategoryInventory& GetOrCreateCategoryInventory(ELootBoxCategory Category)
	{
		if (!CategoryInventories.Contains(Category))
		{
			CategoryInventories.Add(Category, FLootBoxCategoryInventory());
		}
		return CategoryInventories[Category];
	}

	// 특정 카테고리에 룩박스 추가
	void AddLootBox(ELootBoxCategory Category, FName LootBoxID, int32 Count = 1)
	{
		FLootBoxCategoryInventory& Inventory = GetOrCreateCategoryInventory(Category);
		Inventory.AddLootBox(LootBoxID, Count);
	}

	// 특정 카테고리의 룩박스 사용
	bool UseLootBox(ELootBoxCategory Category, FName LootBoxID)
	{
		if (CategoryInventories.Contains(Category))
		{
			return CategoryInventories[Category].UseLootBox(LootBoxID);
		}
		return false;
	}

	// 특정 카테고리의 룩박스 개수 확인
	int32 GetLootBoxCount(ELootBoxCategory Category, FName LootBoxID) const
	{
		if (CategoryInventories.Contains(Category))
		{
			return CategoryInventories[Category].GetLootBoxCount(LootBoxID);
		}
		return 0;
	}
};
