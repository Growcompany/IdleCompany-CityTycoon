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

#endif // WITH_DEV_AUTOMATION_TESTS
