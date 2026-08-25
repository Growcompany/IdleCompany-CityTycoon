#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DisciplineDisplayTable.generated.h"

// 산업별 직능 표시명 — 슬롯 의미는 EProductionDiscipline 고정, 표시만 산업별 재해석 (DECISION §7.2)
USTRUCT(BlueprintType)
struct FDisciplineDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FString CompanyType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FText Plan;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FText Dev;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FText Graphics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FText Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FText Server;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discipline")
	FText QA;
};
