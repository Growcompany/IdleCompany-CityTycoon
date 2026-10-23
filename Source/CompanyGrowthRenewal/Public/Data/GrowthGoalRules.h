#pragma once

#include "CoreMinimal.h"
#include "Data/GameSaveData.h"
#include "Table/GoalTable.h"
#include "Table/ProjectDataTable.h"

namespace GrowthGoalRules
{
	inline int32 RepeatCap(const FGoalTable& Row) { return Row.RepeatLimit == 0 ? MAX_int32 : FMath::Max(1, Row.RepeatLimit); }
	inline bool ShouldEvaluateSnapshot(const FGoalTable& Row) { return !Row.bCountActions && RepeatCap(Row) == 1; }
	inline bool CanRecordSuccess(const FGoalTable& Row, int32 Count) { return Count >= 0 && Count < RepeatCap(Row); }
	inline int64 AddProgress(int64 Current, int64 Amount)
	{
		Current = FMath::Max<int64>(0, Current);
		return Amount > 0 ? Current + FMath::Min(Amount, MAX_int64 - Current) : Current;
	}
	inline int64 TargetForClaim(const FGoalTable& Row, int32 ClaimCount)
	{
		const int64 Base = FMath::Max<int64>(1, Row.ConditionAmount);
		const int64 Increment = FMath::Max<int64>(0, Row.RepeatTargetIncrement);
		const int64 Count = FMath::Max(0, ClaimCount);
		const int64 Growth = Increment > 0 && Count > (MAX_int64 - Base) / Increment
			? MAX_int64 - Base : Increment * Count;
		const int64 Target = Base + Growth;
		return Row.RepeatTargetCap > 0 ? FMath::Min(Target, FMath::Max(Base, Row.RepeatTargetCap)) : Target;
	}
	inline bool PrerequisiteMet(FName PrerequisiteID, const TArray<FName>& FirstSuccessIDs)
	{
		return PrerequisiteID.IsNone() || FirstSuccessIDs.Contains(PrerequisiteID);
	}

	inline bool HasShipped(const FGameSaveData& Data, ECompanyType Industry, int32 ProjectIndex,
		int32 BuildingIndex = INDEX_NONE)
	{
		return Data.ShippedProjects.ContainsByPredicate([=](const FShippedProjectRecord& Record)
		{
			return Record.Industry == Industry && Record.ProjectIndex == ProjectIndex
				&& (BuildingIndex == INDEX_NONE || Record.BuildingIndex == BuildingIndex);
		});
	}

	inline bool MatchesCompany(const FGoalTable& Row, const FBuildingEntitySaveData& Building,
		const FGameSaveData& Data)
	{
		return Building.BuildingData.CompanyType == Row.CompanyType
			&& (Row.RequiredProjectIndex <= 0
				|| HasShipped(Data, Row.CompanyType, Row.RequiredProjectIndex, Building.BuildingIndex));
	}

	inline bool SuppressPlanningRecruitIntro(const FGoalTable& Row, int32 GuideBuildingIndex, int32 OpenBuildingIndex)
	{
		return Row.ConditionType == EMissionConditionType::RecruitGoalDiscipline
			&& Row.RequiredDiscipline == EProductionDiscipline::Plan
			&& GuideBuildingIndex != INDEX_NONE && GuideBuildingIndex == OpenBuildingIndex;
	}

	inline bool HasUpgradedDesk(const FWorkstationSaveData& Desk)
	{
		if (GetComputerSetupLevelNumber(Desk.ComputerSetupLevel) > 1) { return true; }
		for (const auto& Upgrade : Desk.UpgradeLevels)
		{
			if (Upgrade.Value > 1) { return true; }
		}
		return false;
	}

	inline bool HasProjectStaff(const FProjectData& Project, int32 BuildingIndex,
		const TArray<FEmployeeInstance>& Employees)
	{
		const int32 Weights[] = {Project.Weight_Plan, Project.Weight_Dev, Project.Weight_Graphics,
			Project.Weight_Sound, Project.Weight_Server, Project.Weight_QA};
		for (int32 Discipline = 0; Discipline < UE_ARRAY_COUNT(Weights); ++Discipline)
		{
			if (Weights[Discipline] <= 0) { continue; }
			const bool bStaffed = Employees.ContainsByPredicate([=](const FEmployeeInstance& Employee)
			{
				return Employee.AssignedBuildingIndex == BuildingIndex
					&& Employee.DisciplinePoints.IsValidIndex(Discipline)
					&& Employee.DisciplinePoints[Discipline] > 0;
			});
			if (!bStaffed) { return false; }
		}
		return true;
	}

	// 현재 보유/출시 상태에서 도출한다. 먼저 플레이한 경우에도 재실행을 요구하지 않는다.
	inline bool GetProgress(const FGoalTable& Row, const FGameSaveData& Data,
		const TArray<FEmployeeInstance>& Employees, int64& OutCurrent, int64& OutTarget)
	{
		OutCurrent = 0;
		OutTarget = FMath::Max<int64>(1, Row.ConditionAmount);
		switch (Row.ConditionType)
		{
		case EMissionConditionType::LevelUpFirstEmployee:
			for (const FEmployeeInstance& Employee : Employees)
			{
				if (Employee.Level > 1) { OutCurrent = 1; break; }
			}
			return true;
		case EMissionConditionType::EnhanceFirstEmployee:
			for (const FEmployeeInstance& Employee : Employees)
			{
				if (Employee.EnhancementLevel > 0) { OutCurrent = 1; break; }
			}
			return true;
		case EMissionConditionType::UpgradeFirstWorkstation:
		case EMissionConditionType::PromoteFirstWorkstation:
			for (const auto& Office : Data.OfficeDataMap)
			{
				for (const FWorkstationSaveData& Desk : Office.Value.OfficeWorkstations)
				{
					if (Row.ConditionType == EMissionConditionType::UpgradeFirstWorkstation
						? HasUpgradedDesk(Desk) : GetComputerSetupLevelNumber(Desk.ComputerSetupLevel) > 1)
					{
						OutCurrent = 1;
						return true;
					}
				}
			}
			return true;
		case EMissionConditionType::RecruitGoalDiscipline:
			for (const FBuildingEntitySaveData& Building : Data.Buildings)
			{
				if (!MatchesCompany(Row, Building, Data)) { continue; }
				int64 Count = 0;
				for (const FEmployeeInstance& Employee : Employees)
				{
					if (Employee.AssignedBuildingIndex == Building.BuildingIndex
						&& Row.RequiredDiscipline != EProductionDiscipline::Count
						&& GetPrimaryDisciplineSlot(Employee) == static_cast<int32>(Row.RequiredDiscipline))
					{
						++Count;
					}
				}
				OutCurrent = FMath::Max(OutCurrent, Count);
			}
			return true;
		case EMissionConditionType::LaunchGoalProject:
			for (const FBuildingEntitySaveData& Building : Data.Buildings)
			{
				if (MatchesCompany(Row, Building, Data) && Row.TargetProjectIndex > 0
					&& HasShipped(Data, Row.CompanyType, Row.TargetProjectIndex, Building.BuildingIndex))
				{
					OutCurrent = 1;
					break;
				}
			}
			return true;
		case EMissionConditionType::BuildIndustry:
			for (const FBuildingEntitySaveData& Building : Data.Buildings)
			{
				if (Building.BuildingData.CompanyType == Row.CompanyType) { ++OutCurrent; }
			}
			return true;
		default:
			return false;
		}
	}
}
