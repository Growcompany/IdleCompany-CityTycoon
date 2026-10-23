#include "Misc/AutomationTest.h"
#include "Data/FactoryUpgradeData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryAutoUnlockPolicyTest,
	"CGR.Factory.AutoUnlockPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryAutoUnlockPolicyTest::RunTest(const FString& Parameters)
{
	auto MakeDefinition = [](EFactoryUpgradeType InUpgradeType, int32 InRequiredHQLevel)
	{
		FFactoryUpgradeDefinition Definition;
		Definition.UpgradeType = InUpgradeType;
		Definition.RequiredHQLevel = InRequiredHQLevel;
		return Definition;
	};

	TestFalse(TEXT("홀드 생산 속도는 자동 계열 아님"),
		FactoryUpgradeUnlockPolicy::IsAutoUnlockType(EFactoryUpgradeType::HoldProductionSpeed));
	TestFalse(TEXT("홀드 생산량은 자동 계열 아님"),
		FactoryUpgradeUnlockPolicy::IsAutoUnlockType(EFactoryUpgradeType::HoldProductionAmount));
	TestFalse(TEXT("인부 추가는 처음부터 — 자동 계열 아님"),
		FactoryUpgradeUnlockPolicy::IsAutoUnlockType(EFactoryUpgradeType::LaborerCount));
	TestTrue(TEXT("오프라인 누적량은 자동 계열"),
		FactoryUpgradeUnlockPolicy::IsAutoUnlockType(EFactoryUpgradeType::AutoCollectionCapacity));

	const TArray<FFactoryUpgradeDefinition> Definitions = {
		MakeDefinition(EFactoryUpgradeType::AutoCollectionCapacity, 10),
		MakeDefinition(EFactoryUpgradeType::HoldProductionSpeed, 0),
		MakeDefinition(EFactoryUpgradeType::HoldProductionAmount, 0),
	};

	int32 RequiredHQLevel = -1;
	TestTrue(TEXT("상한 행의 HQ 요구 레벨을 해석"),
		FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(Definitions, RequiredHQLevel));
	TestEqual(TEXT("해금 레벨은 DT 값"), RequiredHQLevel, 10);

	// DT 재임포트로 행이 날아간 경우 — 조용히 해금되지 말고 실패해야 한다(loud failure)
	TArray<FFactoryUpgradeDefinition> MissingDefinitions = Definitions;
	MissingDefinitions.RemoveAll([](const FFactoryUpgradeDefinition& Definition)
	{
		return Definition.UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity;
	});
	RequiredHQLevel = 999;
	TestFalse(TEXT("상한 행 누락은 실패"),
		FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(MissingDefinitions, RequiredHQLevel));
	TestEqual(TEXT("행 누락 실패 시 출력은 0"), RequiredHQLevel, 0);

	// RequiredHQLevel 미저작(0) — DT 셀이 비면 전원 해금이 되는 사고를 막는다
	TArray<FFactoryUpgradeDefinition> NonPositiveDefinitions = Definitions;
	for (FFactoryUpgradeDefinition& Definition : NonPositiveDefinitions)
	{
		if (Definition.UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity)
		{
			Definition.RequiredHQLevel = 0;
			break;
		}
	}
	RequiredHQLevel = 999;
	TestFalse(TEXT("0 이하 값은 실패"),
		FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(NonPositiveDefinitions, RequiredHQLevel));
	TestEqual(TEXT("0 값 실패 시 출력은 0"), RequiredHQLevel, 0);

	TArray<FFactoryUpgradeDefinition> NegativeDefinitions = Definitions;
	for (FFactoryUpgradeDefinition& Definition : NegativeDefinitions)
	{
		if (Definition.UpgradeType == EFactoryUpgradeType::AutoCollectionCapacity)
		{
			Definition.RequiredHQLevel = -1;
			break;
		}
	}
	RequiredHQLevel = 999;
	TestFalse(TEXT("음수 값은 실패"),
		FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(NegativeDefinitions, RequiredHQLevel));
	TestEqual(TEXT("음수 값 실패 시 출력은 0"), RequiredHQLevel, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryCostCurveTest,
	"CGR.Factory.CostCurve",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryCostCurveTest::RunTest(const FString& Parameters)
{
	constexpr EFactoryUpgradeType SpeedType = EFactoryUpgradeType::HoldProductionSpeed;
	const FFactoryCostCurve Curve = FFactoryUpgradeConfig::GetFallbackCostCurve(SpeedType);

	TestEqual(TEXT("Lv0 비용은 BaseCost"), Curve.CostAtLevel(0), Curve.BaseCost);
	TestEqual(TEXT("생산속도 첫 구매(Lv1)는 146원"), Curve.CostAtLevel(1), 146LL);
	TestTrue(TEXT("비용은 레벨에 대해 단조 증가"), Curve.CostAtLevel(100) > Curve.CostAtLevel(99));

	// 멱함수의 정의적 성질 — 실효 성장률이 후반으로 갈수록 낮아진다.
	// 비교는 비율이 아니라 '증가분'끼리 해야 한다(둘 다 1 근처라 비율에 배수를 곱하면 항상 거짓).
	// 등비로 되돌아가면 두 증가분이 같아져 이 단언이 RED 가 된다.
	const double EarlyGrowth = static_cast<double>(Curve.CostAtLevel(11)) / static_cast<double>(Curve.CostAtLevel(10)) - 1.0;
	const double LateGrowth = static_cast<double>(Curve.CostAtLevel(1001)) / static_cast<double>(Curve.CostAtLevel(1000)) - 1.0;
	TestTrue(TEXT("비용 곡선은 초반이 가파르고 후반이 완만"), EarlyGrowth > LateGrowth * 10.0);

	// 벌크 = 레벨별 단건 비용의 정확한 합 (표시=청구 계약)
	int64 RepeatedSingleCost = 0;
	for (int32 Offset = 0; Offset < 50; ++Offset)
	{
		RepeatedSingleCost += Curve.CostAtLevel(1 + Offset);
	}
	TestEqual(TEXT("x50 비용은 같은 구간 x1 합과 동일"), Curve.BulkCost(1, 50), RepeatedSingleCost);
	TestEqual(TEXT("x1 벌크는 단건과 동일"), Curve.BulkCost(7, 1), Curve.CostAtLevel(7));
	TestEqual(TEXT("Count 0 이하는 0원"), Curve.BulkCost(7, 0), 0LL);

	TestEqual(TEXT("음수 레벨은 Lv0 으로 정규화"), Curve.CostAtLevel(-5), Curve.CostAtLevel(0));
	TestEqual(TEXT("레벨 오버플로는 int64 최댓값으로 포화"), Curve.BulkCost(MAX_int32, 2), MAX_int64);

	// 상한과 효과 수렴점이 어긋나면 "돈만 먹고 효과 0"인 구간이 생긴다 — 그 정합을 여기서 잠근다.
	const int32 SpeedMaxLevel = FFactoryUpgradeConfig::GetMaxLevel(SpeedType);
	TestEqual(TEXT("생산속도 상한은 효과 수렴점 351"), SpeedMaxLevel, 351);
	TestTrue(TEXT("상한값과 그 위 레벨의 효과가 같다(= 수렴 지점)"),
		FMath::IsNearlyEqual(
			FFactoryUpgradeConfig::CalculateUpgradeValue(SpeedType, SpeedMaxLevel),
			FFactoryUpgradeConfig::CalculateUpgradeValue(SpeedType, SpeedMaxLevel + 1), KINDA_SMALL_NUMBER));
	TestFalse(TEXT("상한 직전 레벨은 아직 수렴 전(= 상한이 과하게 낮지 않다)"),
		FMath::IsNearlyEqual(
			FFactoryUpgradeConfig::CalculateUpgradeValue(SpeedType, SpeedMaxLevel - 1),
			FFactoryUpgradeConfig::CalculateUpgradeValue(SpeedType, SpeedMaxLevel), KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryEarlyCostCurveTest,
	"CGR.Factory.EarlyCostCurve",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryEarlyCostCurveTest::RunTest(const FString& Parameters)
{
	for (const ECostCurveType CurveType : { ECostCurveType::Power, ECostCurveType::Geometric })
	{
		FFactoryUpgradeDefinition Definition;
		Definition.BaseCost = 100;
		Definition.CostCurveType = CurveType;
		const FFactoryCostCurve Original = Definition.GetCostCurve();
		Definition.EarlyCostMultiplier = 0.1f;
		Definition.FullCostFromLevel = 10;
		const FFactoryCostCurve Discounted = Definition.GetCostCurve();
		TestTrue(TEXT("첫 구매는 실제로 저렴해진다"), Discounted.CostAtLevel(1) < Original.CostAtLevel(1));
		int64 SingleTotal = 0;
		for (int32 Level = 1; Level <= 20; ++Level)
		{
			TestTrue(TEXT("전환 경계에서도 가격이 감소하지 않는다"),
				Discounted.CostAtLevel(Level) >= Discounted.CostAtLevel(Level - 1));
			if (Level >= Definition.FullCostFromLevel)
			{
				TestEqual(TEXT("중후반 가격은 원래 곡선과 일치"), Discounted.CostAtLevel(Level), Original.CostAtLevel(Level));
			}
			SingleTotal += Definition.CalculateUpgradeCost(Level);
		}
		TestEqual(TEXT("초반 보정 경계를 넘는 벌크도 단건 결제 합계와 일치"), Discounted.BulkCost(1, 20), SingleTotal);
		Definition.FullCostFromLevel = 1;
		TestEqual(TEXT("보정 구간이 없으면 기본 가격"), Definition.CalculateUpgradeCost(1), Original.CostAtLevel(1));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
