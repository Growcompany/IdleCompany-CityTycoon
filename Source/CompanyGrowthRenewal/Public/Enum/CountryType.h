#pragma once

#include "CoreMinimal.h"
#include "CountryType.generated.h"

/**
 * 세계지도 나라 타입.
 * 값은 세이브와 DT RowName 매핑의 계약이므로 순서를 바꾸거나 중간에 끼워 넣지 말 것.
 */
UENUM(BlueprintType)
enum class ECountryType : uint8
{
	None		UMETA(DisplayName = "없음"),
	Korea		UMETA(DisplayName = "한국"),
	China		UMETA(DisplayName = "중국"),
	Japan		UMETA(DisplayName = "일본"),
	Germany		UMETA(DisplayName = "독일"),
	USA			UMETA(DisplayName = "미국"),
	SouthAfrica	UMETA(DisplayName = "남아공"),
	Australia	UMETA(DisplayName = "호주"),
	Canada		UMETA(DisplayName = "캐나다"),
	Brazil		UMETA(DisplayName = "브라질"),
	Saudi		UMETA(DisplayName = "사우디"),
	Singapore	UMETA(DisplayName = "싱가포르")
};

/**
 * 메달리온 원반(88px) 안에 들어갈 2글자 국가 코드(ISO 3166-1 alpha-2).
 * DT 컬럼도 국기 텍스처도 없이 enum 에서 파생시킨다 — 새 국가를 넣으면 여기도 같이 늘려야 한다.
 */
inline const TCHAR* ToCountryCode(ECountryType Country)
{
	switch (Country)
	{
	case ECountryType::Korea:       return TEXT("KR");
	case ECountryType::China:       return TEXT("CN");
	case ECountryType::Japan:       return TEXT("JP");
	case ECountryType::Germany:     return TEXT("DE");
	case ECountryType::USA:         return TEXT("US");
	case ECountryType::SouthAfrica: return TEXT("ZA");
	case ECountryType::Australia:   return TEXT("AU");
	case ECountryType::Canada:      return TEXT("CA");
	case ECountryType::Brazil:      return TEXT("BR");
	case ECountryType::Saudi:       return TEXT("SA");
	case ECountryType::Singapore:   return TEXT("SG");
	default:                        return TEXT("");
	}
}
