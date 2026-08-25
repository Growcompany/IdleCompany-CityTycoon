#pragma once

#include "CoreMinimal.h"

struct FTutorialCompletionStateRules
{
	static bool ResolveOnLoad(
		bool bHasSave,
		bool bSavedCompleted,
		bool bStartsWithoutTutorial,
		bool bDevJumpSessionActive)
	{
		if (bDevJumpSessionActive)
		{
			return false;
		}

		if (bStartsWithoutTutorial)
		{
			return true;
		}

		return bHasSave && bSavedCompleted;
	}

	static bool ShouldRestoreOpeningChain(
		bool bHasSave,
		FName SavedMissionID,
		int32 SavedBuildingCount,
		bool bSavedTutorialCompleted)
	{
		return bHasSave
			&& SavedMissionID.IsNone()
			&& SavedBuildingCount == 0
			&& !bSavedTutorialCompleted;
	}

	static bool ResolveDevJumpSessionActive(
		bool bWasActive,
		bool bJumpApplied,
		bool bReachedTerminalCompletion)
	{
		return !bReachedTerminalCompletion && (bWasActive || bJumpApplied);
	}
};
