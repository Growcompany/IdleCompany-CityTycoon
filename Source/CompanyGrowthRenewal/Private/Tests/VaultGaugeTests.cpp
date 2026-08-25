#include "Misc/AutomationTest.h"
#include "Enum/GaugeHealth.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRVaultGaugeDisplayEligibilityTest,
	"CGR.VaultGauge.DisplayEligibility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRVaultGaugeDisplayEligibilityTest::RunTest(const FString& Parameters)
{
	// Bar 는 프로젝트 시간 축이라 금고 상태(용량/잔량/버블)와 직교한다 —
	// 만액 버블이 떠 있어도 프로젝트는 계속 돌므로 함께 표시한다.
	TestFalse(TEXT("운영 없으면 숨김"), ShouldDisplayVaultGauge(false));
	TestTrue(TEXT("운영 중이면 표시"), ShouldDisplayVaultGauge(true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeOperationProgressTest,
	"CGR.VaultGauge.OperationProgress",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeOperationProgressTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("절반 경과"), ComputeOperationProgress(50.0f, 100.0f), 0.5f);
	TestEqual(TEXT("시작 직후"), ComputeOperationProgress(0.0f, 100.0f), 0.0f);
	TestEqual(TEXT("총시간 0 은 0 (0 나눗셈 방어)"), ComputeOperationProgress(10.0f, 0.0f), 0.0f);
	TestEqual(TEXT("총시간 음수도 0"), ComputeOperationProgress(10.0f, -5.0f), 0.0f);
	TestEqual(TEXT("초과 경과는 1 로 클램프"), ComputeOperationProgress(150.0f, 100.0f), 1.0f);
	TestEqual(TEXT("음수 경과는 0 으로 클램프"), ComputeOperationProgress(-10.0f, 100.0f), 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeHealthClassifyTest,
	"CGR.VaultGauge.ClassifyHealth",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeHealthClassifyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("운영 없음 = Idle"), ClassifyGaugeHealth(0.9f, false), EGaugeHealth::Idle);
	TestEqual(TEXT("시작 = Earning"), ClassifyGaugeHealth(0.0f, true), EGaugeHealth::Earning);
	TestEqual(TEXT("0.75 경계 = Earning"), ClassifyGaugeHealth(0.75f, true), EGaugeHealth::Earning);
	TestEqual(TEXT("0.75 초과 = EndingSoon"), ClassifyGaugeHealth(0.7501f, true), EGaugeHealth::EndingSoon);
	TestEqual(TEXT("음수 클램프"), ClassifyGaugeHealth(-1.0f, true), EGaugeHealth::Earning);
	TestEqual(TEXT("1 초과 클램프"), ClassifyGaugeHealth(2.0f, true), EGaugeHealth::EndingSoon);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeBarOpacityTest,
	"CGR.VaultGauge.BarOpacity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeBarOpacityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Idle track opacity"), ResolveVaultGaugeBarOpacity(EGaugeHealth::Idle), 0.55f);
	TestEqual(TEXT("Earning track opacity"), ResolveVaultGaugeBarOpacity(EGaugeHealth::Earning), 1.0f);
	TestEqual(TEXT("EndingSoon track opacity"), ResolveVaultGaugeBarOpacity(EGaugeHealth::EndingSoon), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeWidthTest,
	"CGR.VaultGauge.ComputeWidth",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeWidthTest::RunTest(const FString& Parameters)
{
	// 건물 화면폭 200 × 비율 0.70 = 140 → 상하한(128, 220) 안이므로 그대로
	TestEqual(TEXT("범위 안은 비례값"), ComputeGaugeWidth(200.0f, 0.70f, 128.0f, 220.0f), 140.0f);

	// 먼 건물(화면폭 40) → 28 이지만 하한 128 으로 올라간다 (너무 작으면 판독 불가)
	TestEqual(TEXT("하한 클램프"), ComputeGaugeWidth(40.0f, 0.70f, 128.0f, 220.0f), 128.0f);

	// 가까운 건물(화면폭 600) → 420 이지만 상한 220 (게이지가 건물보다 커지면 소속 모호)
	TestEqual(TEXT("상한 클램프"), ComputeGaugeWidth(600.0f, 0.70f, 128.0f, 220.0f), 220.0f);

	// 화면폭 0(투영 실패 등)이어도 하한은 보장 — 폭 0 위젯이 생기지 않게
	TestEqual(TEXT("폭 0 도 하한 보장"), ComputeGaugeWidth(0.0f, 0.70f, 128.0f, 220.0f), 128.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeBubbleLiftTest,
	"CGR.VaultGauge.BubbleLift",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeBubbleLiftTest::RunTest(const FString& Parameters)
{
	// Bar 와 버블은 앵커·정렬·ZOrder 가 같아 안 올리면 정확히 겹친다 — 표현별로 올릴 높이가 갈린다.
	TestEqual(TEXT("Bar 는 Bar 리프트"), ComputeBubbleLiftPx(EVaultGaugePresentation::Bar, 42.0f, 40.0f), 42.0f);
	TestEqual(TEXT("Pearl 은 Pearl 리프트"), ComputeBubbleLiftPx(EVaultGaugePresentation::Pearl, 42.0f, 40.0f), 40.0f);

	// 노브는 PIE 튜닝 대상이라 값을 해석하지 않고 그대로 전달해야 한다 (내부 상수화 금지)
	TestEqual(TEXT("Bar 값 그대로 전달"), ComputeBubbleLiftPx(EVaultGaugePresentation::Bar, 7.5f, 99.0f), 7.5f);
	TestEqual(TEXT("Pearl 값 그대로 전달"), ComputeBubbleLiftPx(EVaultGaugePresentation::Pearl, 99.0f, 7.5f), 7.5f);

	// 0 = 리프트 없음. 컨테이너가 이 값을 "항목 제거" 로 해석하므로 통과 가능해야 한다.
	TestEqual(TEXT("0 도 그대로"), ComputeBubbleLiftPx(EVaultGaugePresentation::Pearl, 42.0f, 0.0f), 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeLODTest,
	"CGR.VaultGauge.ResolveLOD",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeLODTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("콜드 0.50 Bar"), ResolveVaultGaugeLOD(0.50f, EVaultGaugeLOD::Uninitialized), EVaultGaugeLOD::Bar);
	TestEqual(TEXT("콜드 0.51 Pearl"), ResolveVaultGaugeLOD(0.51f, EVaultGaugeLOD::Uninitialized), EVaultGaugeLOD::Pearl);
	TestEqual(TEXT("콜드 0.62 Pearl"), ResolveVaultGaugeLOD(0.62f, EVaultGaugeLOD::Uninitialized), EVaultGaugeLOD::Pearl);
	TestEqual(TEXT("콜드 0.6201 Hidden"), ResolveVaultGaugeLOD(0.6201f, EVaultGaugeLOD::Uninitialized), EVaultGaugeLOD::Hidden);
	TestEqual(TEXT("콜드 0.63 Hidden"), ResolveVaultGaugeLOD(0.63f, EVaultGaugeLOD::Uninitialized), EVaultGaugeLOD::Hidden);
	TestEqual(TEXT("Bar 0.52 유지"), ResolveVaultGaugeLOD(0.52f, EVaultGaugeLOD::Bar), EVaultGaugeLOD::Bar);
	TestEqual(TEXT("Bar 0.53 Pearl"), ResolveVaultGaugeLOD(0.53f, EVaultGaugeLOD::Bar), EVaultGaugeLOD::Pearl);
	TestEqual(TEXT("Bar 0.65 Hidden 직행"), ResolveVaultGaugeLOD(0.65f, EVaultGaugeLOD::Bar), EVaultGaugeLOD::Hidden);
	TestEqual(TEXT("Pearl 0.48 유지"), ResolveVaultGaugeLOD(0.48f, EVaultGaugeLOD::Pearl), EVaultGaugeLOD::Pearl);
	TestEqual(TEXT("Pearl 0.47 Bar"), ResolveVaultGaugeLOD(0.47f, EVaultGaugeLOD::Pearl), EVaultGaugeLOD::Bar);
	TestEqual(TEXT("Pearl 0.64 유지"), ResolveVaultGaugeLOD(0.64f, EVaultGaugeLOD::Pearl), EVaultGaugeLOD::Pearl);
	TestEqual(TEXT("Pearl 0.65 Hidden"), ResolveVaultGaugeLOD(0.65f, EVaultGaugeLOD::Pearl), EVaultGaugeLOD::Hidden);
	TestEqual(TEXT("Hidden 0.60 유지"), ResolveVaultGaugeLOD(0.60f, EVaultGaugeLOD::Hidden), EVaultGaugeLOD::Hidden);
	TestEqual(TEXT("Hidden 0.59 Pearl"), ResolveVaultGaugeLOD(0.59f, EVaultGaugeLOD::Hidden), EVaultGaugeLOD::Pearl);
	TestEqual(TEXT("Hidden 0.47 Bar 직행"), ResolveVaultGaugeLOD(0.47f, EVaultGaugeLOD::Hidden), EVaultGaugeLOD::Bar);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialGaugeReservationTest,
	"CGR.VaultGauge.TutorialReservation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialGaugeReservationTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Hidden LOD 일반 건물은 제외"),
		ShouldIncludeGaugeAtLOD(EVaultGaugeLOD::Hidden, 7, INDEX_NONE));
	TestTrue(TEXT("Hidden LOD에서도 튜토리얼 예약 건물은 포함"),
		ShouldIncludeGaugeAtLOD(EVaultGaugeLOD::Hidden, 7, 7));
	TestTrue(TEXT("Bar LOD 일반 건물은 포함"),
		ShouldIncludeGaugeAtLOD(EVaultGaugeLOD::Bar, 7, INDEX_NONE));

	TestEqual(TEXT("예약 건물은 줌과 무관하게 Bar"),
		ResolveGaugePresentation(EVaultGaugeLOD::Hidden, false, true),
		EVaultGaugePresentation::Bar);
	TestEqual(TEXT("일반 Pearl LOD는 Pearl"),
		ResolveGaugePresentation(EVaultGaugeLOD::Pearl, false, false),
		EVaultGaugePresentation::Pearl);

	const FVector2D Center(0.0f, 0.0f);
	TArray<FGaugeCandidate> Cands;
	for (int32 Idx = 1; Idx <= 3; ++Idx)
	{
		FGaugeCandidate Candidate;
		Candidate.BuildingIndex = Idx;
		Candidate.CanvasPos = FVector2D(static_cast<float>(Idx * 10), 0.0f);
		Cands.Add(Candidate);
	}
	FGaugeCandidate Reserved;
	Reserved.BuildingIndex = 99;
	Reserved.CanvasPos = FVector2D(10000.0f, 0.0f);
	Cands.Add(Reserved);

	SelectGaugesByProximity(Cands, Center, 2, 99);
	TestEqual(TEXT("캡 적용 후에도 예약 건물 유지"), Cands.Num(), 2);
	TestEqual(TEXT("예약 건물이 첫 슬롯"), Cands[0].BuildingIndex, 99);
	TestEqual(TEXT("나머지는 가장 가까운 일반 건물"), Cands[1].BuildingIndex, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeFillTipTest,
	"CGR.VaultGauge.FillTip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeFillTipTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("중앙 위치"), ComputeFillTipOffset(96.0f, 0.5f, 3.0f, 5.0f), 45.5f);
	TestEqual(TEXT("음수 fill 왼쪽 클램프"), ComputeFillTipOffset(96.0f, -1.0f, 3.0f, 5.0f), 3.0f);
	TestEqual(TEXT("1 초과 fill 오른쪽 클램프"), ComputeFillTipOffset(96.0f, 2.0f, 3.0f, 5.0f), 88.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeSelectTest,
	"CGR.VaultGauge.SelectByProximity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeSelectTest::RunTest(const FString& Parameters)
{
	const FVector2D Center(1000.0f, 500.0f);

	auto Make = [](int32 Idx, float X, float Y)
	{
		FGaugeCandidate C;
		C.BuildingIndex = Idx;
		C.CanvasPos = FVector2D(X, Y);
		return C;
	};

	{
		// 중앙에서 먼 순서로 넣고, 가까운 순으로 정렬되는지
		TArray<FGaugeCandidate> Cands;
		Cands.Add(Make(10, 1300.0f, 500.0f));  // dist 300
		Cands.Add(Make(11, 1050.0f, 500.0f));  // dist 50
		Cands.Add(Make(12, 1100.0f, 500.0f));  // dist 100
		SelectGaugesByProximity(Cands, Center, 0);   // Cap 0 = 무제한
		TestEqual(TEXT("무제한이면 전부 유지"), Cands.Num(), 3);
		TestEqual(TEXT("가장 가까운 것이 첫째"), Cands[0].BuildingIndex, 11);
		TestEqual(TEXT("둘째"), Cands[1].BuildingIndex, 12);
		TestEqual(TEXT("가장 먼 것이 마지막"), Cands[2].BuildingIndex, 10);
	}
	{
		// 캡이 걸리면 가까운 상위 N 개만 남는다
		TArray<FGaugeCandidate> Cands;
		Cands.Add(Make(10, 1300.0f, 500.0f));
		Cands.Add(Make(11, 1050.0f, 500.0f));
		Cands.Add(Make(12, 1100.0f, 500.0f));
		SelectGaugesByProximity(Cands, Center, 2);
		TestEqual(TEXT("캡 2 로 절단"), Cands.Num(), 2);
		TestEqual(TEXT("남은 것은 가까운 둘"), Cands[0].BuildingIndex, 11);
		TestEqual(TEXT("남은 것은 가까운 둘 (2)"), Cands[1].BuildingIndex, 12);
	}
	{
		// 동거리 동점 = BuildingIndex 오름차순 고정 (unstable sort 로 인한 깜빡임 방지)
		TArray<FGaugeCandidate> Cands;
		Cands.Add(Make(30, 1100.0f, 500.0f));
		Cands.Add(Make(20, 900.0f, 500.0f));   // 같은 거리 100
		Cands.Add(Make(25, 1000.0f, 600.0f));  // 같은 거리 100
		SelectGaugesByProximity(Cands, Center, 0);
		TestEqual(TEXT("동점 tie-break 첫째"), Cands[0].BuildingIndex, 20);
		TestEqual(TEXT("동점 tie-break 둘째"), Cands[1].BuildingIndex, 25);
		TestEqual(TEXT("동점 tie-break 셋째"), Cands[2].BuildingIndex, 30);
	}
	{
		// 캡이 후보 수보다 크면 절단 없음 (SetNum 오작동 방어)
		TArray<FGaugeCandidate> Cands;
		Cands.Add(Make(1, 1000.0f, 500.0f));
		SelectGaugesByProximity(Cands, Center, 16);
		TestEqual(TEXT("캡 > 후보수 면 그대로"), Cands.Num(), 1);
	}
	{
		// 빈 배열에서 크래시하지 않는다
		TArray<FGaugeCandidate> Cands;
		SelectGaugesByProximity(Cands, Center, 16);
		TestEqual(TEXT("빈 배열 안전"), Cands.Num(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeReconcileTriggerTest,
	"CGR.VaultGauge.ReconcileTriggers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeReconcileTriggerTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("회사/버블 갱신은 후보 재조정"),
		ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::CompanyOrBubbleRefresh));
	TestTrue(TEXT("운영 시작은 후보 재조정"),
		ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::OperationStarted));
	TestTrue(TEXT("운영 완료는 후보 재조정"),
		ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::OperationCompleted));
	TestTrue(TEXT("건물 목록 변경은 후보 재조정"),
		ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::BuildingListChanged));
	TestTrue(TEXT("추적 액터 만료는 후보 재조정"),
		ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::TrackedBuildingExpired));
	TestFalse(TEXT("일반 창고 값 틱은 전체 후보 재조정 안 함"),
		ShouldRequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger::WarehouseValueUpdated));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGaugeExpiredCandidateTest,
	"CGR.VaultGauge.ExpiredCandidate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGaugeExpiredCandidateTest::RunTest(const FString& Parameters)
{
	const FGaugeCandidate ExpiredCandidate;

	TestEqual(TEXT("만료 약참조는 정지 카메라보다 우선"),
		ResolveVaultGaugeCandidateFrameAction(ExpiredCandidate.BuildingPtr.IsValid(), false),
		EVaultGaugeCandidateFrameAction::CollapseAndReconcile);
	TestEqual(TEXT("유효 후보와 정지 카메라는 재투영 생략"),
		ResolveVaultGaugeCandidateFrameAction(true, false),
		EVaultGaugeCandidateFrameAction::SkipReprojection);
	TestEqual(TEXT("유효 후보와 변경 카메라는 캐시 액터 투영"),
		ResolveVaultGaugeCandidateFrameAction(true, true),
		EVaultGaugeCandidateFrameAction::ProjectCachedBuilding);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
