#include "Data/SkylineBeatTiming.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRSkylineBeatTimingTest,
	"CGR.SideContent.Skyline.BeatTiming",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRSkylineBeatTimingTest::RunTest(const FString& Parameters)
{
	FSkylineBeatTuning Tuning;

	// 첫 층은 튜닝 값 그대로다
	const FSkylineBeatDurations First = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, 0, false);
	TestEqual(TEXT("첫 층 개발 비트"), First.WorkMs, 380);
	TestEqual(TEXT("첫 층 돌파 비트"), First.SlamMs, 300);

	// 층이 올라갈수록 빨라진다
	const FSkylineBeatDurations Third = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, 2, false);
	TestTrue(TEXT("세 번째 층이 첫 층보다 빠르다"), Third.WorkMs + Third.SlamMs < First.WorkMs + First.SlamMs);

	// 한 층 전체가 하한 아래로 내려가지 않는다
	for (int32 Index = 0; Index < 40; ++Index)
	{
		const FSkylineBeatDurations Beats = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, Index, false);
		TestTrue(TEXT("층 전체가 하한 이상"), Beats.WorkMs + Beats.SlamMs >= Tuning.MinFloorBeatMs);
	}

	// 하한에 걸려도 두 비트의 비율은 유지된다
	const FSkylineBeatDurations Deep = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, 30, false);
	const float Ratio = static_cast<float>(Deep.WorkMs) / static_cast<float>(FMath::Max(1, Deep.SlamMs));
	TestTrue(TEXT("개발:돌파 비율 유지"), FMath::IsNearlyEqual(Ratio, 380.0f / 300.0f, 0.08f));

	// 보스 층은 같은 인덱스의 일반 층보다 확실히 무겁다
	const FSkylineBeatDurations Boss = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, 6, true);
	const FSkylineBeatDurations Normal = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, 6, false);
	TestTrue(TEXT("보스가 더 느리다"), Boss.WorkMs + Boss.SlamMs > Normal.WorkMs + Normal.SlamMs);

	// 가속 1.0 이면 모든 층이 같다
	FSkylineBeatTuning Flat = Tuning;
	Flat.AccelPerFloor = 1.0f;
	const FSkylineBeatDurations FlatA = FSkylineBeatTimeline::ComputeFloorBeats(Flat, 0, false);
	const FSkylineBeatDurations FlatB = FSkylineBeatTimeline::ComputeFloorBeats(Flat, 9, false);
	TestEqual(TEXT("가속 없으면 개발 비트 동일"), FlatA.WorkMs, FlatB.WorkMs);
	TestEqual(TEXT("가속 없으면 돌파 비트 동일"), FlatA.SlamMs, FlatB.SlamMs);

	// 모션 최소화 경로 — 둘 다 0 이면 0 을 돌려준다(타이머 없이 결과 직행)
	FSkylineBeatTuning Zero;
	Zero.WorkMs = 0;
	Zero.SlamMs = 0;
	const FSkylineBeatDurations None = FSkylineBeatTimeline::ComputeFloorBeats(Zero, 0, false);
	TestEqual(TEXT("모션 최소화 개발 0"), None.WorkMs, 0);
	TestEqual(TEXT("모션 최소화 돌파 0"), None.SlamMs, 0);

	// 10층 등반 총합이 6초 이내다 (수용 기준 §4-1)
	int32 TotalMs = 0;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FSkylineBeatDurations Beats = FSkylineBeatTimeline::ComputeFloorBeats(Tuning, Index, Index == 6);
		TotalMs += Beats.WorkMs + Beats.SlamMs;
	}
	TestTrue(TEXT("10층 등반 6초 이내"), TotalMs <= 6000);

	return true;
}

#endif
