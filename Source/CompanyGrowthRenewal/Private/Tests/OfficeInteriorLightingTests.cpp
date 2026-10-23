#include "Misc/AutomationTest.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "GameMode/OfficeGameMode.h"
#include "Office/DecorationActor.h"
#include "Office/DoubleWorkstationActor.h"
#include "Office/OfficeInterior.h"
#include "Office/SingleWorkstationActor.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OfficeInteriorLightingTests
{
struct FTestWorld
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);

    explicit FTestWorld(bool bOffice)
    {
        if (!TestWorld) return;
        if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
        if (bOffice)
        {
            AOfficeGameMode* OfficeMode = TestWorld->SpawnActor<AOfficeGameMode>();
            if (FObjectPropertyBase* Authority = FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), TEXT("AuthorityGameMode")))
            {
                Authority->SetObjectPropertyValue_InContainer(TestWorld, OfficeMode);
            }
        }
    }

    ~FTestWorld()
    {
        if (!TestWorld) return;
        if (GEngine) GEngine->DestroyWorldContext(TestWorld);
        TestWorld->DestroyWorld(false);
    }
};

void CheckMeshes(FAutomationTestBase& Test, AActor* TestActor, bool bInterior, const TCHAR* Context)
{
    if (!Test.TestNotNull(Context, TestActor)) return;
    TInlineComponentArray<UMeshComponent*> Meshes(TestActor);
    Test.TestTrue(FString::Printf(TEXT("%s has visual components"), Context), !Meshes.IsEmpty());
    for (const UMeshComponent* Mesh : Meshes)
    {
        const FLightingChannels& Channels = Mesh->LightingChannels;
        Test.TestTrue(FString::Printf(TEXT("%s / %s uses only channel %d"), Context, *Mesh->GetName(), bInterior ? 1 : 0),
            Channels.bChannel0 == !bInterior && Channels.bChannel1 == bInterior && !Channels.bChannel2);
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeLightingFloorExpansionTest,
    "CGR.Office.InteriorLighting.FloorExpansionAndReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOfficeLightingFloorExpansionTest::RunTest(const FString& Parameters)
{
    using namespace OfficeInteriorLightingTests;
    FTestWorld Fixture(true);
    if (!TestNotNull(TEXT("Office world"), Fixture.TestWorld)) return false;
    AOfficeInterior* Interior = Fixture.TestWorld->SpawnActor<AOfficeInterior>();
    UStaticMesh* Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
    if (!TestNotNull(TEXT("Interior"), Interior) || !TestNotNull(TEXT("Floor mesh"), Cube)) return false;
    Interior->SetFloorTileMesh(Cube);
    Interior->GenerateFloorTiles();
    FArrayProperty* TilesProperty = FindFProperty<FArrayProperty>(AOfficeInterior::StaticClass(), TEXT("FloorTiles"));
    if (!TestNotNull(TEXT("Floor tile array property"), TilesProperty)) return false;
    FScriptArrayHelper Tiles(TilesProperty, TilesProperty->ContainerPtrToValuePtr<void>(Interior));
    const int32 InitialTiles = Tiles.Num();
    TestTrue(TEXT("Initial tiles exist"), InitialTiles > 0);
    CheckMeshes(*this, Interior, true, TEXT("Initial floor and walls"));
    TestTrue(TEXT("Left expansion succeeds"), Interior->ExpandLeft());
    TestTrue(TEXT("Right expansion succeeds"), Interior->ExpandRight());
    TestTrue(TEXT("Expansion added tiles"), Tiles.Num() > InitialTiles);
    TestEqual(TEXT("Expanded tile coverage"), Tiles.Num(), Interior->GetTileCount().X * Interior->GetTileCount().Y);
    CheckMeshes(*this, Interior, true, TEXT("Expanded tiles"));
    Interior->SetFloorTileMesh(Cube);
    CheckMeshes(*this, Interior, true, TEXT("Replaced floor finish"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeLightingFurnitureTest,
    "CGR.Office.InteriorLighting.FurnitureInitialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOfficeLightingFurnitureTest::RunTest(const FString& Parameters)
{
    using namespace OfficeInteriorLightingTests;
    FTestWorld Fixture(true);
    if (!TestNotNull(TEXT("Office world"), Fixture.TestWorld)) return false;
    UStaticMesh* Cube = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))).LoadSynchronous();
    if (!TestNotNull(TEXT("Decoration mesh"), Cube)) return false;
    ADecorationActor* Decoration = Fixture.TestWorld->SpawnActor<ADecorationActor>();
    if (!TestNotNull(TEXT("New decoration"), Decoration)) return false;
    Decoration->SetDecorationMesh(Cube);
    CheckMeshes(*this, Decoration, true, TEXT("New decoration"));
    Decoration->SetActorTransform(FTransform(FRotator(0, 90, 0), FVector(500, 300, 0), FVector(1.2)));
    Decoration->SetDecorationMesh(Cube);
    CheckMeshes(*this, Decoration, true, TEXT("Decoration transform and mesh reassignment"));
    ASingleWorkstationActor* Single = Fixture.TestWorld->SpawnActor<ASingleWorkstationActor>();
    ADoubleWorkstationActor* Double = Fixture.TestWorld->SpawnActor<ADoubleWorkstationActor>();
    if (!TestNotNull(TEXT("Single workstation"), Single) || !TestNotNull(TEXT("Double workstation"), Double)) return false;
    // 공통 초기화는 파생 생성자가 만든 의자/PC/추가 슬롯도 포함한다. DT 설정은 이 테스트의 대상이 아니다.
    Single->AWorkstationActorBase::InitializeWorkstation();
    Double->AWorkstationActorBase::InitializeWorkstation();
    CheckMeshes(*this, Single, true, TEXT("Single workstation and accessories"));
    CheckMeshes(*this, Double, true, TEXT("Double workstation and accessories"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeLightingWorkerIsolationTest,
    "CGR.Office.InteriorLighting.WorkerCosmeticsAndCaptureIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOfficeLightingWorkerIsolationTest::RunTest(const FString& Parameters)
{
    using namespace OfficeInteriorLightingTests;
    FTestWorld Fixture(true);
    if (!TestNotNull(TEXT("Office world"), Fixture.TestWorld)) return false;
    AFunnyOfficeworker* Worker = Fixture.TestWorld->SpawnActor<AFunnyOfficeworker>();
    if (!TestNotNull(TEXT("Office worker"), Worker)) return false;
    CheckMeshes(*this, Worker, true, TEXT("Spawned worker and equipment slots"));
    auto ApplyAppearance = [Worker](bool bGacha)
    {
        Worker->ApplyWorkerCosmetics(EEmployeeDepartment::Development, EEmployeeRank::Intern,
            EGachaTier::Normal, 7, EEmployeeGender::Male, bGacha);
    };
    ApplyAppearance(false);
    CheckMeshes(*this, Worker, true, TEXT("Appearance refresh"));
    ApplyAppearance(true);
    CheckMeshes(*this, Worker, false, TEXT("Gacha reveal keeps capture lighting"));
    Worker->bIsPortraitMode = true;
    ApplyAppearance(false);
    CheckMeshes(*this, Worker, false, TEXT("Portrait keeps capture lighting"));
    Worker->bIsPortraitMode = false;
    ApplyAppearance(false);
    CheckMeshes(*this, Worker, true, TEXT("Normal office appearance restored"));

    FTestWorld OtherFixture(false);
    if (!TestNotNull(TEXT("Other map world"), OtherFixture.TestWorld)) return false;
    AFunnyOfficeworker* OtherWorker = OtherFixture.TestWorld->SpawnActor<AFunnyOfficeworker>();
    CheckMeshes(*this, OtherWorker, false, TEXT("Other map worker unchanged"));
    return true;
}
#endif
