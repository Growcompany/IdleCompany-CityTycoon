#pragma once

struct FGoalBoardLoadEvaluationDecision
{
	bool bScheduleSaveForNextTick = false;
	bool bBroadcastBoardChanged = false;
};

struct FGoalBoardLoadEvaluationRules
{
	static FGoalBoardLoadEvaluationDecision Resolve(
		bool bEvaluationDirty,
		bool bSaveAttempted,
		bool bSaveSucceeded)
	{
		if (!bEvaluationDirty)
		{
			return { false, true };
		}

		if (!bSaveAttempted)
		{
			return { true, false };
		}

		return { false, bSaveSucceeded };
	}
};
