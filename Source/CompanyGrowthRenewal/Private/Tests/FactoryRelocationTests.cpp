#include "Entity/Factory/BrickFactory.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Player/Components/PlotPlacementRules.h"
#include "Table/BuildingData.h"
#include "Table/FactoryAppearanceTable.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace FactoryRelocationTests
{
UStaticMesh* LoadStageMesh(const UDataTable* AppearanceTable, int32 Stage)
{
	if (!AppearanceTable) return nullptr;
	const FName RowName(*FString::Printf(TEXT("Level_%02d"), Stage));
	const FFactoryAppearanceRow* Row = AppearanceTable->FindRow<FFactoryAppearanceRow>(RowName, TEXT("Factory footprint automation"));
	return Row && Row->Level == Stage ? Row->Mesh.LoadSynchronous() : nullptr;
}

struct FFixture
{
	UWorld* TestWorld = nullptr;
	ABrickFactory* Factory = nullptr;
	FFixture()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (!TestWorld) return;
		if (GEngine) GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(TestWorld);
		Factory = TestWorld->SpawnActor<ABrickFactory>();
	}
	~FFixture()
	{
		if (!TestWorld) return;
		if (GEngine) GEngine->DestroyWorldContext(TestWorld);
		TestWorld->DestroyWorld(false);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryRelocationTransactionTest,
	"CGR.Factory.Relocation.CommittedSaveAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRFactoryRelocationTransactionTest::RunTest(const FString& Parameters)
{
	FactoryRelocationTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Factory fixture"), Fixture.Factory)) return false;
	const FTransform Original(FRotator(0.f, 90.f, 0.f), FVector(4500.f, -2600.f, 10900.f));
	const FTransform Destination(FRotator(0.f, 180.f, 0.f), FVector(15000.f, 4200.f, 9700.f));
	Fixture.Factory->SetActorTransform(Original);
	TestTrue(TEXT("Relocation starts"), Fixture.Factory->BeginRelocation());
	TestFalse(TEXT("A second move cannot replace the original transaction"), Fixture.Factory->BeginRelocation());
	Fixture.Factory->SetActorTransform(Destination);
	TestTrue(TEXT("Autosave retains the original transform during preview"),
		Fixture.Factory->GetFactoryData().PlacementTransform.Equals(Original));
	TestTrue(TEXT("Preview does not commit a plot"), Fixture.Factory->GetOwningPlotId().IsNone());
	Fixture.Factory->FinishRelocation(false);
	TestFalse(TEXT("Cancellation ends relocation"), Fixture.Factory->IsRelocating());
	TestTrue(TEXT("Cancellation restores position and rotation"), Fixture.Factory->GetActorTransform().Equals(Original));
	TestTrue(TEXT("Move starts after cancellation"), Fixture.Factory->BeginRelocation());
	Fixture.Factory->SetActorTransform(Destination);
	const FName DestinationPlot(TEXT("RelocationTestPlot"));
	Fixture.Factory->FinishRelocation(true, DestinationPlot);
	const FFactorySaveData Saved = Fixture.Factory->GetFactoryData();
	TestTrue(TEXT("Confirmed transform is saved"), Saved.PlacementTransform.Equals(Destination));
	TestEqual(TEXT("Confirmed plot is saved"), Saved.OwningPlotId, DestinationPlot);
	TestFalse(TEXT("Confirmation ends relocation"), Fixture.Factory->IsRelocating());
	Fixture.Factory->SetActorTransform(Original);
	Fixture.Factory->SetFactoryData(Saved);
	TestTrue(TEXT("Save reload restores the relocated factory"), Fixture.Factory->GetActorTransform().Equals(Destination));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryRelocationFootprintTest,
	"CGR.Factory.Relocation.StableFootprintAcrossStages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRFactoryRelocationFootprintTest::RunTest(const FString& Parameters)
{
	FactoryRelocationTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Factory fixture"), Fixture.Factory)) return false;
	UStaticMesh* FinalMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/CompanyGrowth/Factory/Expansion/SM_FactoryExpansion_06"));
	if (!TestNotNull(TEXT("Final stage mesh"), FinalMesh)) return false;
	const FVector BoundsCenter = FinalMesh->GetBoundingBox().GetCenter();
	FStructProperty* OffsetProperty = FindFProperty<FStructProperty>(ABrickFactory::StaticClass(), TEXT("PlacementFootprintOffset"));
	if (!TestNotNull(TEXT("Authored reservation anchor"), OffsetProperty)) return false;
	UClass* FactoryBlueprintClass = LoadClass<ABrickFactory>(nullptr,
		TEXT("/Game/CompanyGrowth/Blueprint/Factory/BP_BrickFactory.BP_BrickFactory_C"));
	if (!TestNotNull(TEXT("Authored factory Blueprint"), FactoryBlueprintClass)) return false;
	const ABrickFactory* BlueprintDefaults = FactoryBlueprintClass->GetDefaultObject<ABrickFactory>();
	const FVector2D AuthoredOffset = *OffsetProperty->ContainerPtrToValuePtr<FVector2D>(BlueprintDefaults);
	if (!TestTrue(TEXT("Factory Blueprint anchor matches the final-stage body center"),
		AuthoredOffset.Equals(FVector2D(BoundsCenter.X, BoundsCenter.Y), 0.01))) return false;
	FFloatProperty* ScaleProperty = FindFProperty<FFloatProperty>(ABrickFactory::StaticClass(), TEXT("FactoryScale"));
	if (!TestNotNull(TEXT("Factory scale property"), ScaleProperty)) return false;
	const float BlueprintScale = ScaleProperty->GetPropertyValue_InContainer(BlueprintDefaults);
	TestTrue(TEXT("Factory Blueprint uses the requested ninety percent scale"), FMath::IsNearlyEqual(BlueprintScale, 0.9f));
	const float FactoryScale = ScaleProperty->GetPropertyValue_InContainer(Fixture.Factory);
	TestTrue(TEXT("Native fixture uses the same authored factory scale"), FMath::IsNearlyEqual(FactoryScale, BlueprintScale));
	UDataTable* AppearanceTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/Factory/DT_FactoryAppearance"));
	if (!TestNotNull(TEXT("Factory appearance table"), AppearanceTable)) return false;
	TestEqual(TEXT("Factory appearance table has eight authored levels"), AppearanceTable->GetRowNames().Num(), 8);
	*OffsetProperty->ContainerPtrToValuePtr<FVector2D>(Fixture.Factory) = AuthoredOffset;
	Fixture.Factory->OnConstruction(Fixture.Factory->GetActorTransform());
	const FVector2D LocalHalf(FPlotPlacementRules::GetPlotCellSize(PlotBaseCellSizeCompact) * FactoryScale);
	UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Fixture.Factory->GetMainMeshComponent());
	if (!TestNotNull(TEXT("Factory body"), Body)) return false;
	UBoxComponent* PlacementBox = Cast<UBoxComponent>(Fixture.Factory->GetDefaultSubobjectByName(TEXT("Box")));
	if (!TestNotNull(TEXT("Factory placement box"), PlacementBox)) return false;
	for (const float Yaw : { 0.f, 90.f, 180.f, 270.f })
	{
		Fixture.Factory->SetActorLocationAndRotation(FVector(5000.f, -12000.f, 10900.f), FRotator(0.f, Yaw, 0.f));
		const FTransform Frame = Fixture.Factory->GetPlacementFootprintTransform();
		for (int32 Stage = 1; Stage <= 8; ++Stage)
		{
			UStaticMesh* StageMesh = FactoryRelocationTests::LoadStageMesh(AppearanceTable, Stage);
			if (!TestNotNull(TEXT("Stage mesh"), StageMesh)) continue;
			Body->SetStaticMesh(StageMesh);
			Fixture.Factory->ReCalcBoxExtent();
			TestTrue(TEXT("Every authored appearance uses the factory scale"),
				Body->GetRelativeScale3D().Equals(FVector(FactoryScale), 0.001));
			TestTrue(TEXT("Appearance scale retains the authored reservation anchor"),
				Body->GetRelativeLocation().Equals(FVector(AuthoredOffset.X, AuthoredOffset.Y, 0.f) * (1.f - FactoryScale), 0.01));
			TestTrue(TEXT("Changing appearance does not move the reserved center"), Fixture.Factory->GetPlacementFootprintTransform().Equals(Frame));
			TestTrue(TEXT("Quarter turns preserve the scaled two by two area"), Fixture.Factory->GetPlacementFootprintHalfExtent().Equals(LocalHalf, 0.01));
			const FVector BoxCenter = PlacementBox->GetComponentLocation();
			const FVector ReservedCenter = Fixture.Factory->GetPlacementFootprintCenter();
			TestTrue(TEXT("The Box XY center matches the reserved plot center for every stage and rotation"),
				FVector2D(BoxCenter.X, BoxCenter.Y).Equals(FVector2D(ReservedCenter.X, ReservedCenter.Y), 0.01));
			const FVector BoxHalfExtent = PlacementBox->GetScaledBoxExtent();
			TestTrue(TEXT("The Box covers the scaled two by two plot for every stage and rotation"),
				FVector2D(BoxHalfExtent.X, BoxHalfExtent.Y).Equals(LocalHalf, 0.01));
			TestTrue(TEXT("The Box rotates with the reserved plot"), PlacementBox->GetComponentQuat().Equals(Frame.GetRotation(), 0.0001));
			const FBox Bounds = StageMesh->GetBoundingBox();
			for (const float X : { Bounds.Min.X, Bounds.Max.X })
			{
				for (const float Y : { Bounds.Min.Y, Bounds.Max.Y })
				{
					const FVector InFootprint = Frame.InverseTransformPositionNoScale(
						Body->GetComponentTransform().TransformPosition(FVector(X, Y, Bounds.Min.Z)));
					TestTrue(TEXT("Every stage body fits the scaled reserved area"),
						FMath::Abs(InFootprint.X) <= LocalHalf.X + 0.01
						&& FMath::Abs(InFootprint.Y) <= LocalHalf.Y + 0.01);
				}
			}
		}
	}
	UFunction* WobbleUpdate = Fixture.Factory->FindFunction(TEXT("Wooble_TimelineUpdate"));
	UFunction* WobbleFinished = Fixture.Factory->FindFunction(TEXT("Wooble_TimelineFinished"));
	if (!TestNotNull(TEXT("Reflected factory wobble update"), WobbleUpdate)
		|| !TestNotNull(TEXT("Reflected factory wobble finish"), WobbleFinished)) return false;
	FFloatProperty* BounceIntensityProperty = FindFProperty<FFloatProperty>(
		ABrickFactory::StaticClass(), TEXT("BounceIntensity"));
	if (!TestNotNull(TEXT("Factory wobble intensity"), BounceIntensityProperty)) return false;
	const float BounceIntensity = BounceIntensityProperty->GetPropertyValue_InContainer(Fixture.Factory);
	if (!TestTrue(TEXT("Native factory has an active wobble intensity"), BounceIntensity > 0.f)) return false;
	Body->SetStaticMesh(FinalMesh);
	float MidPulse = 0.5f;
	// EditorPreview 월드는 액터 초기화 전이므로 ProcessEvent의 에디터 스크립트 게이트를 연다.
	{
		FEditorScriptExecutionGuard ScriptExecutionGuard;
		Fixture.Factory->ProcessEvent(WobbleUpdate, &MidPulse);
	}
	const FVector PulseScale = Body->GetRelativeScale3D();
	TestTrue(*FString::Printf(TEXT("Wobble applies its sine pulse: scale=%s, base=%.3f, intensity=%.3f, actorsInitialized=%d"),
		*PulseScale.ToString(), FactoryScale, BounceIntensity, Fixture.TestWorld->AreActorsInitialized()),
		PulseScale.Equals(FVector(FactoryScale * (1.f + BounceIntensity)), 0.001));
	TestTrue(TEXT("Wobble keeps the final-stage center on the reservation anchor"),
		Body->GetComponentTransform().TransformPosition(BoundsCenter).Equals(
			Fixture.Factory->GetPlacementFootprintCenter() + FVector(0.f, 0.f, BoundsCenter.Z * Body->GetRelativeScale3D().Z), 0.01));
	{
		FEditorScriptExecutionGuard ScriptExecutionGuard;
		Fixture.Factory->ProcessEvent(WobbleFinished, nullptr);
	}
	TestTrue(TEXT("Wobble finish restores the factory body scale"),
		Body->GetRelativeScale3D().Equals(FVector(FactoryScale), 0.001));
	TestTrue(TEXT("Wobble finish restores the reservation anchor"),
		Body->GetRelativeLocation().Equals(FVector(AuthoredOffset.X, AuthoredOffset.Y, 0.f) * (1.f - FactoryScale), 0.01));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFactoryRelocationRotationTest,
	"CGR.Factory.Relocation.RotationKeepsReservedCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRFactoryRelocationRotationTest::RunTest(const FString& Parameters)
{
	FactoryRelocationTests::FFixture Fixture;
	if (!TestNotNull(TEXT("Factory fixture"), Fixture.Factory)) return false;
	FStructProperty* OffsetProperty = FindFProperty<FStructProperty>(ABrickFactory::StaticClass(), TEXT("PlacementFootprintOffset"));
	if (!TestNotNull(TEXT("Reservation anchor"), OffsetProperty)) return false;
	// 두 축 모두 치우친 피벗에서 액터 원점 회전을 예약 중심 회전과 구별한다.
	*OffsetProperty->ContainerPtrToValuePtr<FVector2D>(Fixture.Factory) = FVector2D(-680.f, 180.f);
	Fixture.Factory->OnConstruction(Fixture.Factory->GetActorTransform());
	for (const float InitialYaw : { 0.f, 47.f, 180.f })
	{
		const FTransform Original(FRotator(0.f, InitialYaw, 0.f), FVector(-28265.f, -40901.f, 10900.f));
		Fixture.Factory->SetActorTransform(Original);
		const FVector ReservedCenter = Fixture.Factory->GetPlacementFootprintCenter();
		const FVector2D ReservedHalfExtent = Fixture.Factory->GetPlacementFootprintHalfExtent();
		TestTrue(TEXT("Relocation starts"), Fixture.Factory->BeginRelocation());
		for (int32 Turn = 1; Turn <= 4; ++Turn)
		{
			Fixture.Factory->RotatePlacementQuarterTurn();
			TestTrue(TEXT("Every quarter turn keeps the reserved center"),
				Fixture.Factory->GetPlacementFootprintCenter().Equals(ReservedCenter, 0.01));
			TestTrue(TEXT("Every quarter turn keeps the reserved area"),
				Fixture.Factory->GetPlacementFootprintHalfExtent().Equals(ReservedHalfExtent, 0.01));
			const FQuat ExpectedRotation = FRotator(0.f, InitialYaw + 90.f * Turn, 0.f).Quaternion();
			TestTrue(TEXT("Rotation still advances by ninety degrees"),
				Fixture.Factory->GetActorQuat().Equals(ExpectedRotation, 0.0001));
			TestTrue(TEXT("Rotation preview leaves the saved transform committed"),
				Fixture.Factory->GetFactoryData().PlacementTransform.Equals(Original));
		}
		TestTrue(TEXT("Four turns restore the full actor transform"), Fixture.Factory->GetActorTransform().Equals(Original, 0.01));
		Fixture.Factory->RotatePlacementQuarterTurn();
		Fixture.Factory->FinishRelocation(false);
		TestTrue(TEXT("Cancel after rotating restores the original transform"), Fixture.Factory->GetActorTransform().Equals(Original, 0.01));
	}
	return true;
}
#endif
