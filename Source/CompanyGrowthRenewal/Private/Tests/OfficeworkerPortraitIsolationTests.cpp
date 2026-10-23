#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeworkerPortraitIsolationTests
{
class FScopedTestWorld
{
public:
	FScopedTestWorld()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (TestWorld && GEngine)
		{
			FWorldContext& TestWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			TestWorldContext.SetCurrentWorld(TestWorld);

			FURL URL;
			TestWorld->InitializeActorsForPlay(URL);
		}
	}

	~FScopedTestWorld()
	{
		if (!TestWorld)
		{
			return;
		}

		if (GEngine)
		{
			GEngine->DestroyWorldContext(TestWorld);
		}
		TestWorld->DestroyWorld(false);
	}

	UWorld* Get() const
	{
		return TestWorld;
	}

private:
	UWorld* TestWorld = nullptr;
};

AFunnyOfficeworker* SpawnDeferredWorker(
	UWorld* TestWorld,
	const FTransform& SpawnTransform,
	const bool bPortraitMode)
{
	if (!TestWorld)
	{
		return nullptr;
	}

	AFunnyOfficeworker* Worker = TestWorld->SpawnActorDeferred<AFunnyOfficeworker>(
		AFunnyOfficeworker::StaticClass(),
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Worker)
	{
		Worker->bIsPortraitMode = bPortraitMode;
		Worker->FinishSpawning(SpawnTransform);
	}
	return Worker;
}

struct FPortraitIsolationTestState
{
	FScopedTestWorld TestWorldScope;
	AFunnyOfficeworker* PortraitWorker = nullptr;
	AFunnyOfficeworker* NormalWorker = nullptr;
};

class FVerifyDelayedBehaviorInit final : public IAutomationLatentCommand
{
public:
	FVerifyDelayedBehaviorInit(
		const TSharedRef<FPortraitIsolationTestState>& InTestState,
		FAutomationTestBase* InTest)
		: TestState(InTestState)
		, Test(InTest)
	{
	}

	virtual bool Update() override
	{
		if (!Test)
		{
			return true;
		}

		UWorld* TestWorld = TestState->TestWorldScope.Get();
		if (!TestWorld
			|| !IsValid(TestState->PortraitWorker)
			|| !IsValid(TestState->NormalWorker))
		{
			Test->AddError(TEXT("portrait isolation latent state became invalid"));
			return true;
		}

		UEmployeeBehaviorComponent* PortraitBehavior = TestState->PortraitWorker->BehaviorComponent;
		UEmployeeBehaviorComponent* NormalBehavior = TestState->NormalWorker->BehaviorComponent;
		if (!PortraitBehavior || !NormalBehavior)
		{
			Test->AddError(TEXT("worker behavior component became invalid"));
			return true;
		}

		// Latent command의 첫 호출과 검증을 서로 다른 엔진 프레임으로 분리한다.
		if (!bObservedFirstEngineFrame)
		{
			bObservedFirstEngineFrame = true;
			FirstEngineFrame = GFrameCounter;

			FTimerManager& TimerManager = TestWorld->GetTimerManager();
			if (!TimerManager.HasBeenTickedThisFrame())
			{
				// 아직 한 번도 tick되지 않은 manager는 여기서 pending timer만 active로 승격한다.
				TimerManager.Tick(0.f);
				bAdvancedTimerManager = true;
			}
			return false;
		}

		if (GFrameCounter == FirstEngineFrame)
		{
			return false;
		}

		FTimerManager& TimerManager = TestWorld->GetTimerManager();
		if (!NormalBehavior->Walking && !TimerManager.HasBeenTickedThisFrame())
		{
			// 첫 유효 tick은 pending 승격, 그 다음 프레임의 0.2초 tick은 0.1초 auto-init을 만료시킨다.
			TimerManager.Tick(bAdvancedTimerManager ? 0.2f : 0.f);
			bAdvancedTimerManager = true;
		}

		if (!NormalBehavior->Walking)
		{
			++WaitedEngineFrames;
			if (WaitedEngineFrames >= 120)
			{
				Test->AddError(TEXT("normal behavior auto-init did not fire within 120 engine frames"));
				return true;
			}
			return false;
		}

		Test->TestFalse(
			TEXT("portrait worker remains outside random roaming after delayed behavior init"),
			TestState->PortraitWorker->IsRoaming());
		Test->TestFalse(
			TEXT("portrait behavior auto-init side effect remains suppressed"),
			PortraitBehavior->Walking);
		Test->TestTrue(
			TEXT("normal behavior auto-init sets Walking after its pending timer fires"),
			NormalBehavior->Walking);
		Test->TestTrue(
			TEXT("normal worker remains roaming after delayed behavior init"),
			TestState->NormalWorker->IsRoaming());
		return true;
	}

private:
	TSharedRef<FPortraitIsolationTestState> TestState;
	FAutomationTestBase* Test = nullptr;
	uint64 FirstEngineFrame = 0;
	int32 WaitedEngineFrames = 0;
	bool bObservedFirstEngineFrame = false;
	bool bAdvancedTimerManager = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeworkerPortraitIsolationTest,
	"CGR.Office.PortraitIsolation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeworkerPortraitIsolationTest::RunTest(const FString& Parameters)
{
	TSharedRef<OfficeworkerPortraitIsolationTests::FPortraitIsolationTestState> TestState =
		MakeShared<OfficeworkerPortraitIsolationTests::FPortraitIsolationTestState>();
	UWorld* TestWorld = TestState->TestWorldScope.Get();
	if (!TestNotNull(TEXT("transient game world"), TestWorld))
	{
		return false;
	}

	TestState->PortraitWorker = OfficeworkerPortraitIsolationTests::SpawnDeferredWorker(
		TestWorld,
		FTransform::Identity,
		true);
	TestState->NormalWorker = OfficeworkerPortraitIsolationTests::SpawnDeferredWorker(
		TestWorld,
		FTransform(FVector(500.f, 0.f, 0.f)),
		false);
	AFunnyOfficeworker* PortraitWorker = TestState->PortraitWorker;
	AFunnyOfficeworker* NormalWorker = TestState->NormalWorker;
	if (!TestNotNull(TEXT("portrait worker"), PortraitWorker)
		|| !TestNotNull(TEXT("normal worker"), NormalWorker))
	{
		return false;
	}
	if (!TestNotNull(TEXT("portrait behavior component"), PortraitWorker->BehaviorComponent)
		|| !TestNotNull(TEXT("normal behavior component"), NormalWorker->BehaviorComponent))
	{
		return false;
	}

	PortraitWorker->StartRandomRoaming();
	NormalWorker->StartRandomRoaming();
	if (!TestTrue(TEXT("portrait worker is roaming before BeginPlay"), PortraitWorker->IsRoaming())
		|| !TestTrue(TEXT("normal worker is roaming before BeginPlay"), NormalWorker->IsRoaming()))
	{
		return false;
	}
	TestFalse(
		TEXT("portrait behavior has no auto-init side effect before BeginPlay"),
		PortraitWorker->BehaviorComponent->Walking);
	TestFalse(
		TEXT("normal behavior has no auto-init side effect before BeginPlay"),
		NormalWorker->BehaviorComponent->Walking);

	PortraitWorker->DispatchBeginPlay();
	NormalWorker->DispatchBeginPlay();
	TestTrue(TEXT("portrait worker completed BeginPlay"), PortraitWorker->HasActorBegunPlay());
	TestTrue(TEXT("normal worker completed BeginPlay"), NormalWorker->HasActorBegunPlay());
	TestTrue(
		TEXT("portrait behavior skips component auto-init before Super BeginPlay"),
		PortraitWorker->BehaviorComponent->bSkipAutoInit);
	TestFalse(
		TEXT("normal behavior preserves component auto-init"),
		NormalWorker->BehaviorComponent->bSkipAutoInit);
	TestFalse(TEXT("portrait worker clears pre-armed roaming during BeginPlay"), PortraitWorker->IsRoaming());
	TestTrue(TEXT("normal worker preserves pre-armed roaming during BeginPlay"), NormalWorker->IsRoaming());
	TestFalse(
		TEXT("normal behavior auto-init is still pending immediately after BeginPlay"),
		NormalWorker->BehaviorComponent->Walking);

	TInlineComponentArray<UPrimitiveComponent*> PortraitPrimitives;
	PortraitWorker->GetComponents(PortraitPrimitives);
	if (!TestTrue(TEXT("portrait worker has primitive components"), PortraitPrimitives.Num() > 0))
	{
		return false;
	}
	for (const UPrimitiveComponent* PrimitiveComponent : PortraitPrimitives)
	{
		TestTrue(
			*FString::Printf(
				TEXT("portrait primitive %s is visible only in SceneCapture"),
				*PrimitiveComponent->GetName()),
			PrimitiveComponent->bVisibleInSceneCaptureOnly);
	}

	TInlineComponentArray<UPrimitiveComponent*> NormalPrimitives;
	NormalWorker->GetComponents(NormalPrimitives);
	if (!TestTrue(TEXT("normal worker has primitive components"), NormalPrimitives.Num() > 0))
	{
		return false;
	}
	for (const UPrimitiveComponent* PrimitiveComponent : NormalPrimitives)
	{
		TestFalse(
			*FString::Printf(
				TEXT("normal primitive %s remains visible outside SceneCapture"),
				*PrimitiveComponent->GetName()),
			PrimitiveComponent->bVisibleInSceneCaptureOnly);
	}

	ADD_LATENT_AUTOMATION_COMMAND(
		OfficeworkerPortraitIsolationTests::FVerifyDelayedBehaviorInit(TestState, this));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
