#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/TableManagerSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CGRGoalTrackerDisplay
{
void BuildDisplaySet(
	const TArray<FGoalBoardEntry>& InAllEntries,
	FName InTrackedGoalID,
	FName InExpandedGoalID,
	bool bInRowsExpanded,
	TArray<FGoalBoardEntry>& OutVisible,
	int32& OutClaimedCount,
	int32& OutHiddenCount);
}

namespace CGRGoalGuideStep
{
int32 ResolveStatInvestStep(bool bHasInvestedAny, bool bInvestCellEnabled);
int32 ResolveRaiseFloorStep(bool bInOffice, bool bManagePanelOpen, bool bEnhancementTabActive);
}

struct FGoalTableRowOrderTestAccessor
{
	static void Initialize(UTableManagerSubsystem& TableMgr, UDataTable* GoalTable)
	{
		TableMgr.GoalDataTable = GoalTable;
		TableMgr.InitializeGoalTable();
	}
};

struct FGoalBoardEntryOrderTestAccessor
{
	static void BuildEntries(
		const UGoalBoardSubsystem& GoalBoard,
		const UTableManagerSubsystem& TableMgr,
		TArray<FGoalBoardEntry>& OutEntries)
	{
		GoalBoard.BuildBoardEntriesFromTableManager(TableMgr, OutEntries);
	}
};

namespace
{
FGoalBoardEntry MakeEntry(const TCHAR* InID, EGoalState InState)
{
	FGoalBoardEntry Entry;
	Entry.GoalID = FName(InID);
	Entry.State = InState;
	return Entry;
}

// 언락 직후 라이브 구성 — 8 진행 + G5/G11/G6 잠금
TArray<FGoalBoardEntry> MakeUnlockMomentEntries()
{
	return {
		MakeEntry(TEXT("G1_RaiseFloor"), EGoalState::InProgress),
		MakeEntry(TEXT("G2_HQLevel3"), EGoalState::InProgress),
		MakeEntry(TEXT("G3_SecondBuilding"), EGoalState::InProgress),
		MakeEntry(TEXT("G4_AcquireCompany"), EGoalState::InProgress),
		MakeEntry(TEXT("G5_DemolishCompany"), EGoalState::Locked),
		MakeEntry(TEXT("G11_AcquirePlot"), EGoalState::Locked),
		MakeEntry(TEXT("G6_BuildOnClearedPlot"), EGoalState::Locked),
		MakeEntry(TEXT("G9_EnhanceStat"), EGoalState::InProgress),
		MakeEntry(TEXT("G10_EquipTrait"), EGoalState::InProgress),
		MakeEntry(TEXT("G7_PlacePainting"), EGoalState::InProgress),
		MakeEntry(TEXT("G8_EquipSkin"), EGoalState::InProgress),
	};
}

// GoalID 사전순과 입력 순서가 다른 픽스처 — 표시 계층이 입력을 재정렬하는 회귀를 잡는다.
TArray<FGoalBoardEntry> MakeNonLexicalEntries()
{
	return {
		MakeEntry(TEXT("GS_Third"), EGoalState::InProgress),
		MakeEntry(TEXT("GS_First"), EGoalState::InProgress),
		MakeEntry(TEXT("GS_Second"), EGoalState::InProgress),
	};
}

// 접힘 분기(표시 대상 >= 임계 6)를 태우면서 GoalID 사전순과 입력 순서가 다른 픽스처.
// BuildDisplaySet 은 GetBoardEntries 가 넘긴 DT 행 순서를 존중해야 한다.
TArray<FGoalBoardEntry> MakeShuffledCollapseEntries()
{
	return {
		MakeEntry(TEXT("GX_Charlie"), EGoalState::InProgress),
		MakeEntry(TEXT("GX_Foxtrot"), EGoalState::InProgress),
		MakeEntry(TEXT("GX_Bravo"), EGoalState::InProgress),
		MakeEntry(TEXT("GX_Alpha"), EGoalState::InProgress),
		MakeEntry(TEXT("GX_Echo"), EGoalState::InProgress),
		MakeEntry(TEXT("GX_Delta"), EGoalState::InProgress),
	};
}

bool ContainsGoal(const TArray<FGoalBoardEntry>& InEntries, const TCHAR* InID)
{
	const FName Target(InID);
	return InEntries.ContainsByPredicate(
		[Target](const FGoalBoardEntry& E) { return E.GoalID == Target; });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalBoardDataTableRowOrderPreservedTest,
	"CGR.GoalBoard.DataTableRowOrderPreserved",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalBoardDataTableRowOrderPreservedTest::RunTest(const FString& Parameters)
{
	UDataTable* FixtureTable = NewObject<UDataTable>();
	FixtureTable->RowStruct = FGoalTable::StaticStruct();

	const TArray<FName> ExpectedOrder = {
		TEXT("G2_HQLevel3"),
		TEXT("G11_AcquirePlot"),
		TEXT("G1_RaiseFloor")
	};
	FGoalTable FixtureRow;
	FixtureRow.ConditionType = EMissionConditionType::None;
	for (const FName GoalID : ExpectedOrder)
	{
		FixtureTable->AddRow(GoalID, FixtureRow);
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UTableManagerSubsystem* TableMgr = NewObject<UTableManagerSubsystem>(GameInstance);
	UGoalBoardSubsystem* GoalBoard = NewObject<UGoalBoardSubsystem>(GameInstance);
	FGoalTableRowOrderTestAccessor::Initialize(*TableMgr, FixtureTable);
	TArray<FGoalBoardEntry> ActualEntries;
	FGoalBoardEntryOrderTestAccessor::BuildEntries(*GoalBoard, *TableMgr, ActualEntries);

	if (TestEqual(TEXT("GoalBoard 엔트리 수가 DT 입력과 같다"), ActualEntries.Num(), ExpectedOrder.Num()))
	{
		for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
		{
			TestEqual(TEXT("GoalBoard 최종 엔트리는 DT 입력 순서를 보존한다"),
				ActualEntries[Index].GoalID, ExpectedOrder[Index]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerLockedRowsExcludedTest,
	"CGR.GoalTracker.LockedRowsExcluded",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerLockedRowsExcludedTest::RunTest(const FString& Parameters)
{
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;
	// 이 테스트는 접힘을 검사하지 않으므로 전량 노출로 고정한다
	CGRGoalTrackerDisplay::BuildDisplaySet(
		MakeUnlockMomentEntries(), NAME_None, NAME_None, true, Visible, ClaimedCount, HiddenCount);

	// 이후 단언이 인덱스로 들어가므로 여기서 끊는다 — TArray::operator[] 는 check() 라 실패가 아니라 크래시다
	if (!TestEqual(TEXT("언락 직후 표시 행은 잠금 3건을 뺀 8행이다"), Visible.Num(), 8))
	{
		return true;
	}
	TestFalse(TEXT("잠금 G5 는 리스트에 없다"), ContainsGoal(Visible, TEXT("G5_DemolishCompany")));
	TestFalse(TEXT("잠금 G11 은 리스트에 없다"), ContainsGoal(Visible, TEXT("G11_AcquirePlot")));
	TestFalse(TEXT("잠금 G6 는 리스트에 없다"), ContainsGoal(Visible, TEXT("G6_BuildOnClearedPlot")));
	TestTrue(TEXT("진행 중 G1 은 리스트에 있다"), ContainsGoal(Visible, TEXT("G1_RaiseFloor")));

	for (const FGoalBoardEntry& Entry : Visible)
	{
		TestTrue(TEXT("표시 행에 Locked 상태가 섞이지 않는다"), Entry.State != EGoalState::Locked);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerClaimedCountIgnoresLockedTest,
	"CGR.GoalTracker.ClaimedCountIgnoresLocked",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerClaimedCountIgnoresLockedTest::RunTest(const FString& Parameters)
{
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;

	// 회귀 방어 — 잠금을 완료로 빼면 여기서 3 이 나온다(헤더 "3/11 완료")
	CGRGoalTrackerDisplay::BuildDisplaySet(
		MakeUnlockMomentEntries(), NAME_None, NAME_None, true, Visible, ClaimedCount, HiddenCount);
	TestEqual(TEXT("잠금 3건이 있어도 언락 직후 완료 수는 0이다"), ClaimedCount, 0);

	TArray<FGoalBoardEntry> Mixed = MakeUnlockMomentEntries();
	Mixed[0].State = EGoalState::Claimed;
	Mixed[1].State = EGoalState::Claimed;
	Mixed[2].State = EGoalState::Claimed;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Mixed, NAME_None, NAME_None, true, Visible, ClaimedCount, HiddenCount);
	TestEqual(TEXT("3건 수령 시 완료 수는 3이다"), ClaimedCount, 3);
	TestEqual(TEXT("3건 수령 + 잠금 3건이면 표시 행은 5행이다"), Visible.Num(), 5);

	// 언락 전 RefreshAll 이 빈 배열로 도는 경로 — 뺄셈 유도였다면 여기서 11 이 나온다
	TArray<FGoalBoardEntry> Empty;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Empty, NAME_None, NAME_None, true, Visible, ClaimedCount, HiddenCount);
	TestEqual(TEXT("빈 입력이면 완료 수는 0이다"), ClaimedCount, 0);
	TestEqual(TEXT("빈 입력이면 표시 행도 0이다"), Visible.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerInputOrderPreservedTest,
	"CGR.GoalTracker.InputOrderPreserved",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerInputOrderPreservedTest::RunTest(const FString& Parameters)
{
	const TArray<FGoalBoardEntry> Input = MakeNonLexicalEntries();
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Input, NAME_None, NAME_None, true, Visible, ClaimedCount, HiddenCount);

	// 행 순서의 소유자는 GetBoardEntries 다 — 이 함수는 걸러내기만 하고 재정렬하지 않는다.
	if (!TestEqual(TEXT("거를 행이 없으므로 입력 3행이 그대로 나온다"), Visible.Num(), Input.Num()))
	{
		return true;
	}
	for (int32 Index = 0; Index < Visible.Num(); ++Index)
	{
		TestEqual(TEXT("표시 순서는 입력 순서 그대로다"),
			Visible[Index].GoalID, Input[Index].GoalID);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerCollapsedTopThreeTest,
	"CGR.GoalTracker.CollapsedTopThree",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerCollapsedTopThreeTest::RunTest(const FString& Parameters)
{
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		MakeUnlockMomentEntries(), NAME_None, NAME_None, false, Visible, ClaimedCount, HiddenCount);

	if (!TestEqual(TEXT("접힘 기본 노출은 상위 3행이다"), Visible.Num(), 3))
	{
		return true;
	}
	TestEqual(TEXT("나머지 5행은 더보기 안에 있다"), HiddenCount, 5);
	TestEqual(TEXT("1행은 G1 이다"), Visible[0].GoalID, FName(TEXT("G1_RaiseFloor")));
	TestEqual(TEXT("2행은 G2 이다"), Visible[1].GoalID, FName(TEXT("G2_HQLevel3")));
	TestEqual(TEXT("3행은 G3 이다"), Visible[2].GoalID, FName(TEXT("G3_SecondBuilding")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerClaimableAlwaysVisibleTest,
	"CGR.GoalTracker.ClaimableAlwaysVisible",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerClaimableAlwaysVisibleTest::RunTest(const FString& Parameters)
{
	// 입력 마지막 G8 이 수령 가능해지면 접힘 상태에서도 반드시 보여야 한다.
	// [수령] 버튼이 행 안에 있어서, 숨기면 수령 경로가 통째로 막힌다
	TArray<FGoalBoardEntry> Entries = MakeUnlockMomentEntries();
	Entries.Last().State = EGoalState::Claimable;

	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Entries, NAME_None, NAME_None, false, Visible, ClaimedCount, HiddenCount);

	TestEqual(TEXT("상위 3 + 수령가능 1 = 4행이 보인다"), Visible.Num(), 4);
	TestTrue(TEXT("수령 가능한 G8 은 접힘 상태에서도 노출된다"),
		ContainsGoal(Visible, TEXT("G8_EquipSkin")));
	TestEqual(TEXT("숨김 수는 4다"), HiddenCount, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerTrackedAndExpandedVisibleTest,
	"CGR.GoalTracker.TrackedAndExpandedVisible",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerTrackedAndExpandedVisibleTest::RunTest(const FString& Parameters)
{
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;

	// 안내 중인 행이 사라지면 [안내 끄기]에 도달할 수 없다
	CGRGoalTrackerDisplay::BuildDisplaySet(
		MakeUnlockMomentEntries(), FName(TEXT("G9_EnhanceStat")), NAME_None,
		false, Visible, ClaimedCount, HiddenCount);
	TestTrue(TEXT("안내 중인 G9 는 접힘 상태에서도 노출된다"),
		ContainsGoal(Visible, TEXT("G9_EnhanceStat")));
	TestEqual(TEXT("상위 3 + 추적 1 = 4행"), Visible.Num(), 4);

	// 펼친 행이 접힘으로 사라지는 것은 조작에 대한 배신
	CGRGoalTrackerDisplay::BuildDisplaySet(
		MakeUnlockMomentEntries(), NAME_None, FName(TEXT("G7_PlacePainting")),
		false, Visible, ClaimedCount, HiddenCount);
	TestTrue(TEXT("펼친 G7 은 접힘 상태에서도 노출된다"),
		ContainsGoal(Visible, TEXT("G7_PlacePainting")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerThresholdTest,
	"CGR.GoalTracker.Threshold",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerThresholdTest::RunTest(const FString& Parameters)
{
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;

	// 표시 대상 5개 = 임계값(6) 미만 → 접지 않는다. 부담이 큰 구간에서만 개입한다
	TArray<FGoalBoardEntry> Five = MakeUnlockMomentEntries();
	Five[0].State = EGoalState::Claimed;
	Five[1].State = EGoalState::Claimed;
	Five[2].State = EGoalState::Claimed;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Five, NAME_None, NAME_None, false, Visible, ClaimedCount, HiddenCount);
	TestEqual(TEXT("표시 대상 5개면 전량 노출한다"), Visible.Num(), 5);
	TestEqual(TEXT("전량 노출이면 숨김 수는 0이다"), HiddenCount, 0);

	// 6개 = 임계값 → 접는다
	TArray<FGoalBoardEntry> Six = MakeUnlockMomentEntries();
	Six[0].State = EGoalState::Claimed;
	Six[1].State = EGoalState::Claimed;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Six, NAME_None, NAME_None, false, Visible, ClaimedCount, HiddenCount);
	TestEqual(TEXT("표시 대상 6개면 상위 3행만 노출한다"), Visible.Num(), 3);
	TestEqual(TEXT("숨김 수는 3이다"), HiddenCount, 3);

	// 사용자가 폈으면 전량
	CGRGoalTrackerDisplay::BuildDisplaySet(
		MakeUnlockMomentEntries(), NAME_None, NAME_None, true, Visible, ClaimedCount, HiddenCount);
	TestEqual(TEXT("펼침 상태면 8행 전부 노출한다"), Visible.Num(), 8);
	TestEqual(TEXT("펼침 상태의 숨김 수는 0이다"), HiddenCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalTrackerCollapsedRespectsInputOrderTest,
	"CGR.GoalTracker.CollapsedRespectsInputOrder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalTrackerCollapsedRespectsInputOrderTest::RunTest(const FString& Parameters)
{
	const TArray<FGoalBoardEntry> Input = MakeShuffledCollapseEntries();
	TArray<FGoalBoardEntry> Visible;
	int32 ClaimedCount = -1;
	int32 HiddenCount = -1;
	CGRGoalTrackerDisplay::BuildDisplaySet(
		Input, NAME_None, NAME_None, false, Visible, ClaimedCount, HiddenCount);

	// 이후 단언이 인덱스로 들어간다 — TArray::operator[] 는 check() 라 실패가 아니라 크래시다
	if (!TestEqual(TEXT("표시 대상 6개면 접힘 분기를 탄다(상위 3행)"), Visible.Num(), 3))
	{
		return true;
	}
	TestEqual(TEXT("숨김 수는 3이다"), HiddenCount, 3);

	// 노출된 상위 3 = 입력 배열의 앞 3개.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestEqual(TEXT("접힘 노출은 입력 앞 3개 그대로다"), Visible[Index].GoalID, Input[Index].GoalID);
	}

	TestFalse(TEXT("입력 4행 GX_Alpha 는 접힌 목록에 노출되지 않는다"), ContainsGoal(Visible, TEXT("GX_Alpha")));
	TestFalse(TEXT("입력 5행 GX_Echo 는 접힌 목록에 노출되지 않는다"), ContainsGoal(Visible, TEXT("GX_Echo")));
	TestFalse(TEXT("입력 6행 GX_Delta 는 접힌 목록에 노출되지 않는다"), ContainsGoal(Visible, TEXT("GX_Delta")));
	return true;
}

// ===== G9 스킬 걸음 전이 (StepLines[2] -> [3]) =====

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalStepStatInvestTest,
	"CGR.GoalStep.StatInvestAdvancesOnFirstPoint",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalStepStatInvestTest::RunTest(const FString& Parameters)
{
	using namespace CGRGoalGuideStep;

	// 아직 안 넣었고 넣을 SP 도 있다 = 투자 유도 걸음
	TestEqual(TEXT("투자 이력 없고 SP 있으면 2번(투자)"), ResolveStatInvestStep(false, true), 2);

	// 1점만 넣어도 다음 걸음 — 전량 소진을 기다리면 SP 가 남은 플레이어가 갇힌다 (이 버그의 재현 조건)
	TestEqual(TEXT("1점 투자 후 SP 가 남아도 3번(별 강화)"), ResolveStatInvestStep(true, true), 3);

	// SP 0 = [+] 비활성 → 지목할 것이 없으니 이력과 무관하게 마지막 걸음
	TestEqual(TEXT("SP 0 이면 이력 없어도 3번"), ResolveStatInvestStep(false, false), 3);
	TestEqual(TEXT("SP 0 + 이력 있으면 3번"), ResolveStatInvestStep(true, false), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGoalStepRaiseFloorTest,
	"CGR.GoalStep.RaiseFloorUsesLiveUIState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGoalStepRaiseFloorTest::RunTest(const FString& Parameters)
{
	using namespace CGRGoalGuideStep;

	TestEqual(TEXT("사무실에서는 나가기 단계"), ResolveRaiseFloorStep(true, false, false), 0);
	TestEqual(TEXT("도시에서 관리 패널이 닫혔으면 건물 선택 단계"), ResolveRaiseFloorStep(false, false, false), 1);
	TestEqual(TEXT("관리 패널에서 강화 탭 전이면 탭 단계"), ResolveRaiseFloorStep(false, true, false), 2);
	TestEqual(TEXT("강화 탭이 열렸으면 빌드업 단계"), ResolveRaiseFloorStep(false, true, true), 3);
	return true;
}

#endif
