#include "Misc/AutomationTest.h"
#include "GameMode/CityOpeningCamera.h"
#include "Entity/Ambient/CityOpeningSequence.h"
#include "Entity/Ambient/AmbientAircraftFlyby.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOpeningFadeTest, "CGR.Camera.CityOpening.FadeCoversCuts",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCityOpeningFadeTest::RunTest(const FString& Parameters)
{
    const float Cut = CityOpeningCamera::FactoryCut;
    {
        TestEqual(TEXT("cut is fully covered"), CityOpeningCamera::FadeAmount(Cut), 1.f);
        TestEqual(TEXT("lighting gets one 10Hz update before reveal"), CityOpeningCamera::FadeAmount(Cut + .1f), 1.f);
        TestEqual(TEXT("next shot revealed"), CityOpeningCamera::FadeAmount(Cut + .33f), 0.f);
        for (const float Sign : { -1.f, 1.f })
        {
            const float Boundary = Cut + (Sign < 0.f ? -.2f : .32f);
            TestTrue(TEXT("fade boundary continuous"), FMath::Abs(CityOpeningCamera::FadeAmount(Boundary - .00001f)
                - CityOpeningCamera::FadeAmount(Boundary + .00001f)) < .001f);
        }
    }
    for (float Seconds = -1.f; Seconds < 20.f; Seconds += .01f)
    {
        const float Fade = CityOpeningCamera::FadeAmount(Seconds);
        if (Fade < 0.f || Fade > 1.f) { AddError(TEXT("fade outside opacity range")); return false; }
    }
    TestEqual(TEXT("late frame never leaves black screen"), CityOpeningCamera::FadeAmount(20.f), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOpeningAuthoredPathTest, "CGR.Camera.CityOpening.AuthoredPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCityOpeningAuthoredPathTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("world"), TestWorld)) return false;
    ACityOpeningSequence* Sequence = TestWorld->SpawnActor<ACityOpeningSequence>();
    if (!Sequence) { TestWorld->DestroyWorld(false); return false; }
    FMinimalViewInfo Landing;
    Landing.Location = FVector(10000., -12000., 7800.);
    Landing.Rotation = FRotator(-42.f, -90.f, 0.f);
    Landing.FOV = 54.f;
    Landing.OffCenterProjectionOffset = FVector2D(.03, 0.);
    TestFalse(TEXT("missing authoring rejected"), Sequence->Start(Landing));
    UStaticMesh* Mesh = NewObject<UStaticMesh>(Sequence);
    for (UStaticMeshComponent* Component : { Sequence->HelicopterBody.Get(), Sequence->MainRotor.Get(), Sequence->TailRotor.Get() })
        Component->SetStaticMesh(Mesh);
    USplineComponent* Path = Sequence->HelicopterPath;
    {
        Path->ClearSplinePoints(false);
        Path->AddSplinePoint(FVector(10000., -20000., 1000.), ESplineCoordinateSpace::World, false);
        Path->AddSplinePoint(FVector(10000., -18000., 1000.), ESplineCoordinateSpace::World, true);
    }
    Sequence->MainRotor->SetRelativeRotation(FRotator(12.f, 23.f, 34.f));
    const FQuat AuthoredRotor = Sequence->MainRotor->GetRelativeRotation().Quaternion();
    Sequence->DispatchBeginPlay();
    AAmbientAircraftFlyby* VisibleAmbient = TestWorld->SpawnActor<AAmbientAircraftFlyby>();
    AAmbientAircraftFlyby* HiddenAmbient = TestWorld->SpawnActor<AAmbientAircraftFlyby>();
    HiddenAmbient->SetActorHiddenInGame(true);
    TestTrue(TEXT("authored sequence starts"), Sequence->Start(Landing));
    TestTrue(TEXT("ambient hidden during opening"), VisibleAmbient->IsHidden());
    AAmbientAircraftFlyby* StreamedAmbient = TestWorld->SpawnActor<AAmbientAircraftFlyby>();
    Sequence->Start(Landing);
    TestTrue(TEXT("second preparation includes streamed ambient"), StreamedAmbient->IsHidden());
    const float HelicopterSample = CityOpeningCamera::FactoryCut * .5f;
    const FMinimalViewInfo First = Sequence->Evaluate(HelicopterSample);
    TestFalse(TEXT("helicopter visible in helicopter section"), Sequence->HelicopterBody->bHiddenInGame);
    const FTransform FirstHelicopter = Sequence->HelicopterTravel->GetComponentTransform();
    const FQuat FirstRotor = Sequence->MainRotor->GetRelativeRotation().Quaternion();
    Sequence->Evaluate(CityOpeningCamera::FactoryCut);
    TestTrue(TEXT("helicopter hidden on factory cut"), Sequence->HelicopterBody->bHiddenInGame);
    Sequence->Evaluate(CityOpeningCamera::Duration - .1f);
    Sequence->Stop();
    TestFalse(TEXT("visible ambient restored"), VisibleAmbient->IsHidden());
    TestTrue(TEXT("originally hidden ambient stays hidden"), HiddenAmbient->IsHidden());
    TestFalse(TEXT("streamed ambient restored"), StreamedAmbient->IsHidden());
    TestTrue(TEXT("authored rotor rest restored"), Sequence->MainRotor->GetRelativeRotation().Quaternion().Equals(AuthoredRotor));
    Sequence->Start(Landing);
    TestTrue(TEXT("repeat camera pose identical"), Sequence->Evaluate(HelicopterSample).Location.Equals(First.Location));
    TestTrue(TEXT("repeat helicopter pose identical"), Sequence->HelicopterTravel->GetComponentTransform().Equals(FirstHelicopter));
    TestTrue(TEXT("repeat rotor phase identical"), Sequence->MainRotor->GetRelativeRotation().Quaternion().Equals(FirstRotor));
    TestTrue(TEXT("helicopter follows authored forward direction"), Sequence->HelicopterTravel->GetForwardVector().Equals(FVector(0., 1., 0.), .001));
    Sequence->Evaluate(CityOpeningCamera::FactoryCut);
    TestTrue(TEXT("direct seek helicopter camera identical"), Sequence->Evaluate(HelicopterSample).Location.Equals(First.Location));
    TestFalse(TEXT("seeking back reveals helicopter"), Sequence->HelicopterBody->bHiddenInGame);
    for (const float EndTime : { CityOpeningCamera::FactoryCut, CityOpeningCamera::FactoryCut + .4f,
        (CityOpeningCamera::FactoryCut + CityOpeningCamera::Duration) * .5f, CityOpeningCamera::Duration, 37.f })
    {
        const auto View = Sequence->Evaluate(EndTime);
        TestTrue(TEXT("factory hold has no zoom or position drift"), View.Location.Equals(Landing.Location));
        TestTrue(TEXT("factory rotation exact"), View.Rotation.Equals(Landing.Rotation));
        TestEqual(TEXT("factory FOV exact"), View.FOV, Landing.FOV);
        TestTrue(TEXT("factory lens shift exact"), View.OffCenterProjectionOffset.Equals(Landing.OffCenterProjectionOffset));
    }
    Sequence->Stop();
    Sequence->Stop();
    TestTrue(TEXT("stop hides helicopter"), Sequence->HelicopterBody->bHiddenInGame);
    TestWorld->DestroyWorld(false);
    return true;
}
#endif
