#include "Entity/Factory/BrickLaborerPath.h"
#include "Entity/Factory/BrickFactory.h"
#include "Entity/Factory/BrickLaborer.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Table/FactoryAppearanceTable.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FactoryDoorTests
{
	UStaticMesh* LoadStageMesh(UDataTable* AppearanceTable, int32 Stage)
	{
		if (!AppearanceTable) return nullptr;
		const FName RowName(*FString::Printf(TEXT("Level_%02d"), Stage));
		const FFactoryAppearanceRow* Appearance = AppearanceTable->FindRow<FFactoryAppearanceRow>(
			RowName, TEXT("Factory door automation"));
		return Appearance && Appearance->Level == Stage ? Appearance->Mesh.LoadSynchronous() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRLaborerDoorPathTest,
	"CGR.Factory.LaborerDoor.AlignedPath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRLaborerDoorPathTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world exists"), TestWorld)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ABrickFactory* Factory = TestWorld->SpawnActor<ABrickFactory>();
	UStaticMeshComponent* Body = Factory ? Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent()) : nullptr;
	UDataTable* AppearanceTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/Factory/DT_FactoryAppearance"));
	UStaticMesh* FinalStage = FactoryDoorTests::LoadStageMesh(AppearanceTable, 8);
	FStructProperty* OffsetProperty = FindFProperty<FStructProperty>(
		ABrickFactory::StaticClass(), TEXT("PlacementFootprintOffset"));
	if (!TestNotNull(TEXT("Factory exists"), Factory)
		|| !TestNotNull(TEXT("Factory body exists"), Body)
		|| !TestNotNull(TEXT("Factory appearance table exists"), AppearanceTable)
		|| !TestNotNull(TEXT("Final stage plot reference exists"), FinalStage)
		|| !TestNotNull(TEXT("Footprint offset exists"), OffsetProperty))
	{
		if (GEngine) GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
		return false;
	}
	FVector2D* FootprintOffset = OffsetProperty->ContainerPtrToValuePtr<FVector2D>(Factory);
	// MainMap_City의 공장 모서리 고정 관목 경계를 부지 로컬 좌표로 재현한다.
	const FBox2D CornerHedge(FVector2D(-2050.f, -1809.f), FVector2D(-1262.f, -1649.f));
	*FootprintOffset = FVector2D(FinalStage->GetBounds().Origin.X, FinalStage->GetBounds().Origin.Y);
	Factory->OnConstruction(Factory->GetActorTransform());
	TestTrue(TEXT("Door paths use the scaled factory appearance"),
		Body->GetRelativeScale3D().Equals(FVector(0.9f), 0.001));

	for (int32 Stage = 1; Stage <= 8; ++Stage)
	{
		UStaticMesh* StageMesh = FactoryDoorTests::LoadStageMesh(AppearanceTable, Stage);
		if (!TestNotNull(TEXT("Saved stage mesh exists"), StageMesh)) continue;
		Body->SetStaticMesh(StageMesh);
		for (const float Yaw : { 0.f, 47.f, 180.f })
		{
			Factory->SetActorLocationAndRotation(FVector(3000.f, 6000.f, 25.f), FRotator(0.f, Yaw, 0.f));
			const FTransform Footprint = Factory->GetPlacementFootprintTransform();
			const FVector2D HalfExtent = Factory->GetPlacementFootprintHalfExtent(false);
			FBox2D BodyBounds(ForceInit);
			const FBox MeshBounds = StageMesh->GetBoundingBox();
			for (const float X : { static_cast<float>(MeshBounds.Min.X), static_cast<float>(MeshBounds.Max.X) })
			{
				for (const float Y : { static_cast<float>(MeshBounds.Min.Y), static_cast<float>(MeshBounds.Max.Y) })
				{
					const FVector Local = Footprint.InverseTransformPositionNoScale(
						Body->GetComponentTransform().TransformPosition(FVector(X, Y, 0.f)));
					BodyBounds += FVector2D(Local.X, Local.Y);
				}
			}
			const int32 ExpectedDoorCount = Stage <= 2 ? 2 : 4;
			TestEqual(TEXT("Current stage exposes its authored laborer sockets"),
				Factory->GetLaborerDoorCount(), ExpectedDoorCount);
			for (int32 DoorIndex = 0; DoorIndex < Factory->GetLaborerDoorCount(); ++DoorIndex)
			{
				const FName SocketName(*FString::Printf(TEXT("LaborerDoor_%d"), DoorIndex));
				TestTrue(TEXT("Factory resolves each socket from its current mesh"),
					Factory->GetLaborerDoorTransform(DoorIndex).Equals(Body->GetSocketTransform(SocketName), 0.01));
			}
			const FVector FrontNormal = Factory->GetLaborerDoorTransform(0).GetUnitAxis(EAxis::X);
			if (Stage <= 2)
			{
				TestTrue(TEXT("Starter front and side doors face into different walls"),
					FMath::Abs(FVector::DotProduct(FrontNormal,
						Factory->GetLaborerDoorTransform(1).GetUnitAxis(EAxis::X))) < 0.001);
			}
			else
			{
				TestTrue(TEXT("Both original doors keep their inward front normal"),
					FrontNormal.Equals(Factory->GetLaborerDoorTransform(1).GetUnitAxis(EAxis::X), 0.001));
				TestTrue(TEXT("Workshop side doors face into opposite walls"),
					FMath::Abs(FVector::DotProduct(FrontNormal,
						Factory->GetLaborerDoorTransform(2).GetUnitAxis(EAxis::X))) < 0.001
					&& Factory->GetLaborerDoorTransform(2).GetUnitAxis(EAxis::X).Equals(
						-Factory->GetLaborerDoorTransform(3).GetUnitAxis(EAxis::X), 0.001));
			}
			for (int32 FromIndex = 0; FromIndex < Factory->GetLaborerDoorCount(); ++FromIndex)
			{
				for (int32 ToIndex = 0; ToIndex < Factory->GetLaborerDoorCount(); ++ToIndex)
				{
					if (FromIndex == ToIndex) continue;
					const FTransform FromDoor = Factory->GetLaborerDoorTransform(FromIndex);
					const FTransform ToDoor = Factory->GetLaborerDoorTransform(ToIndex);
					for (int32 Lane = 0; Lane < 5; ++Lane)
					{
						FBrickLaborerPath PairPath;
						PairPath.BuildBounded(FromDoor, ToDoor, 140.f, 220.f, Lane,
							Footprint, HalfExtent, 130.f, BodyBounds);
						TestTrue(TEXT("Every route family starts and ends inside the requested sockets"),
							PairPath.InsideFrom.Equals(FromDoor.GetLocation() + FromDoor.GetUnitAxis(EAxis::X) * 140.f, 0.01)
							&& PairPath.InsideTo.Equals(ToDoor.GetLocation() + ToDoor.GetUnitAxis(EAxis::X) * 140.f, 0.01));
						TestTrue(TEXT("Exit and entrance point along their own doorway normals"),
							PairPath.DirectionAtDistance(140.f).Equals(-FromDoor.GetUnitAxis(EAxis::X), 0.001)
							&& PairPath.DirectionAtDistance(PairPath.Length() - 140.f).Equals(
								ToDoor.GetUnitAxis(EAxis::X), 0.001));
						for (int32 Sample = 0; Sample <= 128; ++Sample)
						{
							const float Distance = PairPath.Length() * Sample / 128.f;
							const FVector Local = Footprint.InverseTransformPositionNoScale(
								PairPath.PositionAtDistance(Distance));
							TestTrue(TEXT("Every rotated door pair and route family stays inside the worker-safe plot"),
								FMath::Abs(Local.X) <= HalfExtent.X - 130.f + 0.02f
								&& FMath::Abs(Local.Y) <= HalfExtent.Y - 130.f + 0.02f);
							if (Stage == 8)
							{
								TestFalse(TEXT("Final-stage door pairs leave worker clearance before the authored corner hedge"),
									CornerHedge.ExpandBy(130.f).IsInside(FVector2D(Local.X, Local.Y)));
							}
							if (Distance > PairPath.ExitLength + 0.01f
								&& Distance < PairPath.ExitLength + PairPath.CurveLength - 0.01f)
							{
								TestTrue(TEXT("Every exterior route clears the expanded body"),
									Local.X <= BodyBounds.Min.X - 130.f + 1.f
									|| Local.X >= BodyBounds.Max.X + 130.f - 1.f
									|| Local.Y <= BodyBounds.Min.Y - 130.f + 1.f
									|| Local.Y >= BodyBounds.Max.Y + 130.f - 1.f);
							}
						}
					}
				}
			}
			const FTransform DoorA = Factory->GetLaborerDoorTransform(0);
			const FTransform DoorB = Factory->GetLaborerDoorTransform(1);
			for (const int32 Crew : { 5, 100 })
			{
				TArray<FBrickLaborerPath> Paths;
				Paths.SetNum(Stage == 8 && Crew == 100 ? 100 : 5);
				for (int32 Lane = 0; Lane < Paths.Num(); ++Lane)
				{
					FBrickLaborerPath& Outgoing = Paths[Lane];
					FBrickLaborerPath Returning;
					Outgoing.BuildBounded(DoorA, DoorB, 140.f, 220.f, Lane,
						Footprint, HalfExtent, 130.f, BodyBounds);
					Returning.BuildBounded(DoorB, DoorA, 140.f, 220.f, Lane,
						Footprint, HalfExtent, 130.f, BodyBounds);
					TestTrue(TEXT("Reverse leg starts at the exact same inside-door point"),
						Outgoing.InsideTo.Equals(Returning.InsideFrom, 0.01));
					TestTrue(TEXT("Exit crosses the authored socket center"),
						Outgoing.PositionAtDistance(140.f).Equals(DoorA.GetLocation(), 0.01));
					TestTrue(TEXT("Entry crosses the authored socket center"),
						Outgoing.PositionAtDistance(Outgoing.Length() - 140.f).Equals(DoorB.GetLocation(), 0.01));
					TestTrue(TEXT("Exit direction stays normal to its door"),
						Outgoing.DirectionAtDistance(140.f).Equals(-DoorA.GetUnitAxis(EAxis::X), 0.001));
					TestTrue(TEXT("Entry direction stays normal to its door"),
						Outgoing.DirectionAtDistance(Outgoing.Length() - 140.f).Equals(DoorB.GetUnitAxis(EAxis::X), 0.001));
					for (const float DoorDistance : { 140.f, 220.f, 359.f })
					{
						const FVector ExitLocal = DoorA.InverseTransformPosition(Outgoing.PositionAtDistance(DoorDistance));
						const FVector EntryLocal = DoorB.InverseTransformPosition(
							Outgoing.PositionAtDistance(Outgoing.Length() - DoorDistance));
						TestTrue(TEXT("Both door approaches remain straight through the wall"),
							FMath::Abs(ExitLocal.Y) < 0.01 && FMath::Abs(EntryLocal.Y) < 0.01);
					}
					TArray<FVector> ActiveControls = { Outgoing.InsideFrom, Outgoing.InsideTo,
						Outgoing.CurveP0, Outgoing.SecondP3 };
					// Side routes use their own segments; the front-curve handles are unused.
					const bool bUsesFrontCurve = Footprint.InverseTransformVectorNoScale(Outgoing.InwardFrom).X >= 0.99f
						&& Footprint.InverseTransformVectorNoScale(Outgoing.InwardTo).X >= 0.99f;
					if (bUsesFrontCurve)
					{
						ActiveControls.Append({ Outgoing.CurveP1, Outgoing.CurveP2, Outgoing.CurveP3,
							Outgoing.SecondP1, Outgoing.SecondP2 });
					}
					for (const FVector& Control : ActiveControls)
					{
						const FVector Local = Footprint.InverseTransformPositionNoScale(Control);
						TestTrue(TEXT("Bezier controls and door approaches fit the worker-safe plot"),
							FMath::Abs(Local.X) <= HalfExtent.X - 130.f + 0.01f
							&& FMath::Abs(Local.Y) <= HalfExtent.Y - 130.f + 0.01f);
					}
					for (int32 Sample = 0; Sample <= 128; ++Sample)
					{
						const float Distance = Outgoing.Length() * Sample / 128.f;
						const FVector Local = Footprint.InverseTransformPositionNoScale(Outgoing.PositionAtDistance(Distance));
						TestTrue(TEXT("Every traveled point stays in the worker-safe plot"),
							FMath::Abs(Local.X) <= HalfExtent.X - 130.f + 0.02f
							&& FMath::Abs(Local.Y) <= HalfExtent.Y - 130.f + 0.02f);
						if (Stage == 8 && Distance > Outgoing.ExitLength
							&& Distance < Outgoing.ExitLength + Outgoing.CurveLength
							&& FMath::Abs(Local.Y) < 400.f)
						{
							TestTrue(TEXT("Central frontage clears the full final-stage body and worker radius"),
								Local.X <= BodyBounds.Min.X - 130.f + 1.f);
						}
					}
					for (int32 Sample = 0; Sample <= 64; ++Sample)
					{
						const float Distance = Returning.Length() * Sample / 64.f;
						const FVector Local = Footprint.InverseTransformPositionNoScale(
							Returning.PositionAtDistance(Distance));
						TestTrue(TEXT("Return leg also stays within the worker-safe plot"),
							FMath::Abs(Local.X) <= HalfExtent.X - 130.f + 0.02f
							&& FMath::Abs(Local.Y) <= HalfExtent.Y - 130.f + 0.02f);
						if (Stage == 8 && Distance > Returning.ExitLength
							&& Distance < Returning.ExitLength + Returning.CurveLength
							&& FMath::Abs(Local.Y) < 400.f)
						{
							TestTrue(TEXT("Return leg clears the central final-stage frontage"),
								Local.X <= BodyBounds.Min.X - 130.f + 1.f);
						}
					}
					const float CurveMiddleDistance = bUsesFrontCurve
						? Outgoing.CurveDistances[32] : Outgoing.CurveLength * 0.5f;
					TestTrue(TEXT("The active exterior curve stays continuous through its midpoint"),
						Outgoing.PositionAtDistance(Outgoing.ExitLength + CurveMiddleDistance - 0.01f)
							.Equals(Outgoing.PositionAtDistance(
								Outgoing.ExitLength + CurveMiddleDistance + 0.01f), 0.03));
					const FVector BeforeJoin = Outgoing.DirectionAtDistance(
						Outgoing.ExitLength + CurveMiddleDistance - 0.05f);
					const FVector AfterJoin = Outgoing.DirectionAtDistance(
						Outgoing.ExitLength + CurveMiddleDistance + 0.05f);
					TestTrue(TEXT("The active exterior curve has a continuous tangent at its midpoint"),
						FVector::DotProduct(BeforeJoin, AfterJoin) > 0.995f);
				}
				// Compare the traveled loci: different timing along one curve cannot pass this.
				auto MaxNearestSeparation = [](const FBrickLaborerPath& First, const FBrickLaborerPath& Second,
					int32 FirstSamples, int32 SecondSamples)
				{
					float MaxNearest = 0.f;
					for (int32 A = 0; A <= FirstSamples; ++A)
					{
						const FVector Point = First.PositionAtDistance(
							First.ExitLength + First.CurveLength * A / FirstSamples);
						float Nearest = TNumericLimits<float>::Max();
						for (int32 B = 0; B <= SecondSamples; ++B)
						{
							const FVector Other = Second.PositionAtDistance(
								Second.ExitLength + Second.CurveLength * B / SecondSamples);
							Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(Point, Other)));
						}
						MaxNearest = FMath::Max(MaxNearest, Nearest);
					}
					return MaxNearest;
				};
				TestTrue(TEXT("Inner and outer routes occupy different ground"),
					MaxNearestSeparation(Paths[0], Paths[1], 64, 128) > 70.f);
				TestTrue(TEXT("Left apron route separates from the inner route"),
					MaxNearestSeparation(Paths[2], Paths[0], 64, 128) > 70.f);
				TestTrue(TEXT("Right apron route separates from the inner route"),
					MaxNearestSeparation(Paths[3], Paths[0], 64, 128) > 70.f);
				const float FifthRouteSeparation = MaxNearestSeparation(Paths[4], Paths[0], 64, 128);
				TestTrue(FString::Printf(TEXT("Fifth route separates from the inner route (stage %d, yaw %.0f, crew %d, gap %.2f cm)"),
					Stage, Yaw, Crew, FifthRouteSeparation), FifthRouteSeparation > 70.f);
				if (Stage == 8 && Crew == 100)
				{
					for (int32 Lane = 0; Lane < 100; ++Lane)
					{
						FBrickLaborerPath Rebuilt;
						Rebuilt.BuildBounded(DoorA, DoorB, 140.f, 220.f, Lane,
							Footprint, HalfExtent, 130.f, BodyBounds);
						for (int32 Sample = 0; Sample <= 4; ++Sample)
						{
							TestTrue(TEXT("A worker keeps the same path when it is rebuilt"),
								Paths[Lane].PositionAtDistance(Paths[Lane].Length() * Sample / 4.f)
									.Equals(Rebuilt.PositionAtDistance(Rebuilt.Length() * Sample / 4.f), 0.001));
						}
						for (int32 Earlier = Lane % 5; Earlier < Lane; Earlier += 5)
						{
							TestTrue(TEXT("Every worker sharing a route family leaves a distinct ground trace"),
								MaxNearestSeparation(Paths[Lane], Paths[Earlier], 16, 32) > 5.f);
						}
					}
					if (FMath::IsNearlyZero(Yaw))
					{
						for (const FIntPoint DoorPair : { FIntPoint(0, 2), FIntPoint(2, 3) })
						{
							TArray<FBrickLaborerPath> SidePaths;
							SidePaths.SetNum(100);
							for (int32 Lane = 0; Lane < SidePaths.Num(); ++Lane)
							{
								SidePaths[Lane].BuildBounded(Factory->GetLaborerDoorTransform(DoorPair.X),
									Factory->GetLaborerDoorTransform(DoorPair.Y), 140.f, 220.f, Lane,
									Footprint, HalfExtent, 130.f, BodyBounds);
								for (int32 Earlier = Lane % 5; Earlier < Lane; Earlier += 5)
								{
									TestTrue(TEXT("Same-family side workers take distinct ground traces"),
										MaxNearestSeparation(SidePaths[Lane], SidePaths[Earlier], 32, 64) > 5.f);
								}
							}
							const FBrickLaborerPath& TurnPath = SidePaths[0];
							float LeastTangentDot = 1.f;
							for (float Along = 0.25f; Along < TurnPath.CurveLength - 0.25f; Along += 0.5f)
							{
								const FVector Before = TurnPath.DirectionAtDistance(TurnPath.ExitLength + Along - 0.1f);
								const FVector After = TurnPath.DirectionAtDistance(TurnPath.ExitLength + Along + 0.1f);
								LeastTangentDot = FMath::Min(LeastTangentDot, static_cast<float>(FVector::DotProduct(Before, After)));
							}
							TestTrue(FString::Printf(TEXT("Side route %d to %d curve joins keep a continuous tangent (dot %.6f)"),
								DoorPair.X, DoorPair.Y, LeastTangentDot), LeastTangentDot > 0.995f);
						}
					}
				}
				if (Stage == 8)
				{
					const float DoorMinY = FMath::Min(
						Footprint.InverseTransformPositionNoScale(DoorA.GetLocation()).Y,
						Footprint.InverseTransformPositionNoScale(DoorB.GetLocation()).Y);
					const float DoorMaxY = FMath::Max(
						Footprint.InverseTransformPositionNoScale(DoorA.GetLocation()).Y,
						Footprint.InverseTransformPositionNoScale(DoorB.GetLocation()).Y);
					float Leftmost = TNumericLimits<float>::Max();
					float Rightmost = -TNumericLimits<float>::Max();
					for (int32 Sample = 0; Sample <= 128; ++Sample)
					{
						const FVector Left = Footprint.InverseTransformPositionNoScale(Paths[2].PositionAtDistance(
							Paths[2].ExitLength + Paths[2].CurveLength * Sample / 128.f));
						const FVector Right = Footprint.InverseTransformPositionNoScale(Paths[3].PositionAtDistance(
							Paths[3].ExitLength + Paths[3].CurveLength * Sample / 128.f));
						Leftmost = FMath::Min(Leftmost, static_cast<float>(Left.Y));
						Rightmost = FMath::Max(Rightmost, static_cast<float>(Right.Y));
					}
					TestTrue(TEXT("One path actually travels into the left apron"), Leftmost < DoorMinY - 100.f);
					TestTrue(TEXT("One path actually travels into the right apron"), Rightmost > DoorMaxY + 100.f);
				}
			}
		}
	}
	if (GEngine) GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRLaborerCompactRouteTest,
	"CGR.Factory.LaborerDoor.CompactRouteIndependentOfPlot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRLaborerCompactRouteTest::RunTest(const FString& Parameters)
{
	const FTransform Footprint = FTransform::Identity;
	const FBox2D BodyBounds(FVector2D(-500.f, -600.f), FVector2D(500.f, 600.f));
	const FTransform FrontA(FRotator::ZeroRotator, FVector(-500.f, -200.f, 0.f));
	const FTransform FrontB(FRotator::ZeroRotator, FVector(-500.f, 200.f, 0.f));
	const FTransform SideDoor(FRotator(0.f, 90.f, 0.f), FVector(0.f, -600.f, 0.f));
	const FBox2D FrontHedge(FVector2D(-1380.f, -1836.f), FVector2D(-1220.f, 1836.f));
	const FBox2D SideHedge(FVector2D(-1836.f, -1400.f), FVector2D(1836.f, -1240.f));

	for (const FTransform& Destination : { FrontB, SideDoor })
	{
		const bool bSideRoute = Destination.GetLocation().Y < BodyBounds.Min.Y + 1.f;
		for (int32 Lane = 0; Lane < 100; ++Lane)
		{
			FBrickLaborerPath ReservedPlotPath;
			FBrickLaborerPath EnlargedPlotPath;
			ReservedPlotPath.BuildBounded(FrontA, Destination, 140.f, 220.f, Lane,
				Footprint, FVector2D(1836.f, 1836.f), 130.f, BodyBounds);
			EnlargedPlotPath.BuildBounded(FrontA, Destination, 140.f, 220.f, Lane,
				Footprint, FVector2D(2500.f, 2500.f), 130.f, BodyBounds);
			TestTrue(FString::Printf(TEXT("%s lane %d follows the current body regardless of spare plot"),
				bSideRoute ? TEXT("Side") : TEXT("Front"), Lane),
				FMath::Abs(ReservedPlotPath.Length() - EnlargedPlotPath.Length()) < 30.f);
			float MostExteriorX = TNumericLimits<float>::Max();
			float MostExteriorY = TNumericLimits<float>::Max();
			bool bTouchesGreenery = false;
			for (int32 Sample = 0; Sample <= 128; ++Sample)
			{
				const FVector Point = ReservedPlotPath.PositionAtDistance(
					ReservedPlotPath.Length() * Sample / 128.f);
				MostExteriorX = FMath::Min(MostExteriorX, Point.X);
				MostExteriorY = FMath::Min(MostExteriorY, Point.Y);
				const FVector2D GroundPoint(Point.X, Point.Y);
				bTouchesGreenery |= FrontHedge.ExpandBy(130.f).IsInside(GroundPoint)
					|| SideHedge.ExpandBy(130.f).IsInside(GroundPoint);
			}
			TestFalse(FString::Printf(TEXT("%s lane %d leaves worker clearance before fixed greenery"),
				bSideRoute ? TEXT("Side") : TEXT("Front"), Lane), bTouchesGreenery);
			TestTrue(FString::Printf(TEXT("%s lane %d stays near its current wall"),
				bSideRoute ? TEXT("Side") : TEXT("Front"), Lane),
				bSideRoute ? MostExteriorY > BodyBounds.Min.Y - 800.f
					: MostExteriorX > BodyBounds.Min.X - 800.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRLaborerDoorActorTest,
	"CGR.Factory.LaborerDoor.ActorCrossingAndTurn",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRLaborerDoorActorTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world exists"), TestWorld)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ABrickFactory* Factory = TestWorld->SpawnActor<ABrickFactory>();
	ABrickLaborer* Laborer = TestWorld->SpawnActor<ABrickLaborer>();
	if (!Factory || !Laborer)
	{
		AddError(TEXT("Native factory and laborer must spawn"));
		if (GEngine) GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
		return false;
	}
	TArray<UStaticMeshComponent*> LaborerStaticMeshes;
	Laborer->GetComponents<UStaticMeshComponent>(LaborerStaticMeshes);
	UStaticMeshComponent* CarriedBrickMesh = nullptr;
	for (UStaticMeshComponent* Component : LaborerStaticMeshes)
	{
		if (Component && Component->GetFName() == TEXT("CarriedBrickMesh"))
		{
			CarriedBrickMesh = Component;
			break;
		}
	}
	TestNotNull(TEXT("Laborer has a carried brick presentation component"), CarriedBrickMesh);
	USkeletalMesh* LaborerBodyAsset = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/LOD/SK_Laborer_Body"));
	TArray<USkeletalMeshComponent*> LaborerParts;
	Laborer->GetComponents(LaborerParts);
	USkeletalMeshComponent* LaborerBody = nullptr;
	for (USkeletalMeshComponent* Component : LaborerParts)
	{
		if (Component && Component->GetFName() == TEXT("Mesh")) LaborerBody = Component;
	}
	if (TestNotNull(TEXT("Laborer body asset exists"), LaborerBodyAsset)
		&& TestNotNull(TEXT("Native laborer body component exists"), LaborerBody))
	{
		LaborerBody->SetSkeletalMeshAsset(LaborerBodyAsset);
	}
	Laborer->DispatchBeginPlay();
	if (CarriedBrickMesh)
	{
		TestNotNull(TEXT("Carried brick asset is loaded"), CarriedBrickMesh->GetStaticMesh().Get());
		TestEqual(TEXT("Brick follows the worker's right hand"),
			CarriedBrickMesh->GetAttachSocketName(), FName(TEXT("RightHand")));
		TestEqual(TEXT("Carried brick has no collision"),
			CarriedBrickMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	Factory->SetActorLocationAndRotation(FVector(3000.f, 6000.f, 25.f), FRotator(0.f, 47.f, 0.f));
	UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent());
	UDataTable* AppearanceTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/Factory/DT_FactoryAppearance"));
	UStaticMesh* PlotReferenceMesh = FactoryDoorTests::LoadStageMesh(AppearanceTable, 8);
	FStructProperty* FootprintOffsetProperty = FindFProperty<FStructProperty>(
		ABrickFactory::StaticClass(), TEXT("PlacementFootprintOffset"));
	if (!TestNotNull(TEXT("Factory appearance table exists"), AppearanceTable)
		|| !TestNotNull(TEXT("Final-stage plot reference mesh exists"), PlotReferenceMesh)
		|| !TestNotNull(TEXT("Factory footprint offset is editable"), FootprintOffsetProperty))
	{
		if (GEngine) GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
		return false;
	}
	FVector2D* FootprintOffset = FootprintOffsetProperty->ContainerPtrToValuePtr<FVector2D>(Factory);
	*FootprintOffset = FVector2D(PlotReferenceMesh->GetBounds().Origin.X,
		PlotReferenceMesh->GetBounds().Origin.Y);
	Factory->OnConstruction(Factory->GetActorTransform());
	if (!TestNotNull(TEXT("Actor test factory body"), Body))
	{
		if (GEngine) GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Actor crossing uses the scaled factory appearance"),
		Body->GetRelativeScale3D().Equals(FVector(0.9f), 0.001));
	UAnimSequence* WalkClip = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Anims/AS_Walk"));
	UAnimSequence* CarryWalkClip = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Laborer/Anims/AS_CarryWalk"));
	ABrickLaborer* OtherLaborer = TestWorld->SpawnActor<ABrickLaborer>();
	if (TestNotNull(TEXT("Normal walk clip exists"), WalkClip)
		&& TestNotNull(TEXT("Carry walk clip exists"), CarryWalkClip)
		&& TestNotNull(TEXT("Second laborer exists"), OtherLaborer)
		&& CarriedBrickMesh)
	{
		TArray<USkeletalMeshComponent*> OtherParts;
		OtherLaborer->GetComponents(OtherParts);
		for (USkeletalMeshComponent* Component : OtherParts)
		{
			if (Component && Component->GetFName() == TEXT("Mesh"))
			{
				Component->SetSkeletalMeshAsset(LaborerBodyAsset);
			}
		}
		OtherLaborer->DispatchBeginPlay();
		TArray<UStaticMeshComponent*> OtherMeshes;
		OtherLaborer->GetComponents(OtherMeshes);
		UStaticMeshComponent* OtherCargo = nullptr;
		for (UStaticMeshComponent* Component : OtherMeshes)
		{
			if (Component && Component->GetFName() == TEXT("CarriedBrickMesh")) OtherCargo = Component;
		}
		if (TestNotNull(TEXT("Second laborer has brick presentation"), OtherCargo))
		{
			Laborer->InitLaborer(Factory, 0, 2);
			OtherLaborer->InitLaborer(Factory, 1, 2);
			Laborer->SetCycleDuration(24.f);
			OtherLaborer->SetCycleDuration(24.f);
			Laborer->SetGait(WalkClip, 1.f, 200.f, CarryWalkClip);
			OtherLaborer->SetGait(WalkClip, 1.f, 200.f, CarryWalkClip);
			TestTrue(TEXT("Even lane carries on its first leg"), CarriedBrickMesh->IsVisible());
			TestFalse(TEXT("Odd lane starts empty-handed"), OtherCargo->IsVisible());
			Laborer->Tick(23.8f);
			Laborer->RefreshPath();
			TestTrue(TEXT("Entry and path refresh preserve the current load"), CarriedBrickMesh->IsVisible());
			Laborer->Tick(0.21f);
			TestFalse(TEXT("Load changes after the interior leg completes"), CarriedBrickMesh->IsVisible());
			OtherLaborer->Tick(24.f);
			TestTrue(TEXT("Previously empty lane carries on its next leg"), OtherCargo->IsVisible());
			Laborer->Tick(24.f);
			TestTrue(TEXT("Subsequent leg restores carried bricks"), CarriedBrickMesh->IsVisible());
		}
	}
	if (OtherLaborer) OtherLaborer->Destroy();
	for (const int32 Stage : { 1, 8 })
	{
		UStaticMesh* StageMesh = FactoryDoorTests::LoadStageMesh(AppearanceTable, Stage);
		if (!TestNotNull(TEXT("Saved factory stage is available"), StageMesh) || !Body) continue;
		Body->SetStaticMesh(StageMesh);
		TestEqual(TEXT("Actor uses the current stage's authored door count"),
			Factory->GetLaborerDoorCount(), Stage == 1 ? 2 : 4);
		const FTransform ExitDoor = Factory->GetLaborerDoorTransform(1);
		const FTransform EntryDoor = Factory->GetLaborerDoorTransform(0);
		for (const int32 Crew : { 5, 100 })
		{
			Laborer->InitLaborer(Factory, 0, Crew);
			Laborer->SetCycleDuration(24.f);
			TestTrue(TEXT("Actor starts inside the center of the saved exit socket"),
				Laborer->GetActorLocation().Equals(ExitDoor.GetLocation() + ExitDoor.GetUnitAxis(EAxis::X) * 140.f, 0.01));
			Laborer->Tick(23.70f);
			const FVector TurnLocation = Laborer->GetActorLocation();
			const FVector BeforeTurn = Laborer->GetActorForwardVector();
			TestTrue(TEXT("Actor is fully inside before turning"),
				TurnLocation.Equals(EntryDoor.GetLocation() + EntryDoor.GetUnitAxis(EAxis::X) * 140.f, 0.01));
			Laborer->Tick(0.14f);
			TestTrue(TEXT("Turning inside does not move sideways"), Laborer->GetActorLocation().Equals(TurnLocation, 0.01));
			TestTrue(TEXT("Actor rotates gradually rather than flipping at the doorstep"),
				FVector::DotProduct(BeforeTurn, Laborer->GetActorForwardVector()) < 0.9);
			Laborer->Tick(0.15f);
			Laborer->Tick(0.02f);
			TestTrue(TEXT("Next leg does not teleport to another lane"),
				FVector::Distance(Laborer->GetActorLocation(), TurnLocation) < 5.0);
			TestTrue(TEXT("Actor leaves along the same doorway normal"),
				Laborer->GetActorForwardVector().Equals(-EntryDoor.GetUnitAxis(EAxis::X), 0.001));
			Laborer->Tick(72.25f);
			TestFalse(TEXT("Multiple legs in one frame leave a finite actor position"),
				Laborer->GetActorLocation().ContainsNaN());
		}
	}
	if (Body && PlotReferenceMesh)
	{
		Body->SetStaticMesh(PlotReferenceMesh);
		TestEqual(TEXT("Actor sequence uses four live factory doors"), Factory->GetLaborerDoorCount(), 4);
		Laborer->InitLaborer(Factory, 0, 5);
		Laborer->SetCycleDuration(24.f);
		bool Visited[4] = {};
		int32 PreviousDoor = INDEX_NONE;
		for (int32 Trip = 0; Trip < 12; ++Trip)
		{
			Laborer->Tick(24.f);
			int32 ArrivedDoor = INDEX_NONE;
			for (int32 DoorIndex = 0; DoorIndex < Factory->GetLaborerDoorCount(); ++DoorIndex)
			{
				const FTransform Door = Factory->GetLaborerDoorTransform(DoorIndex);
				if (Laborer->GetActorLocation().Equals(
					Door.GetLocation() + Door.GetUnitAxis(EAxis::X) * 140.f, 0.01))
				{
					ArrivedDoor = DoorIndex;
					Visited[DoorIndex] = true;
					break;
				}
			}
			TestTrue(TEXT("Every completed leg lands inside an authored door"), ArrivedDoor != INDEX_NONE);
			if (PreviousDoor != INDEX_NONE)
			{
				TestTrue(TEXT("A new leg never targets the door it just entered"), ArrivedDoor != PreviousDoor);
			}
			PreviousDoor = ArrivedDoor;
		}
		for (bool bDoorVisited : Visited)
		{
			TestTrue(TEXT("Deterministic trips eventually visit every factory door"), bDoorVisited);
		}
		const FTransform DoorZeroTransform = Factory->GetLaborerDoorTransform(0);
		const FTransform DoorOneTransform = Factory->GetLaborerDoorTransform(1);
		USceneComponent* DoorZero = NewObject<USceneComponent>(Factory, TEXT("LaborerDoor_0"));
		USceneComponent* DoorOne = NewObject<USceneComponent>(Factory, TEXT("LaborerDoor_1"));
		DoorZero->SetupAttachment(Factory->GetRootComponent());
		DoorOne->SetupAttachment(Factory->GetRootComponent());
		DoorZero->RegisterComponent();
		DoorOne->RegisterComponent();
		DoorZero->SetWorldTransform(DoorZeroTransform);
		DoorOne->SetWorldTransform(DoorOneTransform);
		Body->SetStaticMesh(nullptr);
		Factory->OnConstruction(Factory->GetActorTransform());
		TestEqual(TEXT("Two authored components supply the fallback door count"), Factory->GetLaborerDoorCount(), 2);
		Laborer->InitLaborer(Factory, 0, 5);
		Laborer->SetCycleDuration(24.f);
		Laborer->Tick(24.f);
		TestTrue(TEXT("Two-door leg enters the first component door"),
			Laborer->GetActorLocation().Equals(DoorZeroTransform.GetLocation()
				+ DoorZeroTransform.GetUnitAxis(EAxis::X) * 140.f, 0.01));
		Laborer->Tick(24.f);
		TestTrue(TEXT("Two-door return enters the other component door"),
			Laborer->GetActorLocation().Equals(DoorOneTransform.GetLocation()
				+ DoorOneTransform.GetUnitAxis(EAxis::X) * 140.f, 0.01));
		USceneComponent* DoorTwo = NewObject<USceneComponent>(Factory, TEXT("LaborerDoor_2"));
		DoorTwo->SetupAttachment(Factory->GetRootComponent());
		DoorTwo->RegisterComponent();
		const FTransform FallbackFootprint = Factory->GetPlacementFootprintTransform();
		DoorTwo->SetWorldLocationAndRotation(
			FallbackFootprint.TransformPositionNoScale(FVector(0.f, -1100.f, 1.f)),
			FRotator(0.f, FallbackFootprint.GetRotation().Rotator().Yaw + 90.f, 0.f));
		Factory->OnConstruction(Factory->GetActorTransform());
		TestEqual(TEXT("A side-facing authored component supplies a third fallback door"),
			Factory->GetLaborerDoorCount(), 3);
		FBrickLaborerPath MeshlessSidePath;
		const FVector2D FallbackHalfExtent = Factory->GetPlacementFootprintHalfExtent(false);
		MeshlessSidePath.BuildBounded(DoorZeroTransform, DoorTwo->GetComponentTransform(), 140.f, 220.f, 2,
			FallbackFootprint, FallbackHalfExtent, 130.f, FBox2D(ForceInit));
		for (int32 Sample = 0; Sample <= 64; ++Sample)
		{
			const FVector Local = FallbackFootprint.InverseTransformPositionNoScale(
				MeshlessSidePath.PositionAtDistance(MeshlessSidePath.Length() * Sample / 64.f));
			TestTrue(TEXT("Meshless side route stays within the worker-safe plot"),
				FMath::Abs(Local.X) <= FallbackHalfExtent.X - 130.f + 0.02f
				&& FMath::Abs(Local.Y) <= FallbackHalfExtent.Y - 130.f + 0.02f);
		}
		ABrickLaborer* SideLaborer = TestWorld->SpawnActor<ABrickLaborer>();
		SideLaborer->InitLaborer(Factory, 2, 5);
		SideLaborer->SetCycleDuration(24.f);
		SideLaborer->Tick(24.f);
		TestTrue(TEXT("A side-facing component route remains finite without a body mesh"),
			!SideLaborer->GetActorLocation().ContainsNaN());
		SideLaborer->Destroy();
		DoorTwo->DestroyComponent();
		DoorOne->DestroyComponent();
		Factory->OnConstruction(Factory->GetActorTransform());
		TestEqual(TEXT("One remaining component is a valid single-door factory"), Factory->GetLaborerDoorCount(), 1);
		Laborer->RefreshPath();
		Laborer->Tick(24.f);
		TestTrue(TEXT("Single-door route continues at its only entrance"),
			Laborer->GetActorLocation().Equals(DoorZeroTransform.GetLocation()
				+ DoorZeroTransform.GetUnitAxis(EAxis::X) * 140.f, 0.01));
	}
	Laborer->RequestRetireAfterCycle();
	Laborer->Tick(24.f);
	TestTrue(TEXT("Retirement still occurs at the next cycle boundary"),
		!IsValid(Laborer) || Laborer->IsActorBeingDestroyed());
	if (GEngine) GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);
	return true;
}

#endif
