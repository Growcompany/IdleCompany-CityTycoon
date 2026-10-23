#include "Entity/Ambient/StreetLampSpotPool.h"
#include "Entity/Plot/CityPlotStreetFurniture.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "ShaderPlatformCachedIniValue.h"
#include "TimeCycle/TimeCycleManager.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace StreetLampSpotPoolTests
{
struct FScopedMobileLocalLights
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.Forward.EnableLocalLights"));
	TSharedPtr<IConsoleVariable> PlatformVariable;
	const FName Tag = TEXT("CGR.Tests.StreetLampSpotPool");

	FScopedMobileLocalLights(int32 Value, EShaderPlatform ShaderPlatform)
	{
		if (!Variable) { return; }
#if ALLOW_OTHER_PLATFORM_CONFIG
		// 미리보기 렌더러는 호스트 대신 대상 플랫폼의 설정을 읽을 수 있다.
		const EShaderPlatform ActualPlatform = GetEditorShaderPlatform(ShaderPlatform);
		const FName IniPlatformName = ShaderPlatformToPlatformName(ActualPlatform);
		if (IniPlatformName != FName(FPlatformProperties::IniPlatformName()) && ActualPlatform != GMaxRHIShaderPlatform)
		{
			PlatformVariable = Variable->GetPlatformValueVariable(IniPlatformName);
			Variable = PlatformVariable.Get();
		}
#endif
		if (Variable) { Variable->Set(Value, ECVF_SetByPreview, Tag); }
	}

	~FScopedMobileLocalLights()
	{
		if (Variable) { Variable->Unset(ECVF_SetByPreview, Tag); }
	}

	bool IsSetTo(int32 Value) const { return Variable && Variable->GetInt() == Value; }
};

struct FFixture
{
	UWorld* TestWorld = nullptr;
	AStreetLampSpotPool* SpotPool = nullptr;
	UStaticMesh* LampMesh = nullptr;

	explicit FFixture(int32 PoolSize = 0, ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num,
		const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube"))
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, FeatureLevel);
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld); }
		TestWorld->InitializeActorsForPlay(FURL());
		LampMesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
		SpotPool = TestWorld->SpawnActor<AStreetLampSpotPool>();
		if (!SpotPool || !LampMesh) { return; }

		FArrayProperty* MeshProperty = FindFProperty<FArrayProperty>(AStreetLampSpotPool::StaticClass(), TEXT("LampMeshes"));
		FIntProperty* SizeProperty = FindFProperty<FIntProperty>(AStreetLampSpotPool::StaticClass(), TEXT("PoolSize"));
		if (!MeshProperty || !CastField<FSoftObjectProperty>(MeshProperty->Inner) || !SizeProperty) { return; }
		auto* Meshes = MeshProperty->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UStaticMesh>>>(SpotPool);
		Meshes->Add(LampMesh);
		SizeProperty->SetPropertyValue_InContainer(SpotPool, PoolSize);
		SpotPool->DispatchBeginPlay();
	}

	~FFixture()
	{
		if (!TestWorld) { return; }
		if (GEngine) { GEngine->DestroyWorldContext(TestWorld); }
		TestWorld->DestroyWorld(false);
	}

	bool IsReady() const { return TestWorld && SpotPool && LampMesh && SpotPool->HasActorBegunPlay(); }

	ULevel* MakeLevel(int32 LampCount, const FVector& Offset = FVector::ZeroVector)
	{
		ULevel* AddedLevel = NewObject<ULevel>(TestWorld);
		AddedLevel->OwningWorld = TestWorld;
		TestWorld->AddLevel(AddedLevel);
		AddedLevel->bIsVisible = true;
		if (LampCount > 0)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = AddedLevel;
			AActor* LampActor = TestWorld->SpawnActor<AActor>(SpawnParams);
			if (!LampActor) { return AddedLevel; }
			UInstancedStaticMeshComponent* Lamps = NewObject<UInstancedStaticMeshComponent>(LampActor);
			LampActor->SetRootComponent(Lamps);
			LampActor->AddInstanceComponent(Lamps);
			Lamps->SetStaticMesh(LampMesh);
			Lamps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Lamps->SetCanEverAffectNavigation(false);
			Lamps->RegisterComponent();
			for (int32 Index = 0; Index < LampCount; ++Index)
			{
				Lamps->AddInstance(FTransform(Offset + FVector(500.f * Index, 0.f, 0.f)));
			}
		}
		return AddedLevel;
	}

	void BroadcastAdded(ULevel* AddedLevel)
	{
		AddedLevel->bIsVisible = true;
		FWorldDelegates::LevelAddedToWorld.Broadcast(AddedLevel, TestWorld);
		FlushRebuild();
	}

	void BroadcastRemoved(ULevel* RemovedLevel)
	{
		TestWorld->RemoveLevel(RemovedLevel);
		RemovedLevel->bIsVisible = false;
		FWorldDelegates::LevelRemovedFromWorld.Broadcast(RemovedLevel, TestWorld);
		FlushRebuild();
	}

	void FlushRebuild()
	{
		// A single actor tick applies the streaming changes collected since the previous tick.
		SpotPool->Tick(0.016f);
	}

	int32 CardCount() const
	{
		int32 Count = 0;
		TArray<UInstancedStaticMeshComponent*> Components;
		SpotPool->GetComponents(Components);
		for (const UInstancedStaticMeshComponent* Component : Components) { Count += Component->GetInstanceCount(); }
		return Count;
	}

	int32 CardComponentCount() const
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		SpotPool->GetComponents(Components);
		return Components.Num();
	}

	UInstancedStaticMeshComponent* CardComponent() const
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		SpotPool->GetComponents(Components);
		return Components.Num() == 1 ? Components[0] : nullptr;
	}

	int32 LightCount() const
	{
		TArray<USpotLightComponent*> Components;
		SpotPool->GetComponents(Components);
		return Components.Num();
	}

	bool CardsVisible() const
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		SpotPool->GetComponents(Components);
		for (const UInstancedStaticMeshComponent* Component : Components)
		{
			if (Component->GetInstanceCount() > 0) { return Component->IsVisible(); }
		}
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolLateLevelTest,
	"CGR.City.StreetLampSpotPool.LateLevelRecoversFromEmptyWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolLateLevelTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture;
	if (!TestTrue(TEXT("pool fixture and engine mesh are ready"), Fixture.IsReady())) { return false; }
	TestEqual(TEXT("the initial empty world has no cards"), Fixture.CardCount(), 0);
	ULevel* StreamedLevel = Fixture.MakeLevel(3);
	if (!TestNotNull(TEXT("streamed level"), StreamedLevel)) { return false; }
	int32 ActorsInLevel = 0;
	for (TActorIterator<AActor> It(Fixture.TestWorld); It; ++It)
	{
		if (It->GetLevel() == StreamedLevel) { ++ActorsInLevel; }
	}
	if (!TestEqual(TEXT("lamp actor is visible through world level iteration"), ActorsInLevel, 1)) { return false; }
	Fixture.BroadcastAdded(StreamedLevel);
	TestEqual(TEXT("late streamed lamps create one shared card per lamp"), Fixture.CardCount(), 3);
	TestTrue(TEXT("late cards inherit the default night visibility"), Fixture.CardsVisible());
	TestEqual(TEXT("card-only configuration creates no dynamic spotlights"), Fixture.LightCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolLevelLifecycleTest,
	"CGR.City.StreetLampSpotPool.LevelRemovalAndReaddAreIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolLevelLifecycleTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture;
	if (!TestTrue(TEXT("pool fixture and engine mesh are ready"), Fixture.IsReady())) { return false; }
	ULevel* FirstLevel = Fixture.MakeLevel(2);
	ULevel* SecondLevel = Fixture.MakeLevel(1);
	Fixture.BroadcastAdded(FirstLevel);
	if (!TestEqual(TEXT("first level creates one card component"), Fixture.CardComponentCount(), 1)) { return false; }
	UInstancedStaticMeshComponent* const SharedCards = Fixture.CardComponent();
	UMaterialInterface* const SharedMaterial = SharedCards->GetMaterial(0);
	if (!TestNotNull(TEXT("shared card material"), SharedMaterial)) { return false; }
	Fixture.BroadcastAdded(SecondLevel);
	TestEqual(TEXT("cards from two loaded levels are combined"), Fixture.CardCount(), 3);
	TestTrue(TEXT("second level reuses the card component and material"),
		Fixture.CardComponentCount() == 1 && Fixture.CardComponent() == SharedCards && SharedCards->GetMaterial(0) == SharedMaterial);
	Fixture.BroadcastAdded(FirstLevel);
	TestEqual(TEXT("duplicate add event does not duplicate cards"), Fixture.CardCount(), 3);
	TestTrue(TEXT("duplicate event reuses the card component and material"),
		Fixture.CardComponentCount() == 1 && Fixture.CardComponent() == SharedCards && SharedCards->GetMaterial(0) == SharedMaterial);
	Fixture.BroadcastRemoved(FirstLevel);
	TestEqual(TEXT("unloaded level cards are removed"), Fixture.CardCount(), 1);
	TestTrue(TEXT("removal reuses the card component and material"),
		Fixture.CardComponentCount() == 1 && Fixture.CardComponent() == SharedCards && SharedCards->GetMaterial(0) == SharedMaterial);
	Fixture.TestWorld->AddLevel(FirstLevel);
	Fixture.BroadcastAdded(FirstLevel);
	TestEqual(TEXT("reloaded level cards return exactly once"), Fixture.CardCount(), 3);
	TestTrue(TEXT("readd reuses the card component and material"),
		Fixture.CardComponentCount() == 1 && Fixture.CardComponent() == SharedCards && SharedCards->GetMaterial(0) == SharedMaterial);
	Fixture.BroadcastRemoved(SecondLevel);
	Fixture.BroadcastRemoved(FirstLevel);
	TestEqual(TEXT("all unloaded levels leave no cards"), Fixture.CardCount(), 0);
	TestTrue(TEXT("empty pool retains its single shared card component and material"),
		Fixture.CardComponentCount() == 1 && Fixture.CardComponent() == SharedCards && SharedCards->GetMaterial(0) == SharedMaterial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolFixedLightBudgetTest,
	"CGR.City.StreetLampSpotPool.LateLevelKeepsFixedLightBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolFixedLightBudgetTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture(2);
	if (!TestTrue(TEXT("pool fixture and engine mesh are ready"), Fixture.IsReady())) { return false; }
	ULevel* FirstLevel = Fixture.MakeLevel(4);
	Fixture.BroadcastAdded(FirstLevel);
	TestEqual(TEXT("four streamed lamps receive four cards"), Fixture.CardCount(), 4);
	TestEqual(TEXT("late lamps use exactly the configured two dynamic spotlights"), Fixture.LightCount(), 2);
	ULevel* SecondLevel = Fixture.MakeLevel(3);
	Fixture.BroadcastAdded(SecondLevel);
	Fixture.BroadcastAdded(FirstLevel);
	TestEqual(TEXT("seven unique lamps receive seven cards"), Fixture.CardCount(), 7);
	TestEqual(TEXT("more streamed lamps never expand the light budget"), Fixture.LightCount(), 2);
	Fixture.BroadcastRemoved(FirstLevel);
	TestEqual(TEXT("light budget remains fixed after unloading"), Fixture.LightCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolMobileLocalLightsDisabledTest,
	"CGR.City.StreetLampSpotPool.MobileForwardWithoutLocalLightsKeepsCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolMobileLocalLightsDisabledTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture(2, ERHIFeatureLevel::ES3_1);
	if (!TestTrue(TEXT("mobile pool fixture is ready"), Fixture.IsReady())) { return false; }
	if (!TestNotNull(TEXT("mobile scene"), Fixture.TestWorld->Scene)) { return false; }
	StreetLampSpotPoolTests::FScopedMobileLocalLights LocalLights(0, Fixture.TestWorld->Scene->GetShaderPlatform());
	if (!TestTrue(TEXT("mobile local lights CVar is set to zero"), LocalLights.IsSetTo(0))) { return false; }
	if (!TestTrue(TEXT("fixture uses a mobile shader platform"), IsMobilePlatform(Fixture.TestWorld->Scene->GetShaderPlatform()))
		|| !TestFalse(TEXT("fixture uses mobile forward shading"), IsMobileDeferredShadingEnabled(Fixture.TestWorld->Scene->GetShaderPlatform()))) { return false; }
	if (!TestFalse(TEXT("mobile renderer reads local lights as disabled"), MobileForwardEnableLocalLights(Fixture.TestWorld->Scene->GetShaderPlatform()))) { return false; }
	ATimeCycleManager* Clock = Fixture.TestWorld->SpawnActor<ATimeCycleManager>();
	if (!TestNotNull(TEXT("clock"), Clock)) { return false; }
	Clock->DispatchBeginPlay();
	Clock->SetCurrentTime(FTimeCycleCode{12, 0, 0});
	ULevel* FirstLevel = Fixture.MakeLevel(2);
	ULevel* SecondLevel = Fixture.MakeLevel(1);
	Fixture.BroadcastAdded(FirstLevel);
	Fixture.BroadcastAdded(SecondLevel);
	TestEqual(TEXT("streamed lamps keep all three floor cards"), Fixture.CardCount(), 3);
	TestFalse(TEXT("daylight hides floor cards"), Fixture.CardsVisible());
	TestEqual(TEXT("disabled mobile local lights create no spotlights"), Fixture.LightCount(), 0);
	Clock->SetCurrentTime(FTimeCycleCode{22, 0, 0});
	Fixture.SpotPool->Tick(0.016f);
	TestTrue(TEXT("night shows floor cards"), Fixture.CardsVisible());
	TestEqual(TEXT("night does not create spotlights"), Fixture.LightCount(), 0);
	Fixture.BroadcastRemoved(FirstLevel);
	TestEqual(TEXT("unloading removes only its floor cards"), Fixture.CardCount(), 1);
	TestTrue(TEXT("remaining floor card stays visible at night"), Fixture.CardsVisible());
	Fixture.TestWorld->AddLevel(FirstLevel);
	Fixture.BroadcastAdded(FirstLevel);
	TestEqual(TEXT("reloading restores floor cards exactly once"), Fixture.CardCount(), 3);
	TestEqual(TEXT("reloading does not create spotlights"), Fixture.LightCount(), 0);
	Clock->SetCurrentTime(FTimeCycleCode{12, 0, 0});
	Fixture.SpotPool->Tick(0.016f);
	TestFalse(TEXT("daylight hides reloaded floor cards"), Fixture.CardsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolOtherRendererLightsTest,
	"CGR.City.StreetLampSpotPool.DesktopAndEnabledMobileKeepSpotlights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolOtherRendererLightsTest::RunTest(const FString& Parameters)
{
	{
		StreetLampSpotPoolTests::FFixture DesktopFixture(2, ERHIFeatureLevel::SM5);
		if (!TestTrue(TEXT("desktop pool fixture is ready"), DesktopFixture.IsReady())) { return false; }
		if (!TestNotNull(TEXT("desktop scene"), DesktopFixture.TestWorld->Scene)) { return false; }
		StreetLampSpotPoolTests::FScopedMobileLocalLights LocalLights(0, DesktopFixture.TestWorld->Scene->GetShaderPlatform());
		if (!TestTrue(TEXT("mobile local lights CVar is set to zero"), LocalLights.IsSetTo(0))) { return false; }
		if (!TestFalse(TEXT("desktop fixture uses a nonmobile shader platform"), IsMobilePlatform(DesktopFixture.TestWorld->Scene->GetShaderPlatform()))) { return false; }
		DesktopFixture.BroadcastAdded(DesktopFixture.MakeLevel(3));
		TestEqual(TEXT("desktop keeps its two spotlights when mobile local lights are disabled"), DesktopFixture.LightCount(), 2);
	}
	StreetLampSpotPoolTests::FFixture MobileFixture(2, ERHIFeatureLevel::ES3_1);
	if (!TestTrue(TEXT("enabled mobile pool fixture is ready"), MobileFixture.IsReady())) { return false; }
	if (!TestNotNull(TEXT("enabled mobile scene"), MobileFixture.TestWorld->Scene)) { return false; }
	StreetLampSpotPoolTests::FScopedMobileLocalLights LocalLights(1, MobileFixture.TestWorld->Scene->GetShaderPlatform());
	if (!TestTrue(TEXT("mobile local lights CVar is set to one"), LocalLights.IsSetTo(1))) { return false; }
	if (!TestTrue(TEXT("enabled fixture uses a mobile shader platform"), IsMobilePlatform(MobileFixture.TestWorld->Scene->GetShaderPlatform()))
		|| !TestFalse(TEXT("enabled fixture uses mobile forward shading"), IsMobileDeferredShadingEnabled(MobileFixture.TestWorld->Scene->GetShaderPlatform()))) { return false; }
	if (!TestTrue(TEXT("mobile renderer reads local lights as enabled"), MobileForwardEnableLocalLights(MobileFixture.TestWorld->Scene->GetShaderPlatform()))) { return false; }
	MobileFixture.BroadcastAdded(MobileFixture.MakeLevel(3));
	TestEqual(TEXT("mobile forward with local lights enabled keeps its two spotlights"), MobileFixture.LightCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolWorldAndDayNightTest,
	"CGR.City.StreetLampSpotPool.WorldIsolationDayNightAndEndPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolWorldAndDayNightTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture;
	if (!TestTrue(TEXT("pool fixture and engine mesh are ready"), Fixture.IsReady())) { return false; }
	ATimeCycleManager* Clock = Fixture.TestWorld->SpawnActor<ATimeCycleManager>();
	if (!TestNotNull(TEXT("clock"), Clock)) { return false; }
	Clock->DispatchBeginPlay();
	Clock->SetCurrentTime(FTimeCycleCode{12, 0, 0});
	ULevel* StreamedLevel = Fixture.MakeLevel(2);
	Fixture.BroadcastAdded(StreamedLevel);
	TestEqual(TEXT("cards exist during the day"), Fixture.CardCount(), 2);
	TestFalse(TEXT("daylight hides late streamed cards"), Fixture.CardsVisible());
	Clock->SetCurrentTime(FTimeCycleCode{22, 0, 0});
	Fixture.SpotPool->Tick(0.016f);
	TestTrue(TEXT("night reveals the same cards"), Fixture.CardsVisible());

	UWorld* ForeignWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("foreign world"), ForeignWorld)) { return false; }
	Fixture.TestWorld->RemoveLevel(StreamedLevel);
	StreamedLevel->bIsVisible = false;
	FWorldDelegates::LevelRemovedFromWorld.Broadcast(StreamedLevel, ForeignWorld);
	Fixture.FlushRebuild();
	TestEqual(TEXT("another world's level event cannot change this pool"), Fixture.CardCount(), 2);
	Fixture.TestWorld->AddLevel(StreamedLevel);
	StreamedLevel->bIsVisible = true;
	TestTrue(TEXT("streaming add delegate is registered while the pool plays"),
		FWorldDelegates::LevelAddedToWorld.IsBoundToObject(Fixture.SpotPool));
	TestTrue(TEXT("streaming remove delegate is registered while the pool plays"),
		FWorldDelegates::LevelRemovedFromWorld.IsBoundToObject(Fixture.SpotPool));
	Fixture.SpotPool->Destroy();
	TestFalse(TEXT("destroy removes the streaming add delegate"),
		FWorldDelegates::LevelAddedToWorld.IsBoundToObject(Fixture.SpotPool));
	TestFalse(TEXT("destroy removes the streaming remove delegate"),
		FWorldDelegates::LevelRemovedFromWorld.IsBoundToObject(Fixture.SpotPool));
	FWorldDelegates::LevelAddedToWorld.Broadcast(StreamedLevel, Fixture.TestWorld);
	ForeignWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolAssignmentInvalidationTest,
	"CGR.City.StreetLampSpotPool.CameraAndStreamingRefreshAssignments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolAssignmentInvalidationTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture(1);
	if (!TestTrue(TEXT("pool fixture is ready"), Fixture.IsReady())) { return false; }
	APlayerController* Controller = Fixture.TestWorld->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("camera controller"), Controller)
		|| !TestNotNull(TEXT("camera manager"), Controller->PlayerCameraManager.Get())) { return false; }

	FMinimalViewInfo View;
	View.Location = FVector(-1000.f, 0.f, 1000.f);
	View.Rotation = FRotator(-45.f, 0.f, 0.f);
	Controller->PlayerCameraManager->SetCameraCachePOV(View);
	ULevel* FirstLevel = Fixture.MakeLevel(1);
	ULevel* SecondLevel = Fixture.MakeLevel(1, FVector(4000.f, 0.f, 0.f));
	Fixture.BroadcastAdded(FirstLevel);
	Fixture.BroadcastAdded(SecondLevel);
	TArray<USpotLightComponent*> Lights;
	Fixture.SpotPool->GetComponents(Lights);
	if (!TestEqual(TEXT("one shared light is available"), Lights.Num(), 1)) { return false; }
	USpotLightComponent* Light = Lights[0];
	TestTrue(TEXT("initial focus selects the first lamp"), Light->IsVisible() && FMath::IsNearlyZero(Light->GetComponentLocation().X));

	View.Location.X = 3000.f;
	Controller->PlayerCameraManager->SetCameraCachePOV(View);
	Fixture.SpotPool->Tick(0.3f);
	TestTrue(TEXT("camera translation selects the second lamp"), FMath::IsNearlyEqual(Light->GetComponentLocation().X, 4000.0));
	View.Location.X = -1000.f;
	Controller->PlayerCameraManager->SetCameraCachePOV(View);
	Fixture.SpotPool->Tick(0.3f);
	TestTrue(TEXT("camera return restores the first lamp"), FMath::IsNearlyZero(Light->GetComponentLocation().X));

	View.Rotation.Pitch = -11.f;
	Controller->PlayerCameraManager->SetCameraCachePOV(View);
	Fixture.SpotPool->Tick(0.3f);
	TestTrue(TEXT("rotation without translation selects the new focus"), FMath::IsNearlyEqual(Light->GetComponentLocation().X, 4000.0));
	Fixture.BroadcastRemoved(SecondLevel);
	TestTrue(TEXT("unloading the selected lamp refreshes a stationary camera"), Light->IsVisible() && FMath::IsNearlyZero(Light->GetComponentLocation().X));
	Fixture.TestWorld->AddLevel(SecondLevel);
	Fixture.BroadcastAdded(SecondLevel);
	TestTrue(TEXT("reloading the selected lamp refreshes a stationary camera"), FMath::IsNearlyEqual(Light->GetComponentLocation().X, 4000.0));
	Fixture.BroadcastRemoved(FirstLevel);
	Fixture.BroadcastRemoved(SecondLevel);
	TestFalse(TEXT("unloading all lamps hides the pooled light"), Light->IsVisible());
	Fixture.TestWorld->AddLevel(FirstLevel);
	Fixture.BroadcastAdded(FirstLevel);
	TestTrue(TEXT("a new streamed lamp recovers the same light"), Light->IsVisible() && FMath::IsNearlyZero(Light->GetComponentLocation().X));

	ATimeCycleManager* Clock = Fixture.TestWorld->SpawnActor<ATimeCycleManager>();
	if (!TestNotNull(TEXT("clock"), Clock)) { return false; }
	Clock->DispatchBeginPlay();
	Clock->SetCurrentTime(FTimeCycleCode{12, 0, 0});
	Fixture.SpotPool->Tick(0.016f);
	TestFalse(TEXT("daylight hides the assigned light"), Light->IsVisible());
	Clock->SetCurrentTime(FTimeCycleCode{22, 0, 0});
	Fixture.SpotPool->Tick(0.016f);
	TestTrue(TEXT("sunset restores a stationary-camera light before the interval elapses"), Light->IsVisible());
	FFloatProperty* DistanceProperty = FindFProperty<FFloatProperty>(AStreetLampSpotPool::StaticClass(), TEXT("MaxAssignDistance"));
	FFloatProperty* DropProperty = FindFProperty<FFloatProperty>(AStreetLampSpotPool::StaticClass(), TEXT("LightDropFromHead"));
	if (!TestNotNull(TEXT("assignment distance property"), DistanceProperty)
		|| !TestNotNull(TEXT("light drop property"), DropProperty)) { return false; }
	DistanceProperty->SetPropertyValue_InContainer(Fixture.SpotPool, 1000.f);
	Fixture.SpotPool->Tick(0.3f);
	TestFalse(TEXT("a smaller assignment radius applies without camera movement"), Light->IsVisible());
	DistanceProperty->SetPropertyValue_InContainer(Fixture.SpotPool, 22000.f);
	const double PreviousLightZ = Light->GetComponentLocation().Z;
	DropProperty->SetPropertyValue_InContainer(Fixture.SpotPool, 100.f);
	Fixture.SpotPool->Tick(0.3f);
	TestTrue(TEXT("restoring the radius and editing drop refresh the stationary view"),
		Light->IsVisible() && FMath::IsNearlyEqual(Light->GetComponentLocation().Z, PreviousLightZ - 60.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlotStreetFurnitureRelocationTest,
	"CGR.City.StreetLampSpotPool.RoadRelocationRestoresAuthoredInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCityPlotStreetFurnitureRelocationTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture;
	if (!TestTrue(TEXT("world is ready"), Fixture.IsReady())) { return false; }
	UStaticMesh* CityLamp = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/City_Pack/Meshes/SM_Street_Lamp.SM_Street_Lamp"));
	if (!TestNotNull(TEXT("authored street lamp mesh"), CityLamp)) { return false; }
	AActor* LampActor = Fixture.TestWorld->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("source actor"), LampActor)) { return false; }
	UInstancedStaticMeshComponent* Lamps = NewObject<UInstancedStaticMeshComponent>(LampActor);
	LampActor->SetRootComponent(Lamps);
	LampActor->AddInstanceComponent(Lamps);
	Lamps->SetStaticMesh(CityLamp);
	Lamps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lamps->RegisterComponent();
	const int32 OriginalIndex = Lamps->AddInstance(FTransform(FVector::ZeroVector));
	FTransform OriginalLocal;
	if (!TestTrue(TEXT("authored transform exists"), Lamps->GetInstanceTransform(OriginalIndex, OriginalLocal, false))) { return false; }

	FCityPlotStreetFurniture Furniture;
	Furniture.Initialize(Fixture.SpotPool);
	FCityStreetSurfaceRegion Road;
	Road.bVehicle = true;
	Road.Bounds = FBox2D(FVector2D(-100.f, -100.f), FVector2D(100.f, 100.f));
	Road.SurfaceZ = CityLamp->GetBoundingBox().Min.Z;
	const TArray<FCityStreetSurfaceRegion> Roads = { Road };
	Furniture.Rebuild(Roads, [](const FBox&, float) { return true; });
	FTransform Moved;
	Lamps->GetInstanceTransform(OriginalIndex, Moved, true);
	TestTrue(TEXT("road moves the authored lamp beyond its base footprint"),
		FVector::Dist2D(Moved.GetLocation(), FVector::ZeroVector) >= 250.f && !Moved.GetScale3D().IsNearlyZero());
	Furniture.Rebuild(Roads, [](const FBox&, float) { return true; });
	FTransform MovedAgain;
	Lamps->GetInstanceTransform(OriginalIndex, MovedAgain, true);
	TestTrue(TEXT("repeated preview never accumulates offsets"), Moved.Equals(MovedAgain));
	Furniture.Rebuild(Roads, [](const FBox&, float) { return false; });
	FTransform Hidden;
	Lamps->GetInstanceTransform(OriginalIndex, Hidden, true);
	TestTrue(TEXT("no safe sidewalk hides the same instance"), Hidden.GetScale3D().IsNearlyZero());
	TestEqual(TEXT("source count remains stable"), Lamps->GetInstanceCount(), 1);
	Furniture.Restore();
	FTransform RestoredLocal;
	Lamps->GetInstanceTransform(OriginalIndex, RestoredLocal, false);
	TestTrue(TEXT("cancel restores exact local transform and index"), RestoredLocal.Equals(OriginalLocal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetLampSpotPoolSourceRefreshTest,
	"CGR.City.StreetLampSpotPool.SourceMoveHideRestoreRefreshesCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStreetLampSpotPoolSourceRefreshTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture(1);
	if (!TestTrue(TEXT("pool is ready"), Fixture.IsReady())) { return false; }
	ULevel* LampLevel = Fixture.MakeLevel(1);
	Fixture.BroadcastAdded(LampLevel);
	UInstancedStaticMeshComponent* Source = nullptr;
	for (AActor* LampActor : LampLevel->Actors)
	{
		if (IsValid(LampActor)) { Source = LampActor->FindComponentByClass<UInstancedStaticMeshComponent>(); }
	}
	if (!TestNotNull(TEXT("source ISM"), Source)) { return false; }
	UInstancedStaticMeshComponent* Cards = Fixture.CardComponent();
	if (!TestNotNull(TEXT("shared cards"), Cards)) { return false; }
	UMaterialInterface* Material = Cards->GetMaterial(0);
	const int32 OriginalLightCount = Fixture.LightCount();
	FTransform OriginalCard;
	Cards->GetInstanceTransform(0, OriginalCard, true);
	const TArray<ULevel*> Changed = { LampLevel };
	Source->UpdateInstanceTransform(0, FTransform(FVector(1000.f, 0.f, 0.f)), true, true, true);
	Fixture.SpotPool->RefreshLampSources(Changed);
	Fixture.FlushRebuild();
	FTransform MovedCard;
	Cards->GetInstanceTransform(0, MovedCard, true);
	TestTrue(TEXT("moved source moves its cached floor card"),
		FMath::IsNearlyEqual(MovedCard.GetLocation().X - OriginalCard.GetLocation().X, 1000.0));
	Source->UpdateInstanceTransform(0, FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
	Fixture.SpotPool->RefreshLampSources(Changed);
	Fixture.FlushRebuild();
	TestEqual(TEXT("zero-scale source has no floor card"), Fixture.CardCount(), 0);
	Source->UpdateInstanceTransform(0, FTransform(FVector::ZeroVector), true, true, true);
	Fixture.SpotPool->RefreshLampSources(Changed);
	Fixture.FlushRebuild();
	FTransform RestoredCard;
	Cards->GetInstanceTransform(0, RestoredCard, true);
	TestTrue(TEXT("restored source recovers its original card"), RestoredCard.Equals(OriginalCard));
	TestTrue(TEXT("card component and material remain shared"),
		Fixture.CardComponent() == Cards && Cards->GetMaterial(0) == Material);
	TestEqual(TEXT("spotlight budget remains fixed"), Fixture.LightCount(), OriginalLightCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlotStreetFurnitureGeneratedRoadLampsTest,
	"CGR.City.StreetLampSpotPool.GeneratedRoadLampsFollowSurfaceAndCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCityPlotStreetFurnitureGeneratedRoadLampsTest::RunTest(const FString& Parameters)
{
	StreetLampSpotPoolTests::FFixture Fixture(2, ERHIFeatureLevel::SM5,
		TEXT("/Game/City_Pack/Meshes/SM_Street_Lamp.SM_Street_Lamp"));
	if (!TestTrue(TEXT("City_Pack lamp pool is ready"), Fixture.IsReady())) { return false; }
	const UStaticMeshSocket* LampSocket = nullptr;
	for (const UStaticMeshSocket* Socket : Fixture.LampMesh->Sockets)
	{
		if (Socket && Socket->SocketName.ToString().StartsWith(TEXT("Lamp"), ESearchCase::IgnoreCase))
		{
			LampSocket = Socket;
			break;
		}
	}
	if (!TestNotNull(TEXT("authored lamp arm socket"), LampSocket)) { return false; }
	AActor* SourceActor = Fixture.TestWorld->SpawnActor<AActor>();
	AActor* RoadOwner = Fixture.TestWorld->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("separate authored lamp actor"), SourceActor)
		|| !TestNotNull(TEXT("road furniture owner"), RoadOwner)) { return false; }
	UInstancedStaticMeshComponent* AuthoredLamps = NewObject<UInstancedStaticMeshComponent>(SourceActor);
	SourceActor->SetRootComponent(AuthoredLamps);
	SourceActor->AddInstanceComponent(AuthoredLamps);
	AuthoredLamps->SetStaticMesh(Fixture.LampMesh);
	AuthoredLamps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AuthoredLamps->RegisterComponent();
	const FVector SourcePosition(-5000.f, -5000.f, -Fixture.LampMesh->GetBoundingBox().Min.Z);
	const int32 SourceIndex = AuthoredLamps->AddInstance(FTransform(SourcePosition));
	FTransform OriginalSource;
	if (!TestTrue(TEXT("authored source transform exists"),
		AuthoredLamps->GetInstanceTransform(SourceIndex, OriginalSource, false))) { return false; }
	const TArray<ULevel*> SourceLevels = { SourceActor->GetLevel() };
	Fixture.SpotPool->RefreshLampSources(SourceLevels);
	Fixture.FlushRebuild();
	TestEqual(TEXT("the authored template has one card"), Fixture.CardCount(), 1);
	UInstancedStaticMeshComponent* SharedCards = Fixture.CardComponent();
	if (!TestNotNull(TEXT("shared card component"), SharedCards)) { return false; }
	UMaterialInterface* SharedMaterial = SharedCards->GetMaterial(0);
	const int32 OriginalLightCount = Fixture.LightCount();

	FCityPlotStreetFurniture Furniture;
	Furniture.Initialize(RoadOwner);
	FCityStreetSurfaceRegion Road;
	Road.bVehicle = true;
	Road.Bounds = FBox2D(FVector2D(0.f, -500.f), FVector2D(12000.f, 500.f));
	Road.SurfaceZ = 0.f;
	const TArray<FCityStreetSurfaceRegion> Roads = { Road };
	auto CanPlace = [](const FBox&, float) { return true; };
	auto GeneratedTransforms = [RoadOwner]()
	{
		TArray<FTransform> Transforms;
		TArray<UInstancedStaticMeshComponent*> Components;
		RoadOwner->GetComponents(Components);
		for (const UInstancedStaticMeshComponent* Component : Components)
		{
			for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
			{
				FTransform WorldTransform;
				if (Component->GetInstanceTransform(Index, WorldTransform, true)) { Transforms.Add(WorldTransform); }
			}
		}
		return Transforms;
	};
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	const TArray<FTransform> InitialGenerated = GeneratedTransforms();
	if (!TestTrue(TEXT("a long internal road creates lamp bodies"), InitialGenerated.Num() > 0)) { return false; }
	bool bNorthSide = false;
	bool bSouthSide = false;
	bool bAllArmsInward = true;
	bool bAllBasesOutsideRoad = true;
	for (int32 Index = 0; Index < InitialGenerated.Num(); ++Index)
	{
		const FTransform& Transform = InitialGenerated[Index];
		const FVector Base = Transform.GetLocation();
		bNorthSide |= Base.Y > 0.f;
		bSouthSide |= Base.Y < 0.f;
		bAllBasesOutsideRoad &= FMath::Abs(Base.Y) >= 650.f;
		const FVector Arm = Transform.TransformVector(LampSocket->RelativeLocation);
		bAllArmsInward &= Base.Y * Arm.Y < 0.f;
		for (int32 Other = 0; Other < Index; ++Other)
		{
			bAllBasesOutsideRoad &= FVector::DistSquared2D(Base,
				InitialGenerated[Other].GetLocation()) >= FMath::Square(1000.f);
		}
	}
	TestTrue(TEXT("generated lamps line both sidewalks"), bNorthSide && bSouthSide);
	TestTrue(TEXT("lamp arms face the carriageway"), bAllArmsInward);
	TestTrue(TEXT("bases clear the road and keep lamp spacing"), bAllBasesOutsideRoad);
	TestEqual(TEXT("each generated body contributes one shared floor card"),
		Fixture.CardCount(), 1 + InitialGenerated.Num());
	TestTrue(TEXT("card ISM and material are reused"),
		Fixture.CardComponent() == SharedCards && SharedCards->GetMaterial(0) == SharedMaterial);
	TestEqual(TEXT("new lamps share the fixed spotlight budget"), Fixture.LightCount(), OriginalLightCount);
	TArray<UInstancedStaticMeshComponent*> InitialComponents;
	RoadOwner->GetComponents(InitialComponents);
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	TArray<UInstancedStaticMeshComponent*> RebuiltComponents;
	RoadOwner->GetComponents(RebuiltComponents);
	const TArray<FTransform> Rebuilt = GeneratedTransforms();
	bool bSameTransforms = Rebuilt.Num() == InitialGenerated.Num();
	if (bSameTransforms)
	{
		for (int32 Index = 0; Index < Rebuilt.Num(); ++Index)
		{
			bSameTransforms &= Rebuilt[Index].Equals(InitialGenerated[Index]);
		}
	}
	TestTrue(TEXT("identical preview reuses component and exact transforms"),
		InitialComponents.Num() == RebuiltComponents.Num()
		&& (InitialComponents.IsEmpty() || InitialComponents[0] == RebuiltComponents[0])
		&& bSameTransforms);
	if (!TestFalse(TEXT("generated component exists before external destruction"),
		RebuiltComponents.IsEmpty())) { return false; }
	UInstancedStaticMeshComponent* RemovedComponent = RebuiltComponents[0];
	RemovedComponent->DestroyComponent();
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	TArray<UInstancedStaticMeshComponent*> RecoveredComponents;
	RoadOwner->GetComponents(RecoveredComponents);
	TestTrue(TEXT("destroyed generated component is replaced and repopulated"),
		RecoveredComponents.Num() == 1 && RecoveredComponents[0] != RemovedComponent
		&& RecoveredComponents[0]->GetInstanceCount() == InitialGenerated.Num());
	TestEqual(TEXT("recovered lamp bodies repopulate the shared cards"),
		Fixture.CardCount(), 1 + InitialGenerated.Num());
	if (!TestEqual(TEXT("one generated component exists before unregister"),
		RecoveredComponents.Num(), 1)) { return false; }
	UInstancedStaticMeshComponent* UnregisteredComponent = RecoveredComponents[0];
	UnregisteredComponent->UnregisterComponent();
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	TArray<UInstancedStaticMeshComponent*> ReplacedComponents;
	RoadOwner->GetComponents(ReplacedComponents);
	TestTrue(TEXT("unregistered generated component is removed before a single replacement is kept"),
		ReplacedComponents.Num() == 1 && ReplacedComponents[0] != UnregisteredComponent
		&& ReplacedComponents[0]->IsRegistered()
		&& ReplacedComponents[0]->GetInstanceCount() == InitialGenerated.Num());
	TestEqual(TEXT("unregistered replacement recovers the same floor cards"),
		Fixture.CardCount(), 1 + InitialGenerated.Num());
	if (!TestEqual(TEXT("one replacement exists before restore"), ReplacedComponents.Num(), 1)) { return false; }
	ReplacedComponents[0]->UnregisterComponent();
	Furniture.Restore();
	Fixture.FlushRebuild();
	TestEqual(TEXT("restore removes unregistered generated components"), GeneratedTransforms().Num(), 0);
	TestEqual(TEXT("restore clears cached cards of unregistered bodies"), Fixture.CardCount(), 1);
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	Furniture.Rebuild(Roads, [](const FBox&, float) { return false; });
	Fixture.FlushRebuild();
	TestEqual(TEXT("rejected ground removes generated bodies"), GeneratedTransforms().Num(), 0);
	TestEqual(TEXT("rejected ground removes generated cards"), Fixture.CardCount(), 1);
	FCityStreetSurfaceRegion RaisedRoad = Road;
	RaisedRoad.SurfaceZ = 250.f;
	const TArray<FCityStreetSurfaceRegion> RaisedRoads = { RaisedRoad };
	Furniture.Rebuild(RaisedRoads, CanPlace);
	Fixture.FlushRebuild();
	TestEqual(TEXT("road height beyond the authored ground tolerance creates no lamps"),
		GeneratedTransforms().Num(), 0);
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	TestEqual(TEXT("valid ground regenerates the same number of lamps"),
		GeneratedTransforms().Num(), InitialGenerated.Num());
	Furniture.Rebuild(TArray<FCityStreetSurfaceRegion>(), CanPlace);
	Fixture.FlushRebuild();
	TestEqual(TEXT("removing the road removes its generated lamp bodies"), GeneratedTransforms().Num(), 0);
	TestEqual(TEXT("removing the road restores the authored-only card count"), Fixture.CardCount(), 1);
	AActor* NeighborActor = Fixture.TestWorld->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("nearby authored lamp actor"), NeighborActor)) { return false; }
	UInstancedStaticMeshComponent* NeighborLamp = NewObject<UInstancedStaticMeshComponent>(NeighborActor);
	NeighborActor->SetRootComponent(NeighborLamp);
	NeighborActor->AddInstanceComponent(NeighborLamp);
	NeighborLamp->SetStaticMesh(Fixture.LampMesh);
	NeighborLamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NeighborLamp->RegisterComponent();
	const FVector NeighborBase = InitialGenerated[0].GetLocation();
	NeighborLamp->AddInstance(FTransform(NeighborBase));
	FCityStreetSurfaceRegion Crossing;
	Crossing.bVehicle = true;
	Crossing.bJunction = true;
	Crossing.SurfaceZ = 0.f;
	bool bCrossingCandidateFound = false;
	for (const FTransform& Transform : InitialGenerated)
	{
		const double X = Transform.GetLocation().X;
		if (FMath::Abs(X - NeighborBase.X) < 2000.0 || X > 10000.0) { continue; }
		Crossing.Bounds = FBox2D(FVector2D(X - 500.0, -1500.0),
			FVector2D(X + 500.0, 1500.0));
		bCrossingCandidateFound = true;
		break;
	}
	if (!TestTrue(TEXT("straight road provides a central junction fixture point"), bCrossingCandidateFound))
	{
		Furniture.Restore();
		return false;
	}
	Furniture.InvalidateSources();
	const TArray<FCityStreetSurfaceRegion> CrossingRoads = { Road, Crossing };
	Furniture.Rebuild(CrossingRoads, CanPlace);
	Fixture.FlushRebuild();
	bool bClearOfNeighborAndCrossing = true;
	for (const FTransform& Transform : GeneratedTransforms())
	{
		const FVector Base = Transform.GetLocation();
		bClearOfNeighborAndCrossing &= FVector::DistSquared2D(Base, NeighborBase) >= FMath::Square(1000.f);
		const FBox2D BaseBounds(FVector2D(Base.X - 70.0, Base.Y - 70.0),
			FVector2D(Base.X + 70.0, Base.Y + 70.0));
		bClearOfNeighborAndCrossing &= !Crossing.Bounds.Intersect(BaseBounds);
	}
	TestTrue(TEXT("authored neighbor spacing and crossing road exclude generated bases"),
		bClearOfNeighborAndCrossing);
	TestEqual(TEXT("crossing layout adds one card per generated body beside two authored lamps"),
		Fixture.CardCount(), 2 + GeneratedTransforms().Num());
	NeighborActor->Destroy();
	Furniture.InvalidateSources();
	Fixture.SpotPool->RefreshLampSources(SourceLevels);
	Fixture.FlushRebuild();
	Furniture.Rebuild(Roads, CanPlace);
	Fixture.FlushRebuild();
	TestTrue(TEXT("road lamps exist immediately before restore"), GeneratedTransforms().Num() > 0);
	Furniture.Restore();
	Fixture.FlushRebuild();
	TestEqual(TEXT("restore removes active generated lamp bodies"), GeneratedTransforms().Num(), 0);
	TestEqual(TEXT("restore removes active generated lamp cards"), Fixture.CardCount(), 1);
	FTransform RestoredSource;
	AuthoredLamps->GetInstanceTransform(SourceIndex, RestoredSource, false);
	TestTrue(TEXT("restore keeps the authored transform and index"), RestoredSource.Equals(OriginalSource));
	TestEqual(TEXT("restore keeps exactly the authored source instance"), AuthoredLamps->GetInstanceCount(), 1);
	TestEqual(TEXT("restore keeps the original spotlight budget"), Fixture.LightCount(), OriginalLightCount);
	return true;
}
#endif
