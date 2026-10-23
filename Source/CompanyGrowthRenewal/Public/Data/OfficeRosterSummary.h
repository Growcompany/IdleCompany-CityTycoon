#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "OfficeRosterSummary.generated.h"

struct FEmployeeInstance;

/** 사무실 직무 구성 목록 한 줄 = 직능 슬롯 하나. */
USTRUCT()
struct FOfficeRosterLine
{
	GENERATED_BODY()

	// 주 직능(직능 포인트 최댓값)이 이 슬롯인 소속 직원 수
	UPROPERTY()
	int32 Headcount = 0;

	// 소속 직원 전원의 이 슬롯 직능 포인트 합 ― 다른 직무 직원 몫도 들어간다
	UPROPERTY()
	int64 Power = 0;
};

namespace OfficeRosterSummary
{
	/** Members = 한 건물 소속 직원. Out 을 비우고 직능 슬롯 순서로 항상 6줄을 채운다. */
	COMPANYGROWTHRENEWAL_API void Build(const TArray<FEmployeeInstance>& Members, TArray<FOfficeRosterLine>& Out,
		ECompanyType CompanyType = ECompanyType::Game);

	/** 전체 명부에서 이 건물 소속(대기석 포함 ― 인원 칩과 같은 집합)만 골라 Build 한다. OutMemberCount = 그 인원. */
	COMPANYGROWTHRENEWAL_API void BuildForBuilding(const TArray<FEmployeeInstance>& AllEmployees, int32 BuildingIndex,
		TArray<FOfficeRosterLine>& Out, int32& OutMemberCount, ECompanyType CompanyType = ECompanyType::Game);
}
