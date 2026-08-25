#include "Office/OfficeExteriorShell.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Office/OfficeExteriorLayout.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeInterior.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeExteriorShell, Log, All);

namespace
{
const FName OfficeGlassMaterialSlotName(TEXT("OfficeGlass"));
}

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

	FacadeFloorBandISM =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FacadeFloorBandISM"));
	FacadeFloorBandISM->SetupAttachment(RootScene);

	FacadeCornerISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FacadeCornerISM"));
	FacadeCornerISM->SetupAttachment(RootScene);

	SkylineBlocksISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SkylineBlocksISM"));
	SkylineBlocksISM->SetupAttachment(RootScene);

	ConfigureVisualComponent(TowerBody);
	ConfigureVisualComponent(TowerTransferLevel);
	ConfigureVisualComponent(ApronSlab);
	ConfigureVisualComponent(FacadeApronISM);
	ConfigureVisualComponent(FacadeFloorBandISM);
	ConfigureVisualComponent(FacadeCornerISM);
	ConfigureVisualComponent(SkylineBlocksISM);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		UStaticMesh* CubeMesh = CubeMeshFinder.Object;
		TowerBody->SetStaticMesh(CubeMesh);
		TowerTransferLevel->SetStaticMesh(CubeMesh);
		ApronSlab->SetStaticMesh(CubeMesh);
		FacadeFloorBandISM->SetStaticMesh(CubeMesh);
		FacadeCornerISM->SetStaticMesh(CubeMesh);
		SkylineBlocksISM->SetStaticMesh(CubeMesh);
	}
}

void AOfficeExteriorShell::BeginPlay()
{
	Super::BeginPlay();

	ResolveTargetInterior();
	if (!TargetInterior)
	{
		return;
	}

	TargetInterior->OnFootprintChanged.AddUObject(
		this,
		&AOfficeExteriorShell::HandleFootprintChanged);
	SynchronizeToInterior();
}

void AOfficeExteriorShell::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TargetInterior)
	{
		TargetInterior->OnFootprintChanged.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AOfficeExteriorShell::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UWorld* ActorWorld = GetWorld();
	if (ActorWorld && ActorWorld->IsGameWorld())
	{
		return;
	}

	BuildPreviewLayout();
}

bool AOfficeExteriorShell::SynchronizeToInterior()
{
	if (!TargetInterior)
	{
		if (!bWarnedMissingInterior)
		{
			UE_LOG(
				LogOfficeExteriorShell,
				Warning,
				TEXT("Office exterior shell could not synchronize because no unique OfficeInterior was assigned or found."));
			bWarnedMissingInterior = true;
		}
		return false;
	}

	const FIntPoint CurrentTileCount = TargetInterior->GetTileCount();
	const FIntPoint MaxTileCount = TargetInterior->GetMaxTileCount();
	if (!FOfficeExteriorLayoutBuilder::IsCanonicalMaxTileCount(MaxTileCount)
		|| !FOfficeFootprintGeometry::IsTileCountValid(CurrentTileCount, MaxTileCount))
	{
		UE_LOG(
			LogOfficeExteriorShell,
			Warning,
			TEXT("Office exterior shell rejected invalid footprint %dx%d with maximum %dx%d."),
			CurrentTileCount.X,
			CurrentTileCount.Y,
			MaxTileCount.X,
			MaxTileCount.Y);
		return false;
	}

	if (bHasBuiltLayout && CurrentTileCount == LastBuiltTileCount)
	{
		return true;
	}

	FOfficeExteriorLayoutInput Input;
	Input.TileCount = CurrentTileCount;
	Input.MaxTileCount = MaxTileCount;
	Input.CurrentFloorBounds = TargetInterior->GetCurrentFloorBoundsLocal();
	Input.MaxFloorBounds = TargetInterior->GetMaxFloorBoundsLocal();
	Input.TileSizeCm = 400.f;
	Input.StructuralFloorZ = TargetInterior->GetStructuralFloorZLocal();
	Input.TowerHeightCm = TowerHeightCm;

	FOfficeExteriorLayoutResult Layout;
	if (!FOfficeExteriorLayoutBuilder::Build(Input, Layout))
	{
		UE_LOG(
			LogOfficeExteriorShell,
			Warning,
			TEXT("Office exterior shell preserved its previous presentation because the Interior geometry was inconsistent."));
		return false;
	}

	AlignToInterior();
	ApplyLayout(Layout);
	LastBuiltTileCount = CurrentTileCount;
	bHasBuiltLayout = true;
	return true;
}

FIntPoint AOfficeExteriorShell::GetLastBuiltTileCount() const
{
	return LastBuiltTileCount;
}

int32 AOfficeExteriorShell::GetDynamicInstanceCount() const
{
	const int32 FacadeCount = FacadeApronISM ? FacadeApronISM->GetInstanceCount() : 0;
	const int32 BandCount = FacadeFloorBandISM ? FacadeFloorBandISM->GetInstanceCount() : 0;
	const int32 CornerCount = FacadeCornerISM ? FacadeCornerISM->GetInstanceCount() : 0;
	return FacadeCount + BandCount + CornerCount;
}

void AOfficeExteriorShell::ResolveTargetInterior()
{
	if (TargetInterior)
	{
		return;
	}

	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return;
	}

	AOfficeInterior* UniqueInterior = nullptr;
	int32 InteriorCount = 0;
	for (TActorIterator<AOfficeInterior> InteriorIt(ActorWorld); InteriorIt; ++InteriorIt)
	{
		UniqueInterior = *InteriorIt;
		++InteriorCount;
		if (InteriorCount > 1)
		{
			UniqueInterior = nullptr;
			break;
		}
	}

	if (InteriorCount == 1)
	{
		TargetInterior = UniqueInterior;
		return;
	}

	if (!bWarnedMissingInterior)
	{
		UE_LOG(
			LogOfficeExteriorShell,
			Warning,
			TEXT("Office exterior shell expected exactly one OfficeInterior, but found %d."),
			InteriorCount);
		bWarnedMissingInterior = true;
	}
}

void AOfficeExteriorShell::HandleFootprintChanged(const FIntPoint TileCount)
{
	if (!TargetInterior)
	{
		return;
	}

	const FIntPoint MaxTileCount = TargetInterior->GetMaxTileCount();
	if (!FOfficeExteriorLayoutBuilder::IsCanonicalMaxTileCount(MaxTileCount)
		|| !FOfficeFootprintGeometry::IsTileCountValid(TileCount, MaxTileCount)
		|| TileCount != TargetInterior->GetTileCount())
	{
		UE_LOG(
			LogOfficeExteriorShell,
			Warning,
			TEXT("Office exterior shell rejected inconsistent footprint event %dx%d."),
			TileCount.X,
			TileCount.Y);
		return;
	}

	if (bHasBuiltLayout && TileCount == LastBuiltTileCount)
	{
		return;
	}

	SynchronizeToInterior();
}

bool AOfficeExteriorShell::BuildPreviewLayout()
{
	const FIntPoint MaxTileCount = TargetInterior
		? TargetInterior->GetMaxTileCount()
		: FOfficeExteriorLayoutBuilder::GetCanonicalMaxTileCount();
	if (!FOfficeExteriorLayoutBuilder::IsCanonicalMaxTileCount(MaxTileCount)
		|| !FOfficeFootprintGeometry::IsTileCountValid(PreviewTileCount, MaxTileCount))
	{
		UE_LOG(
			LogOfficeExteriorShell,
			Warning,
			TEXT("Office exterior preview rejected footprint %dx%d with maximum %dx%d."),
			PreviewTileCount.X,
			PreviewTileCount.Y,
			MaxTileCount.X,
			MaxTileCount.Y);
		return false;
	}

	FOfficeExteriorLayoutInput Input;
	Input.TileCount = PreviewTileCount;
	Input.MaxTileCount = MaxTileCount;
	Input.CurrentFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(PreviewTileCount, 400.f);
	Input.MaxFloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(MaxTileCount, 400.f);
	Input.TileSizeCm = 400.f;
	Input.StructuralFloorZ = TargetInterior
		? TargetInterior->GetStructuralFloorZLocal()
		: FOfficeFootprintGeometry::StructuralFloorZ;
	Input.TowerHeightCm = TowerHeightCm;

	FOfficeExteriorLayoutResult Layout;
	if (!FOfficeExteriorLayoutBuilder::Build(Input, Layout))
	{
		return false;
	}

	if (TargetInterior)
	{
		AlignToInterior();
	}
	ApplyLayout(Layout);
	LastBuiltTileCount = PreviewTileCount;
	bHasBuiltLayout = true;
	return true;
}

void AOfficeExteriorShell::ApplyLayout(const FOfficeExteriorLayoutResult& Layout)
{
	TowerBody->SetRelativeTransform(Layout.TowerBodyTransform);
	TowerTransferLevel->SetRelativeTransform(Layout.TransferLevelTransform);
	ApronSlab->SetRelativeTransform(Layout.ApronSlabTransform);

	FacadeApronISM->ClearInstances();
	if (FacadeBayMesh)
	{
		FacadeApronISM->SetStaticMesh(FacadeBayMesh);
	}
	else
	{
		FacadeApronISM->SetStaticMesh(nullptr);
		if (!bWarnedMissingFacadeMesh)
		{
			UE_LOG(
				LogOfficeExteriorShell,
				Warning,
				TEXT("Office exterior shell has no FacadeBayMesh; facade instances were cleared without a fallback."));
			bWarnedMissingFacadeMesh = true;
		}
	}

	ApplyConfiguredMaterials();

	if (FacadeBayMesh)
	{
		for (const FTransform& BayTransform : Layout.FacadeBayTransforms)
		{
			FacadeApronISM->AddInstance(BayTransform);
		}
	}

	FacadeFloorBandISM->ClearInstances();
	for (const FTransform& BandTransform : Layout.FloorBandTransforms)
	{
		FacadeFloorBandISM->AddInstance(BandTransform);
	}

	FacadeCornerISM->ClearInstances();
	for (const FTransform& CornerTransform : Layout.CornerAndTrimTransforms)
	{
		FacadeCornerISM->AddInstance(CornerTransform);
	}

	SkylineBlocksISM->ClearInstances();
	for (const FTransform& SkylineTransform : Layout.SkylineTransforms)
	{
		SkylineBlocksISM->AddInstance(SkylineTransform);
	}
}

void AOfficeExteriorShell::ApplyConfiguredMaterials()
{
	TowerBody->SetMaterial(0, StructureMaterial);
	TowerTransferLevel->SetMaterial(0, StructureMaterial);
	ApronSlab->SetMaterial(0, StructureMaterial);
	FacadeFloorBandISM->SetMaterial(0, StructureMaterial);
	FacadeCornerISM->SetMaterial(0, StructureMaterial);
	SkylineBlocksISM->SetMaterial(0, SkylineMaterial);

	FacadeApronISM->EmptyOverrideMaterials();
	if (!FacadeBayMesh || !GlassTrimMaterial)
	{
		return;
	}

	const int32 GlassMaterialIndex = FacadeBayMesh->GetMaterialIndex(OfficeGlassMaterialSlotName);
	if (GlassMaterialIndex == INDEX_NONE)
	{
		if (!bWarnedMissingOfficeGlassSlot)
		{
			UE_LOG(
				LogOfficeExteriorShell,
				Warning,
				TEXT("Office exterior facade mesh '%s' has no OfficeGlass material slot; authored materials were preserved."),
				*FacadeBayMesh->GetName());
			bWarnedMissingOfficeGlassSlot = true;
		}
		return;
	}

	FacadeApronISM->SetMaterial(GlassMaterialIndex, GlassTrimMaterial);
}

void AOfficeExteriorShell::AlignToInterior()
{
	if (!TargetInterior)
	{
		return;
	}

	const FTransform InteriorTransform = TargetInterior->GetActorTransform();
	if (!GetActorTransform().Equals(InteriorTransform))
	{
		SetActorTransform(InteriorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AOfficeExteriorShell::ConfigureVisualComponent(UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
}
