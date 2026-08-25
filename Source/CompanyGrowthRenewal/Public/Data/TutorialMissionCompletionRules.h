#pragma once

#include "CoreMinimal.h"

enum class ETutorialMissionCompletionMode : uint8
{
	ImmediateAdvance,
	FinalClaim
};

struct FTutorialMissionCompletionRules
{
	static ETutorialMissionCompletionMode Resolve(bool bHasNextMission)
	{
		return bHasNextMission
			? ETutorialMissionCompletionMode::ImmediateAdvance
			: ETutorialMissionCompletionMode::FinalClaim;
	}
};
