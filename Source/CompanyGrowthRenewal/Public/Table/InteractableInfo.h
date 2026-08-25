// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/InteractableType.h"
#include "Enum/ResourceType.h"
#include "Enum/BuildingType.h"
#include "InteractableInfo.generated.h"

USTRUCT(BlueprintType)
struct FInteractableInfo : public FTableRowBase
{
	GENERATED_BODY()

	// 데이터 테이블의 Row Name을 저장 (런타임에 할당됨)
	UPROPERTY(BlueprintReadOnly, Category = "Generic")
	FName RowName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generic")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generic")
	EInteractableType InteractableType; // for filtering

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Properties")
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Properties")
	float ResourceAmount = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Properties")
	float CollectionTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Properties")
	int CollectionValue = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop Properties")
	float GrowthTime = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop Properties")
	int HarvestAmount = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Properties")
	EBuildingType BuildingType = EBuildingType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Info")
	float BoundGap = 0.0f;
};
