// Fill out your copyright notice in the Description page of Project Settings.

#include "Office/DecorationActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameMode/OfficeGameMode.h"
#include "Engine/World.h"

ADecorationActor::ADecorationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 루트 컴포넌트 생성
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	// 메시 컴포넌트 생성 (NavMesh 차단용)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootScene);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_WorldStatic);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);  // 클릭 감지
	MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore); // 배치 모드 전용 채널 (미리보기만 반응)
	MeshComponent->SetCanEverAffectNavigation(true);

	// BoxComponent 생성 (배치 시 Overlap 충돌 감지용)
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootScene);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	BoxComponent->SetGenerateOverlapEvents(true);
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
}

void ADecorationActor::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() && GetWorld()->GetAuthGameMode<AOfficeGameMode>())
	{
		TInlineComponentArray<UStaticMeshComponent*> DecorationMeshes(this);
		for (UStaticMeshComponent* DecorationMesh : DecorationMeshes)
		{
			DecorationMesh->SetLightingChannels(false, true, false);
		}
	}
}

void ADecorationActor::SetDecorationMesh(UStaticMesh* NewMesh)
{
	if (MeshComponent && NewMesh)
	{
		MeshComponent->SetStaticMesh(NewMesh);
		if (IsFloorCovering())
		{
			// 클릭/재배치 채널은 유지하고 물리 이동 및 내비게이션에서는 제외한다.
			const ECollisionResponse PlacementResponse = MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel2);
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
			MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
			MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, PlacementResponse);
			MeshComponent->SetCanEverAffectNavigation(false);
			BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BoxComponent->SetGenerateOverlapEvents(false);
			BoxComponent->SetCanEverAffectNavigation(false);
		}
		if (GetWorld() && GetWorld()->GetAuthGameMode<AOfficeGameMode>())
		{
			MeshComponent->SetLightingChannels(false, true, false);
		}

		// BoxComponent 크기 재계산
		RecalcBoxExtent();

		UE_LOG(LogTemp, Warning, TEXT("[DecorationActor] SetDecorationMesh - BoxExtent updated for %s"), *GetName());
	}
}

void ADecorationActor::RecalcBoxExtent()
{
	if (!MeshComponent || !BoxComponent) return;

	// 메시 컴포넌트의 로컬 바운드만 사용 (GetActorBounds는 BoxComponent도 포함해서 재귀적으로 커짐)
	FBoxSphereBounds MeshBounds = MeshComponent->CalcLocalBounds();
	FVector BoundExtent = MeshBounds.BoxExtent;

	// BoxComponent 크기 및 위치 설정
	BoxComponent->SetBoxExtent(BoundExtent);
	BoxComponent->SetRelativeLocation(MeshBounds.Origin);

	// Overlap 정보 즉시 업데이트 (스폰 직후 충돌 감지용)
	BoxComponent->UpdateOverlaps();
}

bool ADecorationActor::CanPlaceOnSurface(EDecorationSurface Surface) const
{
	return AllowedSurface == Surface;
}

bool ADecorationActor::IsUnlocked(int32 CurrentLevel) const
{
	return CurrentLevel >= UnlockLevel;
}

bool ADecorationActor::BlocksPlacementWith(const AActor* OtherActor) const
{
	const ADecorationActor* OtherDecoration = Cast<ADecorationActor>(OtherActor);
	return IsFloorCovering() == (OtherDecoration && OtherDecoration->IsFloorCovering());
}

FBox2D ADecorationActor::GetFloorFootprint(const FTransform& FloorTransform) const
{
	FBox2D Result(ForceInit);
	if (!BoxComponent) return Result;
	const FVector Extent = BoxComponent->GetUnscaledBoxExtent();
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Offset(Extent.X * ((Corner & 1) ? 1 : -1),
			Extent.Y * ((Corner & 2) ? 1 : -1), Extent.Z * ((Corner & 4) ? 1 : -1));
		const FVector Position = FloorTransform.InverseTransformPosition(BoxComponent->GetComponentTransform().TransformPosition(Offset));
		Result += FVector2D(Position.X, Position.Y);
	}
	return Result;
}
