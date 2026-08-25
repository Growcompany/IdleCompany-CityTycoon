#include "Misc/AutomationTest.h"
#include "Data/ProjectBoardData.h"
#include "Data/TierLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTierUnlockTest,
	"CGR.Tier.Unlock",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTierUnlockTest::RunTest(const FString& Parameters)
{
	// 레이아웃은 전역 정적이라 앞선 테스트가 확장 레이아웃을 남기면 7/10 전제가 통째로 무너진다
	FTierLayout::ResetToDefault();

	// 7/10 클리어만으로 승급한다 — 빌딩 레벨 게이트는 존재하지 않는다
	{
		FProjectTierProgress P;
		TestEqual(TEXT("시작 티어는 1"), P.CurrentTier, 1);
		for (int32 i = 1; i <= 6; ++i)
		{
			TestFalse(TEXT("6개까지는 승급 없음"), P.RecordProjectClear(i));
		}
		TestEqual(TEXT("6개 시점 티어 유지"), P.CurrentTier, 1);
		TestTrue(TEXT("7번째 클리어에서 승급"), P.RecordProjectClear(7));
		TestEqual(TEXT("T2 도달"), P.CurrentTier, 2);
	}

	// 같은 프로젝트 재개발은 클리어로 세지 않는다
	{
		FProjectTierProgress P;
		for (int32 i = 1; i <= 7; ++i) { P.RecordProjectClear(i); }
		TestFalse(TEXT("중복 기록은 false"), P.RecordProjectClear(3));
		TestEqual(TEXT("중복은 티어를 올리지 않는다"), P.CurrentTier, 2);
		TestEqual(TEXT("중복은 배열도 늘리지 않는다"), P.ClearedProjects.Num(), 7);
	}

	// 다음 티어 구간을 미리 클리어해도 현재 티어는 오르지 않는다
	{
		FProjectTierProgress P;
		for (int32 i = 11; i <= 20; ++i) { P.RecordProjectClear(i); }   // T2 구간 10개
		TestEqual(TEXT("T1 구간 클리어 0 이면 승급 없음"), P.CurrentTier, 1);
	}

	// MAX_TIER 상한 — 더 이상 오르지 않는다
	{
		FProjectTierProgress P;
		P.CurrentTier = TierConstants::MAX_TIER;
		int32 Start = 0, End = 0;
		FProjectTierProgress::GetTierProjectRange(TierConstants::MAX_TIER, Start, End);
		for (int32 i = Start; i <= End; ++i) { P.RecordProjectClear(i); }
		TestEqual(TEXT("T10 에서 더 오르지 않는다"), P.CurrentTier, TierConstants::MAX_TIER);
	}

	// 한 번의 클리어는 최대 한 단계 — 연쇄 승급 없음
	{
		FProjectTierProgress P;
		for (int32 i = 1; i <= 10; ++i) { P.RecordProjectClear(i); }    // T1 전량
		TestEqual(TEXT("T1 을 10/10 채워도 T2 까지만"), P.CurrentTier, 2);
	}

	return true;
}

#endif
