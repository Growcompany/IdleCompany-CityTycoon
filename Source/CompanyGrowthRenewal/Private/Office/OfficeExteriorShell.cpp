#include "Office/OfficeExteriorShell.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CGGameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeNeighborhoodSnapshot.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeExteriorShell, Log, All);

AOfficeExteriorShell::AOfficeExteriorShell()
{
	PrimaryActorTick.bCanEverTick = false;
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);
	RootScene->SetMobility(EComponentMobility::Movable);

	TowerBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerBody"));
	TowerBody->SetupAttachment(RootScene);
	TowerTransferLevel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerTransferLevel"));
	TowerTransferLevel->SetupAttachment(RootScene);
	ApronSlab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApronSlab"));
	ApronSlab->SetupAttachment(RootScene);
	FacadeApronISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FacadeApronISM"));
	FacadeApronISM->SetupAttachment(RootScene);
	FacadeFloorBandISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FacadeFloorBandISM"));
	FacadeFloorBandISM->SetupAttachment(RootScene);
	FacadeCornerISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FacadeCornerISM"));
	FacadeCornerISM->SetupAttachment(RootScene);
	SkylineClimbBandISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SkylineClimbBandISM"));
	SkylineClimbBandISM->SetupAttachment(RootScene);

	ConfigureVisualComponent(TowerBody);
	ConfigureVisualComponent(TowerTransferLevel);
	ConfigureVisualComponent(ApronSlab);
	ConfigureVisualComponent(FacadeApronISM);
	ConfigureVisualComponent(FacadeFloorBandISM);
	ConfigureVisualComponent(FacadeCornerISM);
	ConfigureVisualComponent(SkylineClimbBandISM);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		UStaticMesh* CubeMesh = CubeMeshFinder.Object;
		ApronSlab->SetStaticMesh(CubeMesh);
		FacadeFloorBandISM->SetStaticMesh(CubeMesh);
		FacadeCornerISM->SetStaticMesh(CubeMesh);
		SkylineClimbBandISM->SetStaticMesh(CubeMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TowerMeshFinder(
		TEXT("/Game/CompanyGrowth/Office/Exterior/Meshes/SM_OfficeTowerOpenSides.SM_OfficeTowerOpenSides"));
	if (TowerMeshFinder.Succeeded())
	{
		TowerBody->SetStaticMesh(TowerMeshFinder.Object);
		TowerTransferLevel->SetStaticMesh(TowerMeshFinder.Object);
	}
	else
	{
		UE_LOG(LogOfficeExteriorShell, Error, TEXT("Office tower open-sides mesh is missing."));
	}
}

void AOfficeExteriorShell::BeginPlay()
{
	Super::BeginPlay();
	ResolveTargetInterior();
	if (!TargetInterior) { return; }
	TargetInterior->OnFootprintChanged.AddUObject(this, &AOfficeExteriorShell::HandleFootprintChanged);
	SynchronizeToInterior();
}

void AOfficeExteriorShell::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TargetInterior) { TargetInterior->OnFootprintChanged.RemoveAll(this); }
	Super::EndPlay(EndPlayReason);
}

void AOfficeExteriorShell::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GetWorld() && !GetWorld()->IsGameWorld()) { BuildPreviewLayout(); }
}

bool AOfficeExteriorShell::BuildLayoutFromInterior(FOfficeExteriorLayoutResult& OutLayout)
{
	if (!TargetInterior) { return false; }
	const FIntPoint TileCount = TargetInterior->GetTileCount();
	const FIntPoint MaxTileCount = TargetInterior->GetMaxTileCount();
	if (!FOfficeExteriorLayoutBuilder::IsCanonicalMaxTileCount(MaxTileCount)
		|| !FOfficeFootprintGeometry::IsTileCountValid(TileCount, MaxTileCount)) { return false; }

	FOfficeExteriorLayoutInput Input;
	Input.TileCount = TileCount;
	Input.MaxTileCount = MaxTileCount;
	Input.CurrentFloorBounds = TargetInterior->GetCurrentFloorBoundsLocal();
	Input.ExteriorFootprintBounds = TargetInterior->GetExteriorBoundsLocal();
	Input.MaxFloorBounds = TargetInterior->GetMaxFloorBoundsLocal();
	Input.TileSizeCm = 400.f;
	Input.StructuralFloorZ = TargetInterior->GetStructuralFloorZLocal();
	Input.TowerHeightCm = TowerHeightCm;

	float HeightWorldCm = PreviewOfficeHeightCm;
	if (const UWorld* ActorWorld = GetWorld(); ActorWorld && ActorWorld->IsGameWorld())
	{
		if (const UCGGameInstance* GameInstance = Cast<UCGGameInstance>(ActorWorld->GetGameInstance()))
		{
			const FOfficeNeighborhoodSnapshot& Snapshot = GameInstance->GetOfficeNeighborhoodSnapshot();
			if (Snapshot.bValid) { HeightWorldCm = Snapshot.OfficeHeightCm; }
		}
	}
	const float PresentationScaleZ = FMath::Max(FMath::Abs(TargetInterior->GetActorScale3D().Z), UE_SMALL_NUMBER);
	Input.OfficeHeightCm = HeightWorldCm / PresentationScaleZ + SkylineClimbLiftCm;
	if (!FOfficeExteriorLayoutBuilder::Build(Input, OutLayout))
	{
		UE_LOG(LogOfficeExteriorShell, Warning, TEXT("Office exterior rejected inconsistent footprint or height."));
		return false;
	}
	LastBuiltTileCount = TileCount;
	return true;
}

bool AOfficeExteriorShell::SynchronizeToInterior()
{
	ResolveTargetInterior();
	FOfficeExteriorLayoutResult Layout;
	if (!BuildLayoutFromInterior(Layout)) { return false; }
	AlignToInterior();
	ApplyLayout(Layout);
	LastClimbLayout = Layout;
	RefreshSkylineClimbBands();
	bHasBuiltLayout = true;
	return true;
}

void AOfficeExteriorShell::SetSkylineClimbLift(float LiftCm)
{
	const float ClampedLift = FMath::Max(0.f, LiftCm);
	if (!FMath::IsFinite(ClampedLift) || FMath::IsNearlyEqual(ClampedLift, SkylineClimbLiftCm)) { return; }
	SkylineClimbLiftCm = ClampedLift;
	SynchronizeToInterior();
}

int32 AOfficeExteriorShell::GetDynamicInstanceCount() const
{
	return (FacadeApronISM ? FacadeApronISM->GetInstanceCount() : 0)
		+ (FacadeFloorBandISM ? FacadeFloorBandISM->GetInstanceCount() : 0)
		+ (FacadeCornerISM ? FacadeCornerISM->GetInstanceCount() : 0);
}

void AOfficeExteriorShell::ResolveTargetInterior()
{
	if (TargetInterior || !GetWorld()) { return; }
	AOfficeInterior* UniqueInterior = nullptr;
	int32 InteriorCount = 0;
	for (TActorIterator<AOfficeInterior> InteriorIt(GetWorld()); InteriorIt; ++InteriorIt)
	{
		UniqueInterior = *InteriorIt;
		if (++InteriorCount > 1) { UniqueInterior = nullptr; break; }
	}
	if (InteriorCount == 1) { TargetInterior = UniqueInterior; }
	else if (!bWarnedMissingInterior)
	{
		UE_LOG(LogOfficeExteriorShell, Warning, TEXT("Office exterior expected one OfficeInterior, found %d."), InteriorCount);
		bWarnedMissingInterior = true;
	}
}

void AOfficeExteriorShell::HandleFootprintChanged(FIntPoint TileCount)
{
	if (TargetInterior && TileCount == TargetInterior->GetTileCount()
		&& (!bHasBuiltLayout || TileCount != LastBuiltTileCount)) { SynchronizeToInterior(); }
}

bool AOfficeExteriorShell::BuildPreviewLayout()
{
	const FIntPoint TileCount = TargetInterior ? TargetInterior->GetTileCount() : PreviewTileCount;
	const FIntPoint MaxTileCount = TargetInterior ? TargetInterior->GetMaxTileCount()
		: FOfficeExteriorLayoutBuilder::GetCanonicalMaxTileCount();
	FOfficeExteriorLayoutInput Input;
	Input.TileCount = TileCount;
	Input.MaxTileCount = MaxTileCount;
	Input.CurrentFloorBounds = TargetInterior ? TargetInterior->GetCurrentFloorBoundsLocal()
		: FOfficeFootprintGeometry::MakeFloorBounds(TileCount, 400.f);
	Input.MaxFloorBounds = TargetInterior ? TargetInterior->GetMaxFloorBoundsLocal()
		: FOfficeFootprintGeometry::MakeFloorBounds(MaxTileCount, 400.f);
	if (TargetInterior) { Input.ExteriorFootprintBounds = TargetInterior->GetExteriorBoundsLocal(); }
	Input.TileSizeCm = 400.f;
	Input.StructuralFloorZ = TargetInterior ? TargetInterior->GetStructuralFloorZLocal()
		: FOfficeFootprintGeometry::StructuralFloorZ;
	Input.TowerHeightCm = TowerHeightCm;
	Input.OfficeHeightCm = PreviewOfficeHeightCm + SkylineClimbLiftCm;
	FOfficeExteriorLayoutResult Layout;
	if (!FOfficeExteriorLayoutBuilder::Build(Input, Layout)) { return false; }
	if (TargetInterior) { AlignToInterior(); }
	ApplyLayout(Layout);
	LastClimbLayout = Layout;
	LastBuiltTileCount = TileCount;
	RefreshSkylineClimbBands();
	bHasBuiltLayout = true;
	return true;
}

void AOfficeExteriorShell::ApplyLayout(const FOfficeExteriorLayoutResult& Layout)
{
	const float StructuralFloorZ = TargetInterior ? TargetInterior->GetStructuralFloorZLocal()
		: FOfficeFootprintGeometry::StructuralFloorZ;
	const float FloorWorldZ = static_cast<float>(GetActorTransform().TransformPosition(
		FVector(0.f, 0.f, StructuralFloorZ)).Z);
	const float PresentationScaleZ = FMath::Max(FMath::Abs(GetActorScale3D().Z), UE_SMALL_NUMBER);
	// 모든 층과 마감이 같은 바닥 기준을 공유하여 높이 페이드가 이어진다.
	UPrimitiveComponent* const VisualComponents[] = {
		TowerBody.Get(), TowerTransferLevel.Get(), ApronSlab.Get(), FacadeApronISM.Get(),
		FacadeFloorBandISM.Get(), FacadeCornerISM.Get(), SkylineClimbBandISM.Get()
	};
	for (UPrimitiveComponent* VisualComponent : VisualComponents)
	{
		VisualComponent->SetCustomPrimitiveDataFloat(0, FloorWorldZ);
		VisualComponent->SetCustomPrimitiveDataFloat(1, PresentationScaleZ);
		VisualComponent->SetTranslucentSortPriority(1);
	}

	TowerBody->SetRelativeTransform(Layout.TowerBodyTransform);
	TowerBody->SetVisibility(Layout.bShowTowerBody, true);
	TowerTransferLevel->SetRelativeTransform(Layout.TransferLevelTransform);
	TowerTransferLevel->SetVisibility(Layout.bShowTransferLevel, true);
	ApronSlab->SetRelativeTransform(Layout.ApronSlabTransform);

	FacadeApronISM->ClearInstances();
	FacadeApronISM->SetStaticMesh(FacadeBayMesh.Get());
	FacadeApronISM->SetNumCustomDataFloats(1);
	if (!FacadeBayMesh && !bWarnedMissingFacadeMesh)
	{
		UE_LOG(LogOfficeExteriorShell, Warning, TEXT("Office facade bay mesh is missing."));
		bWarnedMissingFacadeMesh = true;
	}
	ApplyConfiguredMaterials();
	if (FacadeBayMesh)
	{
		const int32 FloorCount = Layout.FloorBandTransforms.Num() / 2;
		const int32 BaysPerFloor = FloorCount > 0 ? Layout.FacadeBayTransforms.Num() / FloorCount : 0;
		for (int32 Index = 0; Index < Layout.FacadeBayTransforms.Num(); ++Index)
		{
			const int32 InstanceIndex = FacadeApronISM->AddInstance(Layout.FacadeBayTransforms[Index]);
			const int32 FloorIndex = BaysPerFloor > 0 ? Index / BaysPerFloor + 1 : 1;
			FacadeApronISM->SetCustomDataValue(InstanceIndex, 0, FloorIndex % 3 == 0 ? 4.f : 3.f,
				Index == Layout.FacadeBayTransforms.Num() - 1);
		}
	}

	FacadeFloorBandISM->ClearInstances();
	FacadeFloorBandISM->SetNumCustomDataFloats(1);
	for (int32 Index = 0; Index < Layout.FloorBandTransforms.Num(); ++Index)
	{
		const int32 InstanceIndex = FacadeFloorBandISM->AddInstance(Layout.FloorBandTransforms[Index]);
		FacadeFloorBandISM->SetCustomDataValue(InstanceIndex, 0, (Index / 2 + 1) % 3 == 0 ? 4.f : 3.f,
			Index == Layout.FloorBandTransforms.Num() - 1);
	}
	FacadeCornerISM->ClearInstances();
	FacadeCornerISM->SetNumCustomDataFloats(1);
	for (int32 Index = 0; Index < Layout.CornerAndTrimTransforms.Num(); ++Index)
	{
		const int32 InstanceIndex = FacadeCornerISM->AddInstance(Layout.CornerAndTrimTransforms[Index]);
		FacadeCornerISM->SetCustomDataValue(InstanceIndex, 0, Index < 3 ? 1.f : 2.f,
			Index == Layout.CornerAndTrimTransforms.Num() - 1);
	}
}

void AOfficeExteriorShell::RefreshSkylineClimbBands()
{
	TArray<FTransform> Bands;
	FSkylineClimbBandLayout::Build(LastClimbLayout.ExteriorFootprintBounds,
		LastClimbLayout.TransferBottomZ, LastClimbLayout.GroundZ,
		FMath::RoundToInt(SkylineClimbLiftCm / 400.f), Bands);
	SkylineClimbBandISM->ClearInstances();
	for (const FTransform& Band : Bands) { SkylineClimbBandISM->AddInstance(Band); }
}

void AOfficeExteriorShell::ApplyConfiguredMaterials()
{
	UMaterialInterface* EffectiveStructure = StructureMaterial.Get();
	TowerBody->SetMaterial(0, TowerContinuationMaterial ? TowerContinuationMaterial.Get() : EffectiveStructure);
	TowerTransferLevel->SetMaterial(0, RecessMaterial ? RecessMaterial.Get() : EffectiveStructure);
	ApronSlab->SetMaterial(0, RoofDeckMaterial ? RoofDeckMaterial.Get()
		: (RecessMaterial ? RecessMaterial.Get() : EffectiveStructure));
	FacadeFloorBandISM->SetMaterial(0, EffectiveStructure);
	FacadeCornerISM->SetMaterial(0, EffectiveStructure);
	SkylineClimbBandISM->SetMaterial(0, EffectiveStructure);
	FacadeApronISM->EmptyOverrideMaterials();
	if (!FacadeBayMesh) { return; }
	const TArray<FStaticMaterial>& Slots = FacadeBayMesh->GetStaticMaterials();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		UMaterialInterface* Material = Slots[Index].MaterialInterface.Get();
		if (Slots[Index].MaterialSlotName == TEXT("OfficeGlass") && GlassTrimMaterial)
		{
			Material = GlassTrimMaterial.Get();
		}
		FacadeApronISM->SetMaterial(Index, Material);
	}
}

void AOfficeExteriorShell::AlignToInterior()
{
	if (TargetInterior && !GetActorTransform().Equals(TargetInterior->GetActorTransform()))
	{
		SetActorTransform(TargetInterior->GetActorTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AOfficeExteriorShell::ConfigureVisualComponent(UPrimitiveComponent* Component)
{
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->PrimaryComponentTick.bCanEverTick = false;
	Component->SetComponentTickEnabled(false);
}
