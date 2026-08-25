// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/EmployeeTypes.h"
#include "Table/ConstructionCost.h"
#include "EmployeeCardTable.generated.h"

/**
 * 직원 카드 데이터 테이블 (직원 전용)
 */
USTRUCT(BlueprintType)
struct FEmployeeCardTable : public FTableRowBase
{
	GENERATED_BODY()

	// 데이터 테이블의 Row Name을 저장 (런타임에 할당됨)
	UPROPERTY(BlueprintReadOnly)
	FName RowName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> UIIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FColor InfoBackgroundColor;

	// 직원 부서
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Employee")
	EEmployeeDepartment Department;

	// 고용 비용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
	TArray<FConstructionCost> ConstructionCosts;

	// TODO: 직원 전용 필드 추가 (예: 급여, 스킬, 능력치 등)

	FString ToString() const
	{
		FString Out = "";
		Out += Name.ToString() + ", ";
		Out += DepartmentToString(Department);
		return Out;
	}
};
