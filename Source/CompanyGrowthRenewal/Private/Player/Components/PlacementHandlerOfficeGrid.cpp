#include "Player/Components/PlacementHandler.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Global/GlobalAssetCache.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Office/OfficeInterior.h"
#include "Office/WorkstationActorBase.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Office/DecorationActor.h"
#include "Engine/StaticMesh.h"

bool UPlacementHandler::IsOfficeEntrancePlacementValid() const
{
	if (CurrentPlacementMode != EPlacementMode::Workstation && CurrentPlacementMode != EPlacementMode::Decoration
		&& CurrentPlacementMode != EPlacementMode::WallDecoration) return true;
	if (!CachedOfficeInterior) return false;
	if (CurrentPlacementMode == EPlacementMode::Workstation)
	{
		FBox2D Footprint;
		return GetOfficeWorkstationFootprint(PlacementWorkstation, Footprint)
			&& !CachedOfficeInterior->IsRecruitmentEntranceFloorBlocked(Footprint);
	}
	if (!PlacementDecoration || !PlacementDecoration->MeshComponent
		|| !PlacementDecoration->MeshComponent->GetStaticMesh()) return false;
	const UStaticMeshComponent* DecorationMesh = PlacementDecoration->MeshComponent;
	const FTransform MeshToOffice = DecorationMesh->GetComponentTransform().GetRelativeTransform(CachedOfficeInterior->GetActorTransform());
	const FBox LocalBounds = DecorationMesh->GetStaticMesh()->GetBoundingBox().TransformBy(MeshToOffice);
	return !CachedOfficeInterior->IsRecruitmentEntranceBlocked(LocalBounds)
		&& !CachedOfficeInterior->IsTelevisionPlacementBlocked(LocalBounds);
}

bool UPlacementHandler::IsOfficeFloorCoveringPlacementValid() const
{
	if (!PlacementDecoration || !CachedOfficeInterior || !GetWorld()) return false;
	const UOfficeManager* Manager = GetWorld()->GetSubsystem<UOfficeManager>();
	if (!Manager) return false;
	const FTransform FloorTransform = CachedOfficeInterior->GetActorTransform();
	const FBox2D PreviewRect = PlacementDecoration->GetFloorFootprint(FloorTransform);
	const FBox2D FloorRect = CachedOfficeInterior->GetCurrentFloorBoundsLocal();
	constexpr double Tolerance = 0.01;
	if (!PreviewRect.bIsValid || PreviewRect.Min.X < FloorRect.Min.X - Tolerance || PreviewRect.Min.Y < FloorRect.Min.Y - Tolerance
		|| PreviewRect.Max.X > FloorRect.Max.X + Tolerance || PreviewRect.Max.Y > FloorRect.Max.Y + Tolerance) return false;
	for (const ADecorationActor* Other : Manager->GetPlacedDecorations())
	{
		if (!IsValid(Other) || Other == PlacementDecoration || !Other->IsFloorCovering()) continue;
		const FBox2D OtherRect = Other->GetFloorFootprint(FloorTransform);
		if (PreviewRect.Min.X < OtherRect.Max.X - Tolerance && PreviewRect.Max.X > OtherRect.Min.X + Tolerance
			&& PreviewRect.Min.Y < OtherRect.Max.Y - Tolerance && PreviewRect.Max.Y > OtherRect.Min.Y + Tolerance) return false;
	}
	return true;
}

bool UPlacementHandler::GetOfficeWorkstationFootprint(const AWorkstationActorBase* Workstation, FBox2D& OutFloorRect) const
{
	OutFloorRect = FBox2D(ForceInit);
	if (!Workstation || !CachedOfficeInterior || !GetWorld() || !GetWorld()->GetGameInstance()) return false;
	const UTableManagerSubsystem* Tables = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!Tables) return false;
	bool bFound = false;
	const FWorkstationCardTable Card = Tables->GetWorkstationCardInfo(Workstation->WorkstationTypeID, bFound);
	if (!bFound || Card.GridSize.X <= 0 || Card.GridSize.Y <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeGrid] Missing/invalid footprint row: %s"), *Workstation->WorkstationTypeID.ToString());
		return false;
	}
	const FTransform FloorTransform = CachedOfficeInterior->GetActorTransform();
	const FVector CenterWorld = Workstation->GetActorTransform().TransformPosition(FVector(Card.GridCenterOffset, 0));
	const FVector CenterLocal = FloorTransform.InverseTransformPosition(CenterWorld);
	const double RelativeYaw = Workstation->GetActorRotation().Yaw - CachedOfficeInterior->GetActorRotation().Yaw;
	const int32 QuarterTurns = FMath::RoundToInt(RelativeYaw / 90.0);
	const bool bSwap = FMath::Abs(QuarterTurns) % 2 != 0;
	const FVector2D HalfSize((bSwap ? Card.GridSize.Y : Card.GridSize.X) * OfficeGridCellSize * 0.5,
		(bSwap ? Card.GridSize.X : Card.GridSize.Y) * OfficeGridCellSize * 0.5);
	const FVector2D Center(CenterLocal.X, CenterLocal.Y);
	OutFloorRect = FBox2D(Center - HalfSize, Center + HalfSize);
	return true;
}

bool UPlacementHandler::IsOfficeWorkstationFootprintValid() const
{
	FBox2D PreviewRect;
	if (!GetOfficeWorkstationFootprint(PlacementWorkstation, PreviewRect)) return false;
	if (!GetWorld()->GetSubsystem<UOfficeManager>()) return false;
	TArray<FBox2D> BlockingRects;
	GetOfficeWorkstationBlockingFootprints(BlockingRects);
	return IsOfficeWorkstationFootprintValid(PreviewRect, BlockingRects);
}

bool UPlacementHandler::IsOfficeWorkstationFootprintValid(const FBox2D& PreviewRect, TArrayView<const FBox2D> BlockingRects) const
{
	const FBox2D FloorRect = CachedOfficeInterior->GetCurrentFloorBoundsLocal();
	constexpr double Tolerance = 0.01;
	if (PreviewRect.Min.X < FloorRect.Min.X - Tolerance || PreviewRect.Min.Y < FloorRect.Min.Y - Tolerance
		|| PreviewRect.Max.X > FloorRect.Max.X + Tolerance || PreviewRect.Max.Y > FloorRect.Max.Y + Tolerance) return false;
	const FBox2D ShrunkPreviewRect = PreviewRect.ExpandBy(-Tolerance);
	for (const FBox2D& OtherRect : BlockingRects)
	{
		if (ShrunkPreviewRect.Intersect(OtherRect)) return false;
	}
	return true;
}

void UPlacementHandler::GetOfficeWorkstationBlockingFootprints(TArray<FBox2D>& OutRects) const
{
	OutRects.Reset();
	const UOfficeManager* OfficeManager = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr;
	if (!OfficeManager || !CachedOfficeInterior) return;
	for (const AWorkstationActorBase* OtherDesk : OfficeManager->GetPlacedWorkstations())
	{
		if (!IsValid(OtherDesk) || OtherDesk == PlacementWorkstation) continue;
		FBox2D OtherRect;
		// 표 행이 없는 책상 하나 때문에 방 전체 배치가 막히면 안 된다.
		if (!GetOfficeWorkstationFootprint(OtherDesk, OtherRect)) continue;
		OutRects.Add(OtherRect);
	}
	// 바닥 장식도 점유 미리보기에 그려지는 사각형 그대로 막는다. 카펫은 겹쳐도 되고 벽 장식은 물리 겹침이 맡는다.
	const FTransform FloorTransform = CachedOfficeInterior->GetActorTransform();
	for (const ADecorationActor* Decoration : OfficeManager->GetPlacedDecorations())
	{
		if (!IsValid(Decoration) || Decoration->AllowedSurface != EDecorationSurface::Grid || Decoration->IsFloorCovering()) continue;
		const FBox2D DecorationRect = Decoration->GetFloorFootprint(FloorTransform);
		if (DecorationRect.bIsValid) OutRects.Add(DecorationRect);
	}
}

void UPlacementHandler::HideOfficeGridPreview()
{
	if (OfficeGridMesh) OfficeGridMesh->SetVisibility(false);
	if (OfficeFootprintMesh) OfficeFootprintMesh->SetVisibility(false);
}

void UPlacementHandler::UpdateOfficeGridPreview()
{
	const bool bNewFloorPlacement = (CurrentPlacementMode == EPlacementMode::Workstation && PlacementWorkstation)
		|| (CurrentPlacementMode == EPlacementMode::Decoration && PlacementDecoration);
	if ((!bOfficeEditSession && !bNewFloorPlacement)
		|| !CachedOfficeInterior || !OfficeGuideBaseMaterial || !GlobalAssetCache)
	{
		HideOfficeGridPreview();
		return;
	}

	if (!OfficeGridMesh)
	{
		OfficeGridMesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("OfficePlacementGrid"));
		OfficeGridMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OfficeGridMesh->SetGenerateOverlapEvents(false);
		OfficeGridMesh->SetCanEverAffectNavigation(false);
		OfficeGridMesh->SetCastShadow(false);
		OfficeGridMesh->RegisterComponent();
		UMaterialInstanceDynamic* GridMaterial = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		GridMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.24f, 0.28f, 0.75f));
		OfficeGridMesh->SetMaterial(1, GridMaterial);
		UMaterialInstanceDynamic* FloorMaterial = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		FloorMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f, 0.55f));
		OfficeGridMesh->SetMaterial(0, FloorMaterial);
		OfficeFootprintOutlineMaterial = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		OfficeGridMesh->SetMaterial(2, OfficeFootprintOutlineMaterial);
		UMaterialInstanceDynamic* OccupiedFill = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		OccupiedFill->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.4f, 0.7f, 0.18f));
		OfficeGridMesh->SetMaterial(3, OccupiedFill);
		UMaterialInstanceDynamic* OccupiedOutline = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		OccupiedOutline->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.35f, 0.7f, 0.85f));
		OfficeGridMesh->SetMaterial(4, OccupiedOutline);
		UMaterialInstanceDynamic* EntranceFill = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		EntranceFill->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.22f, 0.18f, 0.16f));
		OfficeGridMesh->SetMaterial(5, EntranceFill);
		UMaterialInstanceDynamic* EntranceOutline = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		EntranceOutline->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.18f, 0.14f, 0.8f));
		OfficeGridMesh->SetMaterial(6, EntranceOutline);
	}
	OfficeGridMesh->SetWorldTransform(CachedOfficeInterior->GetActorTransform());
	const FIntPoint TileCount = CachedOfficeInterior->GetTileCount();
	// 문 위치는 확장·문 상태를 따라 바뀌므로 방 크기가 바뀌거나 격자를 다시 띄울 때 새로 그린다.
	const bool bRefreshEntrance = OfficeGridTileCount != TileCount || !OfficeGridMesh->IsVisible();
	if (OfficeGridTileCount != TileCount)
	{
		const FBox2D FloorBounds = CachedOfficeInterior->GetCurrentFloorBoundsLocal();
		const double GridZ = CachedOfficeInterior->GetStructuralFloorZLocal() + 1.5;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		auto AddStrip = [&](double MinX, double MinY, double MaxX, double MaxY)
		{
			const int32 First = Vertices.Num();
			Vertices.Append({FVector(MinX, MinY, GridZ), FVector(MaxX, MinY, GridZ),
				FVector(MaxX, MaxY, GridZ), FVector(MinX, MaxY, GridZ)});
			Triangles.Append({First, First + 2, First + 1, First, First + 3, First + 2});
		};
		// 모든 선을 한 섹션에 모으고 방 크기가 바뀔 때만 재생성한다.
		constexpr double HalfLineWidth = 1.0;
		for (double X = FloorBounds.Min.X;
			X <= FloorBounds.Max.X; X += OfficeGridCellSize)
		{
			AddStrip(FMath::Max(FloorBounds.Min.X, X - HalfLineWidth), FloorBounds.Min.Y,
				FMath::Min(FloorBounds.Max.X, X + HalfLineWidth), FloorBounds.Max.Y);
		}
		for (double Y = FloorBounds.Min.Y;
			Y <= FloorBounds.Max.Y; Y += OfficeGridCellSize)
		{
			AddStrip(FloorBounds.Min.X, FMath::Max(FloorBounds.Min.Y, Y - HalfLineWidth),
				FloorBounds.Max.X, FMath::Min(FloorBounds.Max.Y, Y + HalfLineWidth));
		}
		OfficeGridMesh->CreateMeshSection_LinearColor(1, Vertices, Triangles, {}, {}, {}, {}, false);
		Vertices.Reset();
		Triangles.Reset();
		AddStrip(FloorBounds.Min.X, FloorBounds.Min.Y, FloorBounds.Max.X, FloorBounds.Max.Y);
		for (FVector& Vertex : Vertices) Vertex.Z -= 0.5;
		OfficeGridMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, {}, {}, {}, false);
		OfficeGridTileCount = TileCount;
	}
	if (bRefreshEntrance)
	{
		// 판정에 쓰는 문 앞 예약 상자를 바닥 안쪽만 잘라 그대로 보여준다.
		const FBox Reserved = CachedOfficeInterior->GetRecruitmentEntranceExclusionBoundsLocal();
		const FBox2D FloorRect = CachedOfficeInterior->GetCurrentFloorBoundsLocal();
		const FBox2D Zone(FVector2D(FMath::Max(Reserved.Min.X, FloorRect.Min.X), FMath::Max(Reserved.Min.Y, FloorRect.Min.Y)),
			FVector2D(FMath::Min(Reserved.Max.X, FloorRect.Max.X), FMath::Min(Reserved.Max.Y, FloorRect.Max.Y)));
		const bool bShowEntrance = Reserved.IsValid && Zone.Min.X < Zone.Max.X && Zone.Min.Y < Zone.Max.Y;
		if (bShowEntrance)
		{
			const double EntranceZ = CachedOfficeInterior->GetStructuralFloorZLocal() + 1.6;
			TArray<FVector> FillVertices;
			TArray<FVector> EdgeVertices;
			TArray<int32> FillTriangles;
			TArray<int32> EdgeTriangles;
			const auto AddQuad = [EntranceZ](TArray<FVector>& Vertices, TArray<int32>& Triangles,
				double MinX, double MinY, double MaxX, double MaxY)
			{
				const int32 First = Vertices.Num();
				Vertices.Append({FVector(MinX, MinY, EntranceZ), FVector(MaxX, MinY, EntranceZ),
					FVector(MaxX, MaxY, EntranceZ), FVector(MinX, MaxY, EntranceZ)});
				Triangles.Append({First, First + 2, First + 1, First, First + 3, First + 2});
			};
			AddQuad(FillVertices, FillTriangles, Zone.Min.X, Zone.Min.Y, Zone.Max.X, Zone.Max.Y);
			const double EntranceEdge = FMath::Min(3.0, FMath::Min(Zone.GetSize().X, Zone.GetSize().Y) * 0.5);
			AddQuad(EdgeVertices, EdgeTriangles, Zone.Min.X, Zone.Min.Y, Zone.Max.X, Zone.Min.Y + EntranceEdge);
			AddQuad(EdgeVertices, EdgeTriangles, Zone.Min.X, Zone.Max.Y - EntranceEdge, Zone.Max.X, Zone.Max.Y);
			AddQuad(EdgeVertices, EdgeTriangles, Zone.Min.X, Zone.Min.Y + EntranceEdge, Zone.Min.X + EntranceEdge, Zone.Max.Y - EntranceEdge);
			AddQuad(EdgeVertices, EdgeTriangles, Zone.Max.X - EntranceEdge, Zone.Min.Y + EntranceEdge, Zone.Max.X, Zone.Max.Y - EntranceEdge);
			OfficeGridMesh->CreateMeshSection_LinearColor(5, FillVertices, FillTriangles, {}, {}, {}, {}, false);
			OfficeGridMesh->CreateMeshSection_LinearColor(6, EdgeVertices, EdgeTriangles, {}, {}, {}, {}, false);
		}
		OfficeGridMesh->SetMeshSectionVisible(5, bShowEntrance);
		OfficeGridMesh->SetMeshSectionVisible(6, bShowEntrance);
	}
	OfficeGridMesh->SetVisibility(true);
	UpdateOfficeOccupiedPreview();
	const bool bWorkstationPreview = CurrentPlacementMode == EPlacementMode::Workstation && PlacementWorkstation;
	const bool bDecorationPreview = CurrentPlacementMode == EPlacementMode::Decoration && PlacementDecoration;
	if (!bWorkstationPreview && !bDecorationPreview)
	{
		if (OfficeFootprintMesh) OfficeFootprintMesh->SetVisibility(false);
		OfficeGridMesh->SetMeshSectionVisible(2, false);
		return;
	}

	if (!OfficeFootprintMesh)
	{
		OfficeFootprintMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("OfficePlacementFootprint"));
		OfficeFootprintMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OfficeFootprintMesh->SetGenerateOverlapEvents(false);
		OfficeFootprintMesh->SetCanEverAffectNavigation(false);
		OfficeFootprintMesh->SetCastShadow(false);
		OfficeFootprintMesh->SetStaticMesh(GlobalAssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Plane));
		OfficeFootprintMaterial = UMaterialInstanceDynamic::Create(OfficeGuideBaseMaterial, this);
		OfficeFootprintMesh->SetMaterial(0, OfficeFootprintMaterial);
		OfficeFootprintMesh->RegisterComponent();
	}
	FBox2D PreviewRect;
	const FTransform FloorTransform = CachedOfficeInterior->GetActorTransform();
	if (bWorkstationPreview)
	{
		if (!GetOfficeWorkstationFootprint(PlacementWorkstation, PreviewRect)) PreviewRect = FBox2D(ForceInit);
	}
	else
	{
		PreviewRect = PlacementDecoration->GetFloorFootprint(FloorTransform);
	}
	if (!PreviewRect.bIsValid)
	{
		OfficeFootprintMesh->SetVisibility(false);
		OfficeGridMesh->SetMeshSectionVisible(2, false);
		return;
	}
	const FVector2D Center = PreviewRect.GetCenter();
	const FVector2D Size = PreviewRect.GetSize();
	double GuideZ = CachedOfficeInterior->GetStructuralFloorZLocal() + 2.f;
	if (bDecorationPreview && PlacementDecoration->IsFloorCovering()
		&& PlacementDecoration->MeshComponent && PlacementDecoration->MeshComponent->GetStaticMesh())
	{
		const UStaticMeshComponent* DecorationMesh = PlacementDecoration->MeshComponent;
		const FTransform MeshToOffice = DecorationMesh->GetComponentTransform().GetRelativeTransform(FloorTransform);
		const FBox CarpetBounds = DecorationMesh->GetStaticMesh()->GetBoundingBox().TransformBy(MeshToOffice);
		GuideZ = FMath::Max(GuideZ, CarpetBounds.Max.Z + 1.0);
	}
	OfficeFootprintMesh->SetWorldLocation(FloorTransform.TransformPosition(
		FVector(Center, GuideZ)));
	OfficeFootprintMesh->SetWorldRotation(CachedOfficeInterior->GetActorRotation());
	OfficeFootprintMesh->SetWorldScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 1.f) * FloorTransform.GetScale3D());
	OfficeFootprintMaterial->SetVectorParameterValue(TEXT("Color"), CanDrop
		? FLinearColor(0.15f, 0.8f, 0.4f, 0.2f) : FLinearColor(1.f, 0.15f, 0.12f, 0.25f));
	OfficeFootprintMesh->SetVisibility(true);

	// 바깥 경계는 판정 사각형과 일치시키고, 굵기는 안쪽으로만 준다.
	// 같은 칸에서 색만 바뀔 때는 정점을 재생성하지 않는다.
	const FProcMeshSection* OutlineSection = OfficeGridMesh->GetProcMeshSection(2);
	const bool bGuideHeightChanged = !OutlineSection || OutlineSection->ProcVertexBuffer.IsEmpty()
		|| !FMath::IsNearlyEqual(OutlineSection->ProcVertexBuffer[0].Position.Z, GuideZ + 0.5, 0.01);
	if (bGuideHeightChanged || !OfficeFootprintOutlineRect.bIsValid || !OfficeFootprintOutlineRect.Min.Equals(PreviewRect.Min, 0.01)
		|| !OfficeFootprintOutlineRect.Max.Equals(PreviewRect.Max, 0.01))
	{
		TArray<FVector> OutlineVertices;
		TArray<int32> OutlineTriangles;
		const double OutlineZ = GuideZ + 0.5;
		auto AddEdge = [&](double MinX, double MinY, double MaxX, double MaxY)
		{
			const int32 First = OutlineVertices.Num();
			OutlineVertices.Append({FVector(MinX, MinY, OutlineZ), FVector(MaxX, MinY, OutlineZ),
				FVector(MaxX, MaxY, OutlineZ), FVector(MinX, MaxY, OutlineZ)});
			OutlineTriangles.Append({First, First + 2, First + 1, First, First + 3, First + 2});
		};
		constexpr double EdgeWidth = 3.0;
		AddEdge(PreviewRect.Min.X, PreviewRect.Min.Y, PreviewRect.Max.X, PreviewRect.Min.Y + EdgeWidth);
		AddEdge(PreviewRect.Min.X, PreviewRect.Max.Y - EdgeWidth, PreviewRect.Max.X, PreviewRect.Max.Y);
		AddEdge(PreviewRect.Min.X, PreviewRect.Min.Y + EdgeWidth, PreviewRect.Min.X + EdgeWidth, PreviewRect.Max.Y - EdgeWidth);
		AddEdge(PreviewRect.Max.X - EdgeWidth, PreviewRect.Min.Y + EdgeWidth, PreviewRect.Max.X, PreviewRect.Max.Y - EdgeWidth);
		OfficeGridMesh->CreateMeshSection_LinearColor(2, OutlineVertices, OutlineTriangles, {}, {}, {}, {}, false);
		OfficeFootprintOutlineRect = PreviewRect;
	}
	OfficeFootprintOutlineMaterial->SetVectorParameterValue(TEXT("Color"), CanDrop
		? FLinearColor(0.08f, 0.75f, 0.25f, 0.95f) : FLinearColor(1.f, 0.08f, 0.06f, 0.95f));
	OfficeGridMesh->SetMeshSectionVisible(2, true);
}
