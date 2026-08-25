// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Enum/UIIconCategory.h"
#include "UIIconData.generated.h"

/**
 * 범용 UI 아이콘 정보 DataTable Row 구조체
 *
 * Row Name 명명 규칙:
 * - "Department_Development" (부서 - 개발)
 * - "Department_Sales"       (부서 - 영업)
 * - "Status_Sick"            (상태 - 병가)
 * - "Buff_Leadership"        (버프 - 리더십)
 * - "BuildingType_Office"    (건물 - 사무실)
 */
USTRUCT(BlueprintType)
struct FUIIconData : public FTableRowBase
{
	GENERATED_BODY()

	// 아이콘 카테고리 (필터링/검색용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	EUIIconCategory Category = EUIIconCategory::None;

	// 표시 이름 (현지화 지원)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FText DisplayName;

	// 설명 (툴팁용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FText Description;

	// 아이콘 텍스처
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	TSoftObjectPtr<UTexture2D> Icon;
};
