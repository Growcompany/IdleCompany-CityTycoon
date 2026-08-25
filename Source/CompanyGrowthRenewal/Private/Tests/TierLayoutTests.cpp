#include "Misc/AutomationTest.h"
#include "Data/TierLayout.h"
#include "Data/ProjectBoardData.h"
#include "Manager/OfficeStageProgressManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTierLayoutDefaultTest,
	"CGR.Tier.LayoutDefault",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTierLayoutDefaultTest::RunTest(const FString& Parameters)
{
	FTierLayout::ResetToDefault();
	int32 S = 0, E = 0;
	FTierLayout::GetRange(1, S, E);
	TestEqual(TEXT("T1 = 1~10"), S, 1); TestEqual(TEXT("T1 end"), E, 10);
	FTierLayout::GetRange(10, S, E);
	TestEqual(TEXT("T10 = 91~100"), S, 91); TestEqual(TEXT("T10 end"), E, 100);
	TestEqual(TEXT("#11 은 T2"), FTierLayout::GetTierForProject(11), 2);
	TestEqual(TEXT("#100 은 T10"), FTierLayout::GetTierForProject(100), 10);
	TestEqual(TEXT("MaxTier 기본 10"), FTierLayout::MaxTier(), 10);
	TestEqual(TEXT("티어당 기본 10개"), FTierLayout::ProjectsPerTier(5), 10);
	TestEqual(TEXT("해금 기본 7"), FTierLayout::ClearToUnlock(3), 7);
	// 기본 레이아웃에서 수익 인덱스는 항등 — 기존 경제 무변경 보장
	for (int32 N = 1; N <= 100; ++N)
	{
		if (FTierLayout::RevenueScaleIndex(N) != N) { AddError(FString::Printf(TEXT("RevenueScaleIndex(%d) != %d"), N, N)); return false; }
	}
	// FProjectTierProgress 정적 함수가 레이아웃을 그대로 위임하는지
	FProjectTierProgress::GetTierProjectRange(2, S, E);
	TestEqual(TEXT("Progress 위임 start"), S, 11); TestEqual(TEXT("Progress 위임 end"), E, 20);
	TestEqual(TEXT("Progress 위임 티어"), FProjectTierProgress::GetTierForProject(91), 10);
	// 등록 안 된 티어 = 빈 범위(0,-1) — 호출부의 (End >= Start) 가드가 곧 "그 티어 없음" 판정이다
	FTierLayout::GetRange(99, S, E);
	TestEqual(TEXT("미등록 티어 start"), S, 0); TestEqual(TEXT("미등록 티어 end"), E, -1);
	TestEqual(TEXT("#0 은 어느 티어도 아님"), FTierLayout::GetTierForProject(0), 0);
	// 빈 레이아웃 주입(DT 결손) = 기본으로 되돌린다. 여기서 0칸이 되면 보드가 통째로 빈다
	FTierLayout::SetLayout({});
	TestEqual(TEXT("빈 레이아웃 = 기본 MaxTier 10"), FTierLayout::MaxTier(), 10);
	TestEqual(TEXT("빈 레이아웃 = 티어당 10개"), FTierLayout::ProjectsPerTier(1), 10);
	FTierLayout::GetRange(1, S, E);
	TestEqual(TEXT("빈 레이아웃 T1 start"), S, 1); TestEqual(TEXT("빈 레이아웃 T1 end"), E, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTierLayoutOverrideTest,
	"CGR.Tier.LayoutOverride",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTierLayoutOverrideTest::RunTest(const FString& Parameters)
{
	// 티어 11 을 101~112 (12개, 해금 8, 수익 앵커 140) 로 추가한 레이아웃
	TArray<FTierRange> L;
	for (int32 T = 1; T <= 10; ++T) { L.Add(FTierRange{ T, (T - 1) * 10 + 1, 10, 7, (T - 1) * 10 + 1 }); }
	L.Add(FTierRange{ 11, 101, 12, 8, 140 });
	FTierLayout::SetLayout(L);

	int32 S = 0, E = 0;
	FTierLayout::GetRange(11, S, E);
	TestEqual(TEXT("T11 start"), S, 101); TestEqual(TEXT("T11 end"), E, 112);
	TestEqual(TEXT("MaxTier 11"), FTierLayout::MaxTier(), 11);
	TestEqual(TEXT("T11 은 12개"), FTierLayout::ProjectsPerTier(11), 12);
	TestEqual(TEXT("#105 는 T11"), FTierLayout::GetTierForProject(105), 11);
	TestEqual(TEXT("T11 해금 8"), FTierLayout::ClearToUnlock(11), 8);
	TestEqual(TEXT("기존 티어 수익 불변"), FTierLayout::RevenueScaleIndex(55), 55);
	TestEqual(TEXT("T11 #103 수익 앵커 140+2"), FTierLayout::RevenueScaleIndex(103), 142);
	TestEqual(TEXT("범위 밖 번호는 0 (loud)"), FTierLayout::GetTierForProject(999), 0);
	// 위임된 FProjectTierProgress 도 확장 레이아웃을 그대로 본다
	FProjectTierProgress Progress;
	Progress.CurrentTier = 11;
	for (int32 N = 101; N <= 108; ++N) { Progress.ClearedProjects.Add(N); }
	TestEqual(TEXT("T11 클리어 8개"), Progress.GetTierClearedCount(11), 8);
	TestTrue(TEXT("T11 8/12 면 다음 티어 조건 충족"), Progress.IsTierUnlocked(12));

	FTierLayout::ResetToDefault();
	TestEqual(TEXT("리셋 후 MaxTier 10"), FTierLayout::MaxTier(), 10);
	TestEqual(TEXT("리셋 후 범위 밖 111"), FTierLayout::GetTierForProject(111), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTierBaselineTest,
	"CGR.Tier.Baseline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTierBaselineTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("빈 배열 = 0"), UOfficeStageProgressManager::ComputeTierBaselineFromScores({}), 0.0f);
	TestEqual(TEXT("홀수 중앙값"), UOfficeStageProgressManager::ComputeTierBaselineFromScores({ 73, 24, 108, 88, 77 }), 77.0f);
	TestEqual(TEXT("짝수 = 가운데 둘 평균"), UOfficeStageProgressManager::ComputeTierBaselineFromScores({ 24, 73, 77, 88 }), 75.0f);
	return true;
}

#endif
