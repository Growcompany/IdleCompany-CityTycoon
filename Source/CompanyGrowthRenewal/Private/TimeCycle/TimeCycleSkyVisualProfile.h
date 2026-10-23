#pragma once

#include "Math/Color.h"

enum class EMainMapVisualPhase : uint8
{
	Dawn,
	Day,
	Dusk,
	Night
};

enum class EMainMapLutRole : uint8
{
	L2,
	DCL2
};

struct FTimeCycleSkyVisualProfile
{
	EMainMapVisualPhase Phase;
	EMainMapLutRole LutRole;
	float LutIntensity;
	FLinearColor Saturation;
	FLinearColor Contrast;
	FLinearColor Gain;
	FLinearColor MidtoneSaturation;
	FLinearColor MidtoneGain;
	float Sharpen;
	float BloomIntensity;
	float BloomThreshold;
	float ShadowsMax;
	float HighlightsMin;
};

namespace TimeCycleSkyVisualProfile
{
	FTimeCycleSkyVisualProfile Calculate(
		float CurrentHour,
		float SunRiseHour,
		float SunSetHour,
		float DawnBlendHours,
		float DuskBlendHours);
}
