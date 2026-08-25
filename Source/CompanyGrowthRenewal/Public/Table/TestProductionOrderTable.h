// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "TestProductionOrderTable.generated.h"

/**
 * DT_TestProductionOrders 의 Row.
 * 테스트용 ProductionOrder 일괄 spawn 시드. 사무실 진행 우회로 공장 탭 단독 검증.
 * RowName 자유 (e.g. "Korea_Semiconductor_1", "Japan_Auto_3" 등 구분용).
 */
USTRUCT(BlueprintType)
struct FTestProductionOrderRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	int32 ProjectIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	FString ProductName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	int32 Quantity = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	EQualityGrade Grade = EQualityGrade::C;

	// SourceBuildingID 는 테스트라 -1(INDEX_NONE) 고정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	int32 SourceBuildingID = -1;

	FTestProductionOrderRow() = default;
};
