#pragma once

#include "CoreMinimal.h"

struct FTutorialMissionRestoreRules
{
	static bool ShouldRestoreClaimReady(
		bool bSavedClaimReady,
		FName SavedMissionID,
		FName RestoredMissionID,
		bool bHasNextMission)
	{
		return bSavedClaimReady
			&& !RestoredMissionID.IsNone()
			&& SavedMissionID == RestoredMissionID
			&& !bHasNextMission;
	}
};
