#pragma once

#include "CoreMinimal.h"
#include "RawMaterialType.generated.h"

/**
 * 원자재 타입 (세계지도 채광소에서 채취)
 * 제조업 회사의 공장 라인 레시피에 사용
 */
UENUM(BlueprintType)
enum class ERawMaterialType : uint8
{
	None       UMETA(DisplayName = "없음"),
	IronOre    UMETA(DisplayName = "철광석"),      // 호주 주력 — 철강판, 자동차, 기계, 대형가전
	Copper     UMETA(DisplayName = "구리"),        // 캐나다 주력 — 전자부품, 배터리, 전선
	Silicon    UMETA(DisplayName = "실리콘"),      // 브라질 주력 — 반도체 칩, 디스플레이
	Lithium    UMETA(DisplayName = "리튬"),        // 호주 — 배터리셀
	Oil        UMETA(DisplayName = "석유"),        // 사우디 독점 — 플라스틱, 정제유, 에너지
	RareEarth  UMETA(DisplayName = "희토류"),      // 중국 독점 — 디스플레이, AI프로세서
	Aluminum   UMETA(DisplayName = "알루미늄"),    // 호주, 캐나다 — 자동차, 항공, 프리미엄 가전
	Wood       UMETA(DisplayName = "목재"),        // 캐나다 주력 — 가구, 내장재
	Gold       UMETA(DisplayName = "금"),          // 남아공 독점 — 주얼리, 고급 커넥터
	DiamondOre UMETA(DisplayName = "다이아몬드"),  // 남아공 독점 — 주얼리, 정밀 절삭

	Max UMETA(Hidden)
};

// 원자재 타입을 한글 문자열로 변환
inline FString RawMaterialTypeToString(ERawMaterialType Type)
{
	switch (Type)
	{
	case ERawMaterialType::IronOre:    return TEXT("철광석");
	case ERawMaterialType::Copper:     return TEXT("구리");
	case ERawMaterialType::Silicon:    return TEXT("실리콘");
	case ERawMaterialType::Lithium:    return TEXT("리튬");
	case ERawMaterialType::Oil:        return TEXT("석유");
	case ERawMaterialType::RareEarth:  return TEXT("희토류");
	case ERawMaterialType::Aluminum:   return TEXT("알루미늄");
	case ERawMaterialType::Wood:       return TEXT("목재");
	case ERawMaterialType::Gold:       return TEXT("금");
	case ERawMaterialType::DiamondOre: return TEXT("다이아몬드");
	default: return TEXT("없음");
	}
}

// StaticEnum 기반 문자열 변환
inline FString EnumToString(ERawMaterialType Value)
{
	const UEnum* EnumPtr = StaticEnum<ERawMaterialType>();
	if (!EnumPtr)
	{
		return FString("Invalid");
	}
	return EnumPtr->GetNameStringByIndex(static_cast<uint8>(Value));
}
