#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "Data/EmployeeTypes.h"
#include "DepartmentDisplayTable.generated.h"

/**
 * 산업(CompanyType) × 부서(Department) → 표시명 매핑
 * 같은 enum 직원이라도 보고 있는 회사의 산업에 따라 표시명이 다름
 * 예: Development = 게임에선 "개발팀", 반도체에선 "검증팀"
 *
 * 단일 진실 원천: docs/01_Systems/Employee/DEPARTMENT_STEP_MATRIX.md (Layer 1)
 */
USTRUCT(BlueprintType)
struct FDepartmentDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	EEmployeeDepartment Department = EEmployeeDepartment::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	// 캐시 TMap 키 생성 (X=CompanyType, Y=Department)
	static FIntPoint MakeKey(ECompanyType InCompanyType, EEmployeeDepartment InDepartment)
	{
		return FIntPoint(static_cast<int32>(InCompanyType), static_cast<int32>(InDepartment));
	}
};
