#pragma once

#include "CoreMinimal.h"

enum class ETutorialMissionCompletionMode : uint8
{
	ImmediateAdvance,
	ManualClaim
};

struct FTutorialMissionCompletionRules
{
	static ETutorialMissionCompletionMode Resolve(bool bManualClaim)
	{
		return bManualClaim
			? ETutorialMissionCompletionMode::ManualClaim
			: ETutorialMissionCompletionMode::ImmediateAdvance;
	}
};
