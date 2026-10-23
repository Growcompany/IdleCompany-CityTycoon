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
		float DuskDirectionalIntensityScale = 1.0f;
		float DuskMinimumDirectionalElevation = 0.0f;

		// 해는 SunSetHour 에 지평선에 닿는다. 이 값이 0이면 가장 붉어야 할 구간이 통째로 없다.
		float PostSunsetTwilightHours = 0.0f;
		float PreSunriseTwilightHours = 0.0f;
		// 전환 구간에서 노을색이 가장 짙어지는 위치(0~1). 0.5 = 대칭 sin 범프.
		float TwilightWarmApex = 0.5f;
		// 밝기 램프 양 끝 기울기를 0으로 만들어 전환의 시작·종료 순간을 지운다.
		bool bSmoothTwilightRamp = false;
		// 대기 태양을 전환할 때 직접광도 0을 통과해 하늘·지표면의 밝기가 연속으로 이어진다.
		bool bContinuousAtmosphereTwilight = false;

		float SunMaxIntensity = 0.8f;
		float NightDirectionalIntensity = 0.0f;
		float DayDirectionalTemperature = 6500.0f;
		float TransitionDirectionalTemperature = 1800.0f;
		float NightDirectionalTemperature = 9000.0f;
		float SolarPitch = 0.0f;
		float SunAzimuth = 0.0f;

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
		float AtmospherePitch = 0.0f;
		float DirectionalYaw = 0.0f;
		float DayAlpha = 0.0f;
		float WarmWeight = 0.0f;
		float SkyIntensity = 0.0f;
		FLinearColor SkyColor = FLinearColor::Black;
		float ExposureBias = 0.0f;
	};

	float NormalizeSunAzimuth(float SunAzimuth);
	FState Calculate(const FParameters& Parameters);
	bool ShouldEnableAtmosphereSunLight(float RawSolarPitch, bool bPreviouslyEnabled);

	// 전환 꼬리를 하루 안으로 가두는 규칙. Calculate 와 정점 시각 계산이 같은 값을 쓰도록
	// 여기 한 곳에만 둔다 — 따로 두면 드리프트하고, 어긋나면 치트가 밤으로 점프한다.
	float ClampDuskTailHours(float PostSunsetTwilightHours, float SunSetHour);
	float ClampDawnLeadHours(float PreSunriseTwilightHours, float SunRiseHour, float DawnBlendHours);
}
