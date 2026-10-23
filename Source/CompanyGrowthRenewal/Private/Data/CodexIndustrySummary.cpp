#include "Data/CodexIndustrySummary.h"
#include "Data/GameSaveData.h"
#include "Data/ShippedRecord.h"

namespace
{
	// 소유 권위는 Buildings 다. OfficeDataMap 은 오피스 진입 시점에 지연 생성돼 소유 판정에 쓸 수 없어, 건물 수를 따로 돌려준다.
	int32 CollectIndustryOffices(const FGameSaveData& Save, ECompanyType Industry, TArray<const FOfficeSaveData*>& Out)
	{
		Out.Reset();
		int32 BuildingCount = 0;
		for (const FBuildingEntitySaveData& Building : Save.Buildings)
		{
			if (Building.BuildingData.CompanyType != Industry) { continue; }
			++BuildingCount;
			if (const FOfficeSaveData* Office = Save.OfficeDataMap.Find(Building.BuildingIndex))
			{
				Out.Add(Office);
			}
		}
		return BuildingCount;
	}
}

void CodexSummary::BuildAllIndustrySummaries(const FGameSaveData& Save, TArray<FCodexIndustrySummary>& Out)
{
	Out.Reset();
	TArray<const FOfficeSaveData*> Offices;

	for (uint8 Raw = (uint8)ECompanyType::None + 1; Raw < (uint8)ECompanyType::Max; ++Raw)
	{
		const ECompanyType Industry = (ECompanyType)Raw;
		FCodexIndustrySummary Row;
		Row.Industry = Industry;

		Row.bOwned = CollectIndustryOffices(Save, Industry, Offices) > 0;
		if (Row.bOwned)
		{
			// 오피스 레코드가 아직 없는 신생 회사도 1티어다 — FProjectTierProgress::CurrentTier 기본값과 같은 기준
			Row.Tier = 1;
			for (const FOfficeSaveData* Office : Offices)
			{
				Row.Tier = FMath::Max(Row.Tier, Office->TierProgress.CurrentTier);
			}
		}

		for (const FShippedProjectRecord& Rec : Save.ShippedProjects)
		{
			if (Rec.Industry != Industry) { continue; }
			Row.CumulativeRevenue += Rec.CumulativeRevenue;
			++Row.ShippedCount;
		}

		Out.Add(Row);
	}
}

void CodexSummary::BuildTopShipped(const FGameSaveData& Save, ECompanyType Industry, int32 Count, TArray<FShippedProjectRecord>& Out)
{
	Out.Reset();
	if (Count <= 0) { return; }

	for (const FShippedProjectRecord& Rec : Save.ShippedProjects)
	{
		if (Rec.Industry == Industry) { Out.Add(Rec); }
	}
	Out.Sort([](const FShippedProjectRecord& A, const FShippedProjectRecord& B)
	{
		if (A.CumulativeRevenue != B.CumulativeRevenue) { return A.CumulativeRevenue > B.CumulativeRevenue; }
		if (A.ReviewScore != B.ReviewScore) { return A.ReviewScore > B.ReviewScore; }
		// TArray::Sort 는 비안정 정렬이라 3차 키가 없으면 완전 동률 레코드의 순서가 실행마다 달라진다
		return A.ProjectIndex < B.ProjectIndex;
	});
	if (Out.Num() > Count) { Out.SetNum(Count); }
}

void CodexSummary::BuildClearedProjects(const FGameSaveData& Save, ECompanyType Industry, TArray<int32>& Out)
{
	Out.Reset();
	TArray<const FOfficeSaveData*> Offices;
	CollectIndustryOffices(Save, Industry, Offices);
	for (const FOfficeSaveData* Office : Offices)
	{
		for (const int32 ProjectIndex : Office->TierProgress.ClearedProjects)
		{
			Out.AddUnique(ProjectIndex);
		}
	}
}
