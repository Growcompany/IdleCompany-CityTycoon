// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OfficeExpansionType.generated.h"

/**
 * 오피스 확장 방향
 * - Left: 왼쪽 방향으로 확장 (X축)
 * - Right: 오른쪽 방향으로 확장 (Y축)
 */
UENUM(BlueprintType)
enum class EOfficeExpandDirection : uint8
{
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right")
};
