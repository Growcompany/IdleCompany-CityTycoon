#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "Enum/ProjectTrait.h"
#include "Table/ProjectDataTable.h"
#include "StepDisplayNameTable.generated.h"

/**
 * 산업(CompanyType) × Variant → Step1~4 표시명 매핑
 * 금융 / IT 산업은 도메인 variant 별로 Step 명칭이 다르고
 * 단일 산업(Game / Automobile / Semiconductor / Electronics)은 VariantKey = NAME_None 사용
 *
 * 단일 진실 원천: docs/01_Systems/Employee/DEPARTMENT_STEP_MATRIX.md (섹션 6 Step 표시명 표)
 */
USTRUCT(BlueprintType)
struct FStepDisplayNameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StepDisplay")
	ECompanyType CompanyType = ECompanyType::None;

	// 단일 산업이면 NAME_None, 금융/IT variant 식별용 (예: Regulatory, Performance, AI 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StepDisplay")
	FName VariantKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StepDisplay")
	FText Step1Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StepDisplay")
	FText Step2Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StepDisplay")
	FText Step3Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StepDisplay")
	FText Step4Name;

	FStepDisplayNameRow()
		: CompanyType(ECompanyType::None)
		, VariantKey(NAME_None)
	{}

	// 단계 번호(1~4)에 해당하는 표시명 반환, 범위 밖이면 빈 FText
	FText GetStepName(int32 StepNumber) const
	{
		switch (StepNumber)
		{
		case 1: return Step1Name;
		case 2: return Step2Name;
		case 3: return Step3Name;
		case 4: return Step4Name;
		default: return FText::GetEmpty();
		}
	}
};

/**
 * 도메인 트레이트 → variant key 매핑
 * 금융/IT 산업의 Step 표시명 분기에 사용
 */
inline FName GetVariantKeyFromTrait(EProjectTrait Trait)
{
	switch (Trait)
	{
	case EProjectTrait::RegulatoryDomain:  return FName(TEXT("Regulatory"));
	case EProjectTrait::PerformanceDomain: return FName(TEXT("Performance"));
	case EProjectTrait::InsuranceDomain:   return FName(TEXT("Insurance"));
	case EProjectTrait::AuditDomain:       return FName(TEXT("Audit"));
	case EProjectTrait::ITGeneralDomain:   return FName(TEXT("General"));
	case EProjectTrait::ITAIDomain:        return FName(TEXT("AI"));
	case EProjectTrait::ITInfraDomain:     return FName(TEXT("Infra"));
	default:                               return NAME_None;
	}
}

/**
 * ProjectData 의 VariantKey 필드 반환
 * DT_Project_*_Import.csv 의 VariantKey 컬럼이 단일 진실 원천
 * (산업/ProjectIndex 별 variant 분포는 Tools/add_variant_column.py 참조)
 */
inline FName GetProjectVariantKey(const FProjectData& ProjectData)
{
	return ProjectData.VariantKey;
}
