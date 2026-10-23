#include "Core/CGGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "TimeCycle/TimeCycleContinuation.h"
#include "TimeCycle/TimeCycleManager.h"
#include "UObject/UnrealType.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace TimeCycleContinuityTests
{
FTimeCycleContinuation MakeState()
{
	FTimeCycleContinuation State;
	State.bValid = true;
	State.CurrentTime = 23 * 3600 + 59 * 60;
	State.NumCycles = 3;
	State.bIsCyclePaused = false;
	State.InitialTime = {8, 0, 0};
	State.CycleDuration = {0, 10, 0};
	State.SunRiseTime = {7, 0, 0};
	State.SunSetTime = {19, 0, 0};
	return State;
}

struct FFixture
{
	UWorld* TestWorld = nullptr;
	UCGGameInstance* Instance = nullptr;
	explicit FFixture(EWorldType::Type WorldType = EWorldType::EditorPreview)
	{
		TestWorld = UWorld::CreateWorld(WorldType, false);
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(WorldType).SetCurrentWorld(TestWorld); }
		Instance = NewObject<UCGGameInstance>();
		TestWorld->SetGameInstance(Instance);
	}
	~FFixture()
	{
		if (!TestWorld) { return; }
		TestWorld->SetGameInstance(nullptr);
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
	}
	ATimeCycleManager* SpawnClock()
	{
		ATimeCycleManager* Clock = TestWorld->SpawnActor<ATimeCycleManager>();
		Clock->DispatchBeginPlay();
		return Clock;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRVisualTimeIsolationTest, "CGR.TimeCycle.VisualOverride.IsolatedFromSimulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRVisualTimeIsolationTest::RunTest(const FString& Parameters)
{
	// 독립 테스트 월드는 actors-initialized 상태가 아니므로 동적 이벤트 실행을 허용한다.
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	TimeCycleContinuityTests::FFixture Fixture(EWorldType::Game);
	if (!TestNotNull(TEXT("fixture world"), Fixture.TestWorld)) { return false; }
	ATimeCycleManager* Clock = Fixture.SpawnClock();
	if (!TestNotNull(TEXT("clock"), Clock)) { return false; }
	Clock->StartCycle();
	const int32 InitialSeconds = Clock->GetCurrentTime().ToSeconds();
	const int32 InitialCycles = Clock->GetNumCycles();
	// 실제 일출/일몰 이벤트가 잘못 호출되면 사이클을 멈추도록 연결한다.
	Clock->OnSunRise.AddDynamic(Clock, &ATimeCycleManager::PauseCycle);
	Clock->OnSunSet.AddDynamic(Clock, &ATimeCycleManager::PauseCycle);
	TestFalse(TEXT("invalid visual time rejected"), Clock->SetVisualTimeOverride({24, 0, 0}));
	TestFalse(TEXT("invalid input leaves override off"), Clock->HasVisualTimeOverride());
	TestTrue(TEXT("night override accepted"), Clock->SetVisualTimeOverride({20, 0, 0}));
	TestTrue(TEXT("visual time is night"), Clock->IsVisualNight());
	TestEqual(TEXT("night override preserves real seconds"), Clock->GetCurrentTime().ToSeconds(), InitialSeconds);
	TestTrue(TEXT("repeated night override safe"), Clock->SetVisualTimeOverride({20, 0, 0}));
	TestTrue(TEXT("morning override accepted"), Clock->SetVisualTimeOverride({10, 0, 0}));
	TestFalse(TEXT("visual time is day"), Clock->IsVisualNight());
	TestEqual(TEXT("morning override preserves cycles"), Clock->GetNumCycles(), InitialCycles);
	Clock->Tick(0.1f);
	TestTrue(TEXT("simulation kept advancing without sunrise/sunset events"), Clock->GetCurrentTime().ToSeconds() > InitialSeconds);
	const int32 ProgressedSeconds = Clock->GetCurrentTime().ToSeconds();
	Clock->ClearVisualTimeOverride();
	Clock->ClearVisualTimeOverride();
	TestFalse(TEXT("repeated clear is safe"), Clock->HasVisualTimeOverride());
	TestEqual(TEXT("clear resumes progressed real time"), Clock->GetVisualTime().ToSeconds(), ProgressedSeconds);
	TestEqual(TEXT("clear preserves cycles"), Clock->GetNumCycles(), InitialCycles);
	Clock->Tick(0.1f);
	TestTrue(TEXT("cycle still running after clear"), Clock->GetCurrentTime().ToSeconds() > ProgressedSeconds);
	Clock->SetCurrentTime({18, 59, 0});
	TestTrue(TEXT("day override accepted across real sunset"), Clock->SetVisualTimeOverride({10, 0, 0}));
	Clock->Tick(1.f);
	TestTrue(TEXT("real sunset event still fired"), Clock->GetCurrentTime().ToSeconds() >= 19 * 3600);
	TestEqual(TEXT("real sunset does not replace visual phase"), Clock->GetVisualTime().ToSeconds(), 10 * 3600);
	const int32 SunsetSeconds = Clock->GetCurrentTime().ToSeconds();
	Clock->Tick(0.1f);
	TestEqual(TEXT("gameplay sunset delegate paused cycle"), Clock->GetCurrentTime().ToSeconds(), SunsetSeconds);
	Clock->ClearVisualTimeOverride();
	TestEqual(TEXT("clear restores real sunset phase"), Clock->GetVisualTime().ToSeconds(), SunsetSeconds);
	TestTrue(TEXT("clear restores night state"), Clock->IsVisualNight());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTimeCycleElapsedTest, "CGR.TimeCycle.Continuity.ElapsedAndMidnight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRTimeCycleElapsedTest::RunTest(const FString& Parameters)
{
	FTimeCycleContinuation State = TimeCycleContinuityTests::MakeState();
	TestTrue(TEXT("advance valid clock"), State.Advance(1.0));
	TestEqual(TEXT("144 game seconds cross midnight"), State.CurrentTime, 84.f);
	TestEqual(TEXT("cycle counts at initial hour, not midnight"), State.NumCycles, 3);
	TestTrue(TEXT("advance through two days and initial hour"), State.Advance(1400.0));
	TestEqual(TEXT("elapsed remains phase accurate"), State.CurrentTime, 28884.f);
	TestEqual(TEXT("three initial-hour crossings counted"), State.NumCycles, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTimeCyclePausedTest, "CGR.TimeCycle.Continuity.PausedAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRTimeCyclePausedTest::RunTest(const FString& Parameters)
{
	FTimeCycleContinuation State = TimeCycleContinuityTests::MakeState();
	State.bIsCyclePaused = true;
	State.CurrentTime = 12345.25f;
	State.CycleDuration = {0, 7, 0};
	State.SunRiseTime = {6, 30, 0};
	State.SunSetTime = {18, 45, 0};
	TestTrue(TEXT("paused clock accepts elapsed"), State.Advance(1000.0));
	TestEqual(TEXT("paused fractional time retained"), State.CurrentTime, 12345.25f);
	TestEqual(TEXT("paused cycles retained"), State.NumCycles, 3);
	State.bIsCyclePaused = false;
	TestTrue(TEXT("custom cycle resumes"), State.Advance(1.0));
	TestEqual(TEXT("existing integer cycle speed preserved"), State.CurrentTime, 12550.25f);
	TestTrue(TEXT("sunrise config untouched"), State.SunRiseTime == FTimeCycleCode({6, 30, 0}));
	TestTrue(TEXT("sunset config untouched"), State.SunSetTime == FTimeCycleCode({18, 45, 0}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTimeCycleMaxTest, "CGR.TimeCycle.Continuity.MaxTimeAcrossMidnight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRTimeCycleMaxTest::RunTest(const FString& Parameters)
{
	FTimeCycleContinuation State = TimeCycleContinuityTests::MakeState();
	State.InitialTime = {22, 0, 0};
	State.MaxTime = {2, 0, 0};
	State.bHasMaxTime = true;
	State.CycleDuration = {0, 4, 0};
	State.CurrentTime = 23 * 3600.f;
	TestTrue(TEXT("max clock advances"), State.Advance(10000.0));
	TestEqual(TEXT("stops at first max even through midnight"), State.CurrentTime, 7200.f);
	TestTrue(TEXT("max pauses cycle"), State.bIsCyclePaused);
	TestEqual(TEXT("max completes one cycle"), State.NumCycles, 4);
	State.Advance(10000.0);
	TestEqual(TEXT("stopped max does not recount"), State.NumCycles, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTimeCycleInvalidTest, "CGR.TimeCycle.Continuity.InvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRTimeCycleInvalidTest::RunTest(const FString& Parameters)
{
	FTimeCycleContinuation State = TimeCycleContinuityTests::MakeState();
	TestFalse(TEXT("negative elapsed rejected"), State.Advance(-1.0));
	TestFalse(TEXT("nonfinite elapsed rejected"), State.Advance(std::numeric_limits<double>::infinity()));
	TestEqual(TEXT("invalid elapsed leaves time intact"), State.CurrentTime, 86340.f);
	State.CycleDuration = {};
	TestFalse(TEXT("zero duration rejected"), State.Advance(10.0));
	State = TimeCycleContinuityTests::MakeState();
	State.CurrentTime = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("nonfinite current time rejected"), State.Advance(1.0));
	State = TimeCycleContinuityTests::MakeState();
	State.SunRiseTime = {24, 0, 0};
	TestFalse(TEXT("out of range time rejected"), State.Advance(1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTimeCycleWorldTransferTest, "CGR.TimeCycle.Continuity.ActorTransferAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRTimeCycleWorldTransferTest::RunTest(const FString& Parameters)
{
	TimeCycleContinuityTests::FFixture Fixture;
	if (!TestNotNull(TEXT("fixture world"), Fixture.TestWorld)) { return false; }
	ATimeCycleManager* Source = Fixture.SpawnClock();
	Source->SetCurrentTime({21, 15, 0});
	Source->StartCycle();
	Source->PauseCycle();
	Source->EndPlay(EEndPlayReason::LevelTransition);
	const FTimeCycleContinuation Captured = Fixture.Instance->GetTimeCycleContinuation();
	TestTrue(TEXT("level transition captures clock"), Captured.bValid);
	TestTrue(TEXT("captured realtime populated"), Captured.CapturedRealtimeSeconds > 0.0);
	ATimeCycleManager* Destination = Fixture.SpawnClock();
	TestEqual(TEXT("new map restores city time rather than initial 08:00"), Destination->GetCurrentTime().ToSeconds(), 76500);
	Destination->Tick(2.f);
	TestEqual(TEXT("pause preserved in new map"), Destination->GetCurrentTime().ToSeconds(), 76500);
	TimeCycleContinuityTests::FFixture SeparateSession;
	TestFalse(TEXT("new game instance cannot inherit another session clock"), SeparateSession.Instance->GetTimeCycleContinuation().bValid);
	TestEqual(TEXT("direct Office uses existing default"), SeparateSession.SpawnClock()->GetCurrentTime().ToSeconds(), 28800);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTimeCycleRestoreElapsedTest, "CGR.TimeCycle.Continuity.RestoreElapsedAndEndReasons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRTimeCycleRestoreElapsedTest::RunTest(const FString& Parameters)
{
	TimeCycleContinuityTests::FFixture Fixture;
	FTimeCycleContinuation State = TimeCycleContinuityTests::MakeState();
	State.CurrentTime = 23 * 3600;
	State.CycleDuration = {0, 20, 0};
	State.SunSetTime = {18, 30, 0};
	State.CapturedRealtimeSeconds = FPlatformTime::Seconds() - 10.0;
	Fixture.Instance->SetTimeCycleContinuation(State);
	ATimeCycleManager* Clock = Fixture.SpawnClock();
	TestTrue(TEXT("loading elapsed uses restored speed"), FMath::Abs(Clock->GetCurrentTime().ToSeconds() - 83520) <= 5);
	TestTrue(TEXT("cycle config restored"), Clock->GetCycleDuration() == State.CycleDuration);
	TestTrue(TEXT("sunset config restored"), Clock->GetSunSetTime() == State.SunSetTime);
	Clock->SetCurrentTime({12, 0, 0});
	Clock->EndPlay(EEndPlayReason::EndPlayInEditor);
	TestEqual(TEXT("PIE stop does not overwrite continuity"), Fixture.Instance->GetTimeCycleContinuation().CurrentTime, State.CurrentTime);
	ATimeCycleManager* DestroyedClock = Fixture.SpawnClock();
	DestroyedClock->SetCurrentTime({11, 0, 0});
	DestroyedClock->EndPlay(EEndPlayReason::Destroyed);
	TestEqual(TEXT("actor destruction does not overwrite continuity"), Fixture.Instance->GetTimeCycleContinuation().CurrentTime, State.CurrentTime);
	return true;
}
#endif
