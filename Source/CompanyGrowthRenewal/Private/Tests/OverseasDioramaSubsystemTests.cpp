#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Entity/OverseasDiorama/OverseasDioramaStage.h"
#include "Manager/OverseasDioramaSubsystem.h"
#include "Misc/App.h"
#include "Tests/OverseasDioramaTestSpecs.h"

namespace
{
	struct FDioSubsystemFixture
	{
		UWorld* World = nullptr;
		UOverseasDioramaSubsystem* Subsystem = nullptr;
		TMap<ECountryType, FOverseasDioramaSpec> Specs;
		TArray<ECountryType> Broadcasts;

		FDioSubsystemFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
			{
				return;
			}
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			Subsystem = World->GetSubsystem<UOverseasDioramaSubsystem>();
			if (!Subsystem)
			{
				return;
			}
			Specs.Add(ECountryType::Korea, OverseasDioramaTestSpecs::Korea4());
			Specs.Add(ECountryType::Japan, OverseasDioramaTestSpecs::Japan6());
			Specs.Add(ECountryType::USA, OverseasDioramaTestSpecs::Usa3());
			Subsystem->SetSpecProviderForTest([this](ECountryType Country, FOverseasDioramaSpec& Out)
			{
				if (const FOverseasDioramaSpec* Found = Specs.Find(Country))
				{
					Out = *Found;
					return true;
				}
				Out = FOverseasDioramaSpec();
				Out.Country = Country;
				return true;
			});
			Subsystem->OnMiniatureUpdated.AddLambda([this](ECountryType Country) { Broadcasts.Add(Country); });
		}

		~FDioSubsystemFixture()
		{
			if (World)
			{
				if (GEngine) { GEngine->DestroyWorldContext(World); }
				World->DestroyWorld(false);
			}
		}

		void TickTimes(int32 Count)
		{
			for (int32 i = 0; i < Count; ++i)
			{
				Subsystem->Tick(0.033f);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSubsystemQueueTest, "CGR.Overseas.Subsystem.QueueOnlyChanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasSubsystemQueueTest::RunTest(const FString& Parameters)
{
	FDioSubsystemFixture F;
	if (!TestNotNull(TEXT("subsystem"), F.Subsystem)) { return false; }
	F.Subsystem->RequestMiniatures({ ECountryType::Korea, ECountryType::Japan });
	F.TickTimes(2);
	TestEqual(TEXT("two captures"), F.Broadcasts.Num(), 2);
	TestTrue(TEXT("korea"), F.Broadcasts.Contains(ECountryType::Korea));
	TestTrue(TEXT("japan"), F.Broadcasts.Contains(ECountryType::Japan));
	FDioramaStillInfo Info;
	TestTrue(TEXT("korea size known"), F.Subsystem->GetMiniature(ECountryType::Korea, Info) != nullptr || Info.SizePx.X > 0 || !FApp::CanEverRender());

	F.Broadcasts.Reset();
	F.Subsystem->RequestMiniatures({ ECountryType::Korea, ECountryType::Japan });
	F.TickTimes(2);
	TestEqual(TEXT("unchanged not recaptured"), F.Broadcasts.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSubsystemDirtyTest, "CGR.Overseas.Subsystem.DirtyRecaptures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasSubsystemDirtyTest::RunTest(const FString& Parameters)
{
	FDioSubsystemFixture F;
	if (!TestNotNull(TEXT("subsystem"), F.Subsystem)) { return false; }
	F.Subsystem->RequestMiniatures({ ECountryType::Korea });
	F.TickTimes(1);
	F.Broadcasts.Reset();
	F.Specs[ECountryType::Korea].Factory.Slots[2] = EDioramaSlotState::Completed;
	F.Subsystem->MarkDirty(ECountryType::Korea);
	F.TickTimes(2);
	TestEqual(TEXT("one recapture"), F.Broadcasts.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSubsystemNoStillWhileLiveTest, "CGR.Overseas.Subsystem.NoStillWhileLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasSubsystemNoStillWhileLiveTest::RunTest(const FString& Parameters)
{
	FDioSubsystemFixture F;
	if (!TestNotNull(TEXT("subsystem"), F.Subsystem)) { return false; }
	TestNotNull(TEXT("live rt"), F.Subsystem->BeginLiveView(ECountryType::Korea, FIntPoint(800, 600), EDioramaModule::Factory));
	F.Subsystem->RequestMiniatures({ ECountryType::Japan });
	F.TickTimes(3);
	TestEqual(TEXT("no still while live"), F.Broadcasts.Num(), 0);
	F.Subsystem->EndLiveView();
	F.TickTimes(2);
	TestEqual(TEXT("both after exit"), F.Broadcasts.Num(), 2);
	TestTrue(TEXT("live country recaptured"), F.Broadcasts.Contains(ECountryType::Korea));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSubsystemLiveRebuildTest, "CGR.Overseas.Subsystem.LiveRebuildKeepsCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasSubsystemLiveRebuildTest::RunTest(const FString& Parameters)
{
	FDioSubsystemFixture F;
	if (!TestNotNull(TEXT("subsystem"), F.Subsystem)) { return false; }
	F.Subsystem->BeginLiveView(ECountryType::Korea, FIntPoint(800, 600), EDioramaModule::Factory);
	AOverseasDioramaStage* Stage = F.Subsystem->GetStageForTest();
	if (!TestNotNull(TEXT("stage"), Stage)) { return false; }
	const FDioramaLiveCamera Before = Stage->GetLiveCameraGoal();
	F.Specs[ECountryType::Korea].Factory.AutomationTier = 3;
	F.Subsystem->MarkDirty(ECountryType::Korea);
	F.TickTimes(1);
	const FDioramaLiveCamera After = Stage->GetLiveCameraGoal();
	TestTrue(TEXT("target kept"), Before.TargetM.Equals(After.TargetM, 0.001));
	TestEqual(TEXT("dist kept"), After.DistM, Before.DistM, 0.001);
	TestEqual(TEXT("rebuilt with new spec"), Stage->GetBuiltSpec().Factory.AutomationTier, 3);
	TestFalse(TEXT("live build"), Stage->IsBuiltMiniature());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSubsystemEmptySpecTest, "CGR.Overseas.Subsystem.EmptySpecNoLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasSubsystemEmptySpecTest::RunTest(const FString& Parameters)
{
	FDioSubsystemFixture F;
	if (!TestNotNull(TEXT("subsystem"), F.Subsystem)) { return false; }
	TestNull(TEXT("no rt"), F.Subsystem->BeginLiveView(ECountryType::Australia, FIntPoint(800, 600), EDioramaModule::Mine));
	TestFalse(TEXT("not live"), F.Subsystem->IsLiveView());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasSubsystemSwitchTest, "CGR.Overseas.Subsystem.SwitchCountryLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasSubsystemSwitchTest::RunTest(const FString& Parameters)
{
	FDioSubsystemFixture F;
	if (!TestNotNull(TEXT("subsystem"), F.Subsystem)) { return false; }
	UTextureRenderTarget2D* First = F.Subsystem->BeginLiveView(ECountryType::Korea, FIntPoint(800, 600), EDioramaModule::Factory);
	UTextureRenderTarget2D* Second = F.Subsystem->BeginLiveView(ECountryType::USA, FIntPoint(800, 600), EDioramaModule::Factory);
	TestTrue(TEXT("same rt"), First != nullptr && First == Second);
	TestTrue(TEXT("usa live"), F.Subsystem->GetLiveCountry() == ECountryType::USA);
	int32 Capturing = 0;
	if (AOverseasDioramaStage* Stage = F.Subsystem->GetStageForTest())
	{
		TArray<USceneCaptureComponent2D*> Captures;
		Stage->GetComponents<USceneCaptureComponent2D>(Captures);
		for (const USceneCaptureComponent2D* Capture : Captures)
		{
			Capturing += Capture->bCaptureEveryFrame ? 1 : 0;
		}
	}
	TestEqual(TEXT("one live capture"), Capturing, 1);
	return true;
}

#endif
