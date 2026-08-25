// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LootBoxCategory.generated.h"

/**
 * 룩박스 카테고리
 * - 각 카테고리마다 별도의 인벤토리와 보상 테이블 사용
 */
UENUM(BlueprintType)
enum class ELootBoxCategory : uint8
{
	BuildingSkin	UMETA(DisplayName = "Building Skin"),
	BuildingItem	UMETA(DisplayName = "Building Item"),
	// 향후 확장 가능: CharacterSkin, Accessory 등
};
