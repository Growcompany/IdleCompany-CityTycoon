#pragma once

#include "CoreMinimal.h"
#include "CompanyType.generated.h"

/**
 * 회사 타입 (산업 분야)
 * 각 회사 타입별로 고유한 스테이지와 스킬을 가짐
 */
UENUM(BlueprintType)
enum class ECompanyType : uint8
{
	None UMETA(DisplayName = "None"),
	Game UMETA(DisplayName = "게임"),
	Electronics UMETA(DisplayName = "전자"),
	Finance UMETA(DisplayName = "금융"),
	IT UMETA(DisplayName = "IT"),
	Semiconductor UMETA(DisplayName = "반도체"),
	Automobile UMETA(DisplayName = "자동차"),

	Max UMETA(Hidden)
};

// 회사 타입을 문자열로 변환
inline FString CompanyTypeToString(ECompanyType Type)
{
	switch (Type)
	{
	case ECompanyType::Game: return TEXT("게임");
	case ECompanyType::Electronics: return TEXT("전자");
	case ECompanyType::Finance: return TEXT("금융");
	case ECompanyType::IT: return TEXT("IT");
	case ECompanyType::Semiconductor: return TEXT("반도체");
	case ECompanyType::Automobile: return TEXT("자동차");
	default: return TEXT("None");
	}
}

// 제조업 타입 여부 판별 (반도체, 자동차, 전자)
inline bool IsManufacturingType(ECompanyType Type)
{
	return Type == ECompanyType::Semiconductor
		|| Type == ECompanyType::Automobile
		|| Type == ECompanyType::Electronics;
}

// 프로젝트 타입 여부 판별 (게임, 금융, IT)
inline bool IsProjectType(ECompanyType Type)
{
	return Type == ECompanyType::Game
		|| Type == ECompanyType::Finance
		|| Type == ECompanyType::IT;
}

// 문자열을 회사 타입으로 변환
inline ECompanyType StringToCompanyType(const FString& TypeString)
{
	if (TypeString == TEXT("게임") || TypeString == TEXT("Game")) return ECompanyType::Game;
	if (TypeString == TEXT("전자") || TypeString == TEXT("Electronics")) return ECompanyType::Electronics;
	if (TypeString == TEXT("금융") || TypeString == TEXT("Finance")) return ECompanyType::Finance;
	if (TypeString == TEXT("IT")) return ECompanyType::IT;
	if (TypeString == TEXT("반도체") || TypeString == TEXT("Semiconductor")) return ECompanyType::Semiconductor;
	if (TypeString == TEXT("자동차") || TypeString == TEXT("Automobile")) return ECompanyType::Automobile;
	return ECompanyType::None;
}
