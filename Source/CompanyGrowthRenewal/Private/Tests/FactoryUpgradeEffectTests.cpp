#include "Entity/Factory/FactoryUpgradeEffectComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FactoryUpgradeEffectTests
{
	// Mirrors the component layout: 12 floor beams, 16 reveal shards, 8 outline blades, then 2 sparks per panel.
	constexpr int32 FixedAccents = 36;
	constexpr int32 FirstShard = 12;
	constexpr int32 FirstOutline = 28;
	constexpr int32 SparksPerPanel = 2;

	int32 ExpectedAccents(int32 PanelCount)
	{
		return FixedAccents + PanelCount * SparksPerPanel;
	}

	struct FFixture
	{
		UWorld* TestWorld = nullptr;
		AActor* Actor = nullptr;
		USceneComponent* Root = nullptr;
		UFactoryUpgradeEffectComponent* Effect = nullptr;
		UStaticMesh* Cube = nullptr;

		FFixture()
		{
			TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
			if (!TestWorld) return;
			if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
			TestWorld->InitializeActorsForPlay(FURL());
			Actor = TestWorld->SpawnActor<AActor>();
			Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
			if (!Actor || !Cube) return;
			Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Actor->AddInstanceComponent(Root);
			Root->RegisterComponent();
			Effect = NewObject<UFactoryUpgradeEffectComponent>(Actor);
			Actor->AddInstanceComponent(Effect);
			Effect->PanelMesh = Cube;
			Effect->PanelMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			Effect->AccentMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			Effect->RegisterComponent();
		}

		~FFixture()
		{
			if (!TestWorld) return;
			if (GEngine) GEngine->DestroyWorldContext(TestWorld);
			TestWorld->DestroyWorld(false);
		}

		bool IsReady() const { return Actor && Root && Effect && Cube &&
			!Effect->PanelMaterial.IsNull() && !Effect->AccentMaterial.IsNull(); }

		UInstancedStaticMeshComponent* FindInstances(const TCHAR* ComponentName) const
		{
			if (!Actor) return nullptr;
			TArray<UInstancedStaticMeshComponent*> Components;
			Actor->GetComponents<UInstancedStaticMeshComponent>(Components);
			for (UInstancedStaticMeshComponent* Component : Components)
			{
				if (Component && Component->GetFName() == FName(ComponentName)) return Component;
			}
			return nullptr;
		}

		UInstancedStaticMeshComponent* Instances() const { return FindInstances(TEXT("FactoryUpgradePanelInstances")); }
		UInstancedStaticMeshComponent* Accents() const { return FindInstances(TEXT("FactoryUpgradeAccentInstances")); }

		void Tick(float DeltaTime)
		{
			UActorComponent* BaseComponent = Effect;
			BaseComponent->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryUpgradeEffectLifecycleTest,
	"CGR.Factory.UpgradeEffect.Lifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryUpgradeEffectLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace FactoryUpgradeEffectTests;
	FFixture Fixture;
	if (!TestTrue(TEXT("world, actor, mesh and material ready"), Fixture.IsReady())) return false;

	const FBox Bounds(FVector(-1000.f, -800.f, 0.f), FVector(1000.f, 800.f, 750.f));
	FString Events;
	int32 CountAtConceal = 0;
	bool bAllPanelsClosed = true;
	TArray<FTransform> ClosedTransforms;
	Fixture.Effect->OnConcealed.AddLambda([&]()
	{
		Events += TEXT("C");
		UInstancedStaticMeshComponent* ISM = Fixture.Instances();
		CountAtConceal = ISM ? ISM->GetInstanceCount() : 0;
		for (int32 Index = 0; ISM && Index < CountAtConceal; ++Index)
		{
			FTransform LocalTransform;
			if (!ISM->GetInstanceTransform(Index, LocalTransform, false) ||
				LocalTransform.GetScale3D().GetMin() <= 0.f || LocalTransform.ContainsNaN())
			{
				bAllPanelsClosed = false;
			}
			ClosedTransforms.Add(LocalTransform);
		}
	});
	Fixture.Effect->OnRevealed.AddLambda([&]() { Events += TEXT("R"); });
	Fixture.Effect->OnFinished.AddLambda([&]() { Events += TEXT("F"); });

	TestTrue(TEXT("configured effect starts"), Fixture.Effect->Play(Fixture.Root, Bounds));
	UInstancedStaticMeshComponent* ISM = Fixture.Instances();
	if (!TestNotNull(TEXT("one ISM is created"), ISM)) return false;
	UInstancedStaticMeshComponent* AccentISM = Fixture.Accents();
	if (!TestNotNull(TEXT("optional accent ISM is created"), AccentISM)) return false;
	const int32 InitialCount = ISM->GetInstanceCount();
	TestTrue(TEXT("bounded shell includes side and roof panels"), InitialCount >= 57 && InitialCount <= 97);
	const int32 AccentCount = ExpectedAccents(InitialCount);
	TestEqual(TEXT("accents hold fixed effects plus two sparks per panel"), AccentISM->GetInstanceCount(), AccentCount);
	TestTrue(TEXT("second Play coalesces"), Fixture.Effect->Play(Fixture.Root, FBox(FVector(-10.f), FVector(10.f))));
	TestEqual(TEXT("coalesced Play keeps the same instances"), ISM->GetInstanceCount(), InitialCount);
	TestEqual(TEXT("coalesced Play keeps the accents"), AccentISM->GetInstanceCount(), AccentCount);

	Fixture.Root->SetWorldTransform(FTransform(FRotator(0.f, 47.f, 0.f),
		FVector(250.f, -400.f, 30.f), FVector(1.2f, 1.2f, 1.2f)));
	TestTrue(TEXT("shell follows moved and rotated attachment"),
		ISM->GetComponentTransform().Equals(Fixture.Root->GetComponentTransform(), 0.01f));
	TestTrue(TEXT("accents follow moved and rotated attachment"),
		AccentISM->GetComponentTransform().Equals(Fixture.Root->GetComponentTransform(), 0.01f));

	Fixture.Tick(Fixture.Effect->Duration * 0.45f);
	TestEqual(TEXT("conceal is first event"), Events, FString(TEXT("C")));
	TestEqual(TEXT("all panels exist before appearance swap"), CountAtConceal, InitialCount);
	TestTrue(TEXT("closed shell has nonzero finite transforms"), bAllPanelsClosed);
	struct FClosedFace
	{
		FVector Normal;
		FVector2D Min;
		FVector2D Max;
		double Depth = 0.0;
	};
	TArray<FClosedFace> ClosedFaces;
	const FBox PanelMeshBounds = Fixture.Cube->GetBoundingBox();
	for (const FTransform& PanelTransform : ClosedTransforms)
	{
		FClosedFace& Face = ClosedFaces.AddDefaulted_GetRef();
		Face.Normal = PanelTransform.GetRotation().GetAxisZ();
		const auto Project = [&Face](const FVector& Point)
		{
			return FMath::Abs(Face.Normal.X) > 0.5f ? FVector2D(Point.Y, Point.Z) :
				FMath::Abs(Face.Normal.Y) > 0.5f ? FVector2D(Point.X, Point.Z) : FVector2D(Point.X, Point.Y);
		};
		bool bFirstCorner = true;
		for (double MeshX : { PanelMeshBounds.Min.X, PanelMeshBounds.Max.X })
		{
			for (double MeshY : { PanelMeshBounds.Min.Y, PanelMeshBounds.Max.Y })
			{
				const FVector FacePoint = PanelTransform.TransformPosition(FVector(MeshX, MeshY, PanelMeshBounds.Max.Z));
				const FVector2D Projected = Project(FacePoint);
				if (bFirstCorner)
				{
					Face.Min = Projected;
					Face.Max = Projected;
					Face.Depth = FVector::DotProduct(FacePoint, Face.Normal);
					bFirstCorner = false;
				}
				else
				{
					Face.Min.X = FMath::Min(Face.Min.X, Projected.X);
					Face.Min.Y = FMath::Min(Face.Min.Y, Projected.Y);
					Face.Max.X = FMath::Max(Face.Max.X, Projected.X);
					Face.Max.Y = FMath::Max(Face.Max.Y, Projected.Y);
				}
			}
		}
	}
	int32 OverlappingFacePairs = 0;
	bool bOverlappingFacesHaveDistinctDepth = true;
	for (int32 First = 0; First < ClosedFaces.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < ClosedFaces.Num(); ++Second)
		{
			const FClosedFace& A = ClosedFaces[First];
			const FClosedFace& B = ClosedFaces[Second];
			if (FVector::DotProduct(A.Normal, B.Normal) < 0.99f ||
				FMath::Min(A.Max.X, B.Max.X) - FMath::Max(A.Min.X, B.Min.X) <= 0.01f ||
				FMath::Min(A.Max.Y, B.Max.Y) - FMath::Max(A.Min.Y, B.Min.Y) <= 0.01f)
			{
			continue;
			}
			++OverlappingFacePairs;
			bOverlappingFacesHaveDistinctDepth &= FMath::Abs(A.Depth - B.Depth) > 0.1f;
		}
	}
	TestTrue(TEXT("closed panels overlap to conceal seams"), OverlappingFacePairs > 0);
	TestTrue(TEXT("overlapping closed faces have distinct plane depths"), bOverlappingFacesHaveDistinctDepth);
	TestEqual(TEXT("panel shader receives one custom float per instance"), ISM->PerInstanceSMCustomData.Num(), InitialCount);
	bool bEnergyFinite = true;
	for (float Energy : ISM->PerInstanceSMCustomData)
	{
		bEnergyFinite &= FMath::IsFinite(Energy) && Energy >= 0.f && Energy <= 1.5f;
	}
	TestTrue(TEXT("panel energy remains finite and bounded"), bEnergyFinite);
	TestEqual(TEXT("accent shader receives energy and shape per instance"), AccentISM->PerInstanceSMCustomData.Num(), AccentCount * 2);
	bool bAccentShapesMatchLayout = AccentISM->PerInstanceSMCustomData.Num() == AccentCount * 2;
	bool bAccentEnergyFinite = bAccentShapesMatchLayout;
	bool bOutlineFlashesAtConceal = bAccentShapesMatchLayout;
	for (int32 Index = 0; bAccentShapesMatchLayout && Index < AccentCount; ++Index)
	{
		const float Energy = AccentISM->PerInstanceSMCustomData[Index * 2];
		const float Kind = AccentISM->PerInstanceSMCustomData[Index * 2 + 1];
		bAccentShapesMatchLayout &= Kind == (Index < FirstShard ? 0.f : Index < FirstOutline ? 1.f : 2.f);
		bAccentEnergyFinite &= FMath::IsFinite(Energy) && Energy >= 0.f && Energy <= 2.5f;
		if (Index >= FirstOutline && Index < FixedAccents)
		{
			bOutlineFlashesAtConceal &= Energy > 1.f;
		}
	}
	TestTrue(TEXT("accent kinds follow strip, shard and blade ranges"), bAccentShapesMatchLayout);
	TestTrue(TEXT("accent energy remains finite and bounded"), bAccentEnergyFinite);
	TestTrue(TEXT("closed shell flashes its corner and roof outline in overdrive"), bOutlineFlashesAtConceal);
	FTransform HorizontalBeam, VerticalBeam;
	TestTrue(TEXT("floor pulse transforms exist"), AccentISM->GetInstanceTransform(0, HorizontalBeam, false) &&
		AccentISM->GetInstanceTransform(2, VerticalBeam, false));
	const FBox CubeBounds = Fixture.Cube->GetBoundingBox();
	TestTrue(TEXT("floor pulse remains above the building base"),
		HorizontalBeam.TransformPosition(FVector(0.f, 0.f, CubeBounds.Min.Z)).Z > Bounds.Min.Z);
	const FVector BeamEnd(CubeBounds.Max.X, 0.f, 0.f);
	TestTrue(TEXT("fading floor pulse keeps adjacent beam ends joined"),
		HorizontalBeam.TransformPosition(BeamEnd).Equals(VerticalBeam.TransformPosition(BeamEnd), 0.01f));
	TestTrue(TEXT("effect remains active during hold"), Fixture.Effect->IsPlaying());
	Fixture.Tick(Fixture.Effect->Duration * 0.07f);
	bool bClosedThroughHold = ClosedTransforms.Num() == InitialCount;
	for (int32 Index = 0; bClosedThroughHold && Index < InitialCount; ++Index)
	{
		FTransform HeldTransform;
		bClosedThroughHold = ISM->GetInstanceTransform(Index, HeldTransform, false) &&
			HeldTransform.Equals(ClosedTransforms[Index], 0.01f);
	}
	TestTrue(TEXT("all panels reached the complete shell before conceal"), bClosedThroughHold);
	Fixture.Tick(Fixture.Effect->Duration * 0.14f);
	bool bReadableRoofPanelInFlight = false;
	for (int32 Index = 0; Index < ClosedTransforms.Num(); ++Index)
	{
		const FTransform& Closed = ClosedTransforms[Index];
		if (Closed.GetRotation().GetAxisZ().Z < 0.9f) continue;
		FTransform Flying;
		if (!ISM->GetInstanceTransform(Index, Flying, false)) continue;
		const FVector ClosedScale = Closed.GetScale3D();
		const FVector FlyingScale = Flying.GetScale3D();
		const double RoofEdge = FMath::Max(ClosedScale.X * CubeBounds.GetSize().X,
			ClosedScale.Y * CubeBounds.GetSize().Y);
		bReadableRoofPanelInFlight |= FVector::Distance(Closed.GetLocation(), Flying.GetLocation()) >= RoofEdge * 0.35f &&
			FlyingScale.X >= ClosedScale.X * 0.65f && FlyingScale.Y >= ClosedScale.Y * 0.65f;
	}
	TestTrue(TEXT("roof panel travels while still readable during reveal"), bReadableRoofPanelInFlight);

	Fixture.Tick(10.f);
	TestEqual(TEXT("large delta delivers ordered events exactly once"), Events, FString(TEXT("CRF")));
	TestFalse(TEXT("effect stops after finish"), Fixture.Effect->IsPlaying());
	TestFalse(TEXT("effect tick is disabled"), Fixture.Effect->IsComponentTickEnabled());
	TestEqual(TEXT("instances are cleared at finish"), ISM->GetInstanceCount(), 0);
	TestEqual(TEXT("accents are cleared at finish"), AccentISM->GetInstanceCount(), 0);
	TestFalse(TEXT("ISM is hidden at finish"), ISM->IsVisible());
	TestFalse(TEXT("accent ISM is hidden at finish"), AccentISM->IsVisible());
	Fixture.Tick(10.f);
	TestEqual(TEXT("post-finish tick emits no events"), Events, FString(TEXT("CRF")));

	TestTrue(TEXT("same component can replay"), Fixture.Effect->Play(Fixture.Root, Bounds));
	TestEqual(TEXT("replay reuses the ISM"), Fixture.Instances(), ISM);
	TestEqual(TEXT("replay reuses the accent ISM"), Fixture.Accents(), AccentISM);
	Fixture.Tick(10.f);
	TestEqual(TEXT("one frame can cross all three events once"), Events, FString(TEXT("CRFCRF")));
	TestEqual(TEXT("large-delta replay also cleans instances"), ISM->GetInstanceCount(), 0);
	TestEqual(TEXT("large-delta replay also cleans accents"), AccentISM->GetInstanceCount(), 0);

	TestTrue(TEXT("component starts after a completed replay"), Fixture.Effect->Play(Fixture.Root, Bounds));
	Fixture.Effect->Cancel();
	Fixture.Tick(10.f);
	TestEqual(TEXT("cancel emits no new event"), Events, FString(TEXT("CRFCRF")));
	TestEqual(TEXT("cancel clears instances"), ISM->GetInstanceCount(), 0);
	TestEqual(TEXT("cancel clears accents"), AccentISM->GetInstanceCount(), 0);
	TestFalse(TEXT("cancel disables tick"), Fixture.Effect->IsComponentTickEnabled());

	UFactoryUpgradeEffectComponent* MissingResources = NewObject<UFactoryUpgradeEffectComponent>(Fixture.Actor);
	Fixture.Actor->AddInstanceComponent(MissingResources);
	MissingResources->RegisterComponent();
	TestFalse(TEXT("missing resources cannot start"), MissingResources->Play(Fixture.Root, Bounds));
	TestFalse(TEXT("missing resources do not leave active playback"), MissingResources->IsPlaying());

	AActor* PanelOnlyActor = Fixture.TestWorld->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("optional-material actor exists"), PanelOnlyActor)) return false;
	USceneComponent* PanelOnlyRoot = NewObject<USceneComponent>(PanelOnlyActor);
	PanelOnlyActor->SetRootComponent(PanelOnlyRoot);
	PanelOnlyActor->AddInstanceComponent(PanelOnlyRoot);
	PanelOnlyRoot->RegisterComponent();
	UFactoryUpgradeEffectComponent* PanelOnly = NewObject<UFactoryUpgradeEffectComponent>(PanelOnlyActor);
	PanelOnlyActor->AddInstanceComponent(PanelOnly);
	PanelOnly->PanelMesh = Fixture.Cube;
	PanelOnly->PanelMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	PanelOnly->RegisterComponent();
	TestTrue(TEXT("missing optional accent still plays panels"), PanelOnly->Play(PanelOnlyRoot, Bounds));
	TArray<UInstancedStaticMeshComponent*> PanelOnlyComponents;
	PanelOnlyActor->GetComponents<UInstancedStaticMeshComponent>(PanelOnlyComponents);
	TestEqual(TEXT("missing optional accent creates only the panel ISM"), PanelOnlyComponents.Num(), 1);
	PanelOnly->Cancel();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryUpgradeEffectSparkTimelineTest,
	"CGR.Factory.UpgradeEffect.SparkTimeline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryUpgradeEffectSparkTimelineTest::RunTest(const FString& Parameters)
{
	using namespace FactoryUpgradeEffectTests;
	FFixture Fixture;
	if (!TestTrue(TEXT("world, actor, mesh and material ready"), Fixture.IsReady())) return false;

	const FBox Bounds(FVector(-1000.f, -800.f, 0.f), FVector(1000.f, 800.f, 750.f));
	if (!TestTrue(TEXT("effect starts"), Fixture.Effect->Play(Fixture.Root, Bounds))) return false;
	UInstancedStaticMeshComponent* ISM = Fixture.Instances();
	UInstancedStaticMeshComponent* AccentISM = Fixture.Accents();
	if (!TestNotNull(TEXT("panel ISM exists"), ISM) || !TestNotNull(TEXT("accent ISM exists"), AccentISM)) return false;
	const int32 PanelCount = ISM->GetInstanceCount();
	const int32 AccentCount = ExpectedAccents(PanelCount);
	if (!TestEqual(TEXT("accent layout matches panel count"), AccentISM->GetInstanceCount(), AccentCount)) return false;

	TArray<float> AssemblyPeak;
	TArray<float> PeelPeak;
	AssemblyPeak.Init(0.f, AccentCount);
	PeelPeak.Init(0.f, AccentCount);
	const FBox Reach = Bounds.ExpandBy(1200.f);
	bool bLitAccentsStayNearShell = true;
	bool bEnergyBounded = true;
	const float Duration = FMath::Clamp(Fixture.Effect->Duration, 1.5f, 2.5f);
	constexpr float Step = 1.f / 120.f;
	float Elapsed = 0.f;
	while (Fixture.Effect->IsPlaying() && Elapsed < Duration + 1.f)
	{
		Fixture.Tick(Step);
		Elapsed += Step;
		if (!Fixture.Effect->IsPlaying() || AccentISM->GetInstanceCount() != AccentCount) break;
		const float Normalized = Elapsed / Duration;
		for (int32 Index = 0; Index < AccentCount; ++Index)
		{
			const float Energy = AccentISM->PerInstanceSMCustomData[Index * 2];
			bEnergyBounded &= FMath::IsFinite(Energy) && Energy >= 0.f && Energy <= 2.5f;
			if (Energy <= 0.01f) continue;
			FTransform Pose;
			bLitAccentsStayNearShell &= AccentISM->GetInstanceTransform(Index, Pose, false) &&
				!Pose.ContainsNaN() && Reach.IsInside(Pose.GetLocation());
			TArray<float>& Peak = Normalized < 0.5f ? AssemblyPeak : PeelPeak;
			Peak[Index] = FMath::Max(Peak[Index], Energy);
		}
	}
	TestFalse(TEXT("timeline finishes"), Fixture.Effect->IsPlaying());
	TestTrue(TEXT("accent energy stays finite and bounded every frame"), bEnergyBounded);
	TestTrue(TEXT("lit accents stay near the shell"), bLitAccentsStayNearShell);

	bool bEverySnapSparks = true;
	bool bEveryPanelLeavesStreaks = true;
	for (int32 Index = FixedAccents; Index < AccentCount; ++Index)
	{
		bEverySnapSparks &= AssemblyPeak[Index] > 1.f;
		bEveryPanelLeavesStreaks &= PeelPeak[Index] > 0.5f;
	}
	TestTrue(TEXT("every panel throws two overdriven sparks as it locks"), bEverySnapSparks);
	TestTrue(TEXT("every ejected panel leaves two afterglow streaks"), bEveryPanelLeavesStreaks);
	bool bOutlineFlashes = true;
	for (int32 Index = FirstOutline; Index < FixedAccents; ++Index)
	{
		bOutlineFlashes &= AssemblyPeak[Index] > 1.f;
	}
	TestTrue(TEXT("outline flashes around the closed shell"), bOutlineFlashes);
	bool bRevealWaveBursts = true;
	for (int32 Index = 8; Index < FirstShard; ++Index)
	{
		bRevealWaveBursts &= PeelPeak[Index] > 1.f;
	}
	TestTrue(TEXT("reveal floor wave bursts in overdrive"), bRevealWaveBursts);
	TestEqual(TEXT("accents are cleared at finish"), AccentISM->GetInstanceCount(), 0);
	return true;
}

#endif
