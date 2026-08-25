#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/EmployeeStatsData.h"
#include "EmployeePotentialDisplayTable.generated.h"

/**
 * 잠재능력 옵션 (EPotentialOptionType) → 한국어 라벨 매핑
 * 단일 진실 원천: DT_EmployeePotentialDisplay (DataImport/DT_EmployeePotentialDisplay_Import.csv)
 */
USTRUCT(BlueprintType)
struct FPotentialOptionDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	EPotentialOptionType OptionType = EPotentialOptionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;
};

/**
 * 추가옵션 (EAdditionalOptionType) → 한국어 라벨 매핑
 * 단일 진실 원천: DT_EmployeeAdditionalDisplay (DataImport/DT_EmployeeAdditionalDisplay_Import.csv)
 */
USTRUCT(BlueprintType)
struct FAdditionalOptionDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	EAdditionalOptionType OptionType = EAdditionalOptionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;
};
