#include "Entity/Factory/BrickFactory.h"
#include "Entity/Factory/BrickLaborer.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CGGameInstance.h"
#include "Data/FactoryUpgradeData.h"
#include "Data/TutorialDerivedMissionRules.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FactoryExpansionEmployeeTests
{
	struct FFixture
	{
		UGameInstance* Instance = nullptr;
		UWorld* TestWorld = nullptr;
		ABrickFactory* Factory = nullptr;
		UResourceItemManager* Resources = nullptr;
		UTableManagerSubsystem* Tables = nullptr;

		FFixture()
		{
			if (!GEngine) return;
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->InitializeStandalone();
			TestWorld = Instance->GetWorld();
			if (!TestWorld) return;
			TestWorld->InitializeActorsForPlay(FURL());
			Resources = Instance->GetSubsystem<UResourceItemManager>();
			Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
			Factory = TestWorld->SpawnActor<ABrickFactory>();
		}

		~FFixture()
		{
			if (Factory) Factory->Destroy();
			if (Instance) Instance->Shutdown();
			if (TestWorld)
			{
				TestWorld->SetGameInstance(nullptr);
				if (GEngine) GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool IsReady() const { return Factory && Resources && Tables; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryChimneyUpgradeSocketTest,
	"CGR.Factory.ChimneySmoke.UpgradeRefreshesSockets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryChimneyUpgradeSocketTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
	{
		AddWarning(TEXT("Skipped: use -saveddirsuffix=FactoryEmployeeVerify to isolate fixture shutdown saves"));
		return true;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;
	FactoryExpansionEmployeeTests::FFixture Fixture;
	if (!TestTrue(TEXT("Factory upgrade fixture is ready"), Fixture.IsReady())) return false;
	UStaticMeshComponent* BodyMesh = Cast<UStaticMeshComponent>(Fixture.Factory->GetMainMeshComponent());
	if (!TestNotNull(TEXT("Factory has its main static mesh component"), BodyMesh)) return false;

	TMap<EResourceType, int64> StartingResources;
	StartingResources.Add(EResourceType::Money, 2'000'000'000'000LL);
	Fixture.Resources->SetAllResources(StartingResources, false);
	Fixture.Factory->SetActorTransform(FTransform(FRotator(0.f, 90.f, 0.f), FVector(700.f, -300.f, 100.f)));
	FFactorySaveData LoadedData = Fixture.Factory->GetFactoryData();
	LoadedData.UpgradeData.FindOrAdd(EFactoryUpgradeType::FactoryExpansion).Level = 7;
	Fixture.Factory->SetFactoryData(LoadedData);
	if (!TestEqual(TEXT("Level 7 has five chimney points"), Fixture.Factory->SpawnPoints.Num(), 5)) return false;
	const TArray<USceneComponent*> PreviousPoints = Fixture.Factory->SpawnPoints;
	for (USceneComponent* Point : PreviousPoints)
	{
		TestTrue(TEXT("Level 7 point starts at its actual socket"), Point &&
			Point->GetComponentLocation().Equals(BodyMesh->GetSocketLocation(Point->GetAttachSocketName()), 0.1f));
	}

	if (!TestTrue(TEXT("The final factory upgrade can be purchased"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 1))) return false;
	if (!TestEqual(TEXT("The final factory upgrade is level 8"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion), 8)) return false;
	if (!TestEqual(TEXT("Level 8 has six chimney points"), Fixture.Factory->SpawnPoints.Num(), 6)) return false;
	for (int32 Index = 0; Index < Fixture.Factory->SpawnPoints.Num(); ++Index)
	{
		USceneComponent* Point = Fixture.Factory->SpawnPoints[Index];
		if (!TestNotNull(TEXT("Every active chimney point exists"), Point)) continue;
		if (PreviousPoints.IsValidIndex(Index))
		{
			TestEqual(TEXT("Existing chimney components are reused"), Point, PreviousPoints[Index]);
		}
		const FVector SocketPosition = BodyMesh->GetSocketLocation(Point->GetAttachSocketName());
		TestTrue(FString::Printf(TEXT("%s refreshes to the final mesh socket without moving the factory"),
			*Point->GetAttachSocketName().ToString()), Point->GetComponentLocation().Equals(SocketPosition, 0.1f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryExpansionLaborerCapacityTest,
	"CGR.Factory.Expansion.LaborerCapacityPurchase",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryExpansionLaborerCapacityTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
	{
		AddWarning(TEXT("Skipped: use -saveddirsuffix=FactoryEmployeeVerify to isolate the save written when the fixture shuts down"));
		return true;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;

	FactoryExpansionEmployeeTests::FFixture Fixture;
	if (!TestTrue(TEXT("Native factory, resources, and factory tables are ready"), Fixture.IsReady())) return false;

	FFactoryUpgradeDefinition LaborerDefinition;
	if (!TestTrue(TEXT("Laborer purchase definition is loaded"),
		Fixture.Tables->GetFactoryUpgradeDefinition(EFactoryUpgradeType::LaborerCount, LaborerDefinition))) return false;
	if (!TestEqual(TEXT("Laborer purchase uses Money"), LaborerDefinition.CostResourceType, EResourceType::Money)) return false;

	const int64 StartingMoney = 2'000'000'000'000LL;
	TMap<EResourceType, int64> StartingResources;
	StartingResources.Add(EResourceType::Money, StartingMoney);
	Fixture.Resources->SetAllResources(StartingResources, false);
	Fixture.Factory->SetFactoryData(Fixture.Factory->GetFactoryData());
	if (!TestEqual(TEXT("Initial factory expansion level"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion), 1)) return false;
	if (!TestEqual(TEXT("Initial starter has one active chimney"),
		Fixture.Factory->SpawnPoints.Num(), 1)) return false;
	if (!TestEqual(TEXT("Initial laborer upgrade level"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::LaborerCount), 1)) return false;
	if (!TestFalse(TEXT("Offline collection remains locked"),
		Fixture.Factory->IsAutoCollectionUnlocked())) return false;

	// A bulk request at factory level 1 hires only the three workers needed to reach five.
	const int64 ExpectedCost = LaborerDefinition.CalculateUpgradeCost(1)
		+ LaborerDefinition.CalculateUpgradeCost(2)
		+ LaborerDefinition.CalculateUpgradeCost(3);
	if (!TestTrue(TEXT("Laborers can be bought before offline collection unlocks"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::LaborerCount, 50))) return false;
	TestEqual(TEXT("x50 stops at the initial five-worker capacity"),
		Fixture.Factory->GetDesiredLaborerCount(), 5);
	TestEqual(TEXT("x50 advances only the three affordable capacity levels"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::LaborerCount), 4);
	TestEqual(TEXT("Only the three actual worker purchases are charged"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), StartingMoney - ExpectedCost);

	const int64 MoneyAtCapacity = Fixture.Resources->GetResourceAmount(EResourceType::Money);
	TestFalse(TEXT("A further purchase at capacity is rejected"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::LaborerCount, 1));
	TestEqual(TEXT("Rejected purchase leaves the wallet unchanged"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyAtCapacity);
	TestEqual(TEXT("Rejected purchase leaves the crew unchanged"),
		Fixture.Factory->GetDesiredLaborerCount(), 5);

	if (!TestTrue(TEXT("Buying factory level 2 succeeds"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 1))) return false;
	TestEqual(TEXT("Factory expansion reaches level 2"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion), 2);
	TestEqual(TEXT("Expanding the factory does not hire workers automatically"),
		Fixture.Factory->GetDesiredLaborerCount(), 5);
	TestEqual(TEXT("Expansion does not change the worker purchase level"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::LaborerCount), 4);

	const int32 Capacities[] = { 10, 15, 25, 40, 60, 80, 100 };
	for (int32 StageIndex = 0; StageIndex < UE_ARRAY_COUNT(Capacities); ++StageIndex)
	{
		const int32 FactoryLevel = StageIndex + 2;
		const int32 Capacity = Capacities[StageIndex];
		const int32 ExistingCrew = StageIndex == 0 ? 5 : Capacities[StageIndex - 1];
		if (FactoryLevel > 2)
		{
			if (!TestTrue(FString::Printf(TEXT("Factory level %d can be purchased"), FactoryLevel),
				Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 1))) return false;
		}
		if (!TestEqual(FString::Printf(TEXT("Factory reaches level %d"), FactoryLevel),
			Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion), FactoryLevel)) return false;
		if (!TestEqual(FString::Printf(TEXT("Factory level %d keeps existing crew"), FactoryLevel),
			Fixture.Factory->GetDesiredLaborerCount(), ExistingCrew)) return false;
		const FFactoryAppearanceRow* Appearance = Fixture.Tables->FindFactoryAppearance(FactoryLevel);
		if (!TestNotNull(FString::Printf(TEXT("Factory level %d has an appearance"), FactoryLevel), Appearance)) return false;
		if (!TestEqual(FString::Printf(TEXT("Factory level %d uses its appearance capacity"), FactoryLevel),
			Fixture.Factory->GetLaborerCapacity(), Appearance->LaborerCapacity)) return false;
		const int32 ExpectedChimneys = FMath::Max(1, FactoryLevel - 2);
		if (!TestEqual(FString::Printf(TEXT("Factory level %d refreshes its active chimney sockets"), FactoryLevel),
			Fixture.Factory->SpawnPoints.Num(), ExpectedChimneys)) return false;

		const int32 PreviousLevel = Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::LaborerCount);
		const int32 Purchases = Capacity - ExistingCrew;
		int64 ExpectedStageCost = 0;
		for (int32 Offset = 0; Offset < Purchases; ++Offset)
		{
			ExpectedStageCost += LaborerDefinition.CalculateUpgradeCost(PreviousLevel + Offset);
		}
		const int64 StageMoney = Fixture.Resources->GetResourceAmount(EResourceType::Money);
		if (!TestTrue(FString::Printf(TEXT("Factory level %d can buy workers up to %d"), FactoryLevel, Capacity),
			Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::LaborerCount, 50))) return false;
		TestEqual(FString::Printf(TEXT("Factory level %d crew reaches capacity"), FactoryLevel),
			Fixture.Factory->GetDesiredLaborerCount(), Capacity);
		TestEqual(FString::Printf(TEXT("Factory level %d worker level reflects purchased count"), FactoryLevel),
			Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::LaborerCount), Capacity - 1);
		TestEqual(FString::Printf(TEXT("Factory level %d charges only %d purchased workers"), FactoryLevel, Purchases),
			Fixture.Resources->GetResourceAmount(EResourceType::Money), StageMoney - ExpectedStageCost);

		const int64 MoneyAtStageCapacity = Fixture.Resources->GetResourceAmount(EResourceType::Money);
		TestFalse(FString::Printf(TEXT("Factory level %d rejects a worker over capacity"), FactoryLevel),
			Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::LaborerCount, 1));
		TestEqual(FString::Printf(TEXT("Factory level %d rejects without spending Money"), FactoryLevel),
			Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyAtStageCapacity);
	}

	const int64 MoneyAtMaximum = Fixture.Resources->GetResourceAmount(EResourceType::Money);
	TestFalse(TEXT("A ninth factory level cannot be purchased"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 1));
	TestEqual(TEXT("The rejected factory expansion does not spend Money"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyAtMaximum);
	const FFactorySaveData SavedAtLevelEight = Fixture.Factory->GetFactoryData();
	Fixture.Factory->SetFactoryData(SavedAtLevelEight);
	TestEqual(TEXT("Restoring the same saved factory level keeps the hundred-worker crew"),
		Fixture.Factory->GetDesiredLaborerCount(), 100);
	TestEqual(TEXT("Restoring the same saved factory level does not lower worker upgrades"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::LaborerCount), 99);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryLaborerGaitUpgradePlaybackTest,
	"CGR.Factory.LaborerGait.UpgradePreservesPlayback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryLaborerGaitUpgradePlaybackTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
	{
		AddWarning(TEXT("Skipped: use -saveddirsuffix=FactoryEmployeeVerify to isolate fixture shutdown saves"));
		return true;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;

	FactoryExpansionEmployeeTests::FFixture Fixture;
	if (!TestTrue(TEXT("Factory upgrade fixture is ready"), Fixture.IsReady())) return false;

	TMap<EResourceType, int64> StartingResources;
	StartingResources.Add(EResourceType::Money, 2'000'000'000'000LL);
	Fixture.Resources->SetAllResources(StartingResources, false);
	Fixture.Factory->SetFactoryData(Fixture.Factory->GetFactoryData());

	auto FindCrew = [&Fixture]()
	{
		TArray<ABrickLaborer*> Crew;
		for (TActorIterator<ABrickLaborer> It(Fixture.TestWorld); It; ++It)
		{
			if (It->GetOwner() == Fixture.Factory)
			{
				Crew.Add(*It);
			}
		}
		return Crew;
	};

	struct FPlaybackSnapshot
	{
		ABrickLaborer* Laborer = nullptr;
		USkeletalMeshComponent* Body = nullptr;
		UAnimSingleNodeInstance* Node = nullptr;
		UAnimationAsset* Asset = nullptr;
		float Position = 0.f;
		FVector Location = FVector::ZeroVector;
	};

	const EFactoryUpgradeType Purchases[] = {
		EFactoryUpgradeType::HoldProductionAmount,
		EFactoryUpgradeType::HoldProductionSpeed,
		EFactoryUpgradeType::LaborerCount
	};
	for (EFactoryUpgradeType Purchase : Purchases)
	{
		const TArray<ABrickLaborer*> ExistingCrew = FindCrew();
		if (!TestEqual(TEXT("Current crew matches the purchased laborer count"),
			ExistingCrew.Num(), Fixture.Factory->GetDesiredLaborerCount())) return false;

		TArray<FPlaybackSnapshot> Before;
		for (ABrickLaborer* Laborer : ExistingCrew)
		{
			USkeletalMeshComponent* Body = nullptr;
			TArray<USkeletalMeshComponent*> Meshes;
			Laborer->GetComponents(Meshes);
			for (USkeletalMeshComponent* Candidate : Meshes)
			{
				if (Candidate && Candidate->GetFName() == TEXT("Mesh"))
				{
					Body = Candidate;
					break;
				}
			}
			if (!TestNotNull(TEXT("Existing worker has the body Mesh component"), Body)) return false;
			UAnimSingleNodeInstance* Node = Body->GetSingleNodeInstance();
			if (!TestNotNull(TEXT("Existing worker uses a live single-node instance"), Node)) return false;
			UAnimationAsset* Asset = Node->GetCurrentAsset();
			if (!TestNotNull(TEXT("Existing worker has a walking animation"), Asset)) return false;
			if (!TestTrue(TEXT("Existing worker animation is playing"), Node->IsPlaying())) return false;
			if (!TestTrue(TEXT("Walking animation has a usable duration"), Node->GetLength() > 0.f)) return false;

			// A time in the middle of the actual clip distinguishes continued playback from lane-phase restart.
			Body->SetPosition(Node->GetLength() * 0.43f, false);
			if (!TestTrue(TEXT("Worker starts the purchase at a mid-clip position"), Body->GetPosition() > 0.f)) return false;
			Before.Add({ Laborer, Body, Node, Asset, Body->GetPosition(), Laborer->GetActorLocation() });
		}

		if (!TestTrue(FString::Printf(TEXT("Upgrade %d succeeds"), static_cast<int32>(Purchase)),
			Fixture.Factory->UpgradeFactory(Purchase, 1))) return false;
		const TArray<ABrickLaborer*> AfterCrew = FindCrew();
		const int32 ExpectedCount = ExistingCrew.Num() + (Purchase == EFactoryUpgradeType::LaborerCount ? 1 : 0);
		TestEqual(TEXT("Only a laborer purchase adds a worker"), AfterCrew.Num(), ExpectedCount);

		for (const FPlaybackSnapshot& Snapshot : Before)
		{
			if (!TestTrue(TEXT("Existing worker actor is retained"), AfterCrew.Contains(Snapshot.Laborer))) return false;
			TArray<USkeletalMeshComponent*> CurrentMeshes;
			Snapshot.Laborer->GetComponents(CurrentMeshes);
			if (!TestTrue(TEXT("Existing worker body component is retained"), CurrentMeshes.Contains(Snapshot.Body))) return false;
			if (!TestEqual(TEXT("Existing worker keeps its animation instance"),
				Snapshot.Body->GetSingleNodeInstance(), Snapshot.Node)) return false;
			TestEqual(TEXT("Existing worker keeps the same walking clip"), Snapshot.Node->GetCurrentAsset(), Snapshot.Asset);
			TestTrue(TEXT("Existing worker stays in motion"), Snapshot.Node->IsPlaying());
			TestTrue(TEXT("Upgrade preserves the current animation time"),
				FMath::IsNearlyEqual(Snapshot.Body->GetPosition(), Snapshot.Position, 0.001f));
			TestTrue(TEXT("Upgrade does not move an existing worker"),
				Snapshot.Laborer->GetActorLocation().Equals(Snapshot.Location, 0.1f));

			const float TickSeconds = FMath::Min(0.05f, Snapshot.Node->GetLength() * 0.1f);
			Snapshot.Body->TickAnimation(TickSeconds, false);
			Snapshot.Body->RefreshBoneTransforms();
			TestTrue(TEXT("Existing worker playback advances after the upgrade"),
				Snapshot.Body->GetPosition() > Snapshot.Position);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryOffscreenBrickCollectionTest,
	"CGR.Factory.BrickCollection.OffscreenCreditsWithoutFlight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryOffscreenBrickCollectionTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
	{
		AddWarning(TEXT("Skipped: use -saveddirsuffix=FactoryEmployeeVerify to isolate fixture shutdown saves"));
		return true;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;
	FactoryExpansionEmployeeTests::FFixture Fixture;
	if (!TestTrue(TEXT("Factory collection fixture is ready"), Fixture.IsReady())) return false;
	UInGameLayerWidget* Layer = CreateWidget<UInGameLayerWidget>(Fixture.Instance, UInGameLayerWidget::StaticClass());
	UUIManagerSubsystem* UIManager = Fixture.Instance->GetSubsystem<UUIManagerSubsystem>();
	FObjectPropertyBase* CanvasProperty = FindFProperty<FObjectPropertyBase>(
		UInGameLayerWidget::StaticClass(), TEXT("EffectCanvas"));
	if (!TestNotNull(TEXT("HUD layer exists"), Layer) || !TestNotNull(TEXT("UI manager exists"), UIManager)
		|| !TestNotNull(TEXT("Effect canvas binding exists"), CanvasProperty)) return false;
	UCanvasPanel* Canvas = NewObject<UCanvasPanel>(Layer);
	CanvasProperty->SetObjectPropertyValue_InContainer(Layer, Canvas);
	TGuardValue<UInGameLayerWidget*> LayerGuard(UIManager->UIInGameMain, Layer);
	const int64 Before = Fixture.Resources->GetResourceAmount(EResourceType::Brick);
	Layer->SpawnBrickCollectAt(FVector2D(150.f, 100.f), EResourceType::Brick, 17, false);
	TestEqual(TEXT("Unprojectable production credits the complete batch immediately"),
		Fixture.Resources->GetResourceAmount(EResourceType::Brick), Before + 17);
	TestEqual(TEXT("Unprojectable production creates no flight widgets"), Canvas->GetChildrenCount(), 0);
	Layer->SpawnBrickCollectAt(FVector2D(-5000.f, -5000.f), EResourceType::Brick, 23);
	TestEqual(TEXT("Production without a ready viewport is preserved exactly once"),
		Fixture.Resources->GetResourceAmount(EResourceType::Brick), Before + 40);
	TestEqual(TEXT("Counter-only batches never add a flight widget"), Canvas->GetChildrenCount(), 0);
	Fixture.Factory->HarvestBricksAt(FVector(50000.f, 0.f, 0.f), 7);
	TestEqual(TEXT("Factory production without a projecting player still credits once"),
		Fixture.Resources->GetResourceAmount(EResourceType::Brick), Before + 47);
	TestEqual(TEXT("A missing projection never produces a flight"), Canvas->GetChildrenCount(), 0);
	UIManager->UIInGameMain = nullptr;
	Fixture.Factory->HarvestBricksAt(FVector(50000.f, 0.f, 0.f), 11);
	TestEqual(TEXT("Factory production survives a missing HUD during transitions"),
		Fixture.Resources->GetResourceAmount(EResourceType::Brick), Before + 58);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryStartupEconomyTest,
	"CGR.Factory.StartupEconomy.FirstCompanyPath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryStartupEconomyTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
	{
		AddError(TEXT("Use -saveddirsuffix=FactoryEmployeeVerify; an unrun startup economy fixture must not pass"));
		return false;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;
	FactoryExpansionEmployeeTests::FFixture Fixture;
	if (!TestTrue(TEXT("Startup factory fixture is ready"), Fixture.IsReady())) return false;
	// InitializeActorsForPlay does not guarantee BeginPlay on this standalone test world.
	// Dispatch it so production timers registered by BeginPlay are exercised by world ticks below.
	if (!Fixture.Factory->HasActorBegunPlay()) Fixture.Factory->DispatchBeginPlay();
	if (!TestTrue(TEXT("Factory BeginPlay has run before timer checks"),
		Fixture.Factory->HasActorBegunPlay())) return false;

	bool bLoaded = false;
	const FResourceInfo MoneyInfo = Fixture.Tables->GetResourceInfo(EResourceType::Money, bLoaded);
	if (!TestTrue(TEXT("Money definition is loaded from DT_Resource"), bLoaded)) return false;
	int64 ExpectedMoney = MoneyInfo.InitialAmount;
	if (!TestEqual(TEXT("Normal resource initialization uses DT_Resource starting Money"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), ExpectedMoney)) return false;
	if (!TestEqual(TEXT("Starter factory has two workers"), Fixture.Factory->GetDesiredLaborerCount(), 2)) return false;

	const EFactoryUpgradeType PurchaseTypes[] = {
		EFactoryUpgradeType::LaborerCount,
		EFactoryUpgradeType::LaborerCount,
		EFactoryUpgradeType::LaborerCount,
		EFactoryUpgradeType::FactoryExpansion,
		EFactoryUpgradeType::HoldProductionSpeed,
		EFactoryUpgradeType::HoldProductionAmount
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PurchaseTypes); ++Index)
	{
		const EFactoryUpgradeType Type = PurchaseTypes[Index];
		FFactoryUpgradeDefinition Definition;
		if (!TestTrue(FString::Printf(TEXT("Purchase %d has a DT cost definition"), Index),
			Fixture.Tables->GetFactoryUpgradeDefinition(Type, Definition))) return false;
		if (!TestEqual(FString::Printf(TEXT("Purchase %d costs Money"), Index),
			Definition.CostResourceType, EResourceType::Money)) return false;
		const int32 Level = Fixture.Factory->GetUpgradeLevel(Type);
		const int64 Cost = Definition.CalculateUpgradeCost(Level);
		if (!TestTrue(FString::Printf(TEXT("Purchase %d has a positive cost"), Index), Cost > 0)) return false;
		if (!TestTrue(FString::Printf(TEXT("Starting budget covers purchase %d"), Index),
			ExpectedMoney >= Cost)) return false;
		if (!TestTrue(FString::Printf(TEXT("Purchase %d succeeds from the startup budget"), Index),
			Fixture.Factory->UpgradeFactory(Type, 1))) return false;
		ExpectedMoney -= Cost;
		if (!TestEqual(FString::Printf(TEXT("Purchase %d charges its DT cost exactly"), Index),
			Fixture.Resources->GetResourceAmount(EResourceType::Money), ExpectedMoney)) return false;
	}
	if (!TestEqual(TEXT("Three hires bring the factory to five workers"),
		Fixture.Factory->GetDesiredLaborerCount(), 5)) return false;
	if (!TestEqual(TEXT("The first expansion reaches level two"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::FactoryExpansion), 2)) return false;

	const int64 BrickBefore = Fixture.Resources->GetResourceAmount(EResourceType::Brick);
	// This nonvisual world has no controller. Its one-shot UI bootstrap runs on the first tick;
	// factory timers still advance normally, while UI input is covered by the standalone playthrough.
	AddExpectedError(TEXT("\\[UIManagerSubsystem\\] PlayerController is null!"), EAutomationExpectedErrorFlags::Contains, 1);
	const float IdleStartTime = Fixture.TestWorld->GetTimeSeconds();
	for (int32 TickIndex = 0; TickIndex < 5; ++TickIndex) Fixture.TestWorld->Tick(LEVELTICK_All, 0.25f);
	if (!TestTrue(TEXT("Idle world time advances through the production observation window"),
		Fixture.TestWorld->GetTimeSeconds() - IdleStartTime >= 1.f)) return false;
	Fixture.Factory->OnLaborerEntered(7);
	if (!TestTrue(TEXT("Idle factory delivery credits Brick"),
		Fixture.Resources->GetResourceAmount(EResourceType::Brick) > BrickBefore)) return false;
	if (!TestEqual(TEXT("Idle factory delivery never credits Money"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), ExpectedMoney)) return false;
	Fixture.Factory->OnInteract_Implementation(nullptr);
	if (!TestTrue(TEXT("Factory enters hold production"), Fixture.Factory->IsHoldingBoost())) return false;
	const float HoldStartTime = Fixture.TestWorld->GetTimeSeconds();
	for (int32 TickIndex = 0; TickIndex < 5; ++TickIndex) Fixture.TestWorld->Tick(LEVELTICK_All, 0.25f);
	if (!TestTrue(TEXT("Hold world time advances through the production observation window"),
		Fixture.TestWorld->GetTimeSeconds() - HoldStartTime >= 1.f)) return false;
	Fixture.Factory->OnLaborerEntered(7);
	Fixture.Factory->OnEndInteract_Implementation(nullptr);
	if (!TestEqual(TEXT("Hold factory delivery never credits Money"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), ExpectedMoney)) return false;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryTutorialPurchaseLimitTest,
	"CGR.Factory.StartupEconomy.TutorialPurchaseLimit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryTutorialPurchaseLimitTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_FactoryEmployeeVerify")))
	{
		AddError(TEXT("Use -saveddirsuffix=FactoryEmployeeVerify for the tutorial purchase fixture"));
		return false;
	}
	if (!TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;
	FactoryExpansionEmployeeTests::FFixture Fixture;
	if (!TestTrue(TEXT("Tutorial purchase fixture is ready"), Fixture.IsReady())) return false;
	UMissionManagerSubsystem* Missions = Fixture.Instance->GetSubsystem<UMissionManagerSubsystem>();
	if (!TestNotNull(TEXT("Mission subsystem exists"), Missions)) return false;
	bool bLoaded = false;
	const int64 StartingMoney = Fixture.Tables->GetResourceInfo(EResourceType::Money, bLoaded).InitialAmount;
	if (!TestTrue(TEXT("Starting Money row is loaded"), bLoaded)) return false;
	if (!TestEqual(TEXT("Fixture starts with the DT Money balance"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), StartingMoney)) return false;

	// Standalone fixture has no normal mission restore API; the dev jump selects DT rows.
	// Do not tick this world: its deferred prerequisite seed is outside this economy check.
	Missions->DevJumpToMission(TEXT("M1_CollectBricks"));
	if (!TestEqual(TEXT("M1 is active"), Missions->GetActiveConditionType(), EMissionConditionType::CollectBricks)) return false;
	TestEqual(TEXT("Mission selection does not grant Money"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), StartingMoney);
	const FFactorySaveData InitialFactory;
	const int32 InitialExpansionLevel = InitialFactory.UpgradeData.FindChecked(EFactoryUpgradeType::FactoryExpansion).Level;
	const int32 InitialCapacity = Fixture.Factory->GetLaborerCapacityAtLevel(InitialExpansionLevel);
	const int32 HiresNeeded = InitialCapacity - Fixture.Factory->GetDesiredLaborerCount();
	TestFalse(TEXT("M1 cannot expand before teaching the full starter crew"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 1));
	TestEqual(TEXT("Early expansion leaves starting Money intact"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), StartingMoney);
	if (!TestEqual(TEXT("M1 limits hiring to the starter capacity"),
		Fixture.Factory->GetAllowedUpgradeCount(EFactoryUpgradeType::LaborerCount), HiresNeeded)) return false;
	if (!TestTrue(TEXT("M1 bulk hire fills only the starter capacity"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::LaborerCount, 50))) return false;
	if (!TestEqual(TEXT("M1 crew reaches starter capacity"), Fixture.Factory->GetDesiredLaborerCount(), InitialCapacity)) return false;
	if (!TestTrue(TEXT("M1 allows the first expansion"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 50))) return false;
	const int64 MoneyAfterM1 = Fixture.Resources->GetResourceAmount(EResourceType::Money);
	TestFalse(TEXT("Repeated M1 hire cannot drain the M2 budget"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::LaborerCount, 1));
	TestFalse(TEXT("Repeated M1 expansion cannot drain the M2 budget"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::FactoryExpansion, 1));
	TestFalse(TEXT("M1 observation cannot buy M2 speed early"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::HoldProductionSpeed, 1));
	TestEqual(TEXT("Rejected M1 purchases leave Money intact"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyAfterM1);

	Missions->DevJumpToMission(TEXT("M2_UpgradeFactory"));
	TestEqual(TEXT("M2 selection does not refill the M1 remainder"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyAfterM1);
	FMissionTable M2;
	if (!TestTrue(TEXT("M2 is active and has a DT target"), Missions->GetActiveMission(M2)
		&& M2.ConditionType == EMissionConditionType::UpgradeFactoryStats)) return false;
	const int32 Target = FTutorialDerivedMissionRules::ResolveTarget(M2.ConditionAmount);
	if (!TestTrue(TEXT("M2 bulk speed stops at its mission target"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::HoldProductionSpeed, 50))) return false;
	TestEqual(TEXT("M2 speed reaches only the required level"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionSpeed), Target);
	const int64 MoneyBeforeAmount = Fixture.Resources->GetResourceAmount(EResourceType::Money);
	TestFalse(TEXT("Held speed repeat is refused at target"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::HoldProductionSpeed, 1));
	TestEqual(TEXT("Refused speed repeat preserves the required amount budget"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyBeforeAmount);
	if (!TestTrue(TEXT("Required amount remains affordable and purchasable"),
		Fixture.Factory->UpgradeFactory(EFactoryUpgradeType::HoldProductionAmount, 50))) return false;
	TestEqual(TEXT("M2 amount reaches only the required level"),
		Fixture.Factory->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionAmount), Target);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
