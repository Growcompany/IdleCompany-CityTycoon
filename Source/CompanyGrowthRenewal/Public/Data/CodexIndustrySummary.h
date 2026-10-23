#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "CodexIndustrySummary.generated.h"

struct FGameSaveData;
struct FShippedProjectRecord;

/** 도감 레일 한 줄 = 산업 하나의 요약. 미보유 산업도 잠금 줄로 나오므로 항상 6행이다. */
USTRUCT()
struct FCodexIndustrySummary
{
	GENERATED_BODY()

	UPROPERTY()
	ECompanyType Industry = ECompanyType::None;

	// 소유 권위 = Buildings. 오피스 레코드는 나중에 생기므로 소유 판정에 쓰지 않는다
	UPROPERTY()
	bool bOwned = false;

	// 같은 산업 건물이 여럿이면 가장 앞선 건물의 티어. 미보유 0, 보유는 최소 1
	UPROPERTY()
	int32 Tier = 0;

	UPROPERTY()
	int64 CumulativeRevenue = 0;

	UPROPERTY()
	int32 ShippedCount = 0;
};

namespace CodexSummary
{
	/** ECompanyType 선언 순서로 6행을 항상 채운다(None/Max 제외). 미보유는 bOwned=false, Tier=0. 보유는 Tier>=1. */
	COMPANYGROWTHRENEWAL_API void BuildAllIndustrySummaries(const FGameSaveData& Save, TArray<FCodexIndustrySummary>& Out);

	/** 누적매출 내림차순 → 평점 내림차순 → ProjectIndex 오름차순. Count 이하로 잘라 반환. */
	COMPANYGROWTHRENEWAL_API void BuildTopShipped(const FGameSaveData& Save, ECompanyType Industry, int32 Count, TArray<FShippedProjectRecord>& Out);

	/** 그 산업 건물들의 ClearedProjects 합집합 — 발견 판정이 건물별로 갈리면 같은 작품이 한쪽에서 '?'가 된다. */
	COMPANYGROWTHRENEWAL_API void BuildClearedProjects(const FGameSaveData& Save, ECompanyType Industry, TArray<int32>& Out);
}
