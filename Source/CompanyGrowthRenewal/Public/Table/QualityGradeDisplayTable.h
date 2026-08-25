#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "QualityGradeDisplayTable.generated.h"

/**
 * 산업(CompanyType) × 품질등급(EQualityGrade) → 한국어 라벨 매핑
 * 같은 등급이라도 산업에 따라 표시명이 다름
 * 예: S 등급 = 게임에선 "명작", 전자에선 "프리미엄", 자동차에선 "최고급"
 *
 * 단일 진실 원천: DT_QualityGradeDisplay (DataImport/DT_QualityGradeDisplay_Import.csv)
 */
USTRUCT(BlueprintType)
struct FQualityGradeDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	EQualityGrade Grade = EQualityGrade::C;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	static FIntPoint MakeKey(ECompanyType InCompanyType, EQualityGrade InGrade)
	{
		return FIntPoint(static_cast<int32>(InCompanyType), static_cast<int32>(InGrade));
	}
};
