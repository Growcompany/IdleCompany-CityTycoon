#pragma once

#include "Math/Color.h"

namespace TimeCycleSkyLighting
{
	struct FParameters
	{
		float CurrentHour = 0.0f;
		float SunRiseHour = 7.0f;
		float SunSetHour = 19.0f;
		float DawnBlendHours = 2.0f;
		float DuskBlendHours = 2.0f;

		float SunMaxIntensity = 0.8f;
		float NightDirectionalIntensity = 0.0f;
		float DayDirectionalTemperature = 6500.0f;
		float TransitionDirectionalTemperature = 1800.0f;
		float NightDirectionalTemperature = 9000.0f;
		float SolarPitch = 0.0f;

		float SkyNightIntensity = 0.2f;
		float SkyDayIntensity = 0.5f;
		FLinearColor NightAmbientColor = FLinearColor(FColor(213, 213, 213));
		FLinearColor BaseSkyColor = FLinearColor::White;
		FLinearColor DuskAmbientColor = FLinearColor(1.0f, 0.42f, 0.16f);
		float DuskAmbientStrength = 0.7f;

		float NightExposureBias = 0.5f;
		float DayExposureBias = -0.15f;
	};

	struct FState
	{
		float SolarIntensity = 0.0f;
		float DirectionalIntensity = 0.0f;
		float DirectionalTemperature = 0.0f;
		float DirectionalPitch = 0.0f;
		float DayAlpha = 0.0f;
		float WarmWeight = 0.0f;
		float SkyIntensity = 0.0f;
		FLinearColor SkyColor = FLinearColor::Black;
		float ExposureBias = 0.0f;
	};

	FState Calculate(const FParameters& Parameters);
	bool ShouldEnableAtmosphereSunLight(float RawSolarPitch, bool bPreviouslyEnabled);
}
