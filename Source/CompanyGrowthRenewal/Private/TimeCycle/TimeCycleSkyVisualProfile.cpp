#include "TimeCycle/TimeCycleSkyVisualProfile.h"

#include "Math/UnrealMathUtility.h"

namespace
{
	constexpr float HoursPerDay = 24.0f;

	FLinearColor UniformColor(float Value)
	{
		return FLinearColor(Value, Value, Value, 1.0f);
	}
}

FTimeCycleSkyVisualProfile TimeCycleSkyVisualProfile::Calculate(
	float CurrentHour,
	float SunRiseHour,
	float SunSetHour,
	float DawnBlendHours,
	float DuskBlendHours)
{
	float NormalizedHour = FMath::Fmod(CurrentHour, HoursPerDay);
	if (NormalizedHour < 0.0f)
	{
		NormalizedHour += HoursPerDay;
	}

	FTimeCycleSkyVisualProfile Profile{};
	Profile.Phase = EMainMapVisualPhase::Night;
	Profile.LutRole = EMainMapLutRole::L2;
	Profile.LutIntensity = 0.70f;
	Profile.Saturation = UniformColor(1.10f);
	Profile.Contrast = UniformColor(1.02f);
	Profile.Gain = UniformColor(1.04f);
	Profile.MidtoneSaturation = FLinearColor::White;
	Profile.MidtoneGain = FLinearColor::White;
	Profile.Sharpen = 0.4f;
	Profile.BloomIntensity = 0.3f;
	Profile.BloomThreshold = 3.0f;
	Profile.ShadowsMax = 0.09f;
	Profile.HighlightsMin = 0.50f;

	const float DawnBlend = FMath::Max(0.01f, DawnBlendHours);
	const float DuskBlend = FMath::Max(0.01f, DuskBlendHours);
	const float DawnStartHour = SunRiseHour - DawnBlend;
	const float DuskStartHour = SunSetHour - DuskBlend;

	if (NormalizedHour >= DawnStartHour && NormalizedHour < SunRiseHour)
	{
		Profile.Phase = EMainMapVisualPhase::Dawn;
		Profile.LutIntensity = 0.80f;
		Profile.Saturation = UniformColor(1.03f);
		Profile.Contrast = UniformColor(1.02f);
		Profile.Gain = FLinearColor(1.025f, 1.0f, 0.96f, 1.0f);
		Profile.MidtoneGain = UniformColor(1.10f);
	}
	else if (NormalizedHour >= SunRiseHour && NormalizedHour < DuskStartHour)
	{
		Profile.Phase = EMainMapVisualPhase::Day;
		Profile.LutRole = EMainMapLutRole::DCL2;
		Profile.LutIntensity = 1.0f;
		Profile.Saturation = FLinearColor::White;
		Profile.Contrast = FLinearColor::White;
		Profile.Gain = FLinearColor::White;
		Profile.MidtoneSaturation = UniformColor(1.15f);
	}
	else if (NormalizedHour >= DuskStartHour && NormalizedHour < SunSetHour)
	{
		Profile.Phase = EMainMapVisualPhase::Dusk;
		Profile.MidtoneSaturation = UniformColor(1.15f);

		const float DuskAlpha = FMath::Clamp(
			(NormalizedHour - DuskStartHour) / DuskBlend,
			0.0f,
			1.0f);
		const FLinearColor WarmApexSaturation = UniformColor(1.04f);
		const FLinearColor WarmApexContrast = UniformColor(1.03f);
		const FLinearColor WarmApexGain(1.04f, 1.0f, 0.94f, 1.0f);

		if (DuskAlpha < 0.5f)
		{
			const float WarmApexAlpha = DuskAlpha * 2.0f;
			Profile.LutRole = EMainMapLutRole::DCL2;
			Profile.LutIntensity = 1.0f - WarmApexAlpha;
			Profile.Saturation = FMath::Lerp(FLinearColor::White, WarmApexSaturation, WarmApexAlpha);
			Profile.Contrast = FMath::Lerp(FLinearColor::White, WarmApexContrast, WarmApexAlpha);
			Profile.Gain = FMath::Lerp(FLinearColor::White, WarmApexGain, WarmApexAlpha);
		}
		else
		{
			const float NightAlpha = (DuskAlpha - 0.5f) * 2.0f;
			Profile.LutRole = EMainMapLutRole::L2;
			Profile.LutIntensity = 0.70f * NightAlpha;
			Profile.Saturation = FMath::Lerp(WarmApexSaturation, UniformColor(1.10f), NightAlpha);
			Profile.Contrast = FMath::Lerp(WarmApexContrast, UniformColor(1.02f), NightAlpha);
			Profile.Gain = FMath::Lerp(WarmApexGain, UniformColor(1.04f), NightAlpha);
		}
	}

	if (Profile.Phase == EMainMapVisualPhase::Night)
	{
		Profile.MidtoneSaturation = UniformColor(1.15f);
	}

	return Profile;
}
