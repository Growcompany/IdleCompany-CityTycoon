#pragma once

#include "Table/MissionTable.h"

struct FMissionTrackerRewardDisplayState
{
	bool bShowSection = false;
	FText Label;
};

struct FMissionTrackerDisplayRules
{
	static FMissionTrackerRewardDisplayState ResolveRewardDisplay(const FMissionTable& Mission)
	{
		FMissionTrackerRewardDisplayState State;
		State.bShowSection = Mission.NextMissionID.IsNone();
		State.Label = State.bShowSection
			? NSLOCTEXT("MissionTracker", "TutorialCompletionReward", "튜토리얼 완료 보상")
			: FText::GetEmpty();
		return State;
	}

	static bool ShouldPresentRewards(const FMissionTable& Mission)
	{
		return ResolveRewardDisplay(Mission).bShowSection;
	}
};
