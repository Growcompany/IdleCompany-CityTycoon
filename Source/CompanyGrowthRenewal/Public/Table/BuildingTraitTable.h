// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/BuildingTraitCategory.h"
#include "Enum/BuildingTraitRequirement.h"
#include "BuildingTraitTable.generated.h"

// 건물 특성 DataTable Row 구조체
// Row Name = TraitID (예: "Revenue_BadgeHolder", "RnD_Nobel", "Mfg_SmartCore")
// CSV 경로: DataImport/DT_BuildingTrait_Import.csv (95행)
// ⚠ 효과 설명 텍스트 컬럼은 두지 않는다 — 코드가 읽지 않는 자유 텍스트를 UI 가 그대로 출력하던 구조가
//   허위 문구의 원인이었다. 표시 문구는 FormatTraitEffectText(대상, 수치) 가 생성한다.
USTRUCT(BlueprintType)
struct FBuildingTraitTableRow : public FTableRowBase
{
	GENERATED_BODY()

	// 특성 고유 ID (Row Name 과 동일, FName 키)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName TraitID = NAME_None;

	// UI 표시 이름 (한국어)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	// 등급 (Common~Mythic, ELootBoxRarity 재사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 분야 (19개). 효과 대상은 GetTargetForCategory 로 파생되므로 별도 컬럼이 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EBuildingTraitCategory Category = EBuildingTraitCategory::Revenue;

	// 장착 제한 (None / Manufacturing / Project)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EBuildingTraitRequirement RequiredType = EBuildingTraitRequirement::None;

	// 효과 수치. 항상 양수 — 증가/감소 의미는 대상이 소유한다(GetTraitTargetPolarity). 특성 1개 = 효과 1개.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float BaseEffect = 0.0f;

	// 아이콘 (TSoftObjectPtr - 프로젝트 표준 패턴)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	// UI 정렬 순서 (분야별 0/10/20.../100/110... 패턴)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	int32 SortOrder = 0;
};
