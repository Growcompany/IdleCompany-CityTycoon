// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/ResourceType.h"
#include "ConstructionCost.generated.h"

/**
 * 건설/고용 비용 구조체
 */
USTRUCT(BlueprintType)
struct FConstructionCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EResourceType ResourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 Cost;
};
