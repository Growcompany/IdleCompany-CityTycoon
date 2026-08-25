#include "Misc/AutomationTest.h"
#include "Manager/TrendManagerSubsystem.h"
#include "UI/PitchBoardText.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PitchBoardText;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPitchBoardWordingTest,
	"CGR.Pitch.BoardWording",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPitchBoardWordingTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("기획 받침"), HasBatchim(TEXT("기획")));
	TestTrue(TEXT("그래픽 받침"), HasBatchim(TEXT("그래픽")));
	TestFalse(TEXT("사운드 받침 없음"), HasBatchim(TEXT("사운드")));
	TestFalse(TEXT("QA 라틴"), HasBatchim(TEXT("QA")));
	TestEqual(TEXT("이"), MakeShortageSentence(FText::FromString(TEXT("그래픽"))).ToString(), FString(TEXT("그래픽이 부족합니다")));
	TestEqual(TEXT("가"), MakeShortageSentence(FText::FromString(TEXT("사운드"))).ToString(), FString(TEXT("사운드가 부족합니다")));
	TestEqual(TEXT("QA 가"), MakeShortageSentence(FText::FromString(TEXT("QA"))).ToString(), FString(TEXT("QA가 부족합니다")));
	TestEqual(TEXT("타일 한 단어"), MakeShortageWord(FText::FromString(TEXT("개발"))).ToString(), FString(TEXT("개발 부족")));
	TestEqual(TEXT("다음 등급 C→B"), NextGradeLetter(TEXT("C")), FString(TEXT("B")));
	TestEqual(TEXT("S 는 없음"), NextGradeLetter(TEXT("S")), FString());
	TestEqual(TEXT("힌트"), MakeNextGradeHint(FText::FromString(TEXT("개발")), TEXT("B")).ToString(), FString(TEXT("개발 투자 시 예상 B")));
	TestEqual(TEXT("최고 등급 문구"), MakeNextGradeHint(FText::FromString(TEXT("개발")), TEXT("")).ToString(), FString(TEXT("현재 팀으로 최고 등급입니다")));
	// 수량 금지 — 문구 어디에도 "명" 이 없어야 한다
	TestFalse(TEXT("수량 없음"), MakeShortageSentence(FText::FromString(TEXT("개발"))).ToString().Contains(TEXT("명")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRPitchBoardRecommendTest,
	"CGR.Pitch.BoardRecommend",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPitchBoardRecommendTest::RunTest(const FString& Parameters)
{
	auto E = [](int32 Idx, bool bRisk, bool bDev, int64 Rev, float Worst)
	{
		FPitchBoardEntry R; R.ProjectIndex = Idx; R.bHasOutlook = true; R.bGateRisk = bRisk; R.bDeveloped = bDev;
		R.EstimatedRevenue = Rev; R.WorstRatio = Worst; return R;
	};
	// 1) 통과·미개발 중 수익 최대
	{
		TArray<FPitchBoardEntry> L = { E(1, false, true, 900, 3.f), E(2, false, false, 300, 1.2f), E(3, false, false, 500, 0.9f), E(4, true, false, 999, 0.4f) };
		TestEqual(TEXT("통과·미개발 수익 최대 = #3"), ComputeRecommendedIndex(L), 3);
	}
	// 2) 통과·미개발 없음 → 통과·개발함 수익 최대
	{
		TArray<FPitchBoardEntry> L = { E(1, false, true, 900, 3.f), E(2, true, false, 300, 0.45f) };
		TestEqual(TEXT("재개발 폴백 = #1"), ComputeRecommendedIndex(L), 1);
	}
	// 3) 통과 없음 → INDEX_NONE, 가장 가까운 미달 = WorstRatio 최대
	{
		TArray<FPitchBoardEntry> L = { E(2, true, false, 300, 0.45f), E(3, true, false, 500, 0.49f) };
		TestEqual(TEXT("추천 없음"), ComputeRecommendedIndex(L), (int32)INDEX_NONE);
		TestEqual(TEXT("가까운 미달 = #3"), ComputeClosestShortIndex(L), 3);
	}
	// 4) 수익 동률은 번호 작은 쪽
	{
		TArray<FPitchBoardEntry> L = { E(5, false, false, 500, 1.f), E(2, false, false, 500, 1.f) };
		TestEqual(TEXT("동률 = #2"), ComputeRecommendedIndex(L), 2);
	}
	// 5) 전망 없음(로스터 0)은 후보 제외
	{
		FPitchBoardEntry NoOutlook; NoOutlook.ProjectIndex = 7; NoOutlook.bHasOutlook = false; NoOutlook.bGateRisk = false;
		TArray<FPitchBoardEntry> L = { NoOutlook };
		TestEqual(TEXT("전망 없음 제외"), ComputeRecommendedIndex(L), (int32)INDEX_NONE);
	}
	// 6) 음수 수익도 후보다 — 초기값 센티널(-1)이 삼키면 안 된다
	{
		TArray<FPitchBoardEntry> L = { E(3, false, false, -500, 0.f), E(9, false, false, -200, 0.f) };
		TestEqual(TEXT("음수끼리는 덜 나쁜 쪽 = #9"), ComputeRecommendedIndex(L), 9);
	}
	// 7) 수익이 정확히 -1 인 단독 후보도 뽑힌다
	{
		TArray<FPitchBoardEntry> L = { E(3, false, false, -1, 0.f) };
		TestEqual(TEXT("수익 -1 단독 = #3"), ComputeRecommendedIndex(L), 3);
	}
	// 8) 달성률 0 동률도 번호 작은 쪽 — BestRatio 센티널에 먹히지 않는다
	{
		TArray<FPitchBoardEntry> L = { E(8, true, false, 0, 0.f), E(4, true, false, 0, 0.f) };
		TestEqual(TEXT("달성률 0 동률 = #4"), ComputeClosestShortIndex(L), 4);
	}
	// 9) 후보가 하나도 없으면 추천도 가장 가까운 미달도 없다
	{
		const TArray<FPitchBoardEntry> Empty;
		TestEqual(TEXT("빈 배열 추천"), ComputeRecommendedIndex(Empty), (int32)INDEX_NONE);
		TestEqual(TEXT("빈 배열 미달"), ComputeClosestShortIndex(Empty), (int32)INDEX_NONE);
	}
	// 타일 분류
	{
		TestEqual(TEXT("잠금 우선"), (int32)ClassifyTile(E(11, false, false, 1, 2.f), true, false), (int32)ETileVerdict::Locked);
		TestEqual(TEXT("선착수"), (int32)ClassifyTile(E(11, false, false, 1, 2.f), false, true), (int32)ETileVerdict::Preview);
		TestEqual(TEXT("재개발"), (int32)ClassifyTile(E(1, false, true, 1, 2.f), false, false), (int32)ETileVerdict::Redevelop);
		TestEqual(TEXT("통과"), (int32)ClassifyTile(E(2, false, false, 1, 2.f), false, false), (int32)ETileVerdict::Pass);
		TestEqual(TEXT("미달"), (int32)ClassifyTile(E(3, true, false, 1, .4f), false, false), (int32)ETileVerdict::Short);
		// 로스터 0 = 판정 불가(Wait). 미달(Short)로 떨어지면 타일이 레드로 칠해져 없는 결론을 내민다
		FPitchBoardEntry NoOutlookTile; NoOutlookTile.ProjectIndex = 5; NoOutlookTile.bHasOutlook = false; NoOutlookTile.bGateRisk = true;
		TestEqual(TEXT("전망 없음 = 대기"), (int32)ClassifyTile(NoOutlookTile, false, false), (int32)ETileVerdict::Wait);
		NoOutlookTile.bGateRisk = false;
		TestEqual(TEXT("전망 없음은 게이트와 무관하게 대기"), (int32)ClassifyTile(NoOutlookTile, false, false), (int32)ETileVerdict::Wait);
		TestEqual(TEXT("잠금은 대기보다도 우선"), (int32)ClassifyTile(NoOutlookTile, true, false), (int32)ETileVerdict::Locked);
		TestEqual(TEXT("잠금+선착수 동시 = 잠금"), (int32)ClassifyTile(E(11, false, false, 1, 2.f), true, true), (int32)ETileVerdict::Locked);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTrendRerollPickTest,
	"CGR.Pitch.TrendRerollPick",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTrendRerollPickTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Pool = { TEXT("동전"), TEXT("보석"), TEXT("우주") };
	TestNotEqual(TEXT("현재 소재 제외"), UTrendManagerSubsystem::PickTrendFromPool(Pool, TEXT("보석"), 1).ToString(), FString(TEXT("보석")));
	TestEqual(TEXT("인덱스 0 = 동전"), UTrendManagerSubsystem::PickTrendFromPool(Pool, TEXT("보석"), 0).ToString(), FString(TEXT("동전")));
	TestEqual(TEXT("풀 1개 = 변경 없음(None)"), UTrendManagerSubsystem::PickTrendFromPool({ TEXT("보석") }, TEXT("보석"), 0).ToString(), FString(TEXT("None")));
	// 인덱스가 남은 풀 밖으로 나가도 클램프 — 호출부 상한이 어긋나도 마지막 칸이 여기서 살아난다
	TestEqual(TEXT("인덱스 초과 = 마지막(우주)"), UTrendManagerSubsystem::PickTrendFromPool(Pool, TEXT("보석"), 9).ToString(), FString(TEXT("우주")));
	// 제외 소재가 풀에 없으면 뺄 것이 없다 — 마지막 칸(Num()-1)까지 실제로 뽑혀야 한다
	TestEqual(TEXT("제외 미포함 = 마지막 칸 도달"), UTrendManagerSubsystem::PickTrendFromPool(Pool, TEXT("없는소재"), Pool.Num() - 1).ToString(), FString(TEXT("우주")));
	TestEqual(TEXT("제외 미포함 = 첫 칸도 그대로"), UTrendManagerSubsystem::PickTrendFromPool(Pool, TEXT("없는소재"), 0).ToString(), FString(TEXT("동전")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRWorstDisciplineTest,
	"CGR.Pitch.WorstDiscipline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRWorstDisciplineTest::RunTest(const FString& Parameters)
{
	int32 WorstSlot = 0;
	float Ratio = 0.f;

	// 최저 달성률 칸을 고른다
	{
		const TArray<float> Scores  = { 90.f, 60.f, 50.f, 20.f, 80.f, 70.f };
		const TArray<float> Targets = { 100.f, 100.f, 100.f, 100.f, 100.f, 100.f };
		TestTrue(TEXT("고른다"), FindWorstDiscipline(Scores, Targets, WorstSlot, Ratio));
		TestEqual(TEXT("최저 = 슬롯 3"), WorstSlot, 3);
		TestEqual(TEXT("비율 0.2"), Ratio, 0.2f);
	}
	// 목표 0(비활성) 칸은 건너뛴다 — 0/0 을 최저로 잡으면 요구하지도 않는 직능을 처방한다
	{
		const TArray<float> Scores  = { 90.f, 0.f, 50.f, 60.f, 0.f, 70.f };
		const TArray<float> Targets = { 100.f, 0.f, 100.f, 100.f, 0.f, 100.f };
		TestTrue(TEXT("비활성 제외 후에도 고른다"), FindWorstDiscipline(Scores, Targets, WorstSlot, Ratio));
		TestEqual(TEXT("최저 = 슬롯 2"), WorstSlot, 2);
	}
	// 전부 비활성 = 고를 칸이 없다
	{
		const TArray<float> Zero6 = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		TestFalse(TEXT("전부 비활성 = false"), FindWorstDiscipline(Zero6, Zero6, WorstSlot, Ratio));
		TestEqual(TEXT("실패는 INDEX_NONE"), WorstSlot, (int32)INDEX_NONE);
		TestEqual(TEXT("실패는 비율 0"), Ratio, 0.f);
	}
	// 크기가 6이 아니면 계산 자체를 하지 않는다
	{
		const TArray<float> Three = { 1.f, 1.f, 1.f };
		const TArray<float> Six   = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
		TestFalse(TEXT("점수 크기 불일치"), FindWorstDiscipline(Three, Six, WorstSlot, Ratio));
		TestFalse(TEXT("목표 크기 불일치"), FindWorstDiscipline(Six, Three, WorstSlot, Ratio));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRRoundsToLevelUpTest,
	"CGR.Pitch.RoundsToLevelUp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRRoundsToLevelUpTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("150 중 0, 판당 75 = 2"), ComputeRoundsToLevelUp(0.f, 150.f, 75.f), 2);
	TestEqual(TEXT("150 중 100, 판당 75 = 1"), ComputeRoundsToLevelUp(100.f, 150.f, 75.f), 1);
	TestEqual(TEXT("이미 도달 = 1 (다음 판에 레벨업)"), ComputeRoundsToLevelUp(150.f, 150.f, 75.f), 1);
	TestEqual(TEXT("지급 0 = 0"), ComputeRoundsToLevelUp(0.f, 150.f, 0.f), 0);
	return true;
}

#endif
