#include "Entity/Ambient/CityOpeningSequence.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "EngineUtils.h"
#include "Entity/Ambient/AmbientAircraftFlyby.h"
#include "Entity/Ambient/TrafficManager.h"
#include "GameMode/CityOpeningCamera.h"

ACityOpeningSequence::ACityOpeningSequence()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("StreamingSource"));
    StreamingSource->DisableStreamingSource();
    HelicopterPath = CreateDefaultSubobject<USplineComponent>(TEXT("HelicopterPath"));
    HelicopterTravel = CreateDefaultSubobject<USceneComponent>(TEXT("HelicopterTravel"));
    for (USceneComponent* Component : { static_cast<USceneComponent*>(HelicopterPath.Get()), HelicopterTravel.Get() })
    {
        Component->SetupAttachment(RootComponent);
        Component->SetMobility(EComponentMobility::Movable);
    }
    HelicopterBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelicopterBody"));
    MainRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainRotor"));
    TailRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TailRotor"));
    HelicopterBody->SetupAttachment(HelicopterTravel);
    MainRotor->SetupAttachment(HelicopterBody);
    TailRotor->SetupAttachment(HelicopterBody);
    for (UStaticMeshComponent* Component : { HelicopterBody.Get(), MainRotor.Get(), TailRotor.Get() })
    {
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
    HelicopterBody->SetCastShadow(false);
    MainRotor->SetCastShadow(false);
    TailRotor->SetCastShadow(false);
    HelicopterPath->ClearSplinePoints();
}

void ACityOpeningSequence::BeginPlay()
{
    Super::BeginPlay();
    MainRotorRest = MainRotor->GetRelativeRotation().Quaternion();
    TailRotorRest = TailRotor->GetRelativeRotation().Quaternion();
    HelicopterTravel->SetHiddenInGame(true, true);
}

bool ACityOpeningSequence::IsAuthored() const
{
    return HelicopterPath->GetNumberOfSplinePoints() >= 2 && HelicopterPath->GetSplineLength() > 1.f
        && HelicopterBody->GetStaticMesh() && MainRotor->GetStaticMesh()
        && TailRotor->GetStaticMesh();
}

bool ACityOpeningSequence::Start(const FMinimalViewInfo& LandingView)
{
    if (!IsAuthored()) return false;
    Landing = LandingView;
    StreamingSource->EnableStreamingSource();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if ((It->IsA<ATrafficManager>() || It->IsA<AAmbientAircraftFlyby>())
            && !AmbientActors.Contains(*It))
        {
            AmbientActors.Add(*It);
            AmbientHiddenStates.Add(It->IsHidden());
            It->SetActorHiddenInGame(true);
        }
    }
    bPlaying = true;
    Evaluate(0.f);
    return true;
}

FMinimalViewInfo ACityOpeningSequence::Evaluate(float ElapsedSeconds)
{
    if (!bPlaying) return Landing;
    const float Seconds = FMath::Clamp(ElapsedSeconds, 0.f, CityOpeningCamera::Duration);
    const int32 Section = CityOpeningCamera::Section(Seconds);
    const auto TravelAt = [](const USplineComponent* Path, float Alpha)
    {
        FTransform Pose = Path->GetTransformAtDistanceAlongSpline(
            Path->GetSplineLength() * FMath::Clamp(Alpha, 0.f, 1.f), ESplineCoordinateSpace::World, false);
        Pose.SetScale3D(FVector::OneVector);
        return Pose;
    };
    const FTransform HelicopterPose = TravelAt(HelicopterPath, Seconds / CityOpeningCamera::FactoryCut);
    HelicopterTravel->SetWorldTransform(HelicopterPose);
    MainRotor->SetRelativeRotation(MainRotorRest * FQuat(FVector::UpVector,
        FMath::DegreesToRadians(FMath::Fmod(2280.f * Seconds, 360.f))));
    TailRotor->SetRelativeRotation(TailRotorRest * FQuat(FVector::ForwardVector,
        FMath::DegreesToRadians(FMath::Fmod(4560.f * Seconds, 360.f))));
    HelicopterTravel->SetHiddenInGame(Section != 0, true);
    if (Section == 0)
        return CityOpeningCamera::MountedView(Landing, HelicopterPose, HelicopterCameraOffset, HelicopterLookAtOffset, HelicopterFOV);
    return Landing;
}

void ACityOpeningSequence::Stop()
{
    StreamingSource->DisableStreamingSource();
    HelicopterTravel->SetHiddenInGame(true, true);
    MainRotor->SetRelativeRotation(MainRotorRest);
    TailRotor->SetRelativeRotation(TailRotorRest);
    for (int32 Index = 0; Index < AmbientActors.Num(); ++Index)
    {
        if (AActor* Ambient = AmbientActors[Index].Get()) Ambient->SetActorHiddenInGame(AmbientHiddenStates[Index]);
    }
    AmbientActors.Reset();
    AmbientHiddenStates.Reset();
    bPlaying = false;
}

void ACityOpeningSequence::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Stop();
    Super::EndPlay(EndPlayReason);
}
