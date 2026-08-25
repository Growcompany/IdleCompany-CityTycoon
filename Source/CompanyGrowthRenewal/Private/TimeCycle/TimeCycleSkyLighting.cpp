#include "TimeCycle/TimeCycleSkyLighting.h"

#include "Math/UnrealMathUtility.h"

bool TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(
	float RawSolarPitch,
	bool bPreviouslyEnabled)
{
	const float RawSunHeight = -FMath::Sin(FMath::DegreesToRadians(RawSolarPitch));
	if (bPreviouslyEnabled)
	{
		return RawSunHeight > 0.0f;
	}

	const float EnableHeight = FMath::Sin(FMath::DegreesToRadians(0.5f));
	return RawSunHeight >= EnableHeight;
}

TimeCycleSkyLighting::FState TimeCycleSkyLighting::Calculate(const FParameters& Parameters)
{
	FState State;

	const float DawnBlend = FMath::Max(0.01f, Parameters.DawnBlendHours);
	const float DuskBlend = FMath::Max(0.01f, Parameters.DuskBlendHours);
	const float DawnStart = Parameters.SunRiseHour - DawnBlend;
	const float DuskStart = Parameters.SunSetHour - DuskBlend;

	float SolarTemperature = Parameters.TransitionDirectionalTemperature;
	if (Parameters.CurrentHour >= DawnStart && Parameters.CurrentHour < Parameters.SunRiseHour)
	{
		const float PhaseAlpha = (Parameters.CurrentHour - DawnStart) / DawnBlend;
		State.DayAlpha = PhaseAlpha;
		State.WarmWeight = FMath::Sin(PhaseAlpha * PI);
		SolarTemperature = FMath::Lerp(
			Parameters.TransitionDirectionalTemperature,
			Parameters.DayDirectionalTemperature,
			PhaseAlpha);
	}
	else if (Parameters.CurrentHour >= Parameters.SunRiseHour
		&& Parameters.CurrentHour < DuskStart)
	{
		State.DayAlpha = 1.0f;
		SolarTemperature = Parameters.DayDirectionalTemperature;
	}
	else if (Parameters.CurrentHour >= DuskStart
		&& Parameters.CurrentHour < Parameters.SunSetHour)
	{
		const float PhaseAlpha = (Parameters.CurrentHour - DuskStart) / DuskBlend;
		State.DayAlpha = 1.0f - PhaseAlpha;
		State.WarmWeight = FMath::Sin(PhaseAlpha * PI);
		SolarTemperature = FMath::Lerp(
			Parameters.DayDirectionalTemperature,
			Parameters.TransitionDirectionalTemperature,
			PhaseAlpha);
	}

	State.DayAlpha = FMath::Clamp(State.DayAlpha, 0.0f, 1.0f);
	State.SolarIntensity = Parameters.SunMaxIntensity * State.DayAlpha;
	const float FillIntensity = Parameters.NightDirectionalIntensity * (1.0f - State.DayAlpha);
	State.DirectionalIntensity = State.SolarIntensity + FillIntensity;
	State.DirectionalTemperature = State.DirectionalIntensity > KINDA_SMALL_NUMBER
		? (State.SolarIntensity * SolarTemperature
			+ FillIntensity * Parameters.NightDirectionalTemperature)
			/ State.DirectionalIntensity
		: SolarTemperature;
	State.DirectionalPitch = -FMath::Abs(FMath::UnwindDegrees(Parameters.SolarPitch));

	State.SkyIntensity = FMath::Lerp(
		Parameters.SkyNightIntensity,
		Parameters.SkyDayIntensity,
		State.DayAlpha);
	State.ExposureBias = FMath::Lerp(
		Parameters.NightExposureBias,
		Parameters.DayExposureBias,
		State.DayAlpha);

	const FLinearColor PhaseAmbient = FMath::Lerp(
		Parameters.NightAmbientColor,
		Parameters.BaseSkyColor,
		State.DayAlpha);
	const float DuskColorAlpha = FMath::Clamp(
		State.WarmWeight * Parameters.DuskAmbientStrength,
		0.0f,
		1.0f);
	State.SkyColor = FMath::Lerp(PhaseAmbient, Parameters.DuskAmbientColor, DuskColorAlpha);

	return State;
}
