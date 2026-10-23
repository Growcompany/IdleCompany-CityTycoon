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

	static bool AreRewardsGrantable(
		const TArray<FMissionReward>& Rewards,
		bool bResourceManagerAvailable,
		bool bItemManagerAvailable)
	{
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
		bool bRequiresDurableCommit,
		bool bSaveSucceeded)
	{
		FTutorialRewardBroadcastDecision Decision;
		Decision.bBroadcastDuringPrepare = !bRequiresDurableCommit;
		Decision.bPublishRewardsAfterCommit = bRequiresDurableCommit && bSaveSucceeded;
		return Decision;
	}

	static FTutorialMissionSaveDecision ResolveSaveDecision(
		bool bRequiresDurableCommit,
		bool bSaveSucceeded)
	{
		FTutorialMissionSaveDecision Decision;
		Decision.bCommitPreparedState = bSaveSucceeded || !bRequiresDurableCommit;
		Decision.bRestoreReadyToClaim = bRequiresDurableCommit && !bSaveSucceeded;
		Decision.bPublishCompletionEvents = Decision.bCommitPreparedState;
		return Decision;
	}
};
