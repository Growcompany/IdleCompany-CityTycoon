#pragma once

#include "CoreMinimal.h"
#include "FunnyPartSlot.generated.h"

// 개인 시드축이 고르는 파츠 슬롯. DT_FunnyPartPool 의 행 분류 키.
UENUM(BlueprintType)
enum class EFunnyPartSlot : uint8
{
	Body		UMETA(DisplayName = "바디"),
	Outerwear	UMETA(DisplayName = "상의"),
	Pants		UMETA(DisplayName = "하의"),
	Shoe		UMETA(DisplayName = "신발"),
	Hair		UMETA(DisplayName = "헤어"),
	Eyebrow		UMETA(DisplayName = "눈썹"),
	Glasses		UMETA(DisplayName = "안경"),

	Max			UMETA(Hidden)
};

// 헤어만 성별로 갈린다. 나머지 슬롯은 전부 Any.
UENUM(BlueprintType)
enum class EFunnyPartGender : uint8
{
	Any		UMETA(DisplayName = "공용"),
	Male	UMETA(DisplayName = "남성"),
	Female	UMETA(DisplayName = "여성")
};
