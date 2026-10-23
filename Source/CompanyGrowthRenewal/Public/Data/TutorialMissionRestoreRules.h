#pragma once

#include "CoreMinimal.h"
#include "Data/ShippedRecord.h"

struct FTutorialMissionRestoreRules
{
	static bool HasRequiredLaunch(const TArray<FShippedProjectRecord>& Records,
		int32 BuildingIndex, ECompanyType Industry, int32 RequiredProjectIndex)
	{
		return BuildingIndex != INDEX_NONE && RequiredProjectIndex > 0
			&& ShippedProjectRecords::Find(Records, BuildingIndex, Industry, RequiredProjectIndex) != nullptr;
	}

	static bool ShouldRestoreClaimReady(
		bool bSavedClaimReady,
		FName SavedMissionID,
		FName RestoredMissionID,
		bool bManualClaim)
	{
		return bSavedClaimReady
			&& !RestoredMissionID.IsNone()
			&& SavedMissionID == RestoredMissionID
			&& bManualClaim;
	}
};
