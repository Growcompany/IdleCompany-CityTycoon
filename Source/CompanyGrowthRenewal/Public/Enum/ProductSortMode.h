#pragma once

#include "CoreMinimal.h"
#include "ProductSortMode.generated.h"

/**
 * EProductSortMode
 * 판매 모달 인벤 카드 정렬 모드. Default = 입력 순서, 나머지는 명시 정렬.
 */
UENUM(BlueprintType)
enum class EProductSortMode : uint8
{
	Default UMETA(DisplayName = "기본순"),
	Price UMETA(DisplayName = "가격순"),
	Quantity UMETA(DisplayName = "보유순"),
	Name UMETA(DisplayName = "이름순")
};

/**
 * ECountrySortMode
 * 11국 라디오의 정렬 모드. Efficiency = PreviewSell 매출 큰 순, Demand = 게이지 비율 큰 순.
 * 카드 미선택 시 Efficiency 의미 없으므로 자동 Demand fallback.
 */
UENUM(BlueprintType)
enum class ECountrySortMode : uint8
{
	Efficiency UMETA(DisplayName = "효율순"),
	Demand UMETA(DisplayName = "수요순")
};
