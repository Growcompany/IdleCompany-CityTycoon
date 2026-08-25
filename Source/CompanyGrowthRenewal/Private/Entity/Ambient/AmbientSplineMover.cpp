// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/AmbientSplineMover.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"

AAmbientSplineMover::AAmbientSplineMover()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Path = CreateDefaultSubobject<USplineComponent>(TEXT("Path"));
	Path->SetupAttachment(Root);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(Root);
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->SetCanEverAffectNavigation(false);
}

void AAmbientSplineMover::BeginPlay()
{
	Super::BeginPlay();

	SplineLength = Path ? Path->GetSplineLength() : 0.f;
	if (SplineLength <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientSplineMover] '%s' Path 스플라인이 비어있어 비활성화합니다."), *GetName());
		SetActorTickEnabled(false);
		return;
	}

	DistanceAlongSpline = FMath::Fmod(FMath::Max(StartDistance, 0.f), SplineLength);
	UpdateTransform(0.f); // 초기 헤딩 즉시 정렬 (스폰 시 휙 도는 것 방지)
}

void AAmbientSplineMover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;
	DistanceAlongSpline += Speed * DeltaTime;

	if (DistanceAlongSpline >= SplineLength)
	{
		if (bLoop)
		{
			DistanceAlongSpline = FMath::Fmod(DistanceAlongSpline, SplineLength);
		}
		else
		{
			DistanceAlongSpline = SplineLength;
			UpdateTransform(DeltaTime);
			SetActorTickEnabled(false);
			return;
		}
	}

	UpdateTransform(DeltaTime);
}

void AAmbientSplineMover::UpdateTransform(float DeltaTime)
{
	if (!Path || !MeshComp) return;

	const FVector Loc = Path->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

	// 룩어헤드 조향: 앞쪽(LookAheadDistance) 지점을 바라봐 급커브에서도 헤딩을 미리 잡음
	float AheadDist = DistanceAlongSpline + LookAheadDistance;
	if (bLoop)
	{
		AheadDist = FMath::Fmod(AheadDist, SplineLength);
	}
	else
	{
		AheadDist = FMath::Min(AheadDist, SplineLength);
	}
	const FVector AheadLoc = Path->GetLocationAtDistanceAlongSpline(AheadDist, ESplineCoordinateSpace::World);

	FVector Dir = AheadLoc - Loc;
	Dir.Z = 0.f; // 배는 수평 유지

	float TargetYaw;
	if (Dir.SizeSquared() > 10.0f) // ~3.16cm 이상만 방향으로 인정 (얕은 커브 오탐 방지)
	{
		TargetYaw = Dir.Rotation().Yaw;
	}
	else
	{
		TargetYaw = Path->GetRotationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World).Yaw;
	}

	// 최대 각속도 제한 yaw 보간 — 큰 배가 헤딩으로 휙 스냅하지 않고 서서히 선회
	if (DeltaTime <= 0.f)
	{
		// 초기화(스폰): 목표 헤딩으로 즉시 정렬
		CurrentYaw = TargetYaw;
		PreviousYaw = TargetYaw;
	}
	else
	{
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
		const float MaxStep = MaxYawRate * DeltaTime;
		CurrentYaw += FMath::Clamp(DeltaYaw, -MaxStep, MaxStep);
	}

	// 여러 바퀴 누적 드리프트 방지 — 정규화 안 하면 |CurrentYaw|>360 에서 FindDeltaAngleDegrees 가 오작동(360 스핀)
	CurrentYaw = FRotator::NormalizeAxis(CurrentYaw);

	// 뱅킹: 실제 선회 각속도에 비례해 안쪽으로 기울임
	const float YawRate = FMath::FindDeltaAngleDegrees(PreviousYaw, CurrentYaw) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
	PreviousYaw = CurrentYaw;
	const float BankRoll = FMath::Clamp(YawRate / MaxYawRate, -1.f, 1.f) * BankAmplitude;

	// 보빙: 상하 + 약한 아이들 롤(뱅킹 위에 가산)
	const float TwoPi = 2.f * PI;
	FVector FinalLoc = Loc;
	FinalLoc.Z += FMath::Sin(ElapsedTime * TwoPi / BobPeriod) * BobAmplitude;
	const float IdleRoll = FMath::Sin(ElapsedTime * TwoPi / RollPeriod) * RollAmplitude;

	// MeshYawOffset 은 스무딩 이후 적용 (스티어링 상태가 아니라 순수 렌더 오프셋)
	const FRotator FinalRot(0.f, CurrentYaw + MeshYawOffset.Yaw, BankRoll + IdleRoll);
	MeshComp->SetWorldLocationAndRotation(FinalLoc, FinalRot);
}
