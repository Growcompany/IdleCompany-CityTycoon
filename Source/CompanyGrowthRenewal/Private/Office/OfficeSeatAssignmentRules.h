#pragma once

#include "Data/EmployeeTypes.h"
#include "Office/WorkstationTypes.h"

namespace OfficeSeatAssignmentRules
{
inline int32 ValidateSavedSeats(FWorkstationSaveData& Workstation,
	const TArray<FEmployeeInstance>& Employees, int32 BuildingIndex, TSet<int32>& SeatedEmployees)
{
	int32 ClearedCount = 0;
	for (FChairSlotState& Chair : Workstation.ChairStates)
	{
		const int32 EmployeeID = Chair.OccupantEmployeeID;
		if (EmployeeID == INDEX_NONE)
		{
			continue;
		}
		const FEmployeeInstance* Employee = Employees.FindByPredicate([EmployeeID](const FEmployeeInstance& Candidate)
		{
			return Candidate.EmployeeID == EmployeeID;
		});
		if (EmployeeID < 0 || !Employee || !Employee->bIsAssigned
			|| Employee->AssignedBuildingIndex != BuildingIndex || SeatedEmployees.Contains(EmployeeID))
		{
			Chair.OccupantEmployeeID = INDEX_NONE;
			++ClearedCount;
		}
		else
		{
			SeatedEmployees.Add(EmployeeID);
		}
	}
	return ClearedCount;
}
}
