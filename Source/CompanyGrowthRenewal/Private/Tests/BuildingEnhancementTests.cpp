#include "Misc/AutomationTest.h"
#include "Data/BuildingEnhancementData.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Table/BuildingData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGREnhancementEconomyTest,
	"CGR.Building.EnhancementEconomy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREnhancementEconomyTest::RunTest(const FString& Parameters)
{
	constexpr EBuildingEnhancementType CostType = EBuildingEnhancementType::MarketingPower;
	constexpr int32 BulkCount = 50;
	int64 RepeatedSingleCost = 0;
	for (int32 Offset = 0; Offset < BulkCount; ++Offset)
	{
		RepeatedSingleCost += UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, Offset);
	}
	TestEqual(TEXT("x50 비용은 같은 구간 x1 합과 동일"),
		UBuildingEnhancementHelper::CalculateBulkUpgradeCost(CostType, 0, BulkCount), RepeatedSingleCost);

	const int64 LevelZeroCost = UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 0);
	const int64 LevelOneCost = UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 1);
	TestEqual(TEXT("음수 단건 레벨은 Lv0 비용으로 정규화"),
		UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, -5), LevelZeroCost);
	TestEqual(TEXT("음수 벌크 시작 레벨은 Lv0부터 연속 계산"),
		UBuildingEnhancementHelper::CalculateBulkUpgradeCost(CostType, -5, 2), LevelZeroCost + LevelOneCost);
	TestEqual(TEXT("벌크 레벨 덧셈 오버플로는 int64 최댓값으로 포화"),
		UBuildingEnhancementHelper::CalculateBulkUpgradeCost(CostType, MAX_int32, 2), MAX_int64);

	TestEqual(TEXT("금고 Lv0 내부 보관시간은 5분"),
		UBuildingEnhancementHelper::CalculateVaultSeconds(0), 300.0f);
	// 멱함수 곡선의 정의적 성질 — 실효 성장률이 레벨이 오를수록 낮아진다(초반 가파름 → 후반 완만).
	// 이게 깨지면 순수 등비로 되돌아간 것이고, 그 순간 Lv6000 도달이 산술적으로 불가능해진다.
	const double EarlyRatio = static_cast<double>(UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 11))
		/ static_cast<double>(UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 10));
	const double LateRatio = static_cast<double>(UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 1001))
		/ static_cast<double>(UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 1000));
	// 비교는 비율이 아니라 '증가분'끼리 해야 한다 — 두 비율 다 1 근처라 비율에 배수를 곱하면 항상 거짓이 된다.
	// 등비 곡선이면 두 증가분이 같아지므로 이 단언이 멱함수 여부를 가른다(현행 실측 약 50배 차).
	TestTrue(TEXT("비용 곡선은 초반이 가파르고 후반이 완만"), (EarlyRatio - 1.0) > (LateRatio - 1.0) * 10.0);
	TestTrue(TEXT("비용은 레벨에 대해 단조 증가"),
		UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 1000)
			> UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 999));

	// 칸수 비례(bScaleCostByFootprint)는 증축 전용이다. Money 강화가 여기 휩쓸리면
	// 2x2 빌딩이 마케팅파워까지 4배를 내게 된다. 폴백 기본값이 false 라 이 경로가 그 검증이다.
	// (켜진 경로 = 빌드업은 DT 리임포트의 expect 검사와 PIE 가 담당 — 자동화 테스트엔 DT 가 없다.)
	TestEqual(TEXT("칸수 비례가 꺼진 슬롯은 footprint 무관"),
		UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 10, 4),
		UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 10, 1));
	TestEqual(TEXT("칸수 0/음수는 1칸으로 정규화"),
		UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 10, 0),
		UBuildingEnhancementHelper::CalculateUpgradeCost(CostType, 10, 1));

	// 아래 두 값은 DT 가 아니라 폴백 상수(k=0.000625)의 산출물이다 — 자동화 테스트엔 PIE 월드가 없어 DT lookup 이 실패한다
	TestTrue(TEXT("금고 Lv100은 약 48분"),
		FMath::IsNearlyEqual(UBuildingEnhancementHelper::CalculateVaultSeconds(100), 2899.2f, 1.0f));
	TestTrue(TEXT("금고 곡선은 12시간 아래에서 증가"),
		UBuildingEnhancementHelper::CalculateVaultSeconds(3000)
			> UBuildingEnhancementHelper::CalculateVaultSeconds(100)
			&& UBuildingEnhancementHelper::CalculateVaultSeconds(3000) < 43200.0f);

	const UProjectOperationManager* OperationManager = GetDefault<UProjectOperationManager>();
	const float FreshT1Capacity = OperationManager->CalculateBaseRevenueForProjectNumber(1)
		* UBuildingEnhancementHelper::CalculateVaultSeconds(0);
	TestEqual(TEXT("첫 프로젝트 5분 용량은 6만원"),
		FMath::RoundToInt64(FreshT1Capacity), 60000LL);
	TestEqual(TEXT("비정상 조회 폴백도 6만원"),
		FMath::RoundToInt64(ABuildingBaseActor::BaseVaultCapacity), 60000LL);

	// Lv2500 = 예산 도달선(Lv2,500~2,900) 표본. 배수는 폴백 EffectPerLevel(0.0008) 산출물 —
	// 위 금고와 같은 이유로 DT 값이 아니다
	const float KeystoneLv2500 = 10.0f * UBuildingEnhancementHelper::CalculateEffectMultiplier(
		EBuildingEnhancementType::KeystoneAuraPower, 2500);
	TestTrue(TEXT("키스톤 기본 10%는 Lv2500에서 30%"),
		FMath::IsNearlyEqual(KeystoneLv2500, 30.0f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGREmployeeCapacityTest,
	"CGR.Building.EmployeeCapacity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREmployeeCapacityTest::RunTest(const FString& Parameters)
{
	// ===== 출하 기본값 추적 구간 — 여기서는 어떤 노브도 재대입하지 않는다 =====
	// 기본 구성(BaseEmployees=2 · PerAddedFloor=1.0 = 1칸 1층당 2명)이 바뀌면 이 구간이 RED 가 돼야 한다.
	{
		FBuildingData Shipped;
		TestEqual(TEXT("[기본값] 신축 0층 상한"), Shipped.GetEmployeeCapacity(0), 2);
		TestEqual(TEXT("[기본값] 증축 1층"), Shipped.GetEmployeeCapacity(1), 3);
		TestEqual(TEXT("[기본값] 증축 10층"), Shipped.GetEmployeeCapacity(10), 12);

		// 신축 상한 = 칸당 2명 정비례 (2 / 4 / 8 / 18)
		Shipped.FootprintWidthCells = 2; Shipped.FootprintDepthCells = 1;
		TestEqual(TEXT("[기본값] 2칸"), Shipped.GetEmployeeCapacity(0), 4);
		Shipped.FootprintWidthCells = 2; Shipped.FootprintDepthCells = 2;
		TestEqual(TEXT("[기본값] 4칸"), Shipped.GetEmployeeCapacity(0), 8);
		Shipped.FootprintWidthCells = 3; Shipped.FootprintDepthCells = 3;
		TestEqual(TEXT("[기본값] 9칸"), Shipped.GetEmployeeCapacity(0), 18);

		// 증축 증가폭도 칸수 배 — 이게 큰 건물을 짓는 이유다 (4칸은 층당 +4)
		Shipped.FootprintWidthCells = 2; Shipped.FootprintDepthCells = 2;
		TestEqual(TEXT("[기본값] 4칸 증축 10층"), Shipped.GetEmployeeCapacity(10), 48);
		Shipped.FootprintWidthCells = 3; Shipped.FootprintDepthCells = 3;
		TestEqual(TEXT("[기본값] 9칸 증축 10층"), Shipped.GetEmployeeCapacity(10), 108);

		// 스타터 프리셋 시드 책상보다 초기 인원이 적으면 빈 책상만 남는다 — 칸수별로 짝지어 본다
		// (DT_OfficeStarterPreset 실측: 1칸 1 · 2칸 3 · 4칸 4~5 · 9칸 6)
		const TArray<TPair<int32, int32>> SeededDesks = { {1, 1}, {2, 3}, {4, 5}, {9, 6} };
		for (const TPair<int32, int32>& Pair : SeededDesks)
		{
			FBuildingData Sized;
			Sized.FootprintWidthCells = Pair.Key;
			Sized.FootprintDepthCells = 1;
			TestTrue(*FString::Printf(TEXT("[기본값] %d칸 신축 상한 >= 시드 책상 %d"), Pair.Key, Pair.Value),
				Sized.GetEmployeeCapacity(0) >= Pair.Value);
		}
	}

	// ===== 공식 구간 — 노브를 명시해 산술을 고정 =====
	FBuildingData Data;
	Data.BaseEmployees = 10;
	Data.PerAddedFloor = 1.0f;
	Data.PerCell = 0.0f;

	TestEqual(TEXT("증축 0층 = 기본 인원 그대로"), Data.GetEmployeeCapacity(0), 10);
	TestEqual(TEXT("증축 1층 = 계수만큼 증가"), Data.GetEmployeeCapacity(1), 11);
	TestEqual(TEXT("증축 10층"), Data.GetEmployeeCapacity(10), 20);

	// 칸수는 (Base + 층가산) 전체에 곱해진다 — 층당 증가폭도 칸수 배가 되어야 넓은 건물이 의미를 갖는다
	Data.FootprintWidthCells = 2; Data.FootprintDepthCells = 1;
	TestEqual(TEXT("2칸 0층 = Base×2"), Data.GetEmployeeCapacity(0), 20);
	TestEqual(TEXT("2칸 5층 = (Base+5)×2"), Data.GetEmployeeCapacity(5), 30);
	Data.FootprintWidthCells = 3; Data.FootprintDepthCells = 3;
	TestEqual(TEXT("9칸 0층 = Base×9"), Data.GetEmployeeCapacity(0), 90);
	TestEqual(TEXT("9칸 5층 = (Base+5)×9"), Data.GetEmployeeCapacity(5), 135);

	// 소수 계수는 내림 — 리시드 없이 튜닝 가능해야 한다
	Data.FootprintWidthCells = 1; Data.FootprintDepthCells = 1;
	Data.PerAddedFloor = 0.5f;
	TestEqual(TEXT("층 계수 0.5, 증축 10층"), Data.GetEmployeeCapacity(10), 15);
	TestEqual(TEXT("층 계수 0.5, 증축 3층은 내림"), Data.GetEmployeeCapacity(3), 11);
	// 내림은 칸수를 곱한 뒤에 한 번만 — 칸마다 잘라내면 큰 건물이 손해를 본다
	Data.FootprintWidthCells = 2; Data.FootprintDepthCells = 2;
	TestEqual(TEXT("층 계수 0.5, 4칸 증축 3층 = (10+1.5)×4"), Data.GetEmployeeCapacity(3), 46);
	Data.PerAddedFloor = 1.0f;

	// BaseEmployees 0 이면 층 가산분만 남는다
	Data.BaseEmployees = 0;
	Data.FootprintWidthCells = 1; Data.FootprintDepthCells = 1;
	TestEqual(TEXT("기본 0 + 0층 = 0"), Data.GetEmployeeCapacity(0), 0);
	TestEqual(TEXT("기본 0 + 4층 = 4"), Data.GetEmployeeCapacity(4), 4);

	// 음수 오염은 하한 0 으로 흡수 (상한이 음수면 게이트 판정이 뒤집힌다)
	Data.BaseEmployees = 10;
	TestEqual(TEXT("음수 층수는 0층 취급"), Data.GetEmployeeCapacity(-5), 10);
	Data.BaseEmployees = -3;
	TestEqual(TEXT("음수 기본 인원은 0 취급"), Data.GetEmployeeCapacity(2), 2);

	// 칸수 오염(0 이하)은 1칸 취급 — 가산항이 음수가 되면 상한이 줄어든다
	Data.BaseEmployees = 10;
	Data.PerCell = 2.0f;
	Data.FootprintWidthCells = 0; Data.FootprintDepthCells = 0;
	TestEqual(TEXT("칸수 0 은 1칸 취급"), Data.GetEmployeeCapacity(0), 10);

	// 키스톤은 직원을 받지 않는다
	Data.FootprintWidthCells = 3; Data.FootprintDepthCells = 3;
	Data.KeystoneAura.bIsKeystone = true;
	TestEqual(TEXT("키스톤은 인원 0"), Data.GetEmployeeCapacity(10), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRHireCapacityGateTest,
	"CGR.Building.HireCapacityGate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRHireCapacityGateTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("빈 건물은 고용 가능"), UEmployeeManager::IsUnderCapacity(0, 12));
	TestTrue(TEXT("정원 직전은 고용 가능"), UEmployeeManager::IsUnderCapacity(11, 12));
	TestFalse(TEXT("정원 도달은 고용 불가"), UEmployeeManager::IsUnderCapacity(12, 12));
	TestFalse(TEXT("정원 초과(데이터 오염)도 고용 불가"), UEmployeeManager::IsUnderCapacity(13, 12));
	// 키스톤은 정원 0 — 직원을 받지 않는다
	TestFalse(TEXT("정원 0 이면 항상 불가"), UEmployeeManager::IsUnderCapacity(0, 0));
	// 정원이 음수로 오염돼도 열리면 안 된다
	TestFalse(TEXT("정원 음수는 항상 불가"), UEmployeeManager::IsUnderCapacity(0, -1));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
