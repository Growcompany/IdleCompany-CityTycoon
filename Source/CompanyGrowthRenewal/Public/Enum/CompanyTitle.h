#pragma once

#include "CoreMinimal.h"
#include "CompanyTitle.generated.h"

UENUM(BlueprintType)
enum class ECompanyTitle : uint8
{
	Small   UMETA(DisplayName = "중소기업"),
	MidSize UMETA(DisplayName = "중견기업"),
	Large   UMETA(DisplayName = "대기업"),
	Global  UMETA(DisplayName = "글로벌 기업"),
	Elite   UMETA(DisplayName = "초일류 기업"),
};

inline FString CompanyTitleToString(ECompanyTitle Title)
{
	switch (Title)
	{
	case ECompanyTitle::Small:   return TEXT("중소기업");
	case ECompanyTitle::MidSize: return TEXT("중견기업");
	case ECompanyTitle::Large:   return TEXT("대기업");
	case ECompanyTitle::Global:  return TEXT("글로벌 기업");
	case ECompanyTitle::Elite:   return TEXT("초일류 기업");
	default:                     return TEXT("중소기업");
	}
}
