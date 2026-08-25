#include "Misc/AutomationTest.h"
#include "Utils/GachaBatchMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaBatchCountTest,
	"CGR.Gacha.BatchMath.Count",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBatchCountTest::RunTest(const FString& Parameters)
{
	using namespace GachaBatchMath;
	TestEqual(TEXT("티켓 3·정원 5 → 3"), ComputeBatchPullCount(3, 5), 3);
	TestEqual(TEXT("티켓 7·정원 5 → 5 상한"), ComputeBatchPullCount(7, 5), 5);
	TestEqual(TEXT("티켓 7·정원 2 → 2 정원"), ComputeBatchPullCount(7, 2), 2);
	TestEqual(TEXT("티켓 0 → 0"), ComputeBatchPullCount(0, 5), 0);
	TestEqual(TEXT("정원 음수 방어 → 0"), ComputeBatchPullCount(3, -1), 0);
	TestEqual(TEXT("티켓 음수 방어 → 0"), ComputeBatchPullCount(-4, 5), 0);
	TestEqual(TEXT("상한 인자 지정은 기본 상한을 대체"), ComputeBatchPullCount(9, 9, 3), 3);
	TestEqual(TEXT("기본 상한 = MaxBatchPull"), ComputeBatchPullCount(99, 99), MaxBatchPull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaBatchOrderTest,
	"CGR.Gacha.BatchMath.Order",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBatchOrderTest::RunTest(const FString& Parameters)
{
	using namespace GachaBatchMath;
	// 뽑기순 등급 [레전4, 고급1, 레어2, 일반0, 에픽3] → 슬롯순 인덱스 [0(레전), 4(에픽), 2(레어), 1(고급), 3(일반)]
	const TArray<int32> Order = ComputeDisplayOrder({4, 1, 2, 0, 3});
	TestEqual(TEXT("정렬 결과 길이"), Order.Num(), 5);
	TestEqual(TEXT("C=최고"), Order[0], 0);
	TestEqual(TEXT("R1=2위"), Order[1], 4);
	TestEqual(TEXT("L1=3위"), Order[2], 2);
	TestEqual(TEXT("R2=4위"), Order[3], 1);
	TestEqual(TEXT("L2=5위"), Order[4], 3);
	// 동률 = 뽑기순 유지(stable)
	const TArray<int32> Tie = ComputeDisplayOrder({2, 2, 2});
	TestEqual(TEXT("동률 안정 0"), Tie[0], 0);
	TestEqual(TEXT("동률 안정 1"), Tie[1], 1);
	TestEqual(TEXT("동률 안정 2"), Tie[2], 2);
	TestEqual(TEXT("빈 입력은 빈 배열"), ComputeDisplayOrder(TArray<int32>()).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaBatchSlotXTest,
	"CGR.Gacha.BatchMath.SlotX",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBatchSlotXTest::RunTest(const FString& Parameters)
{
	using namespace GachaBatchMath;
	// 확정값: CardW 560, SideScale 0.58, Gap 24 → SideW 324.8, X1 = 280+24+162.4 = 466.4, X2 = 466.4+324.8+24 = 815.2
	constexpr float CardW = 560.f;
	constexpr float SideScale = 0.58f;
	constexpr float GapPx = 24.f;
	TestEqual(TEXT("C"), SlotXOffset(0, CardW, SideScale, GapPx), 0.f);
	TestTrue(TEXT("R1"), FMath::IsNearlyEqual(SlotXOffset(1, CardW, SideScale, GapPx), 466.4f, 0.01f));
	TestTrue(TEXT("L1"), FMath::IsNearlyEqual(SlotXOffset(2, CardW, SideScale, GapPx), -466.4f, 0.01f));
	TestTrue(TEXT("R2"), FMath::IsNearlyEqual(SlotXOffset(3, CardW, SideScale, GapPx), 815.2f, 0.01f));
	TestTrue(TEXT("L2"), FMath::IsNearlyEqual(SlotXOffset(4, CardW, SideScale, GapPx), -815.2f, 0.01f));
	// 전폭은 리터럴이 아니라 함수 산출값으로 재야 슬롯 공식이 바뀔 때 여기서 잡힌다
	const float FullWidth = 2.f * SlotXOffset(3, CardW, SideScale, GapPx) + CardW * SideScale;
	TestTrue(TEXT("5장 전폭 < 2304 (16:10 안전 예산)"), FullWidth < 2304.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
