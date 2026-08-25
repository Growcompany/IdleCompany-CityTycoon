// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/AmbientVehicleMover.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AAmbientVehicleMover::AAmbientVehicleMover()
{
	// 차량 프리셋: 출렁임 없음, 코너 약한 뱅킹, 도시 스케일 속도.
	BobAmplitude = 0.f;
	RollAmplitude = 0.f;
	BankAmplitude = 1.5f;
	Speed = 1200.f;

	// 차 메시 forward축이 +Y → 진행방향(+X)에 정렬되도록 -90 보정(차가 뒤로 가면 +90 으로).
	MeshYawOffset = FRotator(0.f, -90.f, 0.f);

	// 엔진 기본 Plane(100cm, normal +Z)을 라이트카드 메시로 사용.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));

	FrontLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontLight"));
	FrontLight->SetupAttachment(MeshComp);
	FrontLight->SetMobility(EComponentMobility::Movable);
	FrontLight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrontLight->SetGenerateOverlapEvents(false);
	FrontLight->SetCanEverAffectNavigation(false);
	FrontLight->SetCastShadow(false);
	FrontLight->SetRelativeRotation(FRotator(0.f, 0.f, -45.f));   // Plane normal +Z → 전방(+Y)+위 45도(상단 카메라 가시성)

	RearLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearLight"));
	RearLight->SetupAttachment(MeshComp);
	RearLight->SetMobility(EComponentMobility::Movable);
	RearLight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RearLight->SetGenerateOverlapEvents(false);
	RearLight->SetCanEverAffectNavigation(false);
	RearLight->SetCastShadow(false);
	RearLight->SetRelativeRotation(FRotator(0.f, 0.f, 45.f));     // 후방(-Y)+위 45도

	if (PlaneMesh.Succeeded())
	{
		FrontLight->SetStaticMesh(PlaneMesh.Object);
		RearLight->SetStaticMesh(PlaneMesh.Object);
	}

	// 라이트 머티리얼 기본값(에셋 존재 시) — 에디터/BP 에서 override 가능. 미존재 시 null 로 두고 에디터에서 지정.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeadMat(
		TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Materials/MI_CarLight_Head.MI_CarLight_Head"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TailMat(
		TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Materials/MI_CarLight_Tail.MI_CarLight_Tail"));
	if (HeadMat.Succeeded()) { HeadLightMaterial = HeadMat.Object; }
	if (TailMat.Succeeded()) { TailLightMaterial = TailMat.Object; }
}

void AAmbientVehicleMover::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLightCardSetup();
}

void AAmbientVehicleMover::BeginPlay()
{
	Super::BeginPlay();
	ApplyLightCardSetup();
}

void AAmbientVehicleMover::ApplyLightCardSetup()
{
	if (FrontLight)
	{
		FrontLight->SetRelativeLocation(FrontLightOffset);
		FrontLight->SetRelativeScale3D(FVector(LightCardScale));
		if (HeadLightMaterial) { FrontLight->SetMaterial(0, HeadLightMaterial); }
	}
	if (RearLight)
	{
		RearLight->SetRelativeLocation(RearLightOffset);
		RearLight->SetRelativeScale3D(FVector(LightCardScale));
		if (TailLightMaterial) { RearLight->SetMaterial(0, TailLightMaterial); }
	}
}
