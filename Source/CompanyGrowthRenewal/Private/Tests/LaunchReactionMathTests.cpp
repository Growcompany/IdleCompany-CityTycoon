#include "Misc/AutomationTest.h"
#include "Utils/LaunchReactionMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRReactionScheduleTest,
	"CGR.Launch.Reaction.Schedule",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRReactionScheduleTest::RunTest(const FString& Parameters)
{
	const TArray<float> A = LaunchReactionMath::BuildReactionSchedule(7, 1234);
	const TArray<float> B = LaunchReactionMath::BuildReactionSchedule(7, 1234);
	TestEqual(TEXT("개수 7"), A.Num(), 7);
	if (A.Num() != 7 || B.Num() != 7) { return false; }
	TestTrue(TEXT("첫 건 = FirstDelay 4s"), FMath::IsNearlyEqual(A[0], 4.f));
	for (int32 i = 1; i < A.Num(); ++i)
	{
		const float Gap = A[i] - A[i - 1];
		TestTrue(TEXT("간격 8~12s"), Gap >= 8.f - KINDA_SMALL_NUMBER && Gap <= 12.f + KINDA_SMALL_NUMBER);
		TestTrue(TEXT("같은 시드 = 같은 스케줄"), FMath::IsNearlyEqual(A[i], B[i]));
	}
	const TArray<float> C = LaunchReactionMath::BuildReactionSchedule(7, 4321);
	bool bDiffers = false;
	for (int32 i = 1; i < C.Num(); ++i) { if (!FMath::IsNearlyEqual(A[i], C[i])) { bDiffers = true; } }
	TestTrue(TEXT("다른 시드 = 다른 간격"), bDiffers);
	TestEqual(TEXT("0건"), LaunchReactionMath::BuildReactionSchedule(0, 1).Num(), 0);
	TestEqual(TEXT("음수 방어"), LaunchReactionMath::BuildReactionSchedule(-3, 1).Num(), 0);
	return true;
}

#endif
