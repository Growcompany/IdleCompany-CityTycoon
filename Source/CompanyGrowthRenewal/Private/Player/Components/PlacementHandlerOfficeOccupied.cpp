#include "Player/Components/PlacementHandler.h"
#include "Components/BoxComponent.h"
#include "Office/DecorationActor.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

void UPlacementHandler::UpdateOfficeOccupiedPreview()
{
	if (!OfficeGridMesh) return;
	// 격자가 뜨는 동안(편집·새 배치)은 기존 점유도 보여야 빈 칸처럼 보이는 자리에 속지 않는다.
	const bool bShow = IsValid(CachedOfficeInterior) && OfficeGridMesh->IsVisible();
	OfficeGridMesh->SetMeshSectionVisible(3, bShow);
	OfficeGridMesh->SetMeshSectionVisible(4, bShow);
	if (!bShow) return;
	const UOfficeManager* Manager = GetWorld()->GetSubsystem<UOfficeManager>();
	if (!Manager) return;
	const int32 RegisteredCount = Manager->GetPlacedWorkstations().Num() + Manager->GetPlacedDecorations().Num();
	AActor* Selection = GetPlacementTargetActor();
	// 변화가 없으면 정점 재구축을 건너뛴다(쓰로틀은 TickComponent 가 맡는다).
	if (OfficeOccupiedRegisteredCount == RegisteredCount && OfficeOccupiedSelection.Get() == Selection) return;
	OfficeOccupiedRegisteredCount = RegisteredCount;
	OfficeOccupiedSelection = Selection;
	const FTransform FloorTransform = CachedOfficeInterior->GetActorTransform();
	const double FloorZ = CachedOfficeInterior->GetStructuralFloorZLocal() + 1.8;
	TArray<FVector> FillVertices;
	TArray<FVector> EdgeVertices;
	TArray<int32> FillTriangles;
	TArray<int32> EdgeTriangles;
	const auto AddQuad = [](TArray<FVector>& Vertices, TArray<int32>& Triangles,
		const FVector& Center, const FVector& U, const FVector& V, double HalfU, double HalfV)
	{
		const int32 First = Vertices.Num();
		Vertices.Append({Center - U * HalfU - V * HalfV, Center + U * HalfU - V * HalfV,
			Center + U * HalfU + V * HalfV, Center - U * HalfU + V * HalfV});
		Triangles.Append({First, First + 2, First + 1, First, First + 3, First + 2});
	};
	const auto AddRectangle = [&](const FVector& Center, const FVector& U, const FVector& V, double HalfU, double HalfV)
	{
		if (HalfU <= 0.01 || HalfV <= 0.01) return;
		AddQuad(FillVertices, FillTriangles, Center, U, V, HalfU, HalfV);
		const double Width = FMath::Min(2.5, FMath::Min(HalfU, HalfV));
		AddQuad(EdgeVertices, EdgeTriangles, Center - V * (HalfV - Width * 0.5), U, V, HalfU, Width * 0.5);
		AddQuad(EdgeVertices, EdgeTriangles, Center + V * (HalfV - Width * 0.5), U, V, HalfU, Width * 0.5);
		AddQuad(EdgeVertices, EdgeTriangles, Center - U * (HalfU - Width * 0.5), U, V, Width * 0.5, HalfV - Width);
		AddQuad(EdgeVertices, EdgeTriangles, Center + U * (HalfU - Width * 0.5), U, V, Width * 0.5, HalfV - Width);
	};
	for (const AWorkstationActorBase* Desk : Manager->GetPlacedWorkstations())
	{
		if (PlacementDecoration && PlacementDecoration->IsFloorCovering()) continue;
		if (!IsValid(Desk) || Desk == PlacementWorkstation) continue;
		FBox2D Rect;
		if (GetOfficeWorkstationFootprint(Desk, Rect))
			AddRectangle(FVector(Rect.GetCenter(), FloorZ), FVector::ForwardVector, FVector::RightVector,
				Rect.GetSize().X * 0.5, Rect.GetSize().Y * 0.5);
	}
	for (const ADecorationActor* Decoration : Manager->GetPlacedDecorations())
	{
		if (!IsValid(Decoration) || Decoration == PlacementDecoration || !Decoration->BoxComponent) continue;
		if (Decoration->IsFloorCovering()
			&& (!PlacementDecoration || !PlacementDecoration->IsFloorCovering())) continue;
		if (PlacementDecoration && PlacementDecoration->IsFloorCovering() && !Decoration->IsFloorCovering()) continue;
		const UBoxComponent* Box = Decoration->BoxComponent;
		const FVector Extent = Box->GetUnscaledBoxExtent();
		const bool bWall = Decoration->AllowedSurface == EDecorationSurface::Wall;
		FVector U = FVector::ForwardVector;
		FVector V = FVector::RightVector;
		FVector Normal = FVector::UpVector;
		if (bWall)
		{
			const FVector Position = Decoration->GetActorLocation();
			const EWallSide Side = FMath::Abs(CachedOfficeInterior->GetWallPlane(EWallSide::Left).PlaneDot(Position))
				< FMath::Abs(CachedOfficeInterior->GetWallPlane(EWallSide::Right).PlaneDot(Position)) ? EWallSide::Left : EWallSide::Right;
			Normal = FloorTransform.InverseTransformVectorNoScale(CachedOfficeInterior->GetWallNormal(Side)).GetSafeNormal();
			const FVector FloorCenter(CachedOfficeInterior->GetCurrentFloorBoundsLocal().GetCenter(), FloorZ);
			const FVector WallPosition = FloorTransform.InverseTransformPosition(CachedOfficeInterior->GetWallLocation(Side));
			if (FVector::DotProduct(FloorCenter - WallPosition, Normal) < 0.0) Normal *= -1.0;
			V = FVector::UpVector;
			U = FVector::CrossProduct(V, Normal).GetSafeNormal();
		}
		FBox Projected(ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector LocalCorner(Extent.X * ((Corner & 1) ? 1 : -1),
				Extent.Y * ((Corner & 2) ? 1 : -1), Extent.Z * ((Corner & 4) ? 1 : -1));
			const FVector FloorCorner = FloorTransform.InverseTransformPosition(Box->GetComponentTransform().TransformPosition(LocalCorner));
			Projected += FVector(FVector::DotProduct(FloorCorner, U), FVector::DotProduct(FloorCorner, V), FVector::DotProduct(FloorCorner, Normal));
		}
		const FVector Center = Projected.GetCenter();
		// 벽 장식은 메시 앞쪽 표면에 붙여 그림 뒤로 가려지지 않게 한다.
		AddRectangle(U * Center.X + V * Center.Y + Normal * (bWall ? Projected.Max.Z + 1.0 : FloorZ),
			U, V, Projected.GetExtent().X, Projected.GetExtent().Y);
	}
	// 표면 정점이 같으면 이동 중에도 GPU 섹션을 다시 만들지 않는다.
	bool bChanged = OfficeOccupiedVertices.Num() != FillVertices.Num();
	if (!bChanged) for (int32 Index = 0; Index < FillVertices.Num(); ++Index)
	{
		if (!OfficeOccupiedVertices[Index].Equals(FillVertices[Index], 0.01)) { bChanged = true; break; }
	}
	if (!bChanged) return;
	OfficeOccupiedVertices = MoveTemp(FillVertices);
	OfficeGridMesh->CreateMeshSection_LinearColor(3, OfficeOccupiedVertices, FillTriangles, {}, {}, {}, {}, false);
	OfficeGridMesh->CreateMeshSection_LinearColor(4, EdgeVertices, EdgeTriangles, {}, {}, {}, {}, false);
}
