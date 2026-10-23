#include "TimeCycle/TimeCycleSkyLighting.h"

#include "Math/Rotator.h"
#include "Math/UnrealMathUtility.h"

float TimeCycleSkyLighting::NormalizeSunAzimuth(float SunAzimuth)
{
	return FRotator::NormalizeAxis(SunAzimuth);
}

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

// if/else 위상 체인은 전환 구간이 0시/24시를 넘지 않을 때만 성립한다.
float TimeCycleSkyLighting::ClampDuskTailHours(float PostSunsetTwilightHours, float SunSetHour)
{
	return FMath::Clamp(PostSunsetTwilightHours, 0.0f, FMath::Max(0.0f, 23.99f - SunSetHour));
}

float TimeCycleSkyLighting::ClampDawnLeadHours(
	float PreSunriseTwilightHours,
	float SunRiseHour,
	float DawnBlendHours)
{
	return FMath::Clamp(
		PreSunriseTwilightHours,
		0.0f,
		FMath::Max(0.0f, SunRiseHour - FMath::Max(0.01f, DawnBlendHours)));
}

TimeCycleSkyLighting::FState TimeCycleSkyLighting::Calculate(const FParameters& Parameters)
{
	FState State;

	const float DawnBlend = FMath::Max(0.01f, Parameters.DawnBlendHours);
	const float DuskBlend = FMath::Max(0.01f, Parameters.DuskBlendHours);
	const float DawnLead = ClampDawnLeadHours(
		Parameters.PreSunriseTwilightHours,
		Parameters.SunRiseHour,
		DawnBlend);
	const float DuskTail = ClampDuskTailHours(
		Parameters.PostSunsetTwilightHours,
		Parameters.SunSetHour);
	const float DawnSpan = DawnBlend + DawnLead;
	const float DuskSpan = DuskBlend + DuskTail;
	const float DawnStart = Parameters.SunRiseHour - DawnSpan;
	const float DuskStart = Parameters.SunSetHour - DuskBlend;
	const float DuskEnd = Parameters.SunSetHour + DuskTail;

	const float WarmApex = FMath::Clamp(Parameters.TwilightWarmApex, 0.01f, 0.99f);
	// apex 에서 1, 양 끝에서 0. apex=0.5 면 sin(PI*t) 와 항등이라 기존 룩이 그대로 보존된다.
	auto WarmBump = [WarmApex](float Phase)
	{
		return Phase <= WarmApex
			? FMath::Sin(HALF_PI * (Phase / WarmApex))
			: FMath::Cos(HALF_PI * ((Phase - WarmApex) / (1.0f - WarmApex)));
	};
	const bool bSmooth = Parameters.bSmoothTwilightRamp;
	auto Ramp = [bSmooth](float Phase)
	{
		return bSmooth ? FMath::SmoothStep(0.0f, 1.0f, Phase) : Phase;
	};

	float SolarTemperature = Parameters.TransitionDirectionalTemperature;
	float DuskDirectWeight = 0.0f;
	if (Parameters.CurrentHour >= DawnStart && Parameters.CurrentHour < Parameters.SunRiseHour)
	{
		const float PhaseAlpha = (Parameters.CurrentHour - DawnStart) / DawnSpan;
		State.DayAlpha = Ramp(PhaseAlpha);
		State.WarmWeight = WarmBump(PhaseAlpha);
		SolarTemperature = FMath::Lerp(
			Parameters.TransitionDirectionalTemperature,
			Parameters.DayDirectionalTemperature,
			State.DayAlpha);
	}
	else if (Parameters.CurrentHour >= Parameters.SunRiseHour
		&& Parameters.CurrentHour < DuskStart)
	{
		State.DayAlpha = 1.0f;
		SolarTemperature = Parameters.DayDirectionalTemperature;
	}
	else if (Parameters.CurrentHour >= DuskStart
		&& Parameters.CurrentHour < DuskEnd)
	{
		const float PhaseAlpha = (Parameters.CurrentHour - DuskStart) / DuskSpan;
		const float Progress = Ramp(PhaseAlpha);
		State.DayAlpha = 1.0f - Progress;
		State.WarmWeight = WarmBump(PhaseAlpha);
		DuskDirectWeight = State.WarmWeight;
		SolarTemperature = FMath::Lerp(
			Parameters.DayDirectionalTemperature,
			Parameters.TransitionDirectionalTemperature,
			Progress);
	}

	State.DayAlpha = FMath::Clamp(State.DayAlpha, 0.0f, 1.0f);
	State.SolarIntensity = Parameters.SunMaxIntensity * State.DayAlpha
		* FMath::Lerp(1.0f, FMath::Max(0.0f, Parameters.DuskDirectionalIntensityScale), DuskDirectWeight);
	float FillWeight = 1.0f;
	if (Parameters.bContinuousAtmosphereTwilight)
	{
		if (State.DayAlpha > 0.0f)
		{
			FillWeight = FMath::SmoothStep(0.0f, 0.5f, State.DayAlpha);
		}
		else
		{
			const float NightSpan = 24.0f - DuskEnd + DawnStart;
			const float FadeHours = FMath::Max(0.01f,
				FMath::Min(0.5f * FMath::Min(DawnBlend, DuskBlend), 0.5f * NightSpan));
			const float HoursSinceDusk = Parameters.CurrentHour >= DuskEnd
				? Parameters.CurrentHour - DuskEnd
				: Parameters.CurrentHour + 24.0f - DuskEnd;
			const float HoursUntilDawn = Parameters.CurrentHour <= DawnStart
				? DawnStart - Parameters.CurrentHour
				: DawnStart + 24.0f - Parameters.CurrentHour;
			FillWeight = FMath::SmoothStep(0.0f, FadeHours, FMath::Min(HoursSinceDusk, HoursUntilDawn));
		}
	}
	const float FillIntensity = Parameters.NightDirectionalIntensity * (1.0f - State.DayAlpha) * FillWeight;
	State.DirectionalIntensity = State.SolarIntensity + FillIntensity;
	State.DirectionalTemperature = State.DirectionalIntensity > KINDA_SMALL_NUMBER
		? (State.SolarIntensity * SolarTemperature
			+ FillIntensity * Parameters.NightDirectionalTemperature)
			/ State.DirectionalIntensity
		: SolarTemperature;
	const float FoldedSolarPitch = FMath::Abs(FMath::UnwindDegrees(Parameters.SolarPitch));
	const float MinimumDuskElevation = FMath::Clamp(Parameters.DuskMinimumDirectionalElevation, 0.0f, 90.0f);
	State.DirectionalPitch = -FMath::Lerp(
		FoldedSolarPitch,
		FMath::Clamp(FoldedSolarPitch, MinimumDuskElevation, 180.0f - MinimumDuskElevation),
		DuskDirectWeight);
	State.AtmospherePitch = Parameters.SolarPitch;
	if (DuskDirectWeight > 0.0f && MinimumDuskElevation > 0.0f)
	{
		// 대기 투과율도 같은 최소 고도를 사용해야 지평선 아래에서 직접광이 소실되지 않는다.
		State.AtmospherePitch = FMath::Max(
			Parameters.SolarPitch, -180.0f + MinimumDuskElevation * DuskDirectWeight);
	}
	State.DirectionalYaw = NormalizeSunAzimuth(Parameters.SunAzimuth);

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
