#pragma once

#include "CoreMinimal.h"

struct FTutorialRevenueCompletionRules
{
	static bool IsPositiveCollection(int64 Amount)
	{
		return Amount > 0;
	}

	static bool CanFinish(bool bReceivedRevenue, bool bWatchingIncome, double ObservationSeconds)
	{
		return bReceivedRevenue && bWatchingIncome && ObservationSeconds >= 1.5;
	}

	static bool RestoreReceipt(bool bSavedReceipt, FName SavedMissionID, FName ActiveMissionID, bool bRevenueMission)
	{
		return bSavedReceipt && bRevenueMission && !ActiveMissionID.IsNone() && SavedMissionID == ActiveMissionID;
	}
};
