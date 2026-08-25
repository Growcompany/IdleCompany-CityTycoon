// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VFXType.generated.h"

/**
 * VFX 타입 열거형
 * 게임 내 다양한 이펙트를 식별하는데 사용
 */
UENUM(BlueprintType)
enum class EVFXType : uint8
{
	None = 0			UMETA(DisplayName = "없음"),
	LevelUp				UMETA(DisplayName = "레벨업"),
	DevScoreHit			UMETA(DisplayName = "개발 타격/루트"),
	DevCrit				UMETA(DisplayName = "개발 크리티컬"),
	DevFever			UMETA(DisplayName = "개발 피버/레전드"),
	CityDrip			UMETA(DisplayName = "인수 코인 드립"),
	CityDemolishDust	UMETA(DisplayName = "철거 먼지"),
};
