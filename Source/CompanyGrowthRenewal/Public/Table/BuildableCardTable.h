// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Enum/InteractableType.h"
#include "Enum/LootBoxRarity.h"
#include "Table/ConstructionCost.h"
#include "BuildableCardTable.generated.h"

/**
 * 건물 카드 데이터 테이블 (건물 전용)
 */
USTRUCT(BlueprintType)
struct FBuildableCardTable : public FTableRowBase
{
	GENERATED_BODY()

	// 데이터 테이블의 Row Name을 저장 (런타임에 할당됨)
	UPROPERTY(BlueprintReadOnly)
	FName RowName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> UIIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	EInteractableType InteractableType;

	// 건물 등급 — 카드 색 표시 전용(오피스 프리셋/슬롯/해금과 무관, footprint 칸수가 그 역할).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 건설 비용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction")
	TArray<FConstructionCost> ConstructionCosts;

	FString ToString() const
	{
		FString Out = "";
		Out += Name.ToString() + ", ";
		Out += EnumToString(InteractableType) + ", ";
		// Out += "Rarity: " + EnumToString(Rarity) + ", "; // TODO: ELootBoxRarity용 EnumToString 필요시 추가
		for (const FConstructionCost& Cost : ConstructionCosts)
		{
			Out += EnumToString(Cost.ResourceType) + ": " + FString::FromInt(Cost.Cost) + ", ";
		}
		return Out;
	}
};
