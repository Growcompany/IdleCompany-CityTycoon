#pragma once

#include "CoreMinimal.h"
#include "Table/MissionTable.h"

struct FTutorialMissionSaveDecision
{
	bool bCommitPreparedState = false;
	bool bRestoreReadyToClaim = true;
	bool bPublishCompletionEvents = false;
};

struct FTutorialRewardBroadcastDecision
{
	bool bBroadcastDuringPrepare = true;
	bool bBroadcastDuringRollback = false;
	bool bPublishRewardsAfterCommit = false;
};

struct FTutorialMissionAtomicCommitRules
{
	static bool ShouldPrepareGoalBoardUnlock(FName NextMissionID)
	{
		return NextMissionID.IsNone();
	}

	static bool ShouldMarkTutorialCompleted(FName NextMissionID)
	{
		return NextMissionID.IsNone();
	}

	static bool AreAllFinalRewardsGrantable(
		const TArray<FMissionReward>& Rewards,
		bool bResourceManagerAvailable,
		bool bItemManagerAvailable)
	{
		if (Rewards.IsEmpty())
		{
			return false;
		}

		for (const FMissionReward& Reward : Rewards)
		{
			const bool bHasResourceReward = Reward.ResourceType != EResourceType::None;
			const bool bHasItemReward = Reward.ItemType != EItemType::None;
			const bool bResourceComponentAbsent = !bHasResourceReward && Reward.Amount == 0;
			const bool bItemComponentAbsent = !bHasItemReward && Reward.ItemAmount == 0;
			const bool bCanGrantResource = bResourceManagerAvailable
				&& Reward.ResourceType != EResourceType::None
				&& Reward.Amount > 0;
			const bool bCanGrantItem = bItemManagerAvailable
				&& Reward.ItemType != EItemType::None
				&& Reward.ItemAmount > 0;

			if ((!bResourceComponentAbsent && !bCanGrantResource)
				|| (!bItemComponentAbsent && !bCanGrantItem)
				|| (bResourceComponentAbsent && bItemComponentAbsent))
			{
				return false;
			}
		}
		return true;
	}

	static FTutorialRewardBroadcastDecision ResolveRewardBroadcastDecision(
		bool bIsTerminalMission,
		bool bSaveSucceeded)
	{
		FTutorialRewardBroadcastDecision Decision;
		Decision.bBroadcastDuringPrepare = !bIsTerminalMission;
		Decision.bPublishRewardsAfterCommit = bIsTerminalMission && bSaveSucceeded;
		return Decision;
	}

	static FTutorialMissionSaveDecision ResolveSaveDecision(
		bool bIsTerminalMission,
		bool bSaveSucceeded)
	{
		FTutorialMissionSaveDecision Decision;
		Decision.bCommitPreparedState = bSaveSucceeded || !bIsTerminalMission;
		Decision.bRestoreReadyToClaim = bIsTerminalMission && !bSaveSucceeded;
		Decision.bPublishCompletionEvents = Decision.bCommitPreparedState;
		return Decision;
	}
};
