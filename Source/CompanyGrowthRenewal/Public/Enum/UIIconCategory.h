// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UIIconCategory.generated.h"

// UI 아이콘 카테고리
UENUM(BlueprintType)
enum class EUIIconCategory : uint8
{
	None            UMETA(DisplayName = "None"),
	Department      UMETA(DisplayName = "부서"),
	Status          UMETA(DisplayName = "상태"),
	Buff            UMETA(DisplayName = "버프"),
	Debuff          UMETA(DisplayName = "디버프"),
	BuildingType    UMETA(DisplayName = "건물 유형"),
	Resource        UMETA(DisplayName = "재화"),
	Skill           UMETA(DisplayName = "스킬"),
	Achievement     UMETA(DisplayName = "업적"),
	Misc            UMETA(DisplayName = "기타")
};
