#include "Entity/Ambient/AmbientWaterFloat.h"

#include "Components/StaticMeshComponent.h"
#include "Entity/Ambient/AmbientVisibilityComponent.h"

AAmbientWaterFloat::AAmbientWaterFloat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Visibility = CreateDefaultSubobject<UAmbientVisibilityComponent>(TEXT("Visibility"));

	if (UStaticMeshComponent* MeshComponent = GetStaticMeshComponent())
	{
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetCanEverAffectNavigation(false);
	}
}

void AAmbientWaterFloat::BeginPlay()
{
	Super::BeginPlay();

	UStaticMeshComponent* MeshComponent = GetStaticMeshComponent();
	bHasValidMesh = MeshComponent && MeshComponent->GetStaticMesh();
	if (!bHasValidMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientWaterFloat] '%s' has no static mesh; float motion is disabled."), *GetName());
		return;
	}

	InitialWorldTransform = MeshComponent->GetComponentTransform();
	ElapsedActiveSeconds = 0.0;
	Visibility->StartMonitoring(MeshComponent);
}

void AAmbientWaterFloat::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHasValidMesh || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f)
	{
		return;
	}

	UStaticMeshComponent* MeshComponent = GetStaticMeshComponent();
	if (!MeshComponent)
	{
		return;
	}

	ElapsedActiveSeconds += static_cast<double>(DeltaTime);

	const double Phase = FMath::IsFinite(PhaseOffsetRadians) ? static_cast<double>(PhaseOffsetRadians) : 0.0;
	const auto SineOffset = [this, Phase](float Period)
	{
		if (!FMath::IsFinite(Period) || Period <= KINDA_SMALL_NUMBER)
		{
			return 0.0;
		}
		const double AngularSpeed = 2.0 * PI / static_cast<double>(Period);
		return FMath::Sin(ElapsedActiveSeconds * AngularSpeed + Phase) - FMath::Sin(Phase);
	};

	const double Bob = FMath::IsFinite(BobAmplitude) ? FMath::Max(BobAmplitude, 0.f) * SineOffset(BobPeriod) : 0.0;
	const double Pitch = FMath::IsFinite(PitchAmplitude) ? FMath::Max(PitchAmplitude, 0.f) * SineOffset(PitchPeriod) : 0.0;
	const double Roll = FMath::IsFinite(RollAmplitude) ? FMath::Max(RollAmplitude, 0.f) * SineOffset(RollPeriod) : 0.0;

	FVector FloatingLocation = InitialWorldTransform.GetLocation();
	FloatingLocation.Z += Bob;
	const FQuat LocalTilt = FRotator(static_cast<float>(Pitch), 0.f, static_cast<float>(Roll)).Quaternion();
	const FQuat FloatingRotation = InitialWorldTransform.GetRotation() * LocalTilt;
	MeshComponent->SetWorldLocationAndRotation(FloatingLocation, FloatingRotation);
}

void AAmbientWaterFloat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Visibility)
	{
		Visibility->StopMonitoring();
	}

	Super::EndPlay(EndPlayReason);
}
