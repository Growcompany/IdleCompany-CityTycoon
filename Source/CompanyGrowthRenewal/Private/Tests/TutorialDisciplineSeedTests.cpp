#include "Misc/AutomationTest.h"
#include "Utils/TutorialDisciplineSeed.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTutorialDisciplineSeedTest,
	"CGR.Tutorial.DisciplineSeed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialDisciplineSeedTest::RunTest(const FString& Parameters)
{
	using namespace TutorialDisciplineSeed;

	const TArray<EProductionDiscipline> A = BuildSeedList(0);
	TestEqual(TEXT("항상 2명"), A.Num(), 2);
	TestTrue(TEXT("OrderRoll 0 = 개발 먼저"), A[0] == EProductionDiscipline::Dev);
	TestTrue(TEXT("OrderRoll 0 = 기획이 뒤로"), A[1] == EProductionDiscipline::Plan);

	const TArray<EProductionDiscipline> B = BuildSeedList(1);
	TestTrue(TEXT("OrderRoll 1 = 기획이 먼저"), B[0] == EProductionDiscipline::Plan);
	TestTrue(TEXT("OrderRoll 1 = 개발이 뒤로"), B[1] == EProductionDiscipline::Dev);

	// 순서만 굴리고 구성은 고정 — 탭탭코인의 활성 2칸(기획/개발)과 1:1 이어야 어느 칸도 안 빈다
	for (int32 O = 0; O < 2; ++O)
	{
		const TArray<EProductionDiscipline> R = BuildSeedList(O);
		TestTrue(TEXT("개발 포함"), R.Contains(EProductionDiscipline::Dev));
		TestTrue(TEXT("기획 포함"), R.Contains(EProductionDiscipline::Plan));
		TestTrue(TEXT("서로 다름"), R[0] != R[1]);
		TestTrue(TEXT("그래픽/사운드/서버/QA 미포함"),
			!R.Contains(EProductionDiscipline::Graphics)
			&& !R.Contains(EProductionDiscipline::Sound)
			&& !R.Contains(EProductionDiscipline::Server)
			&& !R.Contains(EProductionDiscipline::QA));
	}

	// 범위 밖 롤 방어 — 모듈러로 접힌다
	const TArray<EProductionDiscipline> C = BuildSeedList(9);
	TestEqual(TEXT("범위 밖도 2명"), C.Num(), 2);
	TestTrue(TEXT("범위 밖도 개발 포함"), C.Contains(EProductionDiscipline::Dev));

	const TArray<EProductionDiscipline> D = BuildSeedList(-3);
	TestEqual(TEXT("음수 롤도 2명"), D.Num(), 2);
	TestTrue(TEXT("음수 롤도 개발 포함"), D.Contains(EProductionDiscipline::Dev));

	return true;
}

#endif
