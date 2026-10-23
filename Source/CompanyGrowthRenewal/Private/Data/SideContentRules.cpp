#include "Data/SideContentRules.h"
#include "Manager/OfficeStageProgressManager.h"

namespace
{
	/** 직원 1명이 한 직능에 내는 파워. 잠재·장비·스탯은 넣지 않는다(설계 §1.1). */
	float SoloPower(const FEmployeeInstance& Employee, EProductionDiscipline Discipline)
	{
		const float Base = UEmployeeTypeHelper::CalculateBaseOutput(Employee.Level);
		const float Affinity = UOfficeStageProgressManager::DisciplineAffinity(FSideContentRules::GetDisciplinePoints(Employee, Discipline));
		const float StarMul = 1.0f + FSideContentRules::StarBonusPerLevel * static_cast<float>(Employee.EnhancementLevel);
		return Base * Affinity * StarMul;
	}

	const FEmployeeInstance* FindByID(const TArray<FEmployeeInstance>& Roster, int32 EmployeeID)
	{
		return Roster.FindByPredicate([EmployeeID](const FEmployeeInstance& E) { return E.EmployeeID == EmployeeID; });
	}
}

int32 FSideContentRules::GetDisciplinePoints(const FEmployeeInstance& Employee, EProductionDiscipline Discipline)
{
	const int32 Index = static_cast<int32>(Discipline);
	return Employee.DisciplinePoints.IsValidIndex(Index) ? Employee.DisciplinePoints[Index] : 0;
}

bool FSideContentRules::IsBusy(const FEmployeeInstance& Employee, int64 NowUnix)
{
	return UEmployeeTypeHelper::IsOnBusinessTrip(Employee, NowUnix);
}

void FSideContentRules::ComputeTeamPower(const TArray<FEmployeeInstance>& Roster, const TArray<int32>& TeamIDs, TArray<float>& OutPower)
{
	OutPower.Reset();
	OutPower.AddZeroed(DisciplineCount);

	for (int32 EmployeeID : TeamIDs)
	{
		const FEmployeeInstance* Employee = FindByID(Roster, EmployeeID);
		if (!Employee) continue;

		for (int32 d = 0; d < DisciplineCount; ++d)
		{
			OutPower[d] += SoloPower(*Employee, static_cast<EProductionDiscipline>(d));
		}
	}
}

float FSideContentRules::Evaluate(const TArray<float>& Power, const TArray<float>& Required, const TArray<float>& Weight)
{
	float WeightSum = 0.0f;
	float Accum = 0.0f;

	for (int32 d = 0; d < DisciplineCount; ++d)
	{
		const float W = Weight.IsValidIndex(d) ? Weight[d] : 0.0f;
		if (W <= 0.0f) continue;

		const float Need = Required.IsValidIndex(d) ? Required[d] : 0.0f;
		const float Have = Power.IsValidIndex(d) ? Power[d] : 0.0f;

		// 요구가 0 인 직능은 이미 충족 — 분모 0 을 만들지 않는다
		const float Ratio = (Need <= 0.0f) ? 1.0f : FMath::Min(Have / Need, 1.0f);

		Accum += W * Ratio;
		WeightSum += W;
	}

	return (WeightSum > 0.0f) ? (Accum / WeightSum) : 0.0f;
}

TArray<int32> FSideContentRules::AutoPickTeam(const TArray<FEmployeeInstance>& Roster, const TArray<float>& Weight, int32 Count, int64 NowUnix)
{
	struct FScored { int32 EmployeeID; float Score; };
	TArray<FScored> Scored;
	Scored.Reserve(Roster.Num());

	float WeightSum = 0.0f;
	for (int32 d = 0; d < DisciplineCount; ++d) WeightSum += Weight.IsValidIndex(d) ? Weight[d] : 0.0f;

	for (const FEmployeeInstance& Employee : Roster)
	{
		if (IsBusy(Employee, NowUnix)) continue;

		float Score = 0.0f;
		for (int32 d = 0; d < DisciplineCount; ++d)
		{
			const float W = Weight.IsValidIndex(d) ? Weight[d] : 0.0f;
			if (W > 0.0f) Score += W * SoloPower(Employee, static_cast<EProductionDiscipline>(d));
		}
		if (WeightSum > 0.0f) Score /= WeightSum;

		Scored.Add({ Employee.EmployeeID, Score });
	}

	Scored.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });

	TArray<int32> Picked;
	const int32 Take = FMath::Clamp(Count, 0, Scored.Num());
	for (int32 i = 0; i < Take; ++i) Picked.Add(Scored[i].EmployeeID);
	return Picked;
}

int32 FSideContentRules::SumDisciplinePoints(const TArray<FEmployeeInstance>& Roster, const TArray<int32>& EmployeeIDs, EProductionDiscipline Discipline)
{
	int32 Sum = 0;
	for (int32 EmployeeID : EmployeeIDs)
	{
		if (const FEmployeeInstance* Employee = FindByID(Roster, EmployeeID))
		{
			Sum += GetDisciplinePoints(*Employee, Discipline);
		}
	}
	return Sum;
}

TArray<int32> FSideContentRules::AutoPickDispatchCrew(const TArray<FEmployeeInstance>& Roster, int32 BuildingIndex, EProductionDiscipline Discipline, int32 Count, int64 NowUnix)
{
	struct FCandidate { int32 EmployeeID; int32 Points; bool bBench; };
	TArray<FCandidate> Candidates;

	for (const FEmployeeInstance& Employee : Roster)
	{
		if (IsBusy(Employee, NowUnix)) continue;

		const bool bBench = (Employee.AssignedBuildingIndex == INDEX_NONE);
		const bool bThisBuilding = (BuildingIndex != INDEX_NONE && Employee.AssignedBuildingIndex == BuildingIndex);
		if (!bBench && !bThisBuilding) continue;

		Candidates.Add({ Employee.EmployeeID, GetDisciplinePoints(Employee, Discipline), bBench });
	}

	// 벤치가 먼저, 그 안에서 직능 높은 순 — 배치 팀을 빼면 그 건물이 손해라서 마지막에 쓴다
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.bBench != B.bBench) return A.bBench;
		return A.Points > B.Points;
	});

	TArray<int32> Picked;
	const int32 Take = FMath::Clamp(Count, 0, Candidates.Num());
	for (int32 i = 0; i < Take; ++i) Picked.Add(Candidates[i].EmployeeID);
	return Picked;
}

int32 FSideContentRules::CountAvailableCrew(const TArray<FEmployeeInstance>& Roster, int32 BuildingIndex, int64 NowUnix)
{
	int32 Num = 0;
	for (const FEmployeeInstance& Employee : Roster)
	{
		if (IsBusy(Employee, NowUnix)) continue;
		const bool bBench = (Employee.AssignedBuildingIndex == INDEX_NONE);
		const bool bThisBuilding = (BuildingIndex != INDEX_NONE && Employee.AssignedBuildingIndex == BuildingIndex);
		if (bBench || bThisBuilding) ++Num;
	}
	return Num;
}

int32 FSideContentRules::RefillEntries(int32 Current, int32 Daily, int32 Cap)
{
	// 이미 상한을 넘겨 들고 있으면 깎지 않는다 — 리필은 더하기만 한다
	if (Current >= Cap) return Current;
	return FMath::Min(Current + FMath::Max(Daily, 0), Cap);
}

TArray<FMissionReward> FSideContentRules::RollLoot(const TArray<FSideDungeonLootRow>& Rows, int32 Stage, int32 Rolls, int32 HQLevel, FRandomStream& Rng, int32 CompanyLevel)
{
	TArray<FMissionReward> Result;

	TArray<const FSideDungeonLootRow*> Pool;
	float TotalWeight = 0.0f;
	for (const FSideDungeonLootRow& Row : Rows)
	{
		if (Row.Weight <= 0.0f) continue;
		if (Stage < Row.MinStage || Stage > Row.MaxStage) continue;
		if (HQLevel < Row.RequiredHQLevel) continue;
		if (CompanyLevel < Row.RequiredCompanyLevel) continue;

		Pool.Add(&Row);
		TotalWeight += Row.Weight;
	}

	if (Pool.Num() == 0 || TotalWeight <= 0.0f) return Result;

	for (int32 r = 0; r < FMath::Max(Rolls, 0); ++r)
	{
		float Pick = Rng.FRandRange(0.0f, TotalWeight);
		const FSideDungeonLootRow* Chosen = Pool.Last();
		for (const FSideDungeonLootRow* Row : Pool)
		{
			Pick -= Row->Weight;
			if (Pick <= 0.0f) { Chosen = Row; break; }
		}

		FMissionReward Reward;
		if (Chosen->ItemType != EItemType::None)
		{
			Reward.ItemType = Chosen->ItemType;
			Reward.ItemAmount = Rng.RandRange(FMath::Min(Chosen->ItemAmountMin, Chosen->ItemAmountMax), FMath::Max(Chosen->ItemAmountMin, Chosen->ItemAmountMax));
		}
		if (Chosen->ResourceType != EResourceType::None)
		{
			Reward.ResourceType = Chosen->ResourceType;
			const int32 Lo = static_cast<int32>(FMath::Min(Chosen->ResourceAmountMin, Chosen->ResourceAmountMax));
			const int32 Hi = static_cast<int32>(FMath::Max(Chosen->ResourceAmountMin, Chosen->ResourceAmountMax));
			Reward.Amount = static_cast<int64>(Rng.RandRange(Lo, Hi));
		}

		const bool bHasItem = (Reward.ItemType != EItemType::None && Reward.ItemAmount > 0);
		const bool bHasResource = (Reward.ResourceType != EResourceType::None && Reward.Amount > 0);
		if (bHasItem || bHasResource) Result.Add(Reward);
	}

	return Result;
}
