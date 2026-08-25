// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/BuildingTraitCategory.h"
#include "BuildingTraitSetBonusTable.generated.h"

// 건물 특성 세트 보너스 DataTable Row
// Row Name 예시: "Revenue_2set", "RnD_3set"
// 같은 Category 의 특성이 한 건물에 RequiredCount 개 장착되면 BonusValue 만큼 추가 효과
// CSV 경로: DataImport/DT_BuildingTraitSetBonus_Import.csv (34행 = 17분야 x 2/3세트)
// ⚠ 설명 텍스트 컬럼은 두지 않는다 — 표시 문구는 CalculateSetBonuses 가 FormatTraitEffectText 로 생성한다.
// Manufacturing/Project 카테고리는 세트 보너스 없음 (본 테이블에 미포함)
USTRUCT(BlueprintType)
struct FBuildingTraitSetBonus : public FTableRowBase
{
	GENERATED_BODY()

	// 세트 카테고리 (어느 분야 세트 보너스인지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EBuildingTraitCategory Category = EBuildingTraitCategory::Revenue;

	// 발동 필요 개수 (2 = 2세트 보너스, 3 = 3세트 보너스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 RequiredCount = 2;

	// 보너스 수치. 항상 양수 — 증가/감소 의미는 Category 의 대상이 소유한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float BonusValue = 0.0f;

	// UI 정렬 순서
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	int32 SortOrder = 0;
};
