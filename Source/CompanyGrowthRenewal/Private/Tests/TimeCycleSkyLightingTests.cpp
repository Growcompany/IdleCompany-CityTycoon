#include "Misc/AutomationTest.h"
#include "TimeCycle/TimeCycleSkyLighting.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TimeCycleSkyLighting::FParameters MakeN1Parameters(float CurrentHour, float SolarPitch)
	{
		TimeCycleSkyLighting::FParameters Parameters;
		Parameters.CurrentHour = CurrentHour;
		Parameters.SunRiseHour = 7.0f;
		Parameters.SunSetHour = 19.0f;
		Parameters.DawnBlendHours = 2.0f;
		Parameters.DuskBlendHours = 2.0f;
		Parameters.SunMaxIntensity = 0.8f;
		Parameters.NightDirectionalIntensity = 0.08f;
		Parameters.DayDirectionalTemperature = 6500.0f;
		Parameters.TransitionDirectionalTemperature = 1800.0f;
		Parameters.NightDirectionalTemperature = 9000.0f;
		Parameters.SolarPitch = SolarPitch;
		Parameters.SkyNightIntensity = 0.2f;
		Parameters.SkyDayIntensity = 0.5f;
		Parameters.NightAmbientColor = FLinearColor(0.3763f, 0.4564f, 0.5776f);
		Parameters.NightExposureBias = 0.5f;
		Parameters.DayExposureBias = -0.15f;
		return Parameters;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyMidnightLightingTest,
	"CGR.TimeCycleSky.Lighting.MidnightNightFill",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyMidnightLightingTest::RunTest(const FString& Parameters)
{
	const TimeCycleSkyLighting::FState State = TimeCycleSkyLighting::Calculate(
		MakeN1Parameters(0.0f, -270.0f));

	TestEqual(TEXT("자정 태양 강도는 0"), State.SolarIntensity, 0.0f);
	TestEqual(TEXT("자정 방향성 fill 강도"), State.DirectionalIntensity, 0.08f);
	TestEqual(TEXT("자정 DayAlpha는 태양 기준 0"), State.DayAlpha, 0.0f);
	TestEqual(TEXT("자정 SkyLight 강도"), State.SkyIntensity, 0.2f);
	TestEqual(TEXT("자정 노출 bias"), State.ExposureBias, 0.5f);
	TestEqual(TEXT("야간 방향성 색온도"), State.DirectionalTemperature, 9000.0f);
	TestEqual(TEXT("야간 방향은 태양 피치를 위쪽 반구로 접음"), State.DirectionalPitch, -90.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyNoonLightingTest,
	"CGR.TimeCycleSky.Lighting.NoonPreservesDay",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyNoonLightingTest::RunTest(const FString& Parameters)
{
	const TimeCycleSkyLighting::FState State = TimeCycleSkyLighting::Calculate(
		MakeN1Parameters(12.0f, -90.0f));

	TestEqual(TEXT("정오 태양 강도는 기존 낮 값 보존"), State.SolarIntensity, 0.8f);
	TestEqual(TEXT("정오 방향성 강도는 태양 강도"), State.DirectionalIntensity, 0.8f);
	TestEqual(TEXT("정오 DayAlpha"), State.DayAlpha, 1.0f);
	TestEqual(TEXT("정오 SkyLight 강도는 기존 낮 값 보존"), State.SkyIntensity, 0.5f);
	TestEqual(TEXT("정오 노출 bias는 기존 낮 값 보존"), State.ExposureBias, -0.15f);
	TestEqual(TEXT("낮 방향성 색온도"), State.DirectionalTemperature, 6500.0f);
	TestEqual(TEXT("낮 방향은 태양 피치를 위쪽 반구로 접음"), State.DirectionalPitch, -90.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyHalfDuskLightingTest,
	"CGR.TimeCycleSky.Lighting.HalfDuskUsesSolarAlpha",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyHalfDuskLightingTest::RunTest(const FString& Parameters)
{
	const TimeCycleSkyLighting::FState State = TimeCycleSkyLighting::Calculate(
		MakeN1Parameters(18.0f, -135.0f));

	TestEqual(TEXT("황혼 절반 태양 강도"), State.SolarIntensity, 0.4f);
	TestEqual(TEXT("황혼 절반 DayAlpha는 night fill이 아닌 태양 기준"), State.DayAlpha, 0.5f);
	TestEqual(TEXT("황혼 절반 방향성 강도는 낮·밤 연속 보간"), State.DirectionalIntensity, 0.44f);
	TestEqual(
		TEXT("황혼 절반 태양 색온도 4150K를 N1 fill과 강도 가중"),
		State.DirectionalTemperature,
		4590.909f,
		0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyZeroFillTemperatureTest,
	"CGR.TimeCycleSky.Lighting.ZeroFillPreservesSolarTemperature",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyZeroFillTemperatureTest::RunTest(const FString& Parameters)
{
	TimeCycleSkyLighting::FParameters HalfDawnParameters = MakeN1Parameters(6.0f, -45.0f);
	HalfDawnParameters.NightDirectionalIntensity = 0.0f;
	TimeCycleSkyLighting::FParameters HalfDuskParameters = MakeN1Parameters(18.0f, -135.0f);
	HalfDuskParameters.NightDirectionalIntensity = 0.0f;

	const TimeCycleSkyLighting::FState HalfDawnState =
		TimeCycleSkyLighting::Calculate(HalfDawnParameters);
	const TimeCycleSkyLighting::FState HalfDuskState =
		TimeCycleSkyLighting::Calculate(HalfDuskParameters);

	TestEqual(TEXT("fill 0 여명 절반은 기존 태양 색온도 4150K 보존"), HalfDawnState.DirectionalTemperature, 4150.0f);
	TestEqual(TEXT("fill 0 황혼 절반은 기존 태양 색온도 4150K 보존"), HalfDuskState.DirectionalTemperature, 4150.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyWeightedFillTemperatureTest,
	"CGR.TimeCycleSky.Lighting.NightFillUsesFiniteIntensityWeightedTemperature",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyWeightedFillTemperatureTest::RunTest(const FString& Parameters)
{
	TimeCycleSkyLighting::FParameters N1Parameters = MakeN1Parameters(18.0f, -135.0f);
	TimeCycleSkyLighting::FParameters N2Parameters = N1Parameters;
	N2Parameters.NightDirectionalIntensity = 0.12f;

	const TimeCycleSkyLighting::FState N1State = TimeCycleSkyLighting::Calculate(N1Parameters);
	const TimeCycleSkyLighting::FState N2State = TimeCycleSkyLighting::Calculate(N2Parameters);

	TestTrue(TEXT("N1 황혼 색온도는 finite"), FMath::IsFinite(N1State.DirectionalTemperature));
	TestTrue(TEXT("N2 황혼 색온도는 finite"), FMath::IsFinite(N2State.DirectionalTemperature));
	TestEqual(TEXT("N1 황혼 강도 가중 색온도"), N1State.DirectionalTemperature, 4590.909f, 0.01f);
	TestEqual(TEXT("N2 황혼 강도 가중 색온도"), N2State.DirectionalTemperature, 4782.609f, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyNightFillIsolationTest,
	"CGR.TimeCycleSky.Lighting.NightFillDoesNotAffectSolarOutputs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyNightFillIsolationTest::RunTest(const FString& Parameters)
{
	TimeCycleSkyLighting::FParameters N1Parameters = MakeN1Parameters(0.0f, -270.0f);
	TimeCycleSkyLighting::FParameters BrighterFillParameters = N1Parameters;
	BrighterFillParameters.NightDirectionalIntensity = 0.12f;

	const TimeCycleSkyLighting::FState N1State = TimeCycleSkyLighting::Calculate(N1Parameters);
	const TimeCycleSkyLighting::FState BrighterFillState = TimeCycleSkyLighting::Calculate(BrighterFillParameters);

	TestEqual(TEXT("N1 자정 방향성 fill"), N1State.DirectionalIntensity, 0.08f);
	TestEqual(TEXT("상향된 자정 방향성 fill"), BrighterFillState.DirectionalIntensity, 0.12f);
	TestEqual(TEXT("N1 fill에서 자정 DayAlpha 보존"), N1State.DayAlpha, 0.0f);
	TestEqual(TEXT("밝은 fill에서도 자정 DayAlpha 보존"), BrighterFillState.DayAlpha, 0.0f);
	TestEqual(TEXT("N1 fill에서 자정 SkyLight 보존"), N1State.SkyIntensity, 0.2f);
	TestEqual(TEXT("밝은 fill에서도 자정 SkyLight 보존"), BrighterFillState.SkyIntensity, 0.2f);
	TestEqual(TEXT("N1 fill에서 자정 노출 보존"), N1State.ExposureBias, 0.5f);
	TestEqual(TEXT("밝은 fill에서도 자정 노출 보존"), BrighterFillState.ExposureBias, 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyAtmosphereGateTest,
	"CGR.TimeCycleSky.Lighting.AtmosphereUsesRawSunHeightHysteresis",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyAtmosphereGateTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("정오 raw pitch는 꺼진 atmosphere 태양을 활성화"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-90.0f, false));
	TestFalse(
		TEXT("자정 raw pitch는 켜진 atmosphere 태양을 비활성화"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-270.0f, true));

	const TimeCycleSkyLighting::FState DawnState = TimeCycleSkyLighting::Calculate(
		MakeN1Parameters(5.5f, -340.714f));
	TestTrue(TEXT("05:30은 여명 DayAlpha가 이미 0보다 큼"), DawnState.DayAlpha > 0.0f);
	TestFalse(
		TEXT("05:30 raw 태양은 지평선 아래라 atmosphere 비활성"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-340.714f, false));

	TestFalse(
		TEXT("지평선에서는 이전 활성 상태도 비활성"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-180.0f, true));
	TestTrue(
		TEXT("0~0.5도 deadband는 이전 활성 상태 유지"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-0.25f, true));
	TestFalse(
		TEXT("0~0.5도 deadband는 이전 비활성 상태 유지"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-0.25f, false));
	TestTrue(
		TEXT("0.5도 초과 고도는 atmosphere 활성"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(-0.6f, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkySunAzimuthTest,
	"CGR.TimeCycleSky.Lighting.SunAzimuthPreservedForDirectionalRotation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkySunAzimuthTest::RunTest(const FString& Parameters)
{
	TimeCycleSkyLighting::FParameters LightingParameters = MakeN1Parameters(12.0f, -90.0f);
	LightingParameters.SunAzimuth = 35.0f;

	const TimeCycleSkyLighting::FState State =
		TimeCycleSkyLighting::Calculate(LightingParameters);

	TestEqual(
		TEXT("35도 태양 방위각은 DirectionalLight 회전용 상태에 보존"),
		State.DirectionalYaw,
		35.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkySunAzimuthNormalizationTest,
	"CGR.TimeCycleSky.Lighting.SunAzimuthNormalizationIsFiniteAndBounded",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkySunAzimuthNormalizationTest::RunTest(const FString& Parameters)
{
	const float WrappedPositive = TimeCycleSkyLighting::NormalizeSunAzimuth(395.0f);
	const float WrappedNegative = TimeCycleSkyLighting::NormalizeSunAzimuth(-325.0f);
	const float ExtremeFinite = TimeCycleSkyLighting::NormalizeSunAzimuth(FLT_MAX);

	TestEqual(TEXT("395도는 35도로 정규화"), WrappedPositive, 35.0f);
	TestEqual(TEXT("-325도는 35도로 정규화"), WrappedNegative, 35.0f);
	TestTrue(TEXT("FLT_MAX 정규화 결과는 finite"), FMath::IsFinite(ExtremeFinite));
	TestTrue(
		TEXT("FLT_MAX 정규화 결과는 -180~180도 범위"),
		ExtremeFinite >= -180.0f && ExtremeFinite <= 180.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyWarmApexIdentityTest,
	"CGR.TimeCycleSky.Lighting.DefaultWarmApexMatchesLegacySineBump",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyWarmApexIdentityTest::RunTest(const FString& Parameters)
{
	// apex 기본값 0.5 는 구 sin(PI*t) 범프와 항등이어야 한다 — MainMap 배치 룩 보존의 근거.
	static const float Phases[] = { 0.1f, 0.25f, 0.5f, 0.75f, 0.9f };
	for (const float Phase : Phases)
	{
		const TimeCycleSkyLighting::FState State = TimeCycleSkyLighting::Calculate(
			MakeN1Parameters(17.0f + 2.0f * Phase, -135.0f));
		TestEqual(
			*FString::Printf(TEXT("황혼 위상 %.2f 노을 가중치는 sin(PI*t)"), Phase),
			State.WarmWeight,
			FMath::Sin(Phase * PI),
			0.0005f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyPostSunsetTwilightTest,
	"CGR.TimeCycleSky.Lighting.PostSunsetTwilightPeaksAtHorizon",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyPostSunsetTwilightTest::RunTest(const FString& Parameters)
{
	auto MakeLongDusk = [](float CurrentHour)
	{
		TimeCycleSkyLighting::FParameters Long = MakeN1Parameters(CurrentHour, -180.0f);
		Long.DuskBlendHours = 3.0f;
		Long.PostSunsetTwilightHours = 1.5f;
		Long.TwilightWarmApex = 3.0f / 4.5f;  // 일몰 시각이 전환 구간에서 차지하는 위치
		Long.bSmoothTwilightRamp = true;
		return Long;
	};

	const TimeCycleSkyLighting::FState Legacy = TimeCycleSkyLighting::Calculate(
		MakeN1Parameters(19.0f, -180.0f));
	TestEqual(TEXT("종전 설정은 일몰 순간 이미 완전한 밤"), Legacy.DayAlpha, 0.0f);
	TestEqual(TEXT("종전 설정은 일몰 순간 노을색이 없음"), Legacy.WarmWeight, 0.0f);

	const TimeCycleSkyLighting::FState AtSunset =
		TimeCycleSkyLighting::Calculate(MakeLongDusk(19.0f));
	TestEqual(TEXT("노을색은 해가 지평선에 닿는 일몰에 최댓값"), AtSunset.WarmWeight, 1.0f, 0.0005f);
	TestEqual(TEXT("일몰 시점에도 낮 빛이 남아 있다"), AtSunset.DayAlpha, 0.259259f, 0.001f);
	TestTrue(TEXT("일몰 SkyLight 강도는 밤보다 밝다"), AtSunset.SkyIntensity > 0.2f);

	const TimeCycleSkyLighting::FState DeepTwilight =
		TimeCycleSkyLighting::Calculate(MakeLongDusk(20.4f));
	TestTrue(TEXT("박명 끝자락은 아직 밤이 아니다"), DeepTwilight.DayAlpha > 0.0f);
	TestTrue(
		TEXT("박명 끝자락 노을색은 일몰보다 옅다"),
		DeepTwilight.WarmWeight < AtSunset.WarmWeight);

	const TimeCycleSkyLighting::FState Night =
		TimeCycleSkyLighting::Calculate(MakeLongDusk(20.5f));
	TestEqual(TEXT("박명이 끝나면 완전한 밤"), Night.DayAlpha, 0.0f);
	TestEqual(TEXT("박명이 끝나면 노을색 없음"), Night.WarmWeight, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkySmoothRampTest,
	"CGR.TimeCycleSky.Lighting.SmoothRampFlattensTransitionEdges",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkySmoothRampTest::RunTest(const FString& Parameters)
{
	TimeCycleSkyLighting::FParameters Smooth = MakeN1Parameters(17.2f, -150.0f);
	Smooth.bSmoothTwilightRamp = true;
	TimeCycleSkyLighting::FParameters Linear = MakeN1Parameters(17.2f, -150.0f);

	const TimeCycleSkyLighting::FState SmoothState = TimeCycleSkyLighting::Calculate(Smooth);
	const TimeCycleSkyLighting::FState LinearState = TimeCycleSkyLighting::Calculate(Linear);

	// 전환 시작 직후에는 완만한 램프가 선형보다 덜 어두워야 한다(기울기 0에서 출발).
	TestTrue(
		TEXT("전환 시작 직후 완만한 램프가 선형보다 밝다"),
		SmoothState.DayAlpha > LinearState.DayAlpha);

	TimeCycleSkyLighting::FParameters SmoothMid = MakeN1Parameters(18.0f, -135.0f);
	SmoothMid.bSmoothTwilightRamp = true;
	TestEqual(
		TEXT("전환 중앙은 완만/선형이 같다"),
		TimeCycleSkyLighting::Calculate(SmoothMid).DayAlpha,
		0.5f,
		0.0005f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyTwilightMidnightGuardTest,
	"CGR.TimeCycleSky.Lighting.TwilightTailStaysInsideSameDay",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyTwilightMidnightGuardTest::RunTest(const FString& Parameters)
{
	// 꼬리가 자정을 넘으면 if/else 위상 체인이 깨지므로 하루 안으로 잘려야 한다.
	auto MakeLateSunset = [](float CurrentHour)
	{
		TimeCycleSkyLighting::FParameters Late = MakeN1Parameters(CurrentHour, -180.0f);
		Late.SunSetHour = 23.5f;
		Late.PostSunsetTwilightHours = 3.0f;
		return Late;
	};

	TestTrue(
		TEXT("자정 직전은 아직 박명"),
		TimeCycleSkyLighting::Calculate(MakeLateSunset(23.9f)).DayAlpha > 0.0f);
	TestEqual(
		TEXT("자정 이후는 밤으로 떨어진다"),
		TimeCycleSkyLighting::Calculate(MakeLateSunset(0.5f)).DayAlpha,
		0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyAtmosphereZeroIntensityTransitionsTest,
	"CGR.TimeCycleSky.Lighting.AtmosphereTransitionsAtZeroIntensity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyAtmosphereZeroIntensityTransitionsTest::RunTest(const FString& Parameters)
{
	auto StateAt = [](float Hour)
	{
		TimeCycleSkyLighting::FParameters Lighting = MakeN1Parameters(Hour, -270.0f);
		Lighting.DuskBlendHours = 3.0f;
		Lighting.PostSunsetTwilightHours = 1.5f;
		Lighting.bSmoothTwilightRamp = true;
		Lighting.bContinuousAtmosphereTwilight = true;
		return TimeCycleSkyLighting::Calculate(Lighting);
	};

	for (float BoundaryHour : {5.0f, 20.5f})
	{
		TestEqual(TEXT("atmosphere registration boundary has no directional light"),
			StateAt(BoundaryHour).DirectionalIntensity, 0.0f);
		TestTrue(TEXT("directional light approaches zero before the boundary"),
			StateAt(BoundaryHour - 0.001f).DirectionalIntensity < 0.0001f);
		TestTrue(TEXT("directional light leaves zero continuously after the boundary"),
			StateAt(BoundaryHour + 0.001f).DirectionalIntensity < 0.0001f);
	}

	TestEqual(TEXT("deep night restores the authored night fill"), StateAt(0.0f).DirectionalIntensity, 0.08f);
	TestEqual(TEXT("full daylight preserves the authored sun intensity"), StateAt(12.0f).DirectionalIntensity, 0.8f);
	TestTrue(TEXT("night fill fades in after dusk"),
		StateAt(20.75f).DirectionalIntensity < StateAt(21.0f).DirectionalIntensity);
	TestTrue(TEXT("night fill fades out before dawn"),
		StateAt(4.75f).DirectionalIntensity < StateAt(4.5f).DirectionalIntensity);
	TestTrue(TEXT("sunset stays inside the post-sunset twilight"), StateAt(19.0f).DayAlpha > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTimeCycleSkyDuskDirectLightTest,
	"CGR.TimeCycleSky.Lighting.DuskDirectLightPreservesOtherPhases",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTimeCycleSkyDuskDirectLightTest::RunTest(const FString& Parameters)
{
	auto MakeTwilight = [](float Hour, float SolarPitch)
	{
		TimeCycleSkyLighting::FParameters Lighting = MakeN1Parameters(Hour, SolarPitch);
		Lighting.DuskBlendHours = 3.0f;
		Lighting.PostSunsetTwilightHours = 1.5f;
		Lighting.TwilightWarmApex = 3.0f / 4.5f;
		Lighting.bSmoothTwilightRamp = true;
		Lighting.bContinuousAtmosphereTwilight = true;
		return Lighting;
	};

	TimeCycleSkyLighting::FParameters DefaultDusk = MakeTwilight(19.0f, -180.0f);
	TimeCycleSkyLighting::FParameters BrighterDusk = DefaultDusk;
	BrighterDusk.DuskDirectionalIntensityScale = 1.5f;
	BrighterDusk.DuskMinimumDirectionalElevation = 10.0f;
	const TimeCycleSkyLighting::FState Baseline = TimeCycleSkyLighting::Calculate(DefaultDusk);
	const TimeCycleSkyLighting::FState Brighter = TimeCycleSkyLighting::Calculate(BrighterDusk);

	TestEqual(TEXT("기본 황혼은 지평선 피치 유지"), Baseline.DirectionalPitch, -180.0f);
	TestEqual(TEXT("기본 황혼의 대기 방향은 실제 태양 유지"), Baseline.AtmospherePitch, DefaultDusk.SolarPitch);
	TestEqual(TEXT("황혼 정점은 지면을 비추는 최소 고도 사용"), Brighter.DirectionalPitch, -170.0f, 0.001f);
	TestTrue(TEXT("황혼 보강은 대기 투과율의 태양도 지평선 위에 유지"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(Brighter.AtmospherePitch, false));
	TestTrue(TEXT("황혼 정점의 태양 직접광 증가"), Brighter.SolarIntensity > Baseline.SolarIntensity);
	TestTrue(TEXT("황혼 정점의 방향광 증가"), Brighter.DirectionalIntensity > Baseline.DirectionalIntensity);
	TestEqual(TEXT("DayAlpha 유지"), Brighter.DayAlpha, Baseline.DayAlpha);
	TestEqual(TEXT("노을 가중치 유지"), Brighter.WarmWeight, Baseline.WarmWeight);
	TestEqual(TEXT("SkyLight 강도 유지"), Brighter.SkyIntensity, Baseline.SkyIntensity);
	TestTrue(TEXT("SkyLight 색 유지"), Brighter.SkyColor.Equals(Baseline.SkyColor));
	TestEqual(TEXT("노출 유지"), Brighter.ExposureBias, Baseline.ExposureBias);

	auto TunedAtHour = [&MakeTwilight](float Hour)
	{
		const float SolarPitch = Hour <= 19.0f
			? FMath::GetMappedRangeValueClamped(FVector2D(12.0f, 19.0f), FVector2D(-90.0f, -180.0f), Hour)
			: FMath::GetMappedRangeValueClamped(FVector2D(19.0f, 24.0f), FVector2D(-180.0f, -270.0f), Hour);
		TimeCycleSkyLighting::FParameters Lighting = MakeTwilight(Hour, SolarPitch);
		Lighting.DuskDirectionalIntensityScale = 1.5f;
		Lighting.DuskMinimumDirectionalElevation = 10.0f;
		return TimeCycleSkyLighting::Calculate(Lighting);
	};
	for (const float Hour : {19.0f - 1.0f / 60.0f, 19.0f + 1.0f / 60.0f})
	{
		const TimeCycleSkyLighting::FState Adjacent = TunedAtHour(Hour);
		TestTrue(TEXT("일몰 180도 unwind 전후 직접광 고도는 연속"),
			FMath::Abs(Adjacent.DirectionalPitch - Brighter.DirectionalPitch) < 0.01f);
		TestTrue(TEXT("일몰 1분 전후 직접광 강도는 연속"),
			FMath::Abs(Adjacent.DirectionalIntensity - Brighter.DirectionalIntensity) < 0.02f);
		TestTrue(TEXT("일몰 1분 전후 대기 태양의 최소 고도는 연속"),
			FMath::Abs(Adjacent.AtmospherePitch - Brighter.AtmospherePitch) < 0.01f);
	}
	TestTrue(TEXT("일몰 후 박명 보강이 대기 ground transmittance로 소실되지 않음"),
		TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(TunedAtHour(20.0f).AtmospherePitch, false));
	TestTrue(TEXT("직접광을 보강해도 박명 끝 직전 강도는 0에 접근"),
		TunedAtHour(20.5f - 0.001f).DirectionalIntensity < 0.0001f);
	TestTrue(TEXT("박명 끝 직후 야간 fill은 0에서 연속 복원"),
		TunedAtHour(20.5f + 0.001f).DirectionalIntensity < 0.0001f);

	for (const float Hour : {5.5f, 12.0f, 0.0f, 16.0f, 20.5f})
	{
		TimeCycleSkyLighting::FParameters Original = MakeTwilight(Hour, -90.0f);
		TimeCycleSkyLighting::FParameters Tuned = Original;
		Tuned.DuskDirectionalIntensityScale = BrighterDusk.DuskDirectionalIntensityScale;
		Tuned.DuskMinimumDirectionalElevation = BrighterDusk.DuskMinimumDirectionalElevation;
		const TimeCycleSkyLighting::FState OriginalState = TimeCycleSkyLighting::Calculate(Original);
		const TimeCycleSkyLighting::FState TunedState = TimeCycleSkyLighting::Calculate(Tuned);
		TestEqual(*FString::Printf(TEXT("%.1f시 방향광 경계·타 위상 강도 유지"), Hour),
			TunedState.DirectionalIntensity, OriginalState.DirectionalIntensity, 0.0001f);
		TestEqual(*FString::Printf(TEXT("%.1f시 방향광 경계·타 위상 피치 유지"), Hour),
			TunedState.DirectionalPitch, OriginalState.DirectionalPitch, 0.0001f);
		TestEqual(*FString::Printf(TEXT("%.1f시 대기 방향 경계·타 위상 유지"), Hour),
			TunedState.AtmospherePitch, OriginalState.AtmospherePitch, 0.0001f);
	}

	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
