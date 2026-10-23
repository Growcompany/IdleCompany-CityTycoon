#include "Misc/AutomationTest.h"
#include "Utils/GachaRevealBeatRunner.h"

#if WITH_DEV_AUTOMATION_TESTS

// 기대값은 기획된 박자 길이와 입력 시각으로 독립 산출한다. 구현 상수를 바꿔도 예산 이탈이 드러나야 한다.
namespace GachaRevealFlowExpect
{
	constexpr float TenThrowMs = 680.f;
	constexpr float TenCutMs = 400.f;
	constexpr float TenUpgradeMs = 370.f;
	constexpr float TenLidMs = 240.f;
	constexpr float TenDealMs = 738.f;
	constexpr float TenCascadeMs = 580.f;
	constexpr float TenStampMs = 665.f;
	constexpr float TenSpotlightWaitAtMs = 720.f;
	constexpr float TenSpotlightHoldMs = 800.f;
	constexpr float TenSpotlightMs = 1960.f;
	constexpr float TenSettleMs = 720.f;
	constexpr float TenResultStartMs = 6353.f;
	constexpr float LowCutMs = 490.f;
	constexpr float ThirtyUpgradeMs = 650.f;
	constexpr float ThirtyDealMs = 859.f;
	constexpr float ThirtyCascadeMs = 771.f;
	constexpr float ThirtySpotlightWaitAtMs = 930.f;
	constexpr float ThirtySpotlightMs = 2170.f;
	constexpr float ThirtyResultStartMs = 6490.f;
	constexpr float SingleDealMs = 540.f;
	constexpr float SingleSpotlightWaitAtMs = 680.f;
	constexpr float SingleSpotlightHoldMs = 900.f;
	constexpr float SingleSpotlightMs = 1760.f;
	constexpr float SingleLowCascadeMs = 540.f;
	constexpr float Partial7DealMs = 672.f;
	constexpr float Partial7CascadeMs = 535.f;
	constexpr float Partial11DealMs = 650.f;
	constexpr float HandCutTailMs = 80.f;
	constexpr float TenResultRealMs = 6353.f;
	constexpr float LowResultRealMs = 3069.f;
	constexpr float SkipTapRealMs = 1790.f;
	constexpr float SkipCascadeRealMs = 1949.5f;
	constexpr float SkipStampRealMs = 2094.5f;
	constexpr float SkipSpotlightRealMs = 2759.5f;
	constexpr float TapWaitStartRealMs = 4393.f;
	constexpr float TapSettleRealMs = 4610.f;
	constexpr float TapResultRealMs = 5330.f;
	constexpr float TapConfirmAtRealMs = 5830.f;
	constexpr float ReduceConfirmAtRealMs = 500.f;
	constexpr float HandCutEnterRealMs = 680.f;
	constexpr float HandUpgradeRealMs = 5080.f;
	constexpr float BackSkipRealMs = 2000.f;
	constexpr float BackConfirmAtRealMs = 2500.f;
	constexpr float UpgradeOffCutMs = 490.f;
	constexpr float UpgradeOffResultStartMs = 6073.f;
	constexpr float UpgradeOffResultRealMs = 6073.f;
	constexpr float UpgradeOffHandCutMs = 170.f;
	constexpr float GroupsStamp0Ms = 720.f;
	constexpr float GroupsStamp1Ms = 665.f;
	constexpr float GroupsStamp0RealMs = 2796.f;
	constexpr float GroupsTap0RealMs = 2896.f;
	constexpr float GroupsStamp1RealMs = 3051.f;
	constexpr float GroupsTap1RealMs = 3116.f;
	constexpr float GroupsSpotlightRealMs = 3266.f;
	constexpr float IntroTapRealMs = 3773.f;
	constexpr float IntroWaitStartRealMs = 3928.f;
	constexpr float IntroWaitEndRealMs = 4128.f;
	constexpr float IntroSettleRealMs = 4238.f;
}

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌
namespace GachaRevealFlowTestUtil
{
	constexpr float FrameSec = 1.f / 60.f;
	constexpr float FrameMs = 1000.f / 60.f;
	constexpr float TimeTolMs = 1.f; // 프레임 합산 float 오차 ― 박자 차이(간격 18ms 이상)보다 훨씬 작다

	struct FFlowLog
	{
		TArray<EGachaRevealBeat> Beats;
		TArray<float> RealMs;
		TArray<float> Speeds;
		float FirstConfirmRealMs = -1.f;
		// 대기 시작·끝을 처음 본 틱의 실시간(그 틱 안 어딘가에서 바뀌었다)
		bool bWasWaiting = false;
		float WaitSeenRealMs = -1.f;
		float WaitEndSeenRealMs = -1.f;
	};

	static TArray<FGachaRevealEntry> MakeEntries(const TArray<int32>& Grades)
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

	static TArray<int32> TenGrades() { return {0, 1, 2, 0, 4, 0, 1, 3, 0, 2}; }

	static TArray<int32> Repeat(int32 Grade, int32 Count)
	{
		TArray<int32> Out;
		Out.Init(Grade, Count);
		return Out;
	}

	static void Bind(FGachaRevealBeatRunner& Runner, FFlowLog& Log)
	{
		Runner.OnBeatStarted = [&Runner, &Log](int32, const FGachaRevealBeatSpec& Spec)
		{
			Log.Beats.Add(Spec.Beat);
			Log.RealMs.Add(Runner.GetElapsedRealMs());
			Log.Speeds.Add(Runner.GetSpeed());
		};
	}

	// 위젯 틱처럼 1/60초씩 나눠 민다
	static void Run(FGachaRevealBeatRunner& Runner, FFlowLog& Log, double Sec)
	{
		double Remaining = Sec;
		while (Remaining > 1e-9)
		{
			const double Step = FMath::Min(Remaining, static_cast<double>(FrameSec));
			Runner.Advance(static_cast<float>(Step));
			Remaining -= Step;
			if (Log.FirstConfirmRealMs < 0.f && Runner.IsConfirmEnabled()) { Log.FirstConfirmRealMs = Runner.GetElapsedRealMs(); }
			const bool bWaiting = Runner.IsWaiting();
			if (bWaiting && !Log.bWasWaiting && Log.WaitSeenRealMs < 0.f) { Log.WaitSeenRealMs = Runner.GetElapsedRealMs(); }
			if (!bWaiting && Log.bWasWaiting && Log.WaitEndSeenRealMs < 0.f) { Log.WaitEndSeenRealMs = Runner.GetElapsedRealMs(); }
			Log.bWasWaiting = bWaiting;
		}
	}

	static FString JoinBeats(const TArray<EGachaRevealBeat>& Beats)
	{
		return FString::JoinBy(Beats, TEXT(","), [](EGachaRevealBeat B) { return FString(FGachaRevealBeatRunner::BeatName(B)); });
	}

	static FString PlanOrder(const FGachaRevealPlan& Plan)
	{
		return FString::JoinBy(Plan.Beats, TEXT(","), [](const FGachaRevealBeatSpec& S) { return FString(FGachaRevealBeatRunner::BeatName(S.Beat)); });
	}

	static FString JoinInts(const TArray<int32>& Values)
	{
		return FString::JoinBy(Values, TEXT(","), [](int32 V) { return FString::FromInt(V); });
	}

	static FString TapName(EGachaRevealTapResult Result)
	{
		switch (Result)
		{
		case EGachaRevealTapResult::Ignored: return TEXT("Ignored");
		case EGachaRevealTapResult::SpedUp: return TEXT("SpedUp");
		case EGachaRevealTapResult::Advanced: return TEXT("Advanced");
		}
		return TEXT("?");
	}

	// Nth 번째(0부터) 진입, 없으면 INDEX_NONE ― 같은 박자가 여러 번(도장 묶음) 나올 때
	static int32 EntryIndex(const FFlowLog& Log, EGachaRevealBeat Beat, int32 Nth)
	{
		int32 Seen = 0;
		for (int32 i = 0; i < Log.Beats.Num(); ++i)
		{
			if (Log.Beats[i] == Beat && Seen++ == Nth) { return i; }
		}
		return INDEX_NONE;
	}

	static float RealAt(const FFlowLog& Log, EGachaRevealBeat Beat, int32 Nth = 0)
	{
		const int32 Idx = EntryIndex(Log, Beat, Nth);
		return Idx == INDEX_NONE ? -1.f : Log.RealMs[Idx];
	}

	static float SpeedAt(const FFlowLog& Log, EGachaRevealBeat Beat, int32 Nth = 0)
	{
		const int32 Idx = EntryIndex(Log, Beat, Nth);
		return Idx == INDEX_NONE ? -1.f : Log.Speeds[Idx];
	}

	static bool SeenWithinFrame(float SeenMs, float ExpectedMs)
	{
		return SeenMs >= ExpectedMs - TimeTolMs && SeenMs <= ExpectedMs + FrameMs + TimeTolMs;
	}

	// 없으면 Beat 가 다른 빈 사양 ― 길이 단언이 0 과 비교돼 실패로 드러난다
	static FGachaRevealBeatSpec SpecOf(const FGachaRevealPlan& Plan, EGachaRevealBeat Beat)
	{
		const FGachaRevealBeatSpec* Found = Plan.Beats.FindByPredicate([Beat](const FGachaRevealBeatSpec& S) { return S.Beat == Beat; });
		return Found ? *Found : FGachaRevealBeatSpec();
	}

	static int32 WaitCount(const FGachaRevealPlan& Plan)
	{
		return Plan.Beats.FilterByPredicate([](const FGachaRevealBeatSpec& S) { return S.bWait; }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowBeatsTenTest,
	"CGR.Gacha.Reveal.Flow.BeatsTen",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowBeatsTenTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	const TArray<FGachaRevealEntry> Entries = MakeEntries(TenGrades());
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(Entries, false, false))) { return false; }

	const FGachaRevealPlan& Plan = Runner.GetPlan();
	TestEqual(TEXT("박자 순서"), PlanOrder(Plan), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Cascade,Stamp,Spotlight,Settle,Result")));
	TestTrue(TEXT("배치 = 10회"), Plan.Layout == EGachaRevealLayout::Ten);
	TestEqual(TEXT("승급 단계 수 = 1(S→SS)"), Plan.UpgradeSteps.Num() - 1, 1);
	TestEqual(TEXT("틈 빛 색 단계"), JoinInts(Plan.UpgradeSteps), FString(TEXT("3,4")));
	if (TestEqual(TEXT("도장 묶음 1개"), Plan.StampGroups.Num(), 1))
	{
		TestEqual(TEXT("묶음 등급 = S"), Plan.StampGroups[0].Key, 3);
		TestEqual(TEXT("묶음 카드 = S 한 장"), JoinInts(Plan.StampGroups[0].Value), FString(TEXT("7")));
	}
	TestEqual(TEXT("Stamp 박자 = 묶음 0"), SpecOf(Plan, EGachaRevealBeat::Stamp).Group, 0);
	TestEqual(TEXT("단독 장면 대상 = PullIndex 4"), Entries.IsValidIndex(Plan.TopIndex) ? Entries[Plan.TopIndex].PullIndex : static_cast<int32>(INDEX_NONE), 4);
	TestEqual(TEXT("대기 박자 = 단독 장면 하나"), WaitCount(Plan), 1);
	TestTrue(TEXT("Spotlight = 대기"), SpecOf(Plan, EGachaRevealBeat::Spotlight).bWait);
	TestEqual(TEXT("Spotlight 자동 넘김 = 800"), SpecOf(Plan, EGachaRevealBeat::Spotlight).WaitTimeoutMs, X::TenSpotlightHoldMs);

	// 탭 없이 끝까지 ― 단독 장면 대기는 800ms 뒤 저절로 넘어간다
	TestFalse(TEXT("첫 틱 전 = 시작 전"), Runner.IsOnBeat(EGachaRevealBeat::Throw));
	Run(Runner, Log, 9.0);
	TestEqual(TEXT("지나간 박자 = 목록 그대로"), JoinBeats(Log.Beats), PlanOrder(Plan));
	TestTrue(TEXT("끝 = Result"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("Result 진입 실시간"), RealAt(Log, EGachaRevealBeat::Result), X::TenResultRealMs, TimeTolMs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowBeatsLowTest,
	"CGR.Gacha.Reveal.Flow.BeatsLow",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowBeatsLowTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries({0, 1, 2}), false, false))) { return false; }

	const FGachaRevealPlan& Plan = Runner.GetPlan();
	TestEqual(TEXT("박자 순서 = 승급·도장·단독 장면 없음"), PlanOrder(Plan), FString(TEXT("Throw,Cut,Lid,Deal,Cascade,Settle,Result")));
	TestEqual(TEXT("승급 색 단계 없음"), Plan.UpgradeSteps.Num(), 0);
	TestEqual(TEXT("도장 묶음 없음"), Plan.StampGroups.Num(), 0);
	TestEqual(TEXT("단독 장면 대상 없음"), Plan.TopIndex, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("대기 박자 없음"), WaitCount(Plan), 0);

	Run(Runner, Log, 5.0);
	TestEqual(TEXT("지나간 박자 = 목록 그대로"), JoinBeats(Log.Beats), PlanOrder(Plan));
	TestTrue(TEXT("끝 = Result"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("Result 진입 실시간"), RealAt(Log, EGachaRevealBeat::Result), X::LowResultRealMs, TimeTolMs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowReduceMotionTest,
	"CGR.Gacha.Reveal.Flow.ReduceMotion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowReduceMotionTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries(TenGrades()), true, false))) { return false; }

	TestEqual(TEXT("박자 목록 = Result 만"), PlanOrder(Runner.GetPlan()), FString(TEXT("Result")));
	TestEqual(TEXT("손 긋기 켜도 Result 만"), PlanOrder(FGachaRevealBeatRunner::BuildPlan(MakeEntries(TenGrades()), true, true)), FString(TEXT("Result")));
	TestEqual(TEXT("첫 틱 전 = 시작 전"), Runner.GetBeatIndex(), static_cast<int32>(INDEX_NONE));

	Runner.Advance(FrameSec);
	TestTrue(TEXT("첫 틱 = Result"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("지나간 박자 = Result 하나"), JoinBeats(Log.Beats), FString(TEXT("Result")));
	TestEqual(TEXT("Result 진입 = 0ms"), RealAt(Log, EGachaRevealBeat::Result), 0.f, TimeTolMs);
	TestEqual(TEXT("결과 화면 탭 무시"), TapName(Runner.Tap()), FString(TEXT("Ignored")));

	Run(Runner, Log, 0.45);
	TestFalse(TEXT("[확인] 0.5초 전 = 꺼짐"), Runner.IsConfirmEnabled());
	Run(Runner, Log, 0.05);
	TestTrue(TEXT("[확인] 0.5초 뒤 = 켜짐"), Runner.IsConfirmEnabled());
	TestTrue(TEXT("[확인] 처음 켜진 때 = 0.5초 이후 첫 틱"),
		Log.FirstConfirmRealMs >= X::ReduceConfirmAtRealMs - TimeTolMs && Log.FirstConfirmRealMs <= X::ReduceConfirmAtRealMs + FrameMs + TimeTolMs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowSkipKeepsHighGradeTest,
	"CGR.Gacha.Reveal.Flow.SkipKeepsHighGrade",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowSkipKeepsHighGradeTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries(TenGrades()), false, false))) { return false; }

	Run(Runner, Log, 1.79);
	if (!TestTrue(TEXT("1.79초 = Deal"), Runner.IsOnBeat(EGachaRevealBeat::Deal))) { return false; }
	TestEqual(TEXT("Deal 100ms 째"), Runner.GetBeatTimeMs(), 100.f, TimeTolMs);
	TestEqual(TEXT("Deal 중 탭 = 배속"), TapName(Runner.Tap()), FString(TEXT("SpedUp")));
	TestEqual(TEXT("배속 4"), Runner.GetSpeed(), GachaRevealTiming::SkipTimeScale);
	TestEqual(TEXT("탭 시각"), Runner.GetElapsedRealMs(), X::SkipTapRealMs, TimeTolMs);

	Run(Runner, Log, 0.5);
	TestEqual(TEXT("Cascade 진입(Deal 남은 638 ÷ 4)"), RealAt(Log, EGachaRevealBeat::Cascade), X::SkipCascadeRealMs, TimeTolMs);
	TestEqual(TEXT("Cascade 는 배속 유지"), SpeedAt(Log, EGachaRevealBeat::Cascade), 4.f);
	TestEqual(TEXT("Stamp 진입(Cascade 580 ÷ 4)"), RealAt(Log, EGachaRevealBeat::Stamp), X::SkipStampRealMs, TimeTolMs);
	TestEqual(TEXT("Stamp 진입 배속 = 1"), SpeedAt(Log, EGachaRevealBeat::Stamp), 1.f);
	TestTrue(TEXT("지금 Stamp"), Runner.IsOnBeat(EGachaRevealBeat::Stamp));
	TestEqual(TEXT("지금 배속 = 1"), Runner.GetSpeed(), 1.f);

	Run(Runner, Log, 1.0);
	TestEqual(TEXT("Spotlight 진입"), RealAt(Log, EGachaRevealBeat::Spotlight), X::SkipSpotlightRealMs, TimeTolMs);
	TestEqual(TEXT("Stamp 가 배속 1로 끝까지 실행됨"), RealAt(Log, EGachaRevealBeat::Spotlight) - RealAt(Log, EGachaRevealBeat::Stamp), X::TenStampMs, TimeTolMs);
	TestEqual(TEXT("Spotlight 진입 배속 = 1"), SpeedAt(Log, EGachaRevealBeat::Spotlight), 1.f);
	TestTrue(TEXT("지금 Spotlight"), Runner.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestFalse(TEXT("아직 대기 전(정보 밀려 들어오기 전)"), Runner.IsWaiting());

	Run(Runner, Log, 0.7);
	TestTrue(TEXT("단독 장면 대기에 도달"), Runner.IsWaiting() && Runner.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestEqual(TEXT("화면 탭 가속도 박자를 빼먹지 않음"), JoinBeats(Log.Beats), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Cascade,Stamp,Spotlight")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowTapAdvancesOneBeatTest,
	"CGR.Gacha.Reveal.Flow.TapAdvancesOneBeat",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowTapAdvancesOneBeatTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries(TenGrades()), false, false))) { return false; }

	Run(Runner, Log, 4.3);
	TestTrue(TEXT("4.3초 = Spotlight"), Runner.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestFalse(TEXT("4.3초 = 아직 대기 전"), Runner.IsWaiting());
	Run(Runner, Log, 0.1);
	if (!TestTrue(TEXT("4.4초 = 단독 장면 대기"), Runner.IsWaiting())) { return false; }
	const FGachaRevealBeatSpec Spot = *Runner.GetCurrentBeat();
	TestEqual(TEXT("대기 시작 실시간"), RealAt(Log, EGachaRevealBeat::Spotlight) + Spot.WaitAtMs, X::TapWaitStartRealMs, TimeTolMs);

	// 앱 복귀처럼 큰 Dt 한 번 = 0.1초만 쓴다 ― 대기를 건너뛰지 않음
	Runner.Advance(30.f);
	TestEqual(TEXT("큰 Dt = 0.1초만"), Runner.GetElapsedRealMs(), 4500.f, TimeTolMs);
	TestTrue(TEXT("큰 Dt 뒤에도 대기"), Runner.IsWaiting());

	TestEqual(TEXT("탭 1 = 대기만 끝냄"), TapName(Runner.Tap()), FString(TEXT("Advanced")));
	TestTrue(TEXT("탭 1 뒤 = 아직 Spotlight(돌아가기)"), Runner.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestFalse(TEXT("탭 1 뒤 = 대기 끝"), Runner.IsWaiting());
	TestEqual(TEXT("탭 1 은 배속을 켜지 않음"), Runner.GetSpeed(), 1.f);
	TestEqual(TEXT("박자 안 위치 = 대기 끝"), Runner.GetBeatTimeMs(), Spot.WaitAtMs + Spot.WaitTimeoutMs, TimeTolMs);
	TestEqual(TEXT("탭 2 = 배속(박자 안 넘김)"), TapName(Runner.Tap()), FString(TEXT("SpedUp")));
	TestEqual(TEXT("탭 3 = 배속(박자 안 넘김)"), TapName(Runner.Tap()), FString(TEXT("SpedUp")));
	TestTrue(TEXT("탭 3번 뒤 = 여전히 Spotlight"), Runner.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestFalse(TEXT("탭 3번 뒤 [확인] 꺼짐"), Runner.IsConfirmEnabled());

	Run(Runner, Log, 0.2);
	TestTrue(TEXT("돌아가기 끝 → Settle"), Runner.IsOnBeat(EGachaRevealBeat::Settle));
	TestEqual(TEXT("Settle 진입(돌아가기 440 ÷ 4)"), RealAt(Log, EGachaRevealBeat::Settle), X::TapSettleRealMs, TimeTolMs);
	TestEqual(TEXT("Settle 진입 배속 = 1"), SpeedAt(Log, EGachaRevealBeat::Settle), 1.f);
	TestEqual(TEXT("정리 중 탭 = 무시"), TapName(Runner.Tap()), FString(TEXT("Ignored")));
	TestEqual(TEXT("정리 중 탭 뒤 배속 = 1"), Runner.GetSpeed(), 1.f);

	Run(Runner, Log, 0.95);
	TestTrue(TEXT("Result"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("Result 진입(Settle 720 배속 1)"), RealAt(Log, EGachaRevealBeat::Result), X::TapResultRealMs, TimeTolMs);
	TestFalse(TEXT("Result+0.32초 = [확인] 꺼짐"), Runner.IsConfirmEnabled());
	TestTrue(TEXT("Result 전에는 한 번도 안 켜짐"), Log.FirstConfirmRealMs < 0.f);
	Run(Runner, Log, 0.2);
	TestTrue(TEXT("Result+0.5초 = [확인] 켜짐"), Runner.IsConfirmEnabled());
	TestTrue(TEXT("[확인] 처음 켜진 때 = Result+0.5초 이후 첫 틱"),
		Log.FirstConfirmRealMs >= X::TapConfirmAtRealMs - TimeTolMs && Log.FirstConfirmRealMs <= X::TapConfirmAtRealMs + FrameMs + TimeTolMs);
	TestEqual(TEXT("박자 하나도 안 빠짐"), JoinBeats(Log.Beats), PlanOrder(Runner.GetPlan()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowTimingTest,
	"CGR.Gacha.Reveal.Flow.Timing",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowTimingTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	namespace GT = GachaRevealTiming;
	constexpr float Tol = 0.01f;

	// 10회 [0,1,2,0,4,0,1,3,0,2] ― 입력 장수와 등급에서 독립 예산을 계산한다
	const FGachaRevealPlan Ten = FGachaRevealBeatRunner::BuildPlan(MakeEntries(TenGrades()), false, false);
	const float ThrowMs = SpecOf(Ten, EGachaRevealBeat::Throw).DurationMs;
	TestEqual(TEXT("Throw = 120+140+280+140"), ThrowMs, 120.f + 140.f + 280.f + 140.f, Tol);
	TestEqual(TEXT("Throw = 들림+빠짐+떨어짐+눌림 상수"), ThrowMs, GT::ThrowLift + GT::ThrowExit + GT::ThrowDrop + GT::ThrowPress, Tol);
	TestEqual(TEXT("Throw 미러"), ThrowMs, X::TenThrowMs, Tol);
	const float CutMs = SpecOf(Ten, EGachaRevealBeat::Cut).DurationMs;
	TestEqual(TEXT("Cut = 320+80"), CutMs, 320.f + 80.f, Tol);
	TestEqual(TEXT("Cut = 가르기+꼬리 상수"), CutMs, GT::Cut + GT::CutTail, Tol);
	TestEqual(TEXT("Cut 미러"), CutMs, X::TenCutMs, Tol);
	TestEqual(TEXT("Upgrade = 1단계 × (쉼+눌림) + 꼬리"), SpecOf(Ten, EGachaRevealBeat::Upgrade).DurationMs, 1.f * (GT::UpgradeGap + GT::UpgradePress) + GT::UpgradeTail, Tol);
	TestEqual(TEXT("Upgrade 미러"), SpecOf(Ten, EGachaRevealBeat::Upgrade).DurationMs, X::TenUpgradeMs, Tol);
	TestEqual(TEXT("Lid = 눌림"), SpecOf(Ten, EGachaRevealBeat::Lid).DurationMs, GT::LidPress, Tol);
	TestEqual(TEXT("Lid 미러"), SpecOf(Ten, EGachaRevealBeat::Lid).DurationMs, X::TenLidMs, Tol);
	TestEqual(TEXT("Deal = 9 × 22 + 비행 + 꼬리"), SpecOf(Ten, EGachaRevealBeat::Deal).DurationMs, 9.f * GT::DealStagger10 + GT::DealFlight + GT::DealTail, Tol);
	TestEqual(TEXT("Deal 미러"), SpecOf(Ten, EGachaRevealBeat::Deal).DurationMs, X::TenDealMs, Tol);
	TestTrue(TEXT("단독 Deal에서도 상자 퇴장 완료"), GT::DealFlight + GT::DealTail >= GT::DealBoxDropDelay + GT::DealBoxDrop);
	TestEqual(TEXT("Cascade = S 미만 8장 × 45 + 꼬리"), SpecOf(Ten, EGachaRevealBeat::Cascade).DurationMs, 8.f * GT::CascadeStagger10 + GT::CascadeTail, Tol);
	TestEqual(TEXT("Cascade 미러"), SpecOf(Ten, EGachaRevealBeat::Cascade).DurationMs, X::TenCascadeMs, Tol);
	TestTrue(TEXT("10회 마지막 뒤집기 종료 보장"), GT::CascadeStagger10 + GT::CascadeTail >= GT::CascadeFlip);
	TestTrue(TEXT("30회 마지막 뒤집기 종료 보장"), GT::CascadeStagger30 + GT::CascadeTail >= GT::CascadeFlip);
	TestEqual(TEXT("Stamp = 상승 + 타격 지연 + 1장 × 55 + 꼬리"), SpecOf(Ten, EGachaRevealBeat::Stamp).DurationMs, GT::StampRise + GT::StampHitDelay + 1.f * GT::StampGap + GT::StampTail, Tol);
	TestEqual(TEXT("Stamp 미러"), SpecOf(Ten, EGachaRevealBeat::Stamp).DurationMs, X::TenStampMs, Tol);
	const FGachaRevealBeatSpec TenSpot = SpecOf(Ten, EGachaRevealBeat::Spotlight);
	TestEqual(TEXT("Spotlight 대기 시작 = (SS 상승 − 던지기) + 던지기 + 눌림 + 정보 지연"), TenSpot.WaitAtMs,
		(GT::SpotlightRiseSS - GT::SpotlightThrow) + GT::SpotlightThrow + GT::SpotlightPress + GT::SpotlightInfoDelay, Tol);
	TestEqual(TEXT("Spotlight 대기 시작 미러"), TenSpot.WaitAtMs, X::TenSpotlightWaitAtMs, Tol);
	TestEqual(TEXT("Spotlight 자동 넘김 = 800"), TenSpot.WaitTimeoutMs, GT::SpotlightHold, Tol);
	TestTrue(TEXT("최고 카드 정보 진입 완료 뒤 자동 전환"), GT::SpotlightHold >= GT::SpotlightInfoSlide);
	TestEqual(TEXT("Spotlight 길이 = 대기 시작 + 800 + 돌아가기 + 풀기"), TenSpot.DurationMs, TenSpot.WaitAtMs + GT::SpotlightHold + GT::SpotlightReturn + GT::SpotlightUndim, Tol);
	TestEqual(TEXT("Spotlight 길이 미러"), TenSpot.DurationMs, X::TenSpotlightMs, Tol);
	TestEqual(TEXT("Settle = 쉼 + 이번 최고 + 아래 띠"), SpecOf(Ten, EGachaRevealBeat::Settle).DurationMs, GT::SettleWait + GT::SettleBest + GT::SettleBottom, Tol);
	TestEqual(TEXT("Settle 미러"), SpecOf(Ten, EGachaRevealBeat::Settle).DurationMs, X::TenSettleMs, Tol);
	TestEqual(TEXT("Result 명목 시작 미러"), SpecOf(Ten, EGachaRevealBeat::Result).StartMs, X::TenResultStartMs, Tol);
	TestTrue(TEXT("10회 자동 결과 6.5초 이내"), SpecOf(Ten, EGachaRevealBeat::Result).StartMs <= 6500.f);
	for (int32 i = 1; i < Ten.Beats.Num(); ++i)
	{
		TestEqual(*FString::Printf(TEXT("박자 %d 시작 = 앞 시작 + 앞 길이"), i), Ten.Beats[i].StartMs, Ten.Beats[i - 1].StartMs + Ten.Beats[i - 1].DurationMs, Tol);
	}

	// 승급이 없으면 뚜껑 전 쉼이 가르기 꼬리에 붙는다
	const FGachaRevealPlan Low = FGachaRevealBeatRunner::BuildPlan(MakeEntries({0, 1, 2}), false, false);
	TestEqual(TEXT("A 이하 Cut = 가르기 + 꼬리 + 승급 꼬리"), SpecOf(Low, EGachaRevealBeat::Cut).DurationMs, GT::Cut + GT::CutTail + GT::UpgradeTail, Tol);
	TestEqual(TEXT("A 이하 Cut 미러"), SpecOf(Low, EGachaRevealBeat::Cut).DurationMs, X::LowCutMs, Tol);

	// 30회: SSS 1장(PullIndex 12) + C 29장
	TArray<int32> ThirtyGrades = Repeat(0, 30);
	ThirtyGrades[12] = 5;
	const FGachaRevealPlan Thirty = FGachaRevealBeatRunner::BuildPlan(MakeEntries(ThirtyGrades), false, false);
	TestTrue(TEXT("30회 배치"), Thirty.Layout == EGachaRevealLayout::Thirty);
	TestEqual(TEXT("30회 순서"), PlanOrder(Thirty), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Cascade,Spotlight,Settle,Result")));
	TestEqual(TEXT("SSS Upgrade = 2단계"), SpecOf(Thirty, EGachaRevealBeat::Upgrade).DurationMs, 2.f * (GT::UpgradeGap + GT::UpgradePress) + GT::UpgradeTail, Tol);
	TestEqual(TEXT("SSS Upgrade 미러"), SpecOf(Thirty, EGachaRevealBeat::Upgrade).DurationMs, X::ThirtyUpgradeMs, Tol);
	TestEqual(TEXT("30회 Deal = 29 × 11 + 비행 + 꼬리"), SpecOf(Thirty, EGachaRevealBeat::Deal).DurationMs, 29.f * GT::DealStagger30 + GT::DealFlight + GT::DealTail, Tol);
	TestEqual(TEXT("30회 Deal 미러"), SpecOf(Thirty, EGachaRevealBeat::Deal).DurationMs, X::ThirtyDealMs, Tol);
	TestEqual(TEXT("30회 Cascade = 29 × 19 + 꼬리"), SpecOf(Thirty, EGachaRevealBeat::Cascade).DurationMs, 29.f * GT::CascadeStagger30 + GT::CascadeTail, Tol);
	TestEqual(TEXT("30회 Cascade 미러"), SpecOf(Thirty, EGachaRevealBeat::Cascade).DurationMs, X::ThirtyCascadeMs, Tol);
	const FGachaRevealBeatSpec ThirtySpot = SpecOf(Thirty, EGachaRevealBeat::Spotlight);
	TestEqual(TEXT("SSS 대기 시작 = (SSS 상승 − 던지기) + 던지기 + 눌림 + 두 번째 타격 + 정보 지연"), ThirtySpot.WaitAtMs,
		(GT::SpotlightRiseSSS - GT::SpotlightThrow) + GT::SpotlightThrow + GT::SpotlightPress + GT::SpotlightSSSHitDelay + GT::SpotlightInfoDelay, Tol);
	TestEqual(TEXT("SSS 대기 시작 미러"), ThirtySpot.WaitAtMs, X::ThirtySpotlightWaitAtMs, Tol);
	TestEqual(TEXT("SSS Spotlight 길이 미러"), ThirtySpot.DurationMs, X::ThirtySpotlightMs, Tol);
	TestEqual(TEXT("30회 결과 명목 시작"), SpecOf(Thirty, EGachaRevealBeat::Result).StartMs, X::ThirtyResultStartMs, Tol);
	TestTrue(TEXT("30회 자동 결과 6.6초 이내"), SpecOf(Thirty, EGachaRevealBeat::Result).StartMs <= 6600.f);

	// 1회 S: Cascade 없음, 대기 900, 칸으로 돌아가지 않음
	const FGachaRevealPlan SingleS = FGachaRevealBeatRunner::BuildPlan(MakeEntries({3}), false, false);
	TestEqual(TEXT("1회 S 순서"), PlanOrder(SingleS), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Spotlight,Settle,Result")));
	TestEqual(TEXT("1회 Deal = 비행 + 꼬리"), SpecOf(SingleS, EGachaRevealBeat::Deal).DurationMs, GT::DealFlight + GT::DealTail, Tol);
	TestEqual(TEXT("1회 Deal 미러"), SpecOf(SingleS, EGachaRevealBeat::Deal).DurationMs, X::SingleDealMs, Tol);
	const FGachaRevealBeatSpec SingleSpot = SpecOf(SingleS, EGachaRevealBeat::Spotlight);
	TestEqual(TEXT("S 대기 시작 = 던지기 + 눌림 + 정보 지연(상승 240 < 300)"), SingleSpot.WaitAtMs, GT::SpotlightThrow + GT::SpotlightPress + GT::SpotlightInfoDelay, Tol);
	TestEqual(TEXT("S 대기 시작 미러"), SingleSpot.WaitAtMs, X::SingleSpotlightWaitAtMs, Tol);
	TestEqual(TEXT("1회 자동 넘김 = 900"), SingleSpot.WaitTimeoutMs, GT::SpotlightHoldSingle, Tol);
	TestEqual(TEXT("1회 자동 넘김 미러"), SingleSpot.WaitTimeoutMs, X::SingleSpotlightHoldMs, Tol);
	TestEqual(TEXT("1회 Spotlight 길이 = 대기 시작 + 900 + 풀기"), SingleSpot.DurationMs, SingleSpot.WaitAtMs + GT::SpotlightHoldSingle + GT::SpotlightUndim, Tol);
	TestEqual(TEXT("1회 Spotlight 길이 미러"), SingleSpot.DurationMs, X::SingleSpotlightMs, Tol);

	// 1회 A 이하: 시안 single() 을 Cascade 가 맡는다
	const FGachaRevealPlan SingleLow = FGachaRevealBeatRunner::BuildPlan(MakeEntries({2}), false, false);
	TestEqual(TEXT("1회 A 순서"), PlanOrder(SingleLow), FString(TEXT("Throw,Cut,Lid,Deal,Cascade,Settle,Result")));
	TestEqual(TEXT("1회 A Cascade = 뒤집기 + 쉼"), SpecOf(SingleLow, EGachaRevealBeat::Cascade).DurationMs, GT::SingleFlip + GT::SingleTail, Tol);
	TestEqual(TEXT("1회 A Cascade 미러"), SpecOf(SingleLow, EGachaRevealBeat::Cascade).DurationMs, X::SingleLowCascadeMs, Tol);

	// 부분 지급: 7장 = 10회 간격, 11장 = 30회 간격
	const FGachaRevealPlan Seven = FGachaRevealBeatRunner::BuildPlan(MakeEntries(Repeat(0, 7)), false, false);
	TestTrue(TEXT("7장 = 10회 배치"), Seven.Layout == EGachaRevealLayout::Ten);
	TestEqual(TEXT("7장 Deal = 6 × 22 + 비행 + 꼬리"), SpecOf(Seven, EGachaRevealBeat::Deal).DurationMs, 6.f * GT::DealStagger10 + GT::DealFlight + GT::DealTail, Tol);
	TestEqual(TEXT("7장 Deal 미러"), SpecOf(Seven, EGachaRevealBeat::Deal).DurationMs, X::Partial7DealMs, Tol);
	TestEqual(TEXT("7장 Cascade = 7 × 45 + 꼬리"), SpecOf(Seven, EGachaRevealBeat::Cascade).DurationMs, 7.f * GT::CascadeStagger10 + GT::CascadeTail, Tol);
	TestEqual(TEXT("7장 Cascade 미러"), SpecOf(Seven, EGachaRevealBeat::Cascade).DurationMs, X::Partial7CascadeMs, Tol);
	const FGachaRevealPlan Eleven = FGachaRevealBeatRunner::BuildPlan(MakeEntries(Repeat(0, 11)), false, false);
	TestTrue(TEXT("11장 = 30회 배치"), Eleven.Layout == EGachaRevealLayout::Thirty);
	TestEqual(TEXT("11장 Deal = 10 × 11 + 비행 + 꼬리"), SpecOf(Eleven, EGachaRevealBeat::Deal).DurationMs, 10.f * GT::DealStagger30 + GT::DealFlight + GT::DealTail, Tol);
	TestEqual(TEXT("11장 Deal 미러"), SpecOf(Eleven, EGachaRevealBeat::Deal).DurationMs, X::Partial11DealMs, Tol);

	// 손 긋기: Cut = 끝날 때까지 대기 + 꼬리
	const FGachaRevealPlan Hand = FGachaRevealBeatRunner::BuildPlan(MakeEntries(TenGrades()), false, true);
	const FGachaRevealBeatSpec HandCut = SpecOf(Hand, EGachaRevealBeat::Cut);
	TestTrue(TEXT("손 긋기 Cut = 대기"), HandCut.bWait);
	TestTrue(TEXT("손 긋기 = 자동 넘김 없음"), HandCut.WaitTimeoutMs < 0.f);
	TestEqual(TEXT("손 긋기 대기 시작 = 0"), HandCut.WaitAtMs, 0.f, Tol);
	TestEqual(TEXT("손 긋기 Cut 길이 = 꼬리"), HandCut.DurationMs, GT::CutTail, Tol);
	TestEqual(TEXT("손 긋기 Cut 미러"), HandCut.DurationMs, X::HandCutTailMs, Tol);
	TestEqual(TEXT("손 긋기 대기 박자 = Cut·Spotlight"), WaitCount(Hand), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowSetupOnceTest,
	"CGR.Gacha.Reveal.Flow.SetupOnce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowSetupOnceTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	const TArray<FGachaRevealEntry> TenEntries = MakeEntries(TenGrades());
	const TArray<FGachaRevealEntry> LowEntries = MakeEntries({0, 1, 2});
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);

	TestFalse(TEXT("빈 결과 = 거절"), Runner.Setup(TArray<FGachaRevealEntry>(), false, false));
	TestFalse(TEXT("빈 결과 뒤 = 실행 아님"), Runner.IsRunning());
	if (!TestTrue(TEXT("첫 Setup"), Runner.Setup(TenEntries, false, false))) { return false; }
	TestFalse(TEXT("시작 전 두 번째 Setup = 거절"), Runner.Setup(LowEntries, false, false));
	TestEqual(TEXT("거절 뒤 장수 그대로"), Runner.GetPlan().Count, 10);
	TestEqual(TEXT("거절 뒤 박자 수 그대로"), Runner.GetPlan().Beats.Num(), 10);

	Run(Runner, Log, 2.0);
	if (!TestTrue(TEXT("진행 중 = Deal"), Runner.IsOnBeat(EGachaRevealBeat::Deal))) { return false; }
	const float BeatTimeBefore = Runner.GetBeatTimeMs();
	TestFalse(TEXT("진행 중 두 번째 Setup = 거절"), Runner.Setup(LowEntries, true, false));
	TestTrue(TEXT("거절 뒤 박자 그대로"), Runner.IsOnBeat(EGachaRevealBeat::Deal));
	TestEqual(TEXT("거절 뒤 박자 안 위치 그대로"), Runner.GetBeatTimeMs(), BeatTimeBefore);
	TestEqual(TEXT("거절 뒤 장수 그대로"), Runner.GetPlan().Count, 10);
	TestFalse(TEXT("거절 뒤 연출 감소 아님"), Runner.GetPlan().bReduceMotion);

	Run(Runner, Log, 10.0);
	TestTrue(TEXT("결과 화면"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestFalse(TEXT("결과 화면이 열린 동안 Setup = 거절"), Runner.Setup(LowEntries, false, false));

	Runner.Reset();
	TestFalse(TEXT("Reset 뒤 = 실행 아님"), Runner.IsRunning());
	TestTrue(TEXT("Reset 뒤 = 현재 박자 없음"), Runner.GetCurrentBeat() == nullptr);
	TestEqual(TEXT("Reset 뒤 탭 = 무시"), TapName(Runner.Tap()), FString(TEXT("Ignored")));
	Runner.Advance(FrameSec);
	TestEqual(TEXT("Reset 뒤 Advance = 시작 안 함"), Runner.GetBeatIndex(), static_cast<int32>(INDEX_NONE));

	// Advance 안(박자 시작 콜백)에서 Reset 후 Setup 해도 거절 ― 도는 중인 박자 목록을 바꾸지 않는다
	bool bInnerSetup = true;
	Runner.OnBeatStarted = [&Runner, &bInnerSetup, &LowEntries](int32, const FGachaRevealBeatSpec&)
	{
		Runner.Reset();
		bInnerSetup = Runner.Setup(LowEntries, false, false);
	};
	if (!TestTrue(TEXT("Reset 뒤 Setup"), Runner.Setup(TenEntries, false, false))) { return false; }
	Runner.Advance(FrameSec);
	TestFalse(TEXT("콜백 안 Setup = 거절"), bInnerSetup);
	TestFalse(TEXT("콜백 안 Reset 뒤 = 실행 아님"), Runner.IsRunning());
	Runner.OnBeatStarted = nullptr;
	TestTrue(TEXT("Advance 밖에서는 다시 Setup"), Runner.Setup(LowEntries, false, false));
	TestEqual(TEXT("새 Setup 장수"), Runner.GetPlan().Count, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowHandOpenWaitTest,
	"CGR.Gacha.Reveal.Flow.HandOpenWait",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowHandOpenWaitTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries(TenGrades()), false, true))) { return false; }

	Run(Runner, Log, 5.0);
	TestTrue(TEXT("5초 = 아직 Cut"), Runner.IsOnBeat(EGachaRevealBeat::Cut));
	TestTrue(TEXT("손 긋기 대기"), Runner.IsWaiting());
	TestEqual(TEXT("Cut 진입"), RealAt(Log, EGachaRevealBeat::Cut), X::HandCutEnterRealMs, TimeTolMs);
	TestEqual(TEXT("대기 동안 박자 시계 멈춤"), Runner.GetClockMs(), X::HandCutEnterRealMs, TimeTolMs);
	TestEqual(TEXT("실시간은 흐름"), Runner.GetElapsedRealMs(), 5000.f, TimeTolMs);
	TestEqual(TEXT("긋는 중 화면 탭 = 무시"), TapName(Runner.Tap()), FString(TEXT("Ignored")));
	TestEqual(TEXT("화면 탭 뒤 배속 = 1"), Runner.GetSpeed(), 1.f);

	TestTrue(TEXT("긋기 완료"), Runner.CompleteHandCut());
	TestFalse(TEXT("두 번째 완료 = 무시"), Runner.CompleteHandCut());
	Run(Runner, Log, 0.05);
	TestTrue(TEXT("꼬리 80 동안 Cut"), Runner.IsOnBeat(EGachaRevealBeat::Cut));
	TestEqual(TEXT("손 뗀 클릭 = 무시"), TapName(Runner.Tap()), FString(TEXT("Ignored")));
	Run(Runner, Log, 0.05);
	TestTrue(TEXT("Upgrade"), Runner.IsOnBeat(EGachaRevealBeat::Upgrade));
	TestEqual(TEXT("Upgrade 진입 = 완료 + 꼬리 80"), RealAt(Log, EGachaRevealBeat::Upgrade), X::HandUpgradeRealMs, TimeTolMs);
	TestEqual(TEXT("승급 중 화면 탭 = 무시(시안 slit 있음)"), TapName(Runner.Tap()), FString(TEXT("Ignored")));
	TestTrue(TEXT("손으로 열기 중 건너뛰기 = 즉시 결과"), Runner.SkipToResult());
	TestTrue(TEXT("손으로 열기 건너뛴 뒤 Result"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("건너뛰기 뒤 손 대기 없음"), Runner.IsWaiting(), false);
	TestEqual(TEXT("건너뛰기 뒤 배속 해제"), Runner.GetSpeed(), 1.f);
	FGachaRevealBeatRunner WaitingRunner;
	FFlowLog WaitingLog;
	if (!TestTrue(TEXT("손 긋기 중 Skip Setup"), WaitingRunner.Setup(MakeEntries(TenGrades()), false, true))) { return false; }
	Run(WaitingRunner, WaitingLog, 1.0);
	TestTrue(TEXT("손 긋기 완료 전 무기한 대기"), WaitingRunner.IsWaiting());
	TestTrue(TEXT("손 긋기 대기 중 건너뛰기"), WaitingRunner.SkipToResult());
	TestTrue(TEXT("손 긋기 입력 없이 즉시 결과"), WaitingRunner.IsOnBeat(EGachaRevealBeat::Result));

	// 손 긋기 꺼짐이면 긋기 완료 호출은 아무 일도 안 한다
	FGachaRevealBeatRunner AutoRunner;
	FFlowLog AutoLog;
	if (!TestTrue(TEXT("자동 가르기 Setup"), AutoRunner.Setup(MakeEntries(TenGrades()), false, false))) { return false; }
	Run(AutoRunner, AutoLog, 1.0);
	TestTrue(TEXT("자동 가르기 Cut"), AutoRunner.IsOnBeat(EGachaRevealBeat::Cut));
	TestFalse(TEXT("자동 가르기 = 대기 아님"), AutoRunner.IsWaiting());
	TestFalse(TEXT("자동 가르기에 완료 호출 = 무시"), AutoRunner.CompleteHandCut());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowBackSkipsToResultTest,
	"CGR.Gacha.Reveal.Flow.BackSkipsToResult",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowBackSkipsToResultTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	TestFalse(TEXT("실행 전 = 거절"), Runner.SkipToResult());
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries(TenGrades()), false, false))) { return false; }

	Run(Runner, Log, 2.0);
	TestTrue(TEXT("2초 = Deal"), Runner.IsOnBeat(EGachaRevealBeat::Deal));
	Runner.Tap();
	TestTrue(TEXT("뒤로가기 = Result 로"), Runner.SkipToResult());
	TestTrue(TEXT("지금 Result"), Runner.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("남은 박자는 시작하지 않고 Result 만"), JoinBeats(Log.Beats), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Result")));
	TestEqual(TEXT("Result 진입 시각"), RealAt(Log, EGachaRevealBeat::Result), X::BackSkipRealMs, TimeTolMs);
	TestEqual(TEXT("Result 배속 = 1(건너뛰기 배속 풀림)"), Runner.GetSpeed(), 1.f);
	TestFalse(TEXT("이미 Result = 거절"), Runner.SkipToResult());
	TestFalse(TEXT("바로 [확인] 아님"), Runner.IsConfirmEnabled());
	Run(Runner, Log, 0.6);
	TestTrue(TEXT("[확인] 처음 켜진 때 = Result+0.5초 이후 첫 틱"),
		Log.FirstConfirmRealMs >= X::BackConfirmAtRealMs - TimeTolMs && Log.FirstConfirmRealMs <= X::BackConfirmAtRealMs + FrameMs + TimeTolMs);

	// SkipToResult 의 박자 시작 콜백 안에서도 Advance·Setup 은 무시된다(헤더 설명)
	FGachaRevealBeatRunner GuardRunner;
	const TArray<FGachaRevealEntry> LowEntries = MakeEntries({0, 1, 2});
	bool bInnerSetup = true;
	float InnerRealMs = -1.f;
	GuardRunner.OnBeatStarted = [&GuardRunner, &bInnerSetup, &InnerRealMs, &LowEntries](int32, const FGachaRevealBeatSpec& Spec)
	{
		if (Spec.Beat != EGachaRevealBeat::Result) { return; }
		GuardRunner.Advance(0.05f);
		InnerRealMs = GuardRunner.GetElapsedRealMs();
		GuardRunner.Reset();
		bInnerSetup = GuardRunner.Setup(LowEntries, false, false);
	};
	if (!TestTrue(TEXT("재진입 시험 Setup"), GuardRunner.Setup(MakeEntries(TenGrades()), false, false))) { return false; }
	GuardRunner.Advance(FrameSec);
	TestTrue(TEXT("재진입 시험 SkipToResult"), GuardRunner.SkipToResult());
	TestEqual(TEXT("콜백 안 Advance = 무시(시간 그대로)"), InnerRealMs, FrameMs, TimeTolMs);
	TestFalse(TEXT("콜백 안 Reset 뒤 Setup = 거절"), bInnerSetup);
	TestFalse(TEXT("콜백 안 Reset 뒤 = 실행 아님"), GuardRunner.IsRunning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowUpgradeOffTest,
	"CGR.Gacha.Reveal.Flow.UpgradeOff",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowUpgradeOffTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	namespace GT = GachaRevealTiming;
	constexpr float Tol = 0.01f;

	// 승급 끔(시안 S.upgrade=false :823 :832) ― 승급 박자 없음, 뚜껑 전 쉼 90은 Cut 끝, 틈 빛 색 단계 목록은 그대로
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup(승급 끔)"), Runner.Setup(MakeEntries(TenGrades()), false, false, false))) { return false; }
	const FGachaRevealPlan& Plan = Runner.GetPlan();
	TestFalse(TEXT("Plan.bUpgrade = false"), Plan.bUpgrade);
	TestEqual(TEXT("순서 = Upgrade 없음"), PlanOrder(Plan), FString(TEXT("Throw,Cut,Lid,Deal,Cascade,Stamp,Spotlight,Settle,Result")));
	TestEqual(TEXT("Cut = 가르기 + 꼬리 + 승급 꼬리"), SpecOf(Plan, EGachaRevealBeat::Cut).DurationMs, GT::Cut + GT::CutTail + GT::UpgradeTail, Tol);
	TestEqual(TEXT("Cut 미러"), SpecOf(Plan, EGachaRevealBeat::Cut).DurationMs, X::UpgradeOffCutMs, Tol);
	TestEqual(TEXT("틈 빛 색 단계는 그대로(끝 값 = 최고 등급 색)"), JoinInts(Plan.UpgradeSteps), FString(TEXT("3,4")));
	TestEqual(TEXT("Result 명목 시작(6353 − 370 + 90)"), SpecOf(Plan, EGachaRevealBeat::Result).StartMs, X::UpgradeOffResultStartMs, Tol);
	Run(Runner, Log, 9.0);
	TestEqual(TEXT("지나간 박자 = 목록 그대로"), JoinBeats(Log.Beats), PlanOrder(Plan));
	TestEqual(TEXT("Result 진입 실시간"), RealAt(Log, EGachaRevealBeat::Result), X::UpgradeOffResultRealMs, TimeTolMs);

	TestTrue(TEXT("기본값 = 승급 켬"), FGachaRevealBeatRunner::BuildPlan(MakeEntries(TenGrades()), false, false).bUpgrade);

	TArray<int32> ThirtyGrades = Repeat(0, 30);
	ThirtyGrades[12] = 5;
	TestEqual(TEXT("30회 SSS + 끔 = Upgrade 없음"), PlanOrder(FGachaRevealBeatRunner::BuildPlan(MakeEntries(ThirtyGrades), false, false, false)),
		FString(TEXT("Throw,Cut,Lid,Deal,Cascade,Spotlight,Settle,Result")));

	const FGachaRevealBeatSpec HandCut = SpecOf(FGachaRevealBeatRunner::BuildPlan(MakeEntries(TenGrades()), false, true, false), EGachaRevealBeat::Cut);
	TestTrue(TEXT("손 긋기 + 끔 = 대기"), HandCut.bWait);
	TestEqual(TEXT("손 긋기 + 끔 Cut = 꼬리 + 승급 꼬리"), HandCut.DurationMs, GT::CutTail + GT::UpgradeTail, Tol);
	TestEqual(TEXT("손 긋기 + 끔 Cut 미러"), HandCut.DurationMs, X::UpgradeOffHandCutMs, Tol);

	TestEqual(TEXT("연출 감소 + 끔 = Result 만"), PlanOrder(FGachaRevealBeatRunner::BuildPlan(MakeEntries(TenGrades()), true, false, false)), FString(TEXT("Result")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowStampGroupsSkipTest,
	"CGR.Gacha.Reveal.Flow.StampGroupsSkip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowStampGroupsSkipTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	namespace GT = GachaRevealTiming;
	constexpr float Tol = 0.01f;

	// [3,4,5,3] → 단독 장면 SSS(2), 도장 묶음 S(0,3) → SS(1)
	const TArray<FGachaRevealEntry> Entries = MakeEntries({3, 4, 5, 3});
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(Entries, false, false))) { return false; }
	const FGachaRevealPlan& Plan = Runner.GetPlan();
	TestEqual(TEXT("박자 순서 = 도장 두 번"), PlanOrder(Plan), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Cascade,Stamp,Stamp,Spotlight,Settle,Result")));
	TestEqual(TEXT("단독 장면 = SSS(인덱스 2)"), Plan.TopIndex, 2);
	if (!TestEqual(TEXT("묶음 2개"), Plan.StampGroups.Num(), 2) || !TestEqual(TEXT("박자 11개"), Plan.Beats.Num(), 11)) { return false; }
	TestEqual(TEXT("묶음 0 = S"), Plan.StampGroups[0].Key, 3);
	TestEqual(TEXT("묶음 0 카드"), JoinInts(Plan.StampGroups[0].Value), FString(TEXT("0,3")));
	TestEqual(TEXT("묶음 1 = SS"), Plan.StampGroups[1].Key, 4);
	TestEqual(TEXT("묶음 1 카드"), JoinInts(Plan.StampGroups[1].Value), FString(TEXT("1")));
	TestEqual(TEXT("첫 Stamp = 묶음 0"), Plan.Beats[6].Group, 0);
	TestEqual(TEXT("둘째 Stamp = 묶음 1"), Plan.Beats[7].Group, 1);
	TestEqual(TEXT("묶음 0 길이 = 상승 + 타격 지연 + 2장 × 55 + 꼬리"), Plan.Beats[6].DurationMs, GT::StampRise + GT::StampHitDelay + 2.f * GT::StampGap + GT::StampTail, Tol);
	TestEqual(TEXT("묶음 0 길이 미러"), Plan.Beats[6].DurationMs, X::GroupsStamp0Ms, Tol);
	TestEqual(TEXT("묶음 1 길이 미러"), Plan.Beats[7].DurationMs, X::GroupsStamp1Ms, Tol);

	Run(Runner, Log, 2.896);
	if (!TestTrue(TEXT("첫 도장 중"), Runner.IsOnBeat(EGachaRevealBeat::Stamp) && Runner.GetCurrentBeat()->Group == 0)) { return false; }
	TestEqual(TEXT("첫 도장 진입"), RealAt(Log, EGachaRevealBeat::Stamp, 0), X::GroupsStamp0RealMs, TimeTolMs);
	TestEqual(TEXT("도장 중 탭 = 배속"), TapName(Runner.Tap()), FString(TEXT("SpedUp")));
	TestEqual(TEXT("도장 중 탭 시각"), Runner.GetElapsedRealMs(), X::GroupsTap0RealMs, TimeTolMs);
	Run(Runner, Log, 0.2);
	TestEqual(TEXT("둘째 도장 진입(첫 도장 남은 620 ÷ 4)"), RealAt(Log, EGachaRevealBeat::Stamp, 1), X::GroupsStamp1RealMs, TimeTolMs);
	TestEqual(TEXT("둘째 도장 진입 배속 = 1"), SpeedAt(Log, EGachaRevealBeat::Stamp, 1), 1.f);
	TestTrue(TEXT("지금 둘째 도장(묶음 1)"), Runner.IsOnBeat(EGachaRevealBeat::Stamp) && Runner.GetCurrentBeat()->Group == 1);

	Run(Runner, Log, 0.02);
	TestEqual(TEXT("둘째 도장 중 탭 = 배속"), TapName(Runner.Tap()), FString(TEXT("SpedUp")));
	TestEqual(TEXT("둘째 도장 중 탭 시각"), Runner.GetElapsedRealMs(), X::GroupsTap1RealMs, TimeTolMs);
	Run(Runner, Log, 0.3);
	TestEqual(TEXT("Spotlight 진입(둘째 도장 남은 600 ÷ 4) ― 도장을 건너뛰지 않음"), RealAt(Log, EGachaRevealBeat::Spotlight), X::GroupsSpotlightRealMs, TimeTolMs);
	TestEqual(TEXT("Spotlight 진입 배속 = 1"), SpeedAt(Log, EGachaRevealBeat::Spotlight), 1.f);
	TestEqual(TEXT("지나간 박자"), JoinBeats(Log.Beats), FString(TEXT("Throw,Cut,Upgrade,Lid,Deal,Cascade,Stamp,Stamp,Spotlight")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowSpotlightIntroTapTest,
	"CGR.Gacha.Reveal.Flow.SpotlightIntroTap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowSpotlightIntroTapTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealFlowTestUtil;
	namespace X = GachaRevealFlowExpect;
	FGachaRevealBeatRunner Runner;
	FFlowLog Log;
	Bind(Runner, Log);
	if (!TestTrue(TEXT("Setup"), Runner.Setup(MakeEntries(TenGrades()), false, false))) { return false; }

	// 단독 장면 들어오는 중(대기 전) 탭 = 배속 ― 남은 들어오기·대기 800·돌아가기가 전부 4배
	Run(Runner, Log, 3.773);
	if (!TestTrue(TEXT("Spotlight 100ms 째"), Runner.IsOnBeat(EGachaRevealBeat::Spotlight))) { return false; }
	TestFalse(TEXT("아직 대기 전"), Runner.IsWaiting());
	TestEqual(TEXT("들어오는 중 탭 = 배속(대기 끝냄 아님)"), TapName(Runner.Tap()), FString(TEXT("SpedUp")));
	TestEqual(TEXT("탭 시각"), Runner.GetElapsedRealMs(), X::IntroTapRealMs, TimeTolMs);

	Run(Runner, Log, 0.35);
	TestTrue(TEXT("4.123초 = 대기 중"), Runner.IsWaiting());
	TestTrue(TEXT("대기 시작(남은 들어오기 620 ÷ 4)"), SeenWithinFrame(Log.WaitSeenRealMs, X::IntroWaitStartRealMs));
	Run(Runner, Log, 0.02);
	TestTrue(TEXT("4.143초 = 대기 끝(800 ÷ 4 = 200)"), Runner.IsOnBeat(EGachaRevealBeat::Spotlight) && !Runner.IsWaiting());
	TestTrue(TEXT("대기 끝 시각"), SeenWithinFrame(Log.WaitEndSeenRealMs, X::IntroWaitEndRealMs));
	Run(Runner, Log, 0.2);
	TestEqual(TEXT("Settle 진입(돌아가기 440 ÷ 4)"), RealAt(Log, EGachaRevealBeat::Settle), X::IntroSettleRealMs, TimeTolMs);
	TestEqual(TEXT("Settle 진입 배속 = 1"), SpeedAt(Log, EGachaRevealBeat::Settle), 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
