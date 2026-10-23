#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/QuestRules.h"

namespace
{
	FQuestTable MakeRow(EQuestPeriod Period, EQuestCounter Counter, int64 Target, FName Track = NAME_None, int32 Tier = 0)
	{
		FQuestTable Row;
		Row.Period = Period; Row.Counter = Counter; Row.Target = Target; Row.Track = Track; Row.Tier = Tier;
		FMissionReward R; R.ResourceType = EResourceType::Diamond; R.Amount = 5; Row.Rewards.Add(R);
		return Row;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestRulesTest, "CGR.LiveOps.QuestRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FQuestRulesTest::RunTest(const FString& Parameters)
{
	TMap<FName, FQuestTable> Rows;
	Rows.Add(TEXT("D_A"), MakeRow(EQuestPeriod::Daily, EQuestCounter::RevenueCollect, 5));
	Rows.Add(TEXT("D_B"), MakeRow(EQuestPeriod::Daily, EQuestCounter::ProjectLaunch, 1));
	Rows.Add(TEXT("W_A"), MakeRow(EQuestPeriod::Weekly, EQuestCounter::LoginDay, 5));
	Rows.Add(TEXT("A_1"), MakeRow(EQuestPeriod::Lifetime, EQuestCounter::GachaPull, 10, TEXT("Gacha"), 1));
	Rows.Add(TEXT("A_2"), MakeRow(EQuestPeriod::Lifetime, EQuestCounter::GachaPull, 50, TEXT("Gacha"), 2));

	TestTrue(TEXT("increment counter"), FQuestRules::IsIncrementCounter(EQuestCounter::RevenueCollect));
	TestFalse(TEXT("absolute counter"), FQuestRules::IsIncrementCounter(EQuestCounter::HQLevel));

	FQuestSaveData Save;
	Save.DayKey = TEXT("2026-09-13"); Save.WeekKey = TEXT("2026-09-07");
	Save.Progress.Add(TEXT("D_A"), 5);   // 완료·미수령 → 롤오버 시 만료 보상
	Save.Progress.Add(TEXT("D_B"), 0);   // 미완료 → 버림
	Save.Claimed.Add(TEXT("D_B"));       // (방어) 수령 목록에 있어도 진행이 목표 미만이면 만료 대상 아님

	TestEqual(TEXT("state claimable"), FQuestRules::StateFor(TEXT("D_A"), Rows[TEXT("D_A")], 5, Save), EQuestState::Claimable);
	TestEqual(TEXT("state in progress"), FQuestRules::StateFor(TEXT("W_A"), Rows[TEXT("W_A")], 2, Save), EQuestState::InProgress);

	TArray<FMissionReward> Expired;
	FQuestRules::Rollover(Save, Rows, EQuestPeriod::Daily, TEXT("2026-09-14"), Expired);
	TestEqual(TEXT("expired rewards = D_A"), Expired.Num(), 1);
	TestEqual(TEXT("day key moved"), Save.DayKey, FString(TEXT("2026-09-14")));
	TestEqual(TEXT("daily progress cleared"), Save.Progress.Num(), 0);
	TestEqual(TEXT("weekly key untouched"), Save.WeekKey, FString(TEXT("2026-09-07")));

	Save.ClaimedLifetime.Add(TEXT("A_1"));
	TestEqual(TEXT("visible tier = lowest unclaimed"), FQuestRules::VisibleLifetimeTier(TEXT("Gacha"), Rows, Save), 2);
	TestEqual(TEXT("A_1 hidden after claim"), FQuestRules::StateFor(TEXT("A_1"), Rows[TEXT("A_1")], 60, Save), EQuestState::Hidden);
	TestEqual(TEXT("A_2 claimable at 60"), FQuestRules::StateFor(TEXT("A_2"), Rows[TEXT("A_2")], 60, Save), EQuestState::Claimable);

	// T4 — 주기 행의 Claimed 상태(진행이 목표를 넘겨도 수령했으면 Claimed)
	FQuestSaveData Marked;
	Marked.Claimed.Add(TEXT("D_A"));
	TestEqual(TEXT("claimed daily row stays Claimed"), FQuestRules::StateFor(TEXT("D_A"), Rows[TEXT("D_A")], 99, Marked), EQuestState::Claimed);
	TestEqual(TEXT("unclaimed daily row at target is Claimable"), FQuestRules::StateFor(TEXT("D_B"), Rows[TEXT("D_B")], 1, Marked), EQuestState::Claimable);

	// T2 — 일일 롤오버가 주간 상태를 건드리지 않고, 주간 롤오버가 이어서 자기 주기만 비운다
	FQuestSaveData Both;
	Both.DayKey = TEXT("2026-09-13"); Both.WeekKey = TEXT("2026-09-07");
	Both.Progress.Add(TEXT("D_A"), 5);
	Both.Progress.Add(TEXT("W_A"), 5);
	Both.Claimed.Add(TEXT("W_A"));

	TArray<FMissionReward> DailyExpired;
	FQuestRules::Rollover(Both, Rows, EQuestPeriod::Daily, TEXT("2026-09-14"), DailyExpired);
	TestEqual(TEXT("daily rollover: D_A expired"), DailyExpired.Num(), 1);
	TestEqual(TEXT("weekly row progress untouched"), Both.Progress.FindRef(TEXT("W_A")), (int64)5);
	TestTrue(TEXT("weekly claim untouched"), Both.Claimed.Contains(TEXT("W_A")));
	TestEqual(TEXT("weekly key untouched by daily rollover"), Both.WeekKey, FString(TEXT("2026-09-07")));

	TArray<FMissionReward> WeeklyExpired;
	FQuestRules::Rollover(Both, Rows, EQuestPeriod::Weekly, TEXT("2026-09-14"), WeeklyExpired);
	// W_A 는 수령 완료라 만료 대상이 아니다
	TestEqual(TEXT("weekly rollover expires nothing claimed"), WeeklyExpired.Num(), 0);
	TestEqual(TEXT("weekly progress cleared"), Both.Progress.Num(), 0);
	TestFalse(TEXT("weekly claim cleared"), Both.Claimed.Contains(TEXT("W_A")));
	TestEqual(TEXT("weekly key moved"), Both.WeekKey, FString(TEXT("2026-09-14")));
	TestEqual(TEXT("day key kept from the earlier daily rollover"), Both.DayKey, FString(TEXT("2026-09-14")));
	return true;
}

#endif
