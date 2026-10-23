#include "Misc/AutomationTest.h"
#include "Utils/GachaRevealModel.h"

#if WITH_DEV_AUTOMATION_TESTS

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌
namespace GachaRevealModelTestUtil
{
	static TArray<FGachaRevealEntry> EntriesFromGrades(std::initializer_list<int32> Grades)
	{
		TArray<FGachaRevealEntry> Entries;
		for (const int32 Grade : Grades)
		{
			FGachaRevealEntry& E = Entries.AddDefaulted_GetRef();
			E.PullIndex = Entries.Num() - 1;
			E.Grade = Grade;
		}
		return Entries;
	}

	static FGachaRevealEntry EquipEntry(int32 PullIndex, int32 Grade, int32 SlotKey, int32 EquippedGrade, bool bEmployeeKnown)
	{
		FGachaRevealEntry E;
		E.PullIndex = PullIndex;
		E.Grade = Grade;
		E.SlotKey = SlotKey;
		E.bEmployeeKnown = bEmployeeKnown;
		E.EquippedGrade = EquippedGrade;
		return E;
	}

	static FString JoinIndices(const TArray<int32>& Values)
	{
		return FString::JoinBy(Values, TEXT(","), [](int32 V) { return FString::FromInt(V); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelTopTest,
	"CGR.Gacha.Reveal.Model.Top",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelTopTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	TestEqual(TEXT("최고 SS 동률 = 먼저 뽑힌 2"), GachaRevealModel::FindTopIndex(EntriesFromGrades({0, 3, 4, 1, 4})), 2);
	TestEqual(TEXT("최고 A 이하 = 단독 장면 없음"), GachaRevealModel::FindTopIndex(EntriesFromGrades({0, 1, 2})), INDEX_NONE);
	TestEqual(TEXT("빈 결과"), GachaRevealModel::FindTopIndex(TArray<FGachaRevealEntry>()), INDEX_NONE);

	// 배열 순서가 아니라 PullIndex 가 「먼저」를 정한다
	TArray<FGachaRevealEntry> Swapped = EntriesFromGrades({5, 5});
	Swapped[0].PullIndex = 7;
	Swapped[1].PullIndex = 3;
	TestEqual(TEXT("동률은 PullIndex 작은 쪽"), GachaRevealModel::FindTopIndex(Swapped), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelStampGroupsTest,
	"CGR.Gacha.Reveal.Model.StampGroups",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelStampGroupsTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	const TArray<TPair<int32, TArray<int32>>> Groups = GachaRevealModel::BuildStampGroups(EntriesFromGrades({3, 5, 3, 4, 0, 4}), 1);
	if (!TestEqual(TEXT("묶음 수(Top 의 SSS 는 빠짐)"), Groups.Num(), 2)) { return false; }
	TestEqual(TEXT("첫 묶음 = S"), Groups[0].Key, 3);
	TestEqual(TEXT("S 묶음 카드"), JoinIndices(Groups[0].Value), FString(TEXT("0,2")));
	TestEqual(TEXT("둘째 묶음 = SS"), Groups[1].Key, 4);
	TestEqual(TEXT("SS 묶음 카드"), JoinIndices(Groups[1].Value), FString(TEXT("3,5")));
	TestEqual(TEXT("S 이상 없음 = 묶음 없음"), GachaRevealModel::BuildStampGroups(EntriesFromGrades({0, 2, 1}), INDEX_NONE).Num(), 0);

	// Top 과 같은 등급의 다른 카드는 도장으로 남는다
	const TArray<TPair<int32, TArray<int32>>> SameAsTop = GachaRevealModel::BuildStampGroups(EntriesFromGrades({4, 3, 4}), 0);
	if (!TestEqual(TEXT("Top 동급 묶음 수"), SameAsTop.Num(), 2)) { return false; }
	TestEqual(TEXT("Top 동급 S 묶음"), JoinIndices(SameAsTop[0].Value), FString(TEXT("1")));
	TestEqual(TEXT("Top 동급 SS 묶음 = Top 만 빠짐"), JoinIndices(SameAsTop[1].Value), FString(TEXT("2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelUpgradeTest,
	"CGR.Gacha.Reveal.Model.Upgrade",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelUpgradeTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	TestEqual(TEXT("A → 승급 없음"), GachaRevealModel::UpgradeSteps(2).Num(), 0);
	TestEqual(TEXT("C → 승급 없음"), GachaRevealModel::UpgradeSteps(0).Num(), 0);
	TestEqual(TEXT("S → A,S"), JoinIndices(GachaRevealModel::UpgradeSteps(3)), FString(TEXT("2,3")));
	TestEqual(TEXT("SS → S,SS"), JoinIndices(GachaRevealModel::UpgradeSteps(4)), FString(TEXT("3,4")));
	TestEqual(TEXT("SSS → S,SS,SSS"), JoinIndices(GachaRevealModel::UpgradeSteps(5)), FString(TEXT("3,4,5")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelSortTest,
	"CGR.Gacha.Reveal.Model.Sort",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelSortTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	TArray<FGachaRevealEntry> Entries = EntriesFromGrades({1, 3, 3, 0});
	Entries[0].bNew = true;
	Entries[2].bNew = true;
	const TSet<int32> NoRecommended;
	TestEqual(TEXT("등급순: 등급 내림 → NEW 먼저"), JoinIndices(GachaRevealModel::SortedOrder(Entries, EGachaRevealSort::Grade, NoRecommended)), FString(TEXT("2,1,0,3")));
	TestEqual(TEXT("뽑은 순"), JoinIndices(GachaRevealModel::SortedOrder(Entries, EGachaRevealSort::PullOrder, NoRecommended)), FString(TEXT("0,1,2,3")));

	// 같은 등급·NEW 같음 → 추천 먼저, NEW 는 추천보다 앞
	TArray<FGachaRevealEntry> Ties = EntriesFromGrades({3, 3, 3});
	Ties[0].bNew = true;
	const TSet<int32> Recommended = {2};
	TestEqual(TEXT("NEW → 추천 → 뽑은 순"), JoinIndices(GachaRevealModel::SortedOrder(Ties, EGachaRevealSort::Grade, Recommended)), FString(TEXT("0,2,1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelRecommendPerSlotTest,
	"CGR.Gacha.Reveal.Model.Recommend.PerSlot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelRecommendPerSlotTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	constexpr int32 Head = 0;
	constexpr int32 Glasses = 1;
	constexpr int32 Hand = 3;
	const TArray<FGachaRevealEntry> Entries = {
		EquipEntry(0, 3, Head, 1, true),
		EquipEntry(1, 4, Head, 1, true),
		EquipEntry(2, 2, Glasses, 0, true),
		EquipEntry(3, 0, Hand, 0, true)};
	TestEqual(TEXT("머리 최고 SS 1개 + 안경 A, 손 C 는 장착 C 와 같아 제외"), JoinIndices(GachaRevealModel::PickRecommended(Entries)), FString(TEXT("1,2")));

	const TArray<FGachaRevealEntry> Tie = {EquipEntry(0, 4, Head, 1, true), EquipEntry(1, 4, Head, 1, true)};
	TestEqual(TEXT("부위 안 동률 = 먼저 뽑힌 것"), JoinIndices(GachaRevealModel::PickRecommended(Tie)), FString(TEXT("0")));

	const TArray<FGachaRevealEntry> NoSlot = {EquipEntry(0, 5, INDEX_NONE, 0, true)};
	TestEqual(TEXT("부위 없는 결과는 추천 없음"), GachaRevealModel::PickRecommended(NoSlot).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelRecommendNoEmployeeTest,
	"CGR.Gacha.Reveal.Model.Recommend.NoEmployee",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelRecommendNoEmployeeTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	// 직원을 알았다면 전부 추천됐을 결과(빈 부위·낮은 장착) ― 직원을 모르면 하나도 없어야 한다
	const TArray<FGachaRevealEntry> Entries = {
		EquipEntry(0, 5, 0, INDEX_NONE, false),
		EquipEntry(1, 4, 1, 0, false),
		EquipEntry(2, 3, 2, 1, false)};
	TestEqual(TEXT("직원 없이 연 화면 = 추천 0"), GachaRevealModel::PickRecommended(Entries).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelRecommendEmptySlotTest,
	"CGR.Gacha.Reveal.Model.Recommend.EmptySlot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelRecommendEmptySlotTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	constexpr int32 Hand = 3;
	const TArray<FGachaRevealEntry> Entries = {EquipEntry(0, 0, Hand, INDEX_NONE, true)};
	TestEqual(TEXT("빈 부위에는 C 도 추천"), JoinIndices(GachaRevealModel::PickRecommended(Entries)), FString(TEXT("0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelSlotsTest,
	"CGR.Gacha.Reveal.Model.Slots",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelSlotsTest::RunTest(const FString& Parameters)
{
	struct FSlotCase { float CanvasW; int32 Count; bool bSettled; float FirstX, FirstY, FirstW, FirstH, LastX, LastY; };
	// 다회 기대값은 기존 그리드 시안에서, 1회는 승인된 작은 카드 배치에서 가져온다.
	const FSlotCase Cases[] = {
		{2304.f, 1, false, 1052.f, 460.f, 200.f, 200.f, 1052.f, 460.f},
		{2304.f, 1, true, 812.f, 460.f, 200.f, 200.f, 812.f, 460.f},
		{2304.f, 10, false, 375.f, 403.f, 290.f, 348.f, 1639.f, 777.f},
		{2304.f, 10, true, 635.f, 403.f, 290.f, 348.f, 1899.f, 777.f},
		{2304.f, 30, false, 200.f, 292.f, 224.f, 224.f, 1640.f, 1012.f},
		{2304.f, 30, true, 580.f, 362.f, 189.f, 189.f, 1810.f, 977.f},
		{2560.f, 10, true, 763.f, 403.f, 290.f, 348.f, 2027.f, 777.f},
		{2560.f, 30, false, 328.f, 292.f, 224.f, 224.f, 1768.f, 1012.f},
		{2560.f, 30, true, 580.f, 298.f, 221.f, 221.f, 2002.f, 1009.f},
		{3120.f, 30, true, 848.f, 292.f, 224.f, 224.f, 2288.f, 1012.f},
		// 공식 경계: 폭 floor(188.5), 높이 round(345.6)
		{2300.f, 30, true, 582.f, 364.f, 188.f, 188.f, 1806.f, 976.f},
		{2264.f, 10, true, 620.f, 405.f, 288.f, 346.f, 1876.f, 777.f},
	};
	constexpr float Tolerance = 0.01f;
	for (const FSlotCase& Case : Cases)
	{
		const FString Label = FString::Printf(TEXT("W%.0f n%d %s"), Case.CanvasW, Case.Count, Case.bSettled ? TEXT("정리") : TEXT("공개"));
		const TArray<FGachaRevealRect> Rects = GachaRevealModel::ComputeSlots(Case.Count, Case.CanvasW, Case.bSettled);
		if (!TestEqual(*(Label + TEXT(" 칸 수")), Rects.Num(), Case.Count)) { continue; }
		const FGachaRevealRect& First = Rects[0];
		const FGachaRevealRect& Last = Rects.Last();
		TestEqual(*(Label + TEXT(" 첫 x")), First.X, Case.FirstX, Tolerance);
		TestEqual(*(Label + TEXT(" 첫 y")), First.Y, Case.FirstY, Tolerance);
		TestEqual(*(Label + TEXT(" 첫 w")), First.W, Case.FirstW, Tolerance);
		TestEqual(*(Label + TEXT(" 첫 h")), First.H, Case.FirstH, Tolerance);
		TestEqual(*(Label + TEXT(" 끝 x")), Last.X, Case.LastX, Tolerance);
		TestEqual(*(Label + TEXT(" 끝 y")), Last.Y, Case.LastY, Tolerance);
	}
	TestEqual(TEXT("0장 = 빈 배열"), GachaRevealModel::ComputeSlots(0, 2560.f, false).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelSlotsPartialTest,
	"CGR.Gacha.Reveal.Model.SlotsPartial",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelSlotsPartialTest::RunTest(const FString& Parameters)
{
	// 일부만 지급: 2..10 = 10회 배치, 11..30 = 30회 배치. 기대값 = 시안 slots() 에 그 등급을 넣은 값
	constexpr float Tolerance = 0.01f;
	struct FSlotXY { float X, Y; };

	// 7장: 첫 줄 5장(10장일 때와 같은 자리), 둘째 줄 2장 가운데 ― rw = 2*290+26 = 606 → x = 100 + (2360-606)/2 = 977
	const TArray<FGachaRevealRect> Seven = GachaRevealModel::ComputeSlots(7, 2560.f, false);
	const FSlotXY SevenXY[] = {{503.f, 403.f}, {819.f, 403.f}, {1135.f, 403.f}, {1451.f, 403.f}, {1767.f, 403.f}, {977.f, 777.f}, {1293.f, 777.f}};
	if (TestEqual(TEXT("7장 칸 수"), Seven.Num(), 7))
	{
		for (int32 i = 0; i < Seven.Num(); ++i)
		{
			TestEqual(*FString::Printf(TEXT("7장 [%d] x"), i), Seven[i].X, SevenXY[i].X, Tolerance);
			TestEqual(*FString::Printf(TEXT("7장 [%d] y"), i), Seven[i].Y, SevenXY[i].Y, Tolerance);
			TestEqual(*FString::Printf(TEXT("7장 [%d] w"), i), Seven[i].W, 290.f, Tolerance);
			TestEqual(*FString::Printf(TEXT("7장 [%d] h"), i), Seven[i].H, 348.f, Tolerance);
		}
	}

	// 20장: 30회 배치 8열 3줄, 마지막 줄 4장 가운데 ― rw = 4*224+3*16 = 944 → x = 100 + (2360-944)/2 = 808
	const TArray<FGachaRevealRect> Twenty = GachaRevealModel::ComputeSlots(20, 2560.f, false);
	if (TestEqual(TEXT("20장 칸 수"), Twenty.Num(), 20))
	{
		TestEqual(TEXT("20장 w"), Twenty[0].W, 224.f, Tolerance);
		TestEqual(TEXT("20장 h"), Twenty[0].H, 224.f, Tolerance);
		TestEqual(TEXT("20장 첫 줄 x"), Twenty[0].X, 328.f, Tolerance);
		TestEqual(TEXT("20장 1줄 y"), Twenty[0].Y, 412.f, Tolerance);
		TestEqual(TEXT("20장 2줄 y"), Twenty[8].Y, 652.f, Tolerance);
		TestEqual(TEXT("20장 3줄 y"), Twenty[16].Y, 892.f, Tolerance);
		const float LastRowX[] = {808.f, 1048.f, 1288.f, 1528.f};
		for (int32 i = 0; i < 4; ++i)
		{
			TestEqual(*FString::Printf(TEXT("20장 마지막 줄 [%d] x"), i), Twenty[16 + i].X, LastRowX[i], Tolerance);
			TestEqual(*FString::Printf(TEXT("20장 마지막 줄 [%d] y"), i), Twenty[16 + i].Y, 892.f, Tolerance);
		}
	}

	// 정리 상태: 「이번 최고」 열 폭도 배치 등급을 따른다(2..10 = 470, 11..30 = 430)
	struct FSettledCase { int32 Count; float FirstX, FirstY, FirstW, FirstH, LastX, LastY; };
	const FSettledCase SettledCases[] = {
		{7, 763.f, 403.f, 290.f, 348.f, 1553.f, 777.f},
		{20, 580.f, 416.5f, 221.f, 221.f, 1765.f, 890.5f},
	};
	for (const FSettledCase& Case : SettledCases)
	{
		const FString Label = FString::Printf(TEXT("%d장 정리"), Case.Count);
		const TArray<FGachaRevealRect> Rects = GachaRevealModel::ComputeSlots(Case.Count, 2560.f, true);
		if (!TestEqual(*(Label + TEXT(" 칸 수")), Rects.Num(), Case.Count)) { continue; }
		TestEqual(*(Label + TEXT(" 첫 x")), Rects[0].X, Case.FirstX, Tolerance);
		TestEqual(*(Label + TEXT(" 첫 y")), Rects[0].Y, Case.FirstY, Tolerance);
		TestEqual(*(Label + TEXT(" 첫 w")), Rects[0].W, Case.FirstW, Tolerance);
		TestEqual(*(Label + TEXT(" 첫 h")), Rects[0].H, Case.FirstH, Tolerance);
		TestEqual(*(Label + TEXT(" 끝 x")), Rects.Last().X, Case.LastX, Tolerance);
		TestEqual(*(Label + TEXT(" 끝 y")), Rects.Last().Y, Case.LastY, Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealModelTallyTest,
	"CGR.Gacha.Reveal.Model.Tally",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealModelTallyTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealModelTestUtil;
	TestEqual(TEXT("고급 장비 가중치 + 결과 {A,S}"), JoinIndices(GachaRevealModel::TallyGrades({0, 0, 5500, 3500, 800, 200}, EntriesFromGrades({2, 3, 3}))), FString(TEXT("5,4,3,2")));
	TestEqual(TEXT("결과에 없어도 가중치>0 이면 표시"), JoinIndices(GachaRevealModel::TallyGrades({45, 30, 18, 6, 1, 0}, EntriesFromGrades({1, 2}))), FString(TEXT("4,3,2,1,0")));
	TestEqual(TEXT("가중치 0 이어도 나온 등급은 표시"), JoinIndices(GachaRevealModel::TallyGrades({0, 30, 50, 17, 3, 0}, EntriesFromGrades({0, 2}))), FString(TEXT("4,3,2,1,0")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
