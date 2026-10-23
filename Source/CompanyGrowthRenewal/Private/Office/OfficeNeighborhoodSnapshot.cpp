#include "Office/OfficeNeighborhoodSnapshot.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/Ambient/StreetLampManager.h"
#include "Entity/Ambient/TrafficManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeComponent.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/CityCompanyDirector.h"
#include "Manager/EntityManager.h"
#include "Manager/SaveLoadManager.h"
#include "Materials/MaterialInstance.h"
#include "Office/OfficeDistantBackdrop.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Office/OfficeInterior.h"
#include "Player/OfficeCameraPawn.h"
#include "TimeCycle/TimeCycleSky.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeNeighborhoodSnapshot, Log, All);

namespace OfficeNeighborhoodSnapshot
{
TAutoConsoleVariable<float> CityPhotoDistanceScale(
	TEXT("cg.Office.CityPhotoDistanceScale"), 0.5f,
	TEXT("Horizontal city-photo distance from the office on next entry (0..1). 1=entry camera, 0=office center. Height is unchanged."), ECVF_Default);

bool IsPersistentAsset(const UObject* Object)
{
	return IsValid(Object) && Object->IsAsset() && !Object->HasAnyFlags(RF_Transient)
		&& !Object->GetOutermost()->ContainsMap();
}

bool IsCityWaterPlane(const UStaticMeshComponent* Component)
{
	if (!Component || !Component->GetStaticMesh()
		|| Component->GetStaticMesh()->GetPathName() != TEXT("/Engine/BasicShapes/Plane.Plane")) { return false; }
	for (int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
	{
		const UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
		while (Material)
		{
			if (Material->GetPathName().StartsWith(TEXT("/Game/CompanyGrowth/Environment/Materials/Water/"))) { return true; }
			const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
			Material = Instance ? Instance->Parent.Get() : nullptr;
		}
	}
	return false;
}

bool ResolveManagedFootprint(ABuildingBaseActor* Building, float HeightCm, FTransform& OutAnchor, FVector2D& OutSize)
{
	UStaticMeshComponent* FootprintComponent = Cast<UStaticMeshComponent>(Building->GetMainMeshComponent());
	TInlineComponentArray<UInstancedStaticMeshComponent*> Components;
	Building->GetComponents(Components);
	for (UInstancedStaticMeshComponent* Component : Components)
	{
		if (Component->GetFName() == TEXT("Body_Module") && Component->GetStaticMesh() && Component->GetInstanceCount() > 0)
		{
			FootprintComponent = Component;
			break;
		}
	}
	if (!FootprintComponent || !FootprintComponent->GetStaticMesh()) { return false; }
	const UInstancedStaticMeshComponent* BodyISM = Cast<UInstancedStaticMeshComponent>(FootprintComponent);
	FTransform FirstTransform = FootprintComponent->GetComponentTransform();
	if (BodyISM && !BodyISM->GetInstanceTransform(0, FirstTransform, true)) { return false; }
	if (!FirstTransform.IsValid()) { return false; }
	const FTransform BuildingAxis(FRotator(0, FirstTransform.Rotator().Yaw, 0), Building->GetActorLocation());
	FBox BodyBounds(ForceInit);
	const int32 InstanceCount = BodyISM ? BodyISM->GetInstanceCount() : 1;
	for (int32 Index = 0; Index < InstanceCount; ++Index)
	{
		FTransform InstanceTransform = FirstTransform;
		if (BodyISM && !BodyISM->GetInstanceTransform(Index, InstanceTransform, true)) { return false; }
		if (!InstanceTransform.IsValid()) { return false; }
		BodyBounds += FootprintComponent->GetStaticMesh()->GetBoundingBox().TransformBy(InstanceTransform.GetRelativeTransform(BuildingAxis));
	}
	if (!BodyBounds.IsValid || BodyBounds.Min.ContainsNaN() || BodyBounds.Max.ContainsNaN()) { return false; }
	OutSize = FVector2D(BodyBounds.GetSize());
	if (OutSize.X <= 0 || OutSize.Y <= 0) { return false; }
	FVector CenterWorld = BuildingAxis.TransformPosition(BodyBounds.GetCenter());
	CenterWorld.Z = Building->GetActorLocation().Z + HeightCm;
	// 실내 -X/+Y 벽이 도시 건축축의 +X/-Y에 오도록 공통 전망 좌표계를 반 바퀴 돌린다.
	OutAnchor = FTransform(BuildingAxis.GetRotation() * FRotator(0.f, 180.f, 0.f).Quaternion(), CenterWorld);
	return OutAnchor.IsValid();
}

bool IsFacadeComponent(const AActor* BuildingActor, const UStaticMeshComponent* Component)
{
	if (!IsValid(Component) || !Component->IsVisible() || Component->bHiddenInGame
		|| !Component->bRenderInMainPass || !Component->GetStaticMesh()) { return false; }
	if (const ABuildingBaseActor* OwnedBuilding = Cast<ABuildingBaseActor>(BuildingActor))
	{
		// 건물의 선택/고스트/비콘 메시를 도시 건축물에 굽지 않는다.
		const FName ComponentName = Component->GetFName();
		return Component == OwnedBuilding->GetMainMeshComponent() || ComponentName == TEXT("Body_Module")
			|| ComponentName == TEXT("Top_Module") || ComponentName == TEXT("Top_Empty_Module");
	}
	return true;
}

}

FOfficeNeighborhoodSnapshot FOfficeNeighborhoodSnapshotBuilder::Capture(ABuildingBaseActor* ManagedBuilding)
{
	FOfficeNeighborhoodSnapshot Result;
	if (!IsValid(ManagedBuilding) || ManagedBuilding->IsActorBeingDestroyed() || !ManagedBuilding->GetWorld()) { return Result; }
	UWorld* SourceWorld = ManagedBuilding->GetWorld();
	if (SourceWorld->IsGameWorld())
	{
		// 원본 도로와 건물 등록이 끝난 뒤 입장 사진을 한 번 촬영한다.
		SourceWorld->BlockTillLevelStreamingCompleted();
		if (!IsValid(ManagedBuilding) || ManagedBuilding->IsActorBeingDestroyed()
			|| ManagedBuilding->GetWorld() != SourceWorld) { return Result; }
	}
	float OfficeFloorHeightCm = 0.f;
	if (!ManagedBuilding->TryGetOfficeFloorHeight(OfficeFloorHeightCm)) { return Result; }
	const float HeightCm = OfficeFloorHeightCm * FMath::Abs(ManagedBuilding->GetActorScale3D().Z);
	FVector2D FootprintSize;
	if (!FMath::IsFinite(HeightCm) || HeightCm <= 0.f
		|| !OfficeNeighborhoodSnapshot::ResolveManagedFootprint(ManagedBuilding, HeightCm,
			Result.SourceAnchorWorldTransform, FootprintSize)) { return Result; }
	Result.bValid = true;
	Result.ManagedBuildingIndex = ManagedBuilding->GetBuildingIndex();
	Result.OfficeHeightCm = HeightCm;

	TSet<AActor*> OwnedBuildings;
	OwnedBuildings.Add(ManagedBuilding);
	if (UEntityManager* EntityManager = SourceWorld->GetSubsystem<UEntityManager>())
	{
		for (ABuildingBaseActor* Building : EntityManager->GetBuildings())
		{
			if (IsValid(Building)) { OwnedBuildings.Add(Building); }
		}
	}
	UCityAcquisitionManager* Acquisition = SourceWorld->GetSubsystem<UCityAcquisitionManager>();
	if (UCityCompanyDirector* Director = SourceWorld->GetSubsystem<UCityCompanyDirector>())
	{
		for (const FCitySkylineEntry& Entry : Director->GetSkylineEntries())
		{
			if (Acquisition && Acquisition->GetState(Entry.BuildingKey) == EAcqState::Cleared) { continue; }
			if (AActor* Building = Entry.Building.Get()) { OwnedBuildings.Add(Building); }
		}
	}
	TArray<UPrimitiveComponent*> PhotoSources;
	for (TActorIterator<AActor> It(SourceWorld); It; ++It)
	{
		if (*It == ManagedBuilding || It->IsHidden() || It->IsActorBeingDestroyed()) { continue; }
		const bool bTrafficManager = It->IsA<ATrafficManager>();
		const bool bStreetLampManager = It->IsA<AStreetLampManager>();
		const bool bOwnedBuilding = OwnedBuildings.Contains(*It);
		if (It->IsA<ABuildingBaseActor>() && !bOwnedBuilding) { continue; }
		const ABrickFactory* Factory = Cast<ABrickFactory>(*It);
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		It->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!IsValid(Primitive) || !Primitive->IsRegistered() || !Primitive->IsVisible()
				|| Primitive->bHiddenInGame || !Primitive->bRenderInMainPass
				|| !Primitive->Bounds.GetBox().IsValid
				|| Primitive->Bounds.GetBox().ComputeSquaredDistanceToPoint(Result.SourceAnchorWorldTransform.GetLocation())
					> FMath::Square(MaxCaptureDistanceCm)) { continue; }
			const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Primitive);
			const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
			const FString MeshPath = Mesh ? Mesh->GetPathName() : FString();
			const bool bOwnedFacade = bOwnedBuilding && Mesh
				&& OfficeNeighborhoodSnapshot::IsFacadeComponent(*It, MeshComponent)
				&& OfficeNeighborhoodSnapshot::IsPersistentAsset(Mesh)
				&& MeshPath != TEXT("/Engine/BasicShapes/Plane.Plane");
			const bool bCityMesh = MeshPath.StartsWith(TEXT("/Game/City_Pack/"))
				&& (!bOwnedBuilding || bOwnedFacade) && !bTrafficManager;
			const bool bTrafficBody = bTrafficManager
				&& MeshPath.StartsWith(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_"));
			const bool bFactoryBody = Factory && Primitive == Factory->GetMainMeshComponent()
				&& Mesh && OfficeNeighborhoodSnapshot::IsPersistentAsset(Mesh);
			const bool bCityWater = !bOwnedBuilding && !bTrafficManager && !bStreetLampManager
				&& OfficeNeighborhoodSnapshot::IsCityWaterPlane(MeshComponent);
			if (Primitive->IsA<ULandscapeComponent>() || bCityMesh || bCityWater || bOwnedFacade || bTrafficBody || bFactoryBody)
			{
				PhotoSources.Add(Primitive);
			}
		}
	}
	if (PhotoSources.IsEmpty())
	{
		UE_LOG(LogOfficeNeighborhoodSnapshot, Warning, TEXT("No city geometry is available for office photo %d."), Result.ManagedBuildingIndex);
		return Result;
	}
	const AOfficeInterior* InteriorDefaults = GetDefault<AOfficeInterior>();
	FIntPoint TileCount = InteriorDefaults->GetTileCount();
	if (UGameInstance* GameInstance = SourceWorld->GetGameInstance())
	{
		if (USaveLoadManager* SaveManager = GameInstance->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveManager->GetCurrentSaveData())
			{
				if (const FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(Result.ManagedBuildingIndex))
				{
					TileCount = FIntPoint(OfficeData->TileCountX, OfficeData->TileCountY);
				}
			}
		}
	}
	TileCount = FOfficeFootprintGeometry::NormalizeTileCount(TileCount, InteriorDefaults->GetMaxTileCount());
	const FVector2D DefaultFloorSize = InteriorDefaults->GetCurrentFloorBoundsLocal().GetSize();
	const float TileSize = DefaultFloorSize.X / InteriorDefaults->GetTileCount().X;
	const FVector2D FloorSize = FOfficeFootprintGeometry::MakeFloorBounds(TileCount, TileSize).GetSize();
	FVector PhotoViewOffset;
	if (!AOfficeCameraPawn::TryGetInitialPhotoViewOffset(FloorSize, AOfficeInterior::PresentationScale, PhotoViewOffset))
	{
		UE_LOG(LogOfficeNeighborhoodSnapshot, Warning, TEXT("Source city photo has no valid office view."));
		return Result;
	}
	const float RequestedDistanceScale = OfficeNeighborhoodSnapshot::CityPhotoDistanceScale.GetValueOnGameThread();
	const float DistanceScale = FMath::IsFinite(RequestedDistanceScale) ? FMath::Clamp(RequestedDistanceScale, 0.f, 1.f) : 0.5f;
	PhotoViewOffset.X *= DistanceScale;
	PhotoViewOffset.Y *= DistanceScale;
	FTransform CaptureFrame = Result.SourceAnchorWorldTransform;
	CaptureFrame.SetLocation(Result.SourceAnchorWorldTransform.TransformPosition(PhotoViewOffset));
	ATimeCycleSky* Sky = nullptr;
	for (TActorIterator<ATimeCycleSky> It(SourceWorld); It; ++It) { Sky = *It; break; }
	TStrongObjectPtr<UOfficeDistantBackdrop> SourceCapture(NewObject<UOfficeDistantBackdrop>(GetTransientPackage(), NAME_None, RF_Transient));
	if (SourceCapture->CaptureSource(ManagedBuilding, Sky, PhotoSources, CaptureFrame))
	{
		Result.SourceBackdrop = SourceCapture.Get();
		UE_LOG(LogOfficeNeighborhoodSnapshot, Log, TEXT("Source city photo: %d components, %d captures, %.1f ms"),
			PhotoSources.Num(), SourceCapture->GetCaptureCount(), SourceCapture->GetCaptureMilliseconds());
	}
	else
	{
		UE_LOG(LogOfficeNeighborhoodSnapshot, Warning, TEXT("Source city photo capture failed for building %d."), Result.ManagedBuildingIndex);
	}
	return Result;
}
