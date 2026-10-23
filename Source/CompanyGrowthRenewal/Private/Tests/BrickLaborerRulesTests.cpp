#include "Misc/AutomationTest.h"
#include "Data/BrickLaborerRules.h"
#include "Data/FactoryUpgradeConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBrickLaborerCrewTest,
	"CGR.Factory.LaborerCrew",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBrickLaborerCrewTest::RunTest(const FString& Parameters)
{
	// 인부 강화값은 1 + 레벨이고, 공장 단계에서 선택한 정원이 이를 제한한다.
	TestEqual(TEXT("인부 강화 레벨 1이면 2명"),
		FBrickLaborerRules::LaborerCountFromValue(
			FFactoryUpgradeConfig::CalculateUpgradeValue(EFactoryUpgradeType::LaborerCount, 1), 5), 2);
	for (const int32 Capacity : { 5, 10, 25, 50, 75, 100 })
	{
		TestEqual(FString::Printf(TEXT("정원 %d명을 넘지 않는다"), Capacity),
			FBrickLaborerRules::LaborerCountFromValue(999.f, Capacity), Capacity);
	}
	TestEqual(TEXT("음수 강화값도 유효 정원에서는 1명 이상"),
		FBrickLaborerRules::LaborerCountFromValue(-5.f, 5), 1);
	TestEqual(TEXT("정원 데이터가 없으면 인부를 만들지 않는다"),
		FBrickLaborerRules::LaborerCountFromValue(999.f, 0), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBrickLaborerRateTest,
	"CGR.Factory.LaborerProductionRate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBrickLaborerRateTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("정원이 없어서 인부가 0명이면 벽돌 생산도 없다"),
		FBrickLaborerRules::BricksPerSecond(4, 0, 40.f, 1.f), 0.f);

	const float PathLength = 8370.f;   // 실측 문 간격 62m x 경로 보정 1.35
	const float WalkSpeed = 200.f;
	const float LegTime = FBrickLaborerRules::LegSeconds(PathLength, WalkSpeed);

	TestTrue(TEXT("한 구간이 사람 걸음 시간이다"), LegTime > 30.f && LegTime < 60.f);

	const int32 SpeedLevels[] = { 1, 51, 200, 351 };
	const float Paces[][2] = { { 1.0f, 0.25f }, { 2.0f, 2.0f } };  // { 이동배속, 생산배수 }

	for (int32 SpeedLevel : SpeedLevels)
	{
		const float CurveCycle =
			FFactoryUpgradeConfig::CalculateUpgradeValue(EFactoryUpgradeType::HoldProductionSpeed, SpeedLevel);

		for (int32 AmountLevel : { 1, 60, 600, 10000 })
		{
			const int32 PerLaborerOutput = FMath::RoundToInt(
				FFactoryUpgradeConfig::CalculateUpgradeValue(EFactoryUpgradeType::HoldProductionAmount, AmountLevel));

			for (const float* Pace : Paces)
			{
				const float SpeedMul = Pace[0];
				const float ProdScale = Pace[1];

				const int32 Count = 7;  // 인원은 생산식에서 곱으로만 들어간다 — 어떤 값이든 성립해야 한다
				const int32 Amount = FBrickLaborerRules::HarvestPerTrip(
					PerLaborerOutput, LegTime, CurveCycle, SpeedMul, ProdScale);

				// 곡선이 뜻하는 생산율 x 배수. 이동 속도를 사람 걸음으로 묶어도 이 값은 지켜져야 한다.
				// 인원 x 1인당능력 / 주기 — 두 축이 곱으로 쌓인다
				const float Expected = static_cast<float>(PerLaborerOutput) * static_cast<float>(Count) / CurveCycle * ProdScale;
				const float Actual = FBrickLaborerRules::BricksPerSecond(Amount, Count, LegTime, SpeedMul);

				// 1회 운반량이 작을수록 정수 반올림 오차가 커지므로 상대 허용치로 본다
				const float Tolerance = FMath::Max(0.08f, 1.0f / static_cast<float>(FMath::Max(1, Amount)));
				const float RelativeError = FMath::Abs(Actual - Expected) / FMath::Max(Expected, KINDA_SMALL_NUMBER);

				TestTrue(
					FString::Printf(TEXT("생산율이 곡선x배수를 따른다 (속도Lv%d 수량Lv%d 배속%.2f: 기대 %.3f/s, 실제 %.3f/s)"),
						SpeedLevel, AmountLevel, SpeedMul, Expected, Actual),
					RelativeError <= Tolerance);
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBrickLaborerPaceTest,
	"CGR.Factory.LaborerPace",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBrickLaborerPaceTest::RunTest(const FString& Parameters)
{
	// 홀드는 이동을 2배로만 올린다 — 사람이 8배로 뛰면 안 되기 때문
	const float LegTime = FBrickLaborerRules::LegSeconds(8370.f, 200.f);
	const float IdleRate = FBrickLaborerRules::BricksPerSecond(4, 10, LegTime, 1.0f);
	const float HoldRate = FBrickLaborerRules::BricksPerSecond(4, 10, LegTime, 2.0f);
	TestEqual(TEXT("같은 운반량이면 홀드 생산은 이동배속만큼만"), HoldRate / IdleRate, 2.0f, 0.001f);

	// 클립 재생 속도는 지면 속도에 비례해야 발이 안 끌린다
	TestEqual(TEXT("기준 속도면 재생 1배"), FBrickLaborerRules::ClipPlayRate(160.f, 160.f), 1.0f, 0.001f);
	TestEqual(TEXT("두 배 빠르면 재생 2배"), FBrickLaborerRules::ClipPlayRate(320.f, 160.f), 2.0f, 0.001f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
