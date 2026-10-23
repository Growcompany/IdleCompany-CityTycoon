#include "Entity/Plot/CityPlotStreetSurface.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "Util/InstancedMeshRefresh.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

#include <limits>

namespace CityPlotStreetSurfacePrivate
{
constexpr double TileSize = 500.0;
constexpr double GeometryTolerance = 0.01;
constexpr int32 MaxRegions = 2048;

struct FVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FVector2D UV = FVector2D::ZeroVector;
	FProcMeshTangent Tangent;
};

struct FMeshData
{
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
};

struct FSectionData
{
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	int32 MaterialIndex = INDEX_NONE;
};

struct FSource
{
	UStaticMeshComponent* Component = nullptr;
	UInstancedStaticMeshComponent* Instances = nullptr;
	UStaticMesh* Mesh = nullptr;
	bool bRoadSource = false;
	int32 Index = INDEX_NONE;
	FTransform Local = FTransform::Identity;
	FTransform World = FTransform::Identity;
	FBox Bounds = FBox(ForceInit);
	TArray<int32> RegionIndices;
};

struct FPatch
{
	FBox2D Bounds = FBox2D(ForceInit);
	float Z = 0.f;
	float CurbBaseZ = 0.f;
	bool bVehicle = false;
	bool bJunction = false;
	bool bInternalJunction = false;
	bool bOpenAtStart = false;
	FBox2D OriginalBounds = FBox2D(ForceInit);
	FVector2D Direction = FVector2D(1.0, 0.0);
	UMaterialInterface* Material = nullptr;
};

bool Overlaps(const FBox2D& A, const FBox2D& B)
{
	return A.Min.X < B.Max.X - GeometryTolerance && B.Min.X < A.Max.X - GeometryTolerance
		&& A.Min.Y < B.Max.Y - GeometryTolerance && B.Min.Y < A.Max.Y - GeometryTolerance;
}

FBox2D Flatten(const FBox& Box)
{
	return FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y));
}

bool ValidRegion(const FCityStreetSurfaceRegion& Region)
{
	return (!Region.bInternalJunction || Region.bJunction)
		&& Region.Bounds.bIsValid && FMath::IsFinite(Region.Bounds.Min.X)
		&& FMath::IsFinite(Region.Bounds.Min.Y) && FMath::IsFinite(Region.Bounds.Max.X)
		&& FMath::IsFinite(Region.Bounds.Max.Y) && FMath::IsFinite(Region.SurfaceZ)
		&& FMath::IsFinite(Region.CurbBaseZ)
		&& Region.Bounds.Min.X < Region.Bounds.Max.X
		&& Region.Bounds.Min.Y < Region.Bounds.Max.Y;
}

bool IsExternalJunction(const FCityStreetSurfaceRegion& Region)
{
	return Region.bJunction && !Region.bInternalJunction;
}

bool IsSourceForRegion(const FCityStreetSurfaceRegion& Region, bool bRoadSource)
{
	return bRoadSource == IsExternalJunction(Region);
}

bool IsSourceAvailable(const FCityStreetSurfaceRegion& Region, bool bRenderable,
	bool bHasGroundSource, bool bHasRoadSource)
{
	return bRenderable && (IsExternalJunction(Region) ? bHasRoadSource : bHasGroundSource);
}

bool IsHorizontal(const FTransform& Transform)
{
	return Transform.GetScale3D().GetAbsMin() > KINDA_SMALL_NUMBER
		&& Transform.GetRotation().GetUpVector().Z > 0.995;
}

FVertex Interpolate(const FVertex& A, const FVertex& B, double Alpha)
{
	FVertex Result;
	Result.Position = FMath::Lerp(A.Position, B.Position, Alpha);
	Result.Normal = FMath::Lerp(A.Normal, B.Normal, Alpha).GetSafeNormal();
	Result.UV = FMath::Lerp(A.UV, B.UV, Alpha);
	Result.Tangent = FProcMeshTangent(
		FMath::Lerp(A.Tangent.TangentX, B.Tangent.TangentX, Alpha).GetSafeNormal(),
		A.Tangent.bFlipTangentY);
	return Result;
}

void Clip(const TArray<FVertex>& Input, int32 Axis, double Limit, bool bKeepGreater,
	TArray<FVertex>& Output)
{
	Output.Reset();
	if (Input.IsEmpty()) { return; }
	auto Coordinate = [Axis](const FVertex& Vertex)
	{
		return Axis == 0 ? Vertex.Position.X : Vertex.Position.Y;
	};
	auto Inside = [Limit, bKeepGreater](double Value)
	{
		return bKeepGreater ? Value >= Limit : Value <= Limit;
	};
	FVertex Previous = Input.Last();
	double PreviousValue = Coordinate(Previous);
	bool bPreviousInside = Inside(PreviousValue);
	for (const FVertex& Current : Input)
	{
		const double CurrentValue = Coordinate(Current);
		const bool bCurrentInside = Inside(CurrentValue);
		if (bCurrentInside != bPreviousInside && !FMath::IsNearlyEqual(PreviousValue, CurrentValue))
		{
			const double Alpha = FMath::Clamp((Limit - PreviousValue) / (CurrentValue - PreviousValue), 0.0, 1.0);
			Output.Add(Interpolate(Previous, Current, Alpha));
		}
		if (bCurrentInside) { Output.Add(Current); }
		Previous = Current;
		PreviousValue = CurrentValue;
		bPreviousInside = bCurrentInside;
	}
}

void ClipToBounds(const TArray<FVertex>& Input, const FBox2D& Bounds, TArray<FVertex>& Output)
{
	TArray<FVertex> First, Second;
	Clip(Input, 0, Bounds.Min.X, true, First);
	Clip(First, 0, Bounds.Max.X, false, Second);
	Clip(Second, 1, Bounds.Min.Y, true, First);
	Clip(First, 1, Bounds.Max.Y, false, Output);
}

bool PolygonOverlaps(const TArray<FVertex>& Polygon, const FBox2D& Box)
{
	double MinX = std::numeric_limits<double>::max();
	double MinY = std::numeric_limits<double>::max();
	double MaxX = -MinX;
	double MaxY = -MinY;
	for (const FVertex& Vertex : Polygon)
	{
		MinX = FMath::Min(MinX, Vertex.Position.X);
		MinY = FMath::Min(MinY, Vertex.Position.Y);
		MaxX = FMath::Max(MaxX, Vertex.Position.X);
		MaxY = FMath::Max(MaxY, Vertex.Position.Y);
	}
	return MinX < Box.Max.X && Box.Min.X < MaxX && MinY < Box.Max.Y && Box.Min.Y < MaxY;
}

void Subtract(const TArray<FVertex>& Polygon, const FBox2D& Box,
	TArray<TArray<FVertex>>& Output)
{
	if (Polygon.Num() < 3) { return; }
	if (!PolygonOverlaps(Polygon, Box))
	{
		Output.Add(Polygon);
		return;
	}
	TArray<FVertex> Remaining = Polygon;
	TArray<FVertex> Outside, Next;
	const struct { int32 Axis; double Limit; bool bInsideGreater; } Sides[] = {
		{ 0, Box.Min.X, true }, { 0, Box.Max.X, false },
		{ 1, Box.Min.Y, true }, { 1, Box.Max.Y, false }
	};
	for (const auto& Side : Sides)
	{
		Clip(Remaining, Side.Axis, Side.Limit, !Side.bInsideGreater, Outside);
		if (Outside.Num() >= 3) { Output.Add(MoveTemp(Outside)); }
		Clip(Remaining, Side.Axis, Side.Limit, Side.bInsideGreater, Next);
		Remaining = MoveTemp(Next);
		if (Remaining.Num() < 3) { return; }
	}
}

void AppendPolygon(const TArray<FVertex>& Polygon, const FVector& Anchor, FMeshData& Data)
{
	for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
	{
		const FVertex* Triangle[3] = { &Polygon[0], &Polygon[Index], &Polygon[Index + 1] };
		if (FVector::CrossProduct(Triangle[1]->Position - Triangle[0]->Position,
			Triangle[2]->Position - Triangle[0]->Position).SizeSquared() <= 0.0001)
		{
			continue;
		}
		const int32 Base = Data.Positions.Num();
		for (const FVertex* Vertex : Triangle)
		{
			Data.Positions.Add(Vertex->Position - Anchor);
			Data.Normals.Add(Vertex->Normal);
			Data.UVs.Add(Vertex->UV);
			Data.Tangents.Add(Vertex->Tangent);
		}
		Data.Triangles.Append({ Base, Base + 1, Base + 2 });
	}
}

bool ExtractSections(UStaticMesh* Mesh, TArray<FSectionData>& Out)
{
	Out.Reset();
	if (!IsValid(Mesh) || !Mesh->bAllowCPUAccess || !Mesh->GetRenderData()
		|| !Mesh->GetRenderData()->LODResources.IsValidIndex(0)) { return false; }
	const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
	for (int32 Section = 0; Section < Mesh->GetNumSections(0); ++Section)
	{
		if (!LOD.Sections.IsValidIndex(Section)) { return false; }
		FSectionData& Data = Out.AddDefaulted_GetRef();
		Data.MaterialIndex = LOD.Sections[Section].MaterialIndex;
		UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Mesh, 0, Section,
			Data.Positions, Data.Triangles, Data.Normals, Data.UVs, Data.Tangents);
		if (Data.Triangles.IsEmpty() || Data.Triangles.Num() % 3 != 0
			|| Data.Normals.Num() != Data.Positions.Num()
			|| Data.UVs.Num() != Data.Positions.Num()
			|| Data.Tangents.Num() != Data.Positions.Num())
		{
			Out.Reset();
			return false;
		}
		for (const int32 VertexIndex : Data.Triangles)
		{
			if (!Data.Positions.IsValidIndex(VertexIndex))
			{
				Out.Reset();
				return false;
			}
		}
	}
	return !Out.IsEmpty();
}

void AppendSource(const FSource& Source, TConstArrayView<FBox2D> Cuts,
	const TArray<FSectionData>& Sections, const FVector& Anchor,
	TMap<UMaterialInterface*, FMeshData>& ByMaterial)
{
	for (const FSectionData& Section : Sections)
	{
		UMaterialInterface* Material = Source.Component->GetMaterial(Section.MaterialIndex);
		FMeshData& Output = ByMaterial.FindOrAdd(Material);
		for (int32 Triangle = 0; Triangle < Section.Triangles.Num(); Triangle += 3)
		{
			TArray<FVertex> Original;
			Original.Reserve(3);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Index = Section.Triangles[Triangle + Corner];
				FVertex& Vertex = Original.AddDefaulted_GetRef();
				Vertex.Position = Source.World.TransformPosition(Section.Positions[Index]);
				Vertex.Normal = Source.World.TransformVectorNoScale(Section.Normals[Index]).GetSafeNormal();
				Vertex.UV = Section.UVs[Index];
				Vertex.Tangent = FProcMeshTangent(
					Source.World.TransformVectorNoScale(Section.Tangents[Index].TangentX).GetSafeNormal(),
					Section.Tangents[Index].bFlipTangentY);
			}
			TArray<TArray<FVertex>> Pieces;
			Pieces.Add(MoveTemp(Original));
			for (const FBox2D& Cut : Cuts)
			{
				TArray<TArray<FVertex>> Next;
				for (const TArray<FVertex>& Piece : Pieces) { Subtract(Piece, Cut, Next); }
				Pieces = MoveTemp(Next);
				if (Pieces.IsEmpty()) { break; }
			}
			for (const TArray<FVertex>& Piece : Pieces) { AppendPolygon(Piece, Anchor, Output); }
		}
	}
}

void SubtractRectangle(const FBox2D& Input, const FBox2D& Cover, TArray<FBox2D>& Output)
{
	if (!Overlaps(Input, Cover))
	{
		Output.Add(Input);
		return;
	}
	const double X0 = FMath::Max(Input.Min.X, Cover.Min.X);
	const double X1 = FMath::Min(Input.Max.X, Cover.Max.X);
	const double Y0 = FMath::Max(Input.Min.Y, Cover.Min.Y);
	const double Y1 = FMath::Min(Input.Max.Y, Cover.Max.Y);
	auto Add = [&](double MinX, double MinY, double MaxX, double MaxY)
	{
		if (MaxX - MinX > GeometryTolerance && MaxY - MinY > GeometryTolerance)
		{
			Output.Add(FBox2D(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY)));
		}
	};
	Add(Input.Min.X, Input.Min.Y, X0, Input.Max.Y);
	Add(X1, Input.Min.Y, Input.Max.X, Input.Max.Y);
	Add(X0, Input.Min.Y, X1, Y0);
	Add(X0, Y1, X1, Input.Max.Y);
}

TArray<FPatch> MakePatches(TConstArrayView<FCityStreetSurfaceRegion> Regions,
	const TArray<uint8>& Renderable)
{
	TArray<int32> Order;
	for (int32 Index = 0; Index < Regions.Num(); ++Index)
	{
		if (Renderable[Index]) { Order.Add(Index); }
	}
	Order.Sort([&](int32 A, int32 B)
	{
		const int32 RankA = Regions[A].bJunction ? 2 : (Regions[A].bVehicle ? 1 : 0);
		const int32 RankB = Regions[B].bJunction ? 2 : (Regions[B].bVehicle ? 1 : 0);
		if (RankA != RankB) { return RankA > RankB; }
		if (Regions[A].Bounds.Min.X != Regions[B].Bounds.Min.X) { return Regions[A].Bounds.Min.X < Regions[B].Bounds.Min.X; }
		if (Regions[A].Bounds.Min.Y != Regions[B].Bounds.Min.Y) { return Regions[A].Bounds.Min.Y < Regions[B].Bounds.Min.Y; }
		return A < B;
	});
	TArray<FPatch> Patches;
	for (const int32 Index : Order)
	{
		const FCityStreetSurfaceRegion& Region = Regions[Index];
		TArray<FBox2D> Pieces = { Region.Bounds };
		for (const FPatch& Existing : Patches)
		{
			TArray<FBox2D> Next;
			for (const FBox2D& Piece : Pieces) { SubtractRectangle(Piece, Existing.Bounds, Next); }
			Pieces = MoveTemp(Next);
			if (Pieces.IsEmpty()) { break; }
		}
		for (const FBox2D& Piece : Pieces)
		{
			FPatch& Patch = Patches.AddDefaulted_GetRef();
			Patch.Bounds = Piece;
			Patch.Z = Region.SurfaceZ;
			Patch.CurbBaseZ = Region.CurbBaseZ;
			Patch.bVehicle = Region.bVehicle || Region.bJunction;
			Patch.bJunction = Region.bJunction;
			Patch.bInternalJunction = Region.bInternalJunction;
			Patch.bOpenAtStart = Region.bOpenAtStart;
			Patch.OriginalBounds = Region.Bounds;
			Patch.Direction = Region.Direction;
			Patch.Material = Region.RoadMaterial;
		}
	}
	return Patches;
}

void ConfigureRender(UPrimitiveComponent* Component)
{
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->PrimaryComponentTick.bCanEverTick = false;
}

UInstancedStaticMeshComponent* NewTileComponent(AActor* Owner, UStaticMesh* Mesh,
	UMaterialInterface* Material, TArray<TWeakObjectPtr<UActorComponent>>& Generated)
{
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(Component);
	Component->SetupAttachment(Owner->GetRootComponent());
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	if (Material) { Component->SetMaterial(0, Material); }
	ConfigureRender(Component);
	Component->RegisterComponent();
	Generated.Add(Component);
	return Component;
}

struct FTileFrame
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D Forward = FVector2D::ZeroVector;
	FVector2D Across = FVector2D::ZeroVector;
	double Yaw = 0.0;
	double Length = 0.0;
	double Width = 0.0;
};

FTileFrame MakeTileFrame(const FPatch& Patch)
{
	FTileFrame Frame;
	const bool bAlongX = FMath::Abs(Patch.Direction.X) >= FMath::Abs(Patch.Direction.Y);
	Frame.Yaw = bAlongX
		? (Patch.Direction.X < 0.0 ? 180.0 : 0.0)
		: (Patch.Direction.Y < 0.0 ? -90.0 : 90.0);
	Frame.Forward = bAlongX
		? FVector2D(Patch.Direction.X < 0.0 ? -1.0 : 1.0, 0.0)
		: FVector2D(0.0, Patch.Direction.Y < 0.0 ? -1.0 : 1.0);
	Frame.Across = FVector2D(Frame.Forward.Y, -Frame.Forward.X);
	Frame.Start = FVector2D(
		Frame.Forward.X > 0.0 || Frame.Across.X > 0.0 ? Patch.Bounds.Min.X : Patch.Bounds.Max.X,
		Frame.Forward.Y > 0.0 || Frame.Across.Y > 0.0 ? Patch.Bounds.Min.Y : Patch.Bounds.Max.Y);
	Frame.Length = bAlongX ? Patch.Bounds.GetSize().X : Patch.Bounds.GetSize().Y;
	Frame.Width = bAlongX ? Patch.Bounds.GetSize().Y : Patch.Bounds.GetSize().X;
	return Frame;
}

FTileFrame MakeOriginalTileFrame(const FPatch& Patch)
{
	FPatch Original = Patch;
	Original.Bounds = Patch.OriginalBounds;
	return MakeTileFrame(Original);
}

void TilePatch(const FPatch& Patch, UInstancedStaticMeshComponent* Component, bool bRoad)
{
	const FTileFrame Frame = MakeTileFrame(Patch);
	for (double U = 0.0; U < Frame.Length - GeometryTolerance; U += TileSize)
	{
		const double ThisLength = FMath::Min(TileSize, Frame.Length - U);
		for (double V = 0.0; V < Frame.Width - GeometryTolerance; V += TileSize)
		{
			const double ThisWidth = FMath::Min(TileSize, Frame.Width - V);
			const FVector2D XY = Frame.Start + Frame.Forward * U + Frame.Across * V;
			const FVector Location(XY.X, XY.Y, Patch.Z - (bRoad ? 0.f : 10.f));
			const FTransform Transform(FRotator(0.f, Frame.Yaw, 0.f), Location,
				FVector(ThisLength / TileSize, ThisWidth / TileSize, 1.0));
			Component->AddInstance(Transform, true);
		}
	}
}

enum class ECurbSide : uint8 { South, North, West, East };

void AddCurbRun(const FPatch& Patch, ECurbSide Side, double Start, double End,
	const FSectionData& Section, const FVector& Anchor, FMeshData& Output)
{
	const double Length = End - Start;
	if (Length <= GeometryTolerance) { return; }
	double Yaw = 0.0;
	FVector2D First = FVector2D::ZeroVector;
	FVector2D Forward = FVector2D::ZeroVector;
	switch (Side)
	{
	case ECurbSide::South:
		First = FVector2D(Start, Patch.Bounds.Min.Y); Forward = FVector2D(1.0, 0.0); break;
	case ECurbSide::North:
		First = FVector2D(End, Patch.Bounds.Max.Y); Forward = FVector2D(-1.0, 0.0); Yaw = 180.0; break;
	case ECurbSide::West:
		First = FVector2D(Patch.Bounds.Min.X, End); Forward = FVector2D(0.0, -1.0); Yaw = -90.0; break;
	case ECurbSide::East:
		First = FVector2D(Patch.Bounds.Max.X, Start); Forward = FVector2D(0.0, 1.0); Yaw = 90.0; break;
	}
	for (double Offset = 0.0; Offset < Length - GeometryTolerance; Offset += TileSize)
	{
		const double TileLength = FMath::Min(TileSize, Length - Offset);
		const FVector2D XY = First + Forward * Offset;
		const FTransform Pose(FRotator(0.0, Yaw, 0.0), FVector(XY.X, XY.Y, Patch.CurbBaseZ));
		for (int32 Triangle = 0; Triangle < Section.Triangles.Num(); Triangle += 3)
		{
			TArray<FVertex> Polygon;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Index = Section.Triangles[Triangle + Corner];
				FVertex& Vertex = Polygon.AddDefaulted_GetRef();
				Vertex.Position = Section.Positions[Index];
				Vertex.Normal = Section.Normals[Index];
				Vertex.UV = Section.UVs[Index];
				Vertex.Tangent = Section.Tangents[Index];
			}
			if (TileLength < TileSize - GeometryTolerance)
			{
				TArray<FVertex> Cropped;
				Clip(Polygon, 0, TileLength, false, Cropped);
				Polygon = MoveTemp(Cropped);
			}
			for (FVertex& Vertex : Polygon)
			{
				Vertex.Position = Pose.TransformPosition(Vertex.Position);
				Vertex.Normal = Pose.TransformVectorNoScale(Vertex.Normal).GetSafeNormal();
				Vertex.Tangent = FProcMeshTangent(
					Pose.TransformVectorNoScale(Vertex.Tangent.TangentX).GetSafeNormal(),
					Vertex.Tangent.bFlipTangentY);
			}
			AppendPolygon(Polygon, Anchor, Output);
		}
	}
}

void AppendExteriorCurbs(const TArray<FPatch>& Patches, const FSectionData& Section,
	const FVector& Anchor, FMeshData& Output)
{
	for (const FPatch& Patch : Patches)
	{
		if (Patch.bJunction && !Patch.bInternalJunction) { continue; }
		for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
		{
			const ECurbSide Side = static_cast<ECurbSide>(SideIndex);
			const bool bHorizontal = Side == ECurbSide::South || Side == ECurbSide::North;
			const double Edge = bHorizontal
				? (Side == ECurbSide::South ? Patch.Bounds.Min.Y : Patch.Bounds.Max.Y)
				: (Side == ECurbSide::West ? Patch.Bounds.Min.X : Patch.Bounds.Max.X);
			if (Patch.bOpenAtStart)
			{
				const bool bAlongX = FMath::Abs(Patch.Direction.X) >= FMath::Abs(Patch.Direction.Y);
				const ECurbSide OpenSide = bAlongX
					? (Patch.Direction.X >= 0.0 ? ECurbSide::West : ECurbSide::East)
					: (Patch.Direction.Y >= 0.0 ? ECurbSide::South : ECurbSide::North);
				const double OpenEdge = bAlongX
					? (Patch.Direction.X >= 0.0 ? Patch.OriginalBounds.Min.X : Patch.OriginalBounds.Max.X)
					: (Patch.Direction.Y >= 0.0 ? Patch.OriginalBounds.Min.Y : Patch.OriginalBounds.Max.Y);
				if (Side == OpenSide && FMath::Abs(Edge - OpenEdge) <= GeometryTolerance) { continue; }
			}
			const double Begin = bHorizontal ? Patch.Bounds.Min.X : Patch.Bounds.Min.Y;
			const double Finish = bHorizontal ? Patch.Bounds.Max.X : Patch.Bounds.Max.Y;
			TArray<double> Breaks = { Begin, Finish };
			for (const FPatch& Other : Patches)
			{
				const double OtherNear = bHorizontal ? Other.Bounds.Min.Y : Other.Bounds.Min.X;
				const double OtherFar = bHorizontal ? Other.Bounds.Max.Y : Other.Bounds.Max.X;
				if (OtherNear > Edge + GeometryTolerance || OtherFar < Edge - GeometryTolerance) { continue; }
				const double OtherBegin = bHorizontal ? Other.Bounds.Min.X : Other.Bounds.Min.Y;
				const double OtherFinish = bHorizontal ? Other.Bounds.Max.X : Other.Bounds.Max.Y;
				if (OtherBegin < Finish && OtherFinish > Begin)
				{
					Breaks.Add(FMath::Clamp(OtherBegin, Begin, Finish));
					Breaks.Add(FMath::Clamp(OtherFinish, Begin, Finish));
				}
			}
			Breaks.Sort();
			for (int32 Part = 1; Part < Breaks.Num(); ++Part)
			{
				const double Start = Breaks[Part - 1];
				const double End = Breaks[Part];
				if (End - Start <= GeometryTolerance) { continue; }
				const double Midpoint = (Start + End) * 0.5;
				const double Outside = Edge + ((Side == ECurbSide::South || Side == ECurbSide::West) ? -0.1 : 0.1);
				const FVector2D Probe = bHorizontal ? FVector2D(Midpoint, Outside) : FVector2D(Outside, Midpoint);
				bool bCovered = false;
				for (const FPatch& Other : Patches)
				{
					if (Other.Bounds.Min.X < Probe.X && Probe.X < Other.Bounds.Max.X
						&& Other.Bounds.Min.Y < Probe.Y && Probe.Y < Other.Bounds.Max.Y)
					{
						if (!Patch.bVehicle || Other.bVehicle)
						{
							bCovered = true;
							break;
						}
					}
				}
				if (!bCovered) { AddCurbRun(Patch, Side, Start, End, Section, Anchor, Output); }
			}
		}
	}
}

UProceduralMeshComponent* NewProcedural(AActor* Owner,
	TArray<TWeakObjectPtr<UActorComponent>>& Generated)
{
	UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(Component);
	Component->SetupAttachment(Owner->GetRootComponent());
	Component->SetMobility(EComponentMobility::Movable);
	ConfigureRender(Component);
	Component->bUseAsyncCooking = false;
	Component->RegisterComponent();
	Component->SetWorldLocation(Owner->GetActorLocation());
	Generated.Add(Component);
	return Component;
}

void TileDoubleRoad(const FPatch& Patch, UInstancedStaticMeshComponent* FullTiles,
	UStaticMesh* Mesh, const TArray<FSectionData>& Sections, AActor* Owner,
	TArray<TWeakObjectPtr<UActorComponent>>& Generated)
{
	const FTileFrame Frame = MakeOriginalTileFrame(Patch);
	for (double Offset = 0.0; Offset < Frame.Length - GeometryTolerance; Offset += TileSize)
	{
		const double SegmentLength = FMath::Min(TileSize, Frame.Length - Offset);
		const FVector2D XY = Frame.Start + Frame.Forward * Offset;
		const FVector2D Far = XY + Frame.Forward * SegmentLength + Frame.Across * Frame.Width;
		const FBox2D TileBounds(FVector2D(FMath::Min(XY.X, Far.X), FMath::Min(XY.Y, Far.Y)),
			FVector2D(FMath::Max(XY.X, Far.X), FMath::Max(XY.Y, Far.Y)));
		if (!Overlaps(TileBounds, Patch.Bounds)) { continue; }
		const FTransform Pose(FRotator(0.0, Frame.Yaw, 0.0), FVector(XY.X, XY.Y, Patch.Z));
		if (SegmentLength >= TileSize - GeometryTolerance
			&& Patch.Bounds.Min.X <= TileBounds.Min.X + GeometryTolerance
			&& Patch.Bounds.Min.Y <= TileBounds.Min.Y + GeometryTolerance
			&& Patch.Bounds.Max.X >= TileBounds.Max.X - GeometryTolerance
			&& Patch.Bounds.Max.Y >= TileBounds.Max.Y - GeometryTolerance)
		{
			FullTiles->AddInstance(Pose, true);
			continue;
		}
		UProceduralMeshComponent* Tail = NewProcedural(Owner, Generated);
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			const FSectionData& Section = Sections[SectionIndex];
			FMeshData Data;
			for (int32 Triangle = 0; Triangle < Section.Triangles.Num(); Triangle += 3)
			{
				TArray<FVertex> Polygon;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 Index = Section.Triangles[Triangle + Corner];
					FVertex& Vertex = Polygon.AddDefaulted_GetRef();
					Vertex.Position = Section.Positions[Index];
					Vertex.Normal = Section.Normals[Index];
					Vertex.UV = Section.UVs[Index];
					Vertex.Tangent = Section.Tangents[Index];
				}
				if (SegmentLength < TileSize - GeometryTolerance)
				{
					TArray<FVertex> Cropped;
					Clip(Polygon, 0, SegmentLength, false, Cropped);
					Polygon = MoveTemp(Cropped);
				}
				for (FVertex& Vertex : Polygon)
				{
					Vertex.Position = Pose.TransformPosition(Vertex.Position);
					Vertex.Normal = Pose.TransformVectorNoScale(Vertex.Normal).GetSafeNormal();
					Vertex.Tangent = FProcMeshTangent(
						Pose.TransformVectorNoScale(Vertex.Tangent.TangentX).GetSafeNormal(),
						Vertex.Tangent.bFlipTangentY);
				}
				TArray<FVertex> Visible;
				ClipToBounds(Polygon, Patch.Bounds, Visible);
				AppendPolygon(Visible, Owner->GetActorLocation(), Data);
			}
			if (Data.Positions.IsEmpty()) { continue; }
			Tail->CreateMeshSection(SectionIndex, Data.Positions, Data.Triangles,
				Data.Normals, Data.UVs, TArray<FColor>(), Data.Tangents, false);
			Tail->SetMaterial(SectionIndex, Mesh->GetMaterial(Section.MaterialIndex));
		}
	}
}
}

bool FCityPlotStreetSurface::Initialize(AActor* InOwner, UStaticMesh* InRoadSquare,
	UStaticMesh* InSidewalkFlat, UStaticMesh* InSidewalkStraight,
	UMaterialInterface* InPedestrianMaterial, UStaticMesh* InRoadDouble,
	UMaterialInterface* InJunctionMaterial)
{
	Restore();
	Owner = InOwner;
	RoadSquare = InRoadSquare;
	RoadDouble = InRoadDouble;
	SidewalkFlat = InSidewalkFlat;
	SidewalkStraight = InSidewalkStraight;
	PedestrianMaterial = InPedestrianMaterial;
	JunctionMaterial = InJunctionMaterial;
	return IsValid(InOwner) && IsValid(InOwner->GetRootComponent())
		&& IsValid(InRoadSquare) && IsValid(InSidewalkFlat)
		&& IsValid(InSidewalkStraight) && IsValid(InPedestrianMaterial)
		&& IsValid(InRoadDouble) && IsValid(InJunctionMaterial);
}

void FCityPlotStreetSurface::Restore()
{
	TSet<UInstancedStaticMeshComponent*> TouchedInstances;
	for (const FModifiedSource& Source : ModifiedSources)
	{
		UStaticMeshComponent* Component = Source.Component.Get();
		if (IsValid(Component) && Source.bInstanced)
		{
			UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component);
			if (IsValid(Instances) && Source.InstanceIndex >= 0
				&& Source.InstanceIndex < Instances->GetInstanceCount())
			{
				Instances->UpdateInstanceTransform(Source.InstanceIndex, Source.OriginalLocal,
					false, true, true);
				TouchedInstances.Add(Instances);
			}
		}
		else if (IsValid(Component))
		{
			Component->SetRelativeTransform(Source.OriginalLocal);
		}
		if (UStaticMeshComponent* Proxy = Source.CollisionProxy.Get()) { Proxy->DestroyComponent(); }
	}
	ModifiedSources.Reset();
	for (UInstancedStaticMeshComponent* Instances : TouchedInstances)
	{
		CGInstancedMesh::RewriteAllInstances(Instances);
	}
	for (const TWeakObjectPtr<UActorComponent>& WeakComponent : GeneratedComponents)
	{
		if (UActorComponent* Component = WeakComponent.Get())
		{
			Component->DestroyComponent();
		}
	}
	GeneratedComponents.Reset();
}

bool FCityPlotStreetSurface::Rebuild(TConstArrayView<FCityStreetSurfaceRegion> Regions,
	TArray<FCityStreetSurfaceRegion>* OutAppliedRegions)
{
	using namespace CityPlotStreetSurfacePrivate;
	if (OutAppliedRegions) { OutAppliedRegions->Reset(); }
	if (Regions.IsEmpty()) { Restore(); return true; }
	AActor* SurfaceOwner = Owner.Get();
	UStaticMesh* RoadMesh = RoadSquare.Get();
	UStaticMesh* DoubleMesh = RoadDouble.Get();
	UStaticMesh* FlatMesh = SidewalkFlat.Get();
	UStaticMesh* StraightMesh = SidewalkStraight.Get();
	UMaterialInterface* PedestrianMat = PedestrianMaterial.Get();
	UMaterialInterface* JunctionMat = JunctionMaterial.Get();
	if (!IsValid(SurfaceOwner) || !IsValid(SurfaceOwner->GetWorld())
		|| !IsValid(RoadMesh) || !IsValid(DoubleMesh) || !IsValid(FlatMesh)
		|| !IsValid(StraightMesh) || !IsValid(PedestrianMat)
		|| !IsValid(JunctionMat)) { Restore(); return false; }
	if (Regions.Num() > MaxRegions)
	{
		UE_LOG(LogTemp, Warning, TEXT("City street surface rejected %d regions (limit %d)."),
			Regions.Num(), MaxRegions);
		Restore();
		return false;
	}
	TArray<FSectionData> StraightSections;
	if (!ExtractSections(StraightMesh, StraightSections) || !StraightSections.IsValidIndex(1))
	{
		UE_LOG(LogTemp, Warning, TEXT("City street curb mesh has no CPU-readable section 1: %s"),
			*StraightMesh->GetPathName());
		Restore();
		return false;
	}
	UMaterialInterface* CurbMaterial = StraightMesh->GetMaterial(StraightSections[1].MaterialIndex);
	if (!IsValid(CurbMaterial)) { Restore(); return false; }
	TArray<FSectionData> DoubleSections;
	if (!ExtractSections(DoubleMesh, DoubleSections) || DoubleSections.Num() != 2
		|| !IsValid(DoubleMesh->GetMaterial(DoubleSections[0].MaterialIndex))
		|| !IsValid(DoubleMesh->GetMaterial(DoubleSections[1].MaterialIndex)))
	{
		UE_LOG(LogTemp, Warning, TEXT("City street double road needs two CPU-readable material sections: %s"),
			*DoubleMesh->GetPathName());
		Restore();
		return false;
	}

	TArray<uint8> Renderable;
	Renderable.Init(0, Regions.Num());
	TArray<uint8> HasGroundSource, HasRoadSource;
	HasGroundSource.Init(0, Regions.Num());
	HasRoadSource.Init(0, Regions.Num());
	FBox Query(ForceInit);
	for (int32 Index = 0; Index < Regions.Num(); ++Index)
	{
		if (!ValidRegion(Regions[Index])) { continue; }
		Renderable[Index] = 1;
		const FBox2D& Bounds = Regions[Index].Bounds;
		Query += FVector(Bounds.Min.X, Bounds.Min.Y, Regions[Index].SurfaceZ - 100.f);
		Query += FVector(Bounds.Max.X, Bounds.Max.Y, Regions[Index].SurfaceZ + 100.f);
	}
	if (!Query.IsValid) { Restore(); return false; }

	TArray<FSource> Sources;
	TMap<UStaticMesh*, TArray<FSectionData>> MeshSections;
	TSet<UStaticMesh*> FailedMeshes;
	TMap<UStaticMeshComponent*, TSet<int32>> SeenSources;
	bool bSplineJunctionConflict = false;
	auto Consider = [&](UStaticMeshComponent* Component, UInstancedStaticMeshComponent* Instances,
		UStaticMesh* Mesh, bool bRoadSource, int32 InstanceIndex,
		const FTransform& Local, const FTransform& World)
	{
		if (!IsHorizontal(World)) { return; }
		const FBox WorldBounds = Mesh->GetBoundingBox().TransformBy(World.ToMatrixWithScale());
		if (!WorldBounds.IsValid || !WorldBounds.Intersect(Query)) { return; }
		TArray<int32> Intersections;
		for (int32 RegionIndex = 0; RegionIndex < Regions.Num(); ++RegionIndex)
		{
			if (Renderable[RegionIndex] && IsSourceForRegion(Regions[RegionIndex], bRoadSource)
				&& Overlaps(Flatten(WorldBounds), Regions[RegionIndex].Bounds)
				&& FMath::Abs(WorldBounds.Max.Z - Regions[RegionIndex].SurfaceZ) <= 100.f)
			{
				Intersections.Add(RegionIndex);
			}
		}
		if (Intersections.IsEmpty()) { return; }
		bool bReadable = !FailedMeshes.Contains(Mesh);
		if (bReadable && !MeshSections.Contains(Mesh))
		{
			TArray<FSectionData> Sections;
			bReadable = ExtractSections(Mesh, Sections);
			if (bReadable) { MeshSections.Add(Mesh, MoveTemp(Sections)); }
			else
			{
				FailedMeshes.Add(Mesh);
				UE_LOG(LogTemp, Warning, TEXT("City street source left intact: CPU geometry unavailable for %s"),
					*Mesh->GetPathName());
			}
		}
		if (bReadable)
		{
			for (const FSectionData& Section : MeshSections.FindChecked(Mesh))
			{
				if (!IsValid(Component->GetMaterial(Section.MaterialIndex)))
				{
					bReadable = false;
					break;
				}
			}
		}
		if (!bReadable)
		{
			for (const int32 RegionIndex : Intersections) { Renderable[RegionIndex] = 0; }
			return;
		}
		TSet<int32>& SeenIndices = SeenSources.FindOrAdd(Component);
		if (SeenIndices.Contains(InstanceIndex)) { return; }
		SeenIndices.Add(InstanceIndex);
		FSource& Source = Sources.AddDefaulted_GetRef();
		Source.Component = Component;
		Source.Instances = Instances;
		Source.Mesh = Mesh;
		Source.bRoadSource = bRoadSource;
		Source.Index = InstanceIndex;
		Source.Local = Local;
		Source.World = World;
		Source.Bounds = WorldBounds;
		Source.RegionIndices = MoveTemp(Intersections);
		for (const int32 RegionIndex : Source.RegionIndices)
		{
			(bRoadSource ? HasRoadSource : HasGroundSource)[RegionIndex] = 1;
		}
	};
	// A hidden ISM instance is absent from overlap queries. Evaluate its authored pose.
	for (const FModifiedSource& Previous : ModifiedSources)
	{
		UStaticMeshComponent* Component = Previous.Component.Get();
		AActor* Actor = IsValid(Component) ? Component->GetOwner() : nullptr;
		ULevel* Level = IsValid(Actor) ? Actor->GetLevel() : nullptr;
		if (!IsValid(Component) || !Component->IsRegistered() || !Component->IsVisible()
			|| !IsValid(Actor) || Actor->IsHidden() || Actor->IsActorBeingDestroyed()
			|| Actor->GetWorld() != SurfaceOwner->GetWorld() || !IsValid(Level)
			|| !Level->bIsVisible || Level->bIsBeingRemoved
			|| Component->GetStaticMesh() != Previous.Mesh.Get()) { continue; }
		UInstancedStaticMeshComponent* Instances = Previous.bInstanced
			? Cast<UInstancedStaticMeshComponent>(Component) : nullptr;
		if (Previous.bInstanced && (!Instances || Previous.InstanceIndex < 0
			|| Previous.InstanceIndex >= Instances->GetInstanceCount())) { continue; }
		FTransform AuthoredWorld = Previous.OriginalLocal;
		if (Previous.bInstanced)
		{
			AuthoredWorld = Previous.OriginalLocal * Component->GetComponentTransform();
		}
		else if (const USceneComponent* Parent = Component->GetAttachParent())
		{
			AuthoredWorld = Previous.OriginalLocal * Parent->GetSocketTransform(Component->GetAttachSocketName());
		}
		Consider(Component, Instances, Previous.Mesh.Get(), Previous.bRoadSource,
			Previous.InstanceIndex, Previous.OriginalLocal, AuthoredWorld);
	}

	for (TActorIterator<AActor> It(SurfaceOwner->GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == SurfaceOwner || Actor->IsHidden()
			|| Actor->IsActorBeingDestroyed()) { continue; }
		for (UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(Actor))
		{
			if (!IsValid(Component) || !Component->IsRegistered() || !Component->IsVisible()) { continue; }
			UStaticMesh* Mesh = Component->GetStaticMesh();
			if (!IsValid(Mesh)) { continue; }
			const FString MeshName = Mesh->GetName();
			const bool bSidewalkMesh = MeshName.StartsWith(TEXT("SM_Sidewalk"))
				&& !MeshName.Contains(TEXT("Bend")) && !MeshName.Contains(TEXT("Step"));
			const bool bSidewalkTile = Mesh == RoadMesh && Component->GetMaterial(0) == PedestrianMat;
			const bool bRoadMesh = MeshName == TEXT("SM_Road_Square")
				|| MeshName == TEXT("SM_Road_Double") || MeshName == TEXT("SM_Double_Road2")
				|| MeshName == TEXT("SM_Road_Double_With_Edge");
			const UMaterialInterface* SourceMaterial = Component->GetMaterial(0);
			const bool bRoadSurface = bRoadMesh && !bSidewalkTile && SourceMaterial
				&& (SourceMaterial->GetPathName().Contains(TEXT("/MI_Road"))
					|| SourceMaterial->GetPathName().Contains(TEXT("/MI_Asphalt"))
					|| SourceMaterial->GetPathName().Contains(TEXT("/MI_White_Lines")));
			if (Cast<USplineMeshComponent>(Component))
			{
				if (bRoadMesh && !bSidewalkTile)
				{
					const FBox SplineBounds = Component->Bounds.GetBox();
					for (const FCityStreetSurfaceRegion& Region : Regions)
					{
						if (IsExternalJunction(Region) && ValidRegion(Region)
							&& Overlaps(Flatten(SplineBounds), Region.Bounds)
							&& FMath::Abs(SplineBounds.Max.Z - Region.SurfaceZ) <= 100.f)
						{
							bSplineJunctionConflict = true;
							break;
						}
					}
				}
				continue;
			}
			if (!bSidewalkMesh && !bSidewalkTile && !bRoadSurface) { continue; }
			if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
			{
				// Restored transforms must be queried before the HISM cluster tree catches up.
				const TArray<int32> Indices =
					Instances->UInstancedStaticMeshComponent::GetInstancesOverlappingBox(Query, true);
				for (const int32 InstanceIndex : Indices)
				{
					FTransform World, Local;
					if (Instances->GetInstanceTransform(InstanceIndex, World, true)
						&& Instances->GetInstanceTransform(InstanceIndex, Local, false))
					{
						Consider(Component, Instances, Mesh, bRoadSurface, InstanceIndex, Local, World);
					}
				}
			}
			else
			{
				Consider(Component, nullptr, Mesh, bRoadSurface, INDEX_NONE,
					Component->GetRelativeTransform(), Component->GetComponentTransform());
			}
		}
	}
	if (bSplineJunctionConflict)
	{
		UE_LOG(LogTemp, Warning, TEXT("City street junction rejected a spline road source."));
		Restore();
		return false;
	}
	for (int32 Index = 0; Index < Renderable.Num(); ++Index)
	{
		if (IsExternalJunction(Regions[Index])
			&& !IsSourceAvailable(Regions[Index], Renderable[Index] != 0,
				HasGroundSource[Index] != 0, HasRoadSource[Index] != 0))
		{
			UE_LOG(LogTemp, Warning, TEXT("City street junction left intact: road source unavailable."));
			Restore();
			return false;
		}
		Renderable[Index] = IsSourceAvailable(Regions[Index], Renderable[Index] != 0,
			HasGroundSource[Index] != 0, HasRoadSource[Index] != 0) ? 1 : 0;
	}
	TArray<FBox2D> Cuts;
	for (int32 Index = 0; Index < Regions.Num(); ++Index)
	{
		if (Renderable[Index]) { Cuts.Add(Regions[Index].Bounds); }
	}
	if (Cuts.IsEmpty()) { Restore(); return false; }
	const TArray<FPatch> Patches = MakePatches(Regions, Renderable);
	for (const FPatch& Patch : Patches)
	{
		if (Patch.bVehicle && !Patch.bJunction
			&& !FMath::IsNearlyEqual(MakeOriginalTileFrame(Patch).Width, 1000.0, 1.0))
		{
			UE_LOG(LogTemp, Warning, TEXT("City street double road rejected a non-1000cm patch."));
			Restore();
			return false;
		}
	}
	// Render geometry is rebuilt; source collision proxies are diffed below.
	for (const TWeakObjectPtr<UActorComponent>& WeakComponent : GeneratedComponents)
	{
		if (UActorComponent* Component = WeakComponent.Get()) { Component->DestroyComponent(); }
	}
	GeneratedComponents.Reset();
	TArray<FModifiedSource> PreviousSources = MoveTemp(ModifiedSources);
	TSet<UInstancedStaticMeshComponent*> ChangedInstances;
	TArray<uint8> ReusedPrevious;
	ReusedPrevious.Init(0, PreviousSources.Num());

	const FVector Anchor = SurfaceOwner->GetActorLocation();
	TMap<UMaterialInterface*, FMeshData> OriginalFloor;
	for (const FSource& Source : Sources)
	{
		TArray<FBox2D> SourceCuts;
		for (const int32 RegionIndex : Source.RegionIndices)
		{
			if (Renderable[RegionIndex]) { SourceCuts.Add(Regions[RegionIndex].Bounds); }
		}
		if (SourceCuts.IsEmpty()) { continue; }
		int32 ReuseIndex = INDEX_NONE;
		for (int32 Index = 0; Index < PreviousSources.Num(); ++Index)
		{
			const FModifiedSource& Previous = PreviousSources[Index];
			if (!ReusedPrevious[Index] && Previous.Component.Get() == Source.Component
				&& Previous.InstanceIndex == Source.Index)
			{
				ReuseIndex = Index;
				break;
			}
		}
		const FModifiedSource* Previous = ReuseIndex != INDEX_NONE ? &PreviousSources[ReuseIndex] : nullptr;
		const bool bReuseProxy = Previous && Previous->Mesh.Get() == Source.Mesh
			&& Previous->OriginalLocal.Equals(Source.Local)
			&& Previous->OriginalWorld.Equals(Source.World)
			&& IsValid(Previous->CollisionProxy.Get());
		UStaticMeshComponent* Proxy = bReuseProxy ? Previous->CollisionProxy.Get() : nullptr;
		if (!bReuseProxy)
		{
			Proxy = NewObject<UStaticMeshComponent>(SurfaceOwner, NAME_None, RF_Transient);
			SurfaceOwner->AddInstanceComponent(Proxy);
			Proxy->SetupAttachment(SurfaceOwner->GetRootComponent());
			Proxy->SetMobility(EComponentMobility::Movable);
			Proxy->SetStaticMesh(Source.Mesh);
			Proxy->SetCollisionEnabled(Source.Component->GetCollisionEnabled());
			Proxy->SetCollisionObjectType(Source.Component->GetCollisionObjectType());
			Proxy->SetCollisionResponseToChannels(Source.Component->GetCollisionResponseToChannels());
			Proxy->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
			Proxy->SetGenerateOverlapEvents(false);
			Proxy->SetCanEverAffectNavigation(false);
			Proxy->SetCastShadow(false);
			Proxy->PrimaryComponentTick.bCanEverTick = false;
			Proxy->RegisterComponent();
			Proxy->SetWorldTransform(Source.World);
			Proxy->SetVisibility(false);
			Proxy->SetHiddenInGame(true);
			bool bHidden = true;
			if (Source.Instances)
			{
				FTransform Hidden = Source.Local;
				Hidden.SetScale3D(FVector::ZeroVector);
				bHidden = Source.Instances->UpdateInstanceTransform(Source.Index, Hidden, false, true, true);
				ChangedInstances.Add(Source.Instances);
			}
			else
			{
				Source.Component->SetRelativeScale3D(FVector::ZeroVector);
			}
			if (!bHidden)
			{
				Proxy->DestroyComponent();
				for (int32 PreviousIndex = 0; PreviousIndex < PreviousSources.Num(); ++PreviousIndex)
				{
					if (!ReusedPrevious[PreviousIndex]) { ModifiedSources.Add(PreviousSources[PreviousIndex]); }
				}
				Restore();
				return false;
			}
			if (Previous)
			{
				if (UStaticMeshComponent* OldProxy = Previous->CollisionProxy.Get())
				{
					OldProxy->DestroyComponent();
				}
			}
		}
		if (ReuseIndex != INDEX_NONE) { ReusedPrevious[ReuseIndex] = 1; }
		FModifiedSource& Modified = ModifiedSources.AddDefaulted_GetRef();
		Modified.Component = Source.Component;
		Modified.InstanceIndex = Source.Index;
		Modified.OriginalLocal = Source.Local;
		Modified.OriginalWorld = Source.World;
		Modified.Mesh = Source.Mesh;
		Modified.CollisionProxy = Proxy;
		Modified.bInstanced = Source.Instances != nullptr;
		Modified.bRoadSource = Source.bRoadSource;
		AppendSource(Source, SourceCuts, MeshSections.FindChecked(Source.Mesh), Anchor, OriginalFloor);
	}
	for (int32 Index = 0; Index < PreviousSources.Num(); ++Index)
	{
		if (ReusedPrevious[Index]) { continue; }
		const FModifiedSource& Previous = PreviousSources[Index];
		if (UStaticMeshComponent* Component = Previous.Component.Get())
		{
			if (Previous.bInstanced)
			{
				if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
				{
					if (Previous.InstanceIndex >= 0 && Previous.InstanceIndex < Instances->GetInstanceCount())
					{
						Instances->UpdateInstanceTransform(Previous.InstanceIndex, Previous.OriginalLocal,
							false, true, true);
						ChangedInstances.Add(Instances);
					}
				}
			}
			else { Component->SetRelativeTransform(Previous.OriginalLocal); }
		}
		if (UStaticMeshComponent* Proxy = Previous.CollisionProxy.Get()) { Proxy->DestroyComponent(); }
	}

	for (auto& Pair : OriginalFloor)
	{
		if (Pair.Value.Positions.IsEmpty()) { continue; }
		UProceduralMeshComponent* Floor = NewProcedural(SurfaceOwner, GeneratedComponents);
		Floor->CreateMeshSection(0, Pair.Value.Positions, Pair.Value.Triangles,
			Pair.Value.Normals, Pair.Value.UVs, TArray<FColor>(), Pair.Value.Tangents, false);
		Floor->SetMaterial(0, Pair.Key);
	}

	UInstancedStaticMeshComponent* DoubleRoadComponent = nullptr;
	UInstancedStaticMeshComponent* JunctionComponent = nullptr;
	UInstancedStaticMeshComponent* PedestrianComponent = nullptr;
	for (const FPatch& Patch : Patches)
	{
		if (Patch.bJunction)
		{
			if (!JunctionComponent)
			{
				JunctionComponent = NewTileComponent(SurfaceOwner, RoadMesh,
					JunctionMat, GeneratedComponents);
			}
			TilePatch(Patch, JunctionComponent, true);
		}
		else if (Patch.bVehicle)
		{
			if (!DoubleRoadComponent)
			{
				DoubleRoadComponent = NewTileComponent(SurfaceOwner, DoubleMesh,
					nullptr, GeneratedComponents);
			}
			TileDoubleRoad(Patch, DoubleRoadComponent, DoubleMesh, DoubleSections,
				SurfaceOwner, GeneratedComponents);
		}
		else
		{
			if (!PedestrianComponent)
			{
				PedestrianComponent = NewTileComponent(SurfaceOwner, FlatMesh,
					PedestrianMat, GeneratedComponents);
			}
			TilePatch(Patch, PedestrianComponent, false);
		}
	}
	FMeshData Curbs;
	AppendExteriorCurbs(Patches, StraightSections[1], Anchor, Curbs);
	if (!Curbs.Positions.IsEmpty())
	{
		UProceduralMeshComponent* Curb = NewProcedural(SurfaceOwner, GeneratedComponents);
		Curb->CreateMeshSection(0, Curbs.Positions, Curbs.Triangles,
			Curbs.Normals, Curbs.UVs, TArray<FColor>(), Curbs.Tangents, false);
		Curb->SetMaterial(0, CurbMaterial);
	}
	if (OutAppliedRegions)
	{
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			if (Renderable[Index]) { OutAppliedRegions->Add(Regions[Index]); }
		}
	}
	// 원본 묶음의 개별 숨김·복원은 모바일(GPUScene 꺼짐)에서 렌더 대응을 틀어 묶음 전체를 엉뚱하게 그린다.
	// 이번 재구성에서 숨김 상태가 실제로 바뀐 묶음만 다시 넣는다 — 배치 미리보기는 초당 여러 번 돈다.
	const double RewriteStart = FPlatformTime::Seconds();
	for (UInstancedStaticMeshComponent* Instances : ChangedInstances)
	{
		if (IsValid(Instances)) { CGInstancedMesh::RewriteAllInstances(Instances); }
	}
	UE_LOG(LogTemp, Verbose, TEXT("[StreetSurface] rewrite %d ISM %.1f ms"),
		ChangedInstances.Num(), (FPlatformTime::Seconds() - RewriteStart) * 1000.0);
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStreetSurfaceCutGeometryTest,
	"CGR.CityWalkways.SurfaceCutGeometry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCityStreetSurfaceCutGeometryTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotStreetSurfacePrivate;
	auto MakeVertex = [](double X, double Y)
	{
		FVertex Vertex;
		Vertex.Position = FVector(X, Y, 100.0);
		Vertex.UV = FVector2D(1.25 + 0.4 * X + 0.1 * Y, -2.0 + 0.2 * X - 0.3 * Y);
		return Vertex;
	};
	const TArray<FVertex> Original = {
		MakeVertex(2.0, 3.0), MakeVertex(8.0, 3.0), MakeVertex(2.0, 9.0)
	};
	const FBox2D Cut(FVector2D(3.0, 4.0), FVector2D(5.0, 6.0));
	TArray<TArray<FVertex>> Pieces;
	Subtract(Original, Cut, Pieces);
	FMeshData Remaining;
	for (const TArray<FVertex>& Piece : Pieces)
	{
		AppendPolygon(Piece, FVector::ZeroVector, Remaining);
	}
	TestTrue(TEXT("The cut leaves renderable triangles"), !Remaining.Triangles.IsEmpty());

	double RemainingArea = 0.0;
	double InsideCutArea = 0.0;
	for (int32 TriangleIndex = 0; TriangleIndex < Remaining.Triangles.Num(); TriangleIndex += 3)
	{
		TArray<FVertex> Triangle;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 Index = Remaining.Triangles[TriangleIndex + Corner];
			const FVector& Position = Remaining.Positions[Index];
			const FVector2D& UV = Remaining.UVs[Index];
			TestTrue(TEXT("Clipped vertices keep the source's affine UVs"),
				FMath::IsNearlyEqual(UV.X, 1.25 + 0.4 * Position.X + 0.1 * Position.Y, 0.0001)
				&& FMath::IsNearlyEqual(UV.Y, -2.0 + 0.2 * Position.X - 0.3 * Position.Y, 0.0001));
			TestTrue(TEXT("The source plane height remains unchanged"),
				FMath::IsNearlyEqual(Position.Z, 100.0, 0.0001));
			FVertex& Vertex = Triangle.AddDefaulted_GetRef();
			Vertex.Position = Position;
		}
		const FVector AB = Triangle[1].Position - Triangle[0].Position;
		const FVector AC = Triangle[2].Position - Triangle[0].Position;
		RemainingArea += FMath::Abs(FVector::CrossProduct(AB, AC).Z) * 0.5;

		TArray<FVertex> Clipped;
		Clip(Triangle, 0, Cut.Min.X, true, Clipped);
		Clip(Clipped, 0, Cut.Max.X, false, Triangle);
		Clip(Triangle, 1, Cut.Min.Y, true, Clipped);
		Clip(Clipped, 1, Cut.Max.Y, false, Triangle);
		double TriangleCutArea = 0.0;
		for (int32 Index = 0; Index < Triangle.Num(); ++Index)
		{
			const FVector& A = Triangle[Index].Position;
			const FVector& B = Triangle[(Index + 1) % Triangle.Num()].Position;
			TriangleCutArea += (A.X * B.Y - B.X * A.Y) * 0.5;
		}
		InsideCutArea += FMath::Abs(TriangleCutArea);
	}
	TestTrue(TEXT("Remaining triangle area equals source area less the rectangular cut"),
		FMath::IsNearlyEqual(RemainingArea, 14.0, 0.0001));
	TestTrue(TEXT("No rendered triangle occupies the cut interior"),
		FMath::IsNearlyZero(InsideCutArea, 0.0001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStreetSurfacePatchPriorityTest,
	"CGR.CityWalkways.SurfacePatchPriority",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCityStreetSurfacePatchPriorityTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotStreetSurfacePrivate;
	FCityStreetSurfaceRegion Pedestrian;
	Pedestrian.Bounds = FBox2D(FVector2D(0.0, 0.0), FVector2D(6.0, 4.0));
	Pedestrian.SurfaceZ = 120.0f;
	Pedestrian.CurbBaseZ = 108.0f;
	FCityStreetSurfaceRegion Vehicle;
	Vehicle.Bounds = FBox2D(FVector2D(2.0, 1.0), FVector2D(5.0, 5.0));
	Vehicle.SurfaceZ = 108.0f;
	Vehicle.CurbBaseZ = 106.0f;
	Vehicle.bVehicle = true;
	const TArray<FCityStreetSurfaceRegion> Regions = { Pedestrian, Vehicle };
	const TArray<uint8> Renderable = { 1, 1 };
	const TArray<FPatch> Patches = MakePatches(Regions, Renderable);
	double VehicleArea = 0.0;
	double PedestrianArea = 0.0;
	for (int32 Index = 0; Index < Patches.Num(); ++Index)
	{
		const FPatch& Patch = Patches[Index];
		const double Area = Patch.Bounds.GetSize().X * Patch.Bounds.GetSize().Y;
		TestTrue(TEXT("All patch pieces have positive area"), Area > 0.0);
		if (Patch.bVehicle)
		{
			VehicleArea += Area;
			TestTrue(TEXT("Vehicle patch keeps its surface and curb heights"),
				Patch.Z == Vehicle.SurfaceZ && Patch.CurbBaseZ == Vehicle.CurbBaseZ);
		}
		else
		{
			PedestrianArea += Area;
			TestTrue(TEXT("Pedestrian patch keeps its surface and curb heights"),
				Patch.Z == Pedestrian.SurfaceZ && Patch.CurbBaseZ == Pedestrian.CurbBaseZ);
		}
		for (int32 OtherIndex = 0; OtherIndex < Index; ++OtherIndex)
		{
			const FBox2D& Other = Patches[OtherIndex].Bounds;
			const double SharedX = FMath::Min(Patch.Bounds.Max.X, Other.Max.X)
				- FMath::Max(Patch.Bounds.Min.X, Other.Min.X);
			const double SharedY = FMath::Min(Patch.Bounds.Max.Y, Other.Max.Y)
				- FMath::Max(Patch.Bounds.Min.Y, Other.Min.Y);
			TestTrue(TEXT("No two rendered patches overlap in XY"),
				SharedX <= 0.0001 || SharedY <= 0.0001);
		}
	}
	TestTrue(TEXT("Vehicle has the full priority area even when supplied second"),
		FMath::IsNearlyEqual(VehicleArea, 12.0, 0.0001));
	TestTrue(TEXT("Pedestrian retains only its area outside the vehicle"),
		FMath::IsNearlyEqual(PedestrianArea, 15.0, 0.0001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStreetSurfaceDoubleLaneFrameTest,
	"CGR.CityWalkways.DoubleLaneFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCityStreetSurfaceDoubleLaneFrameTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotStreetSurfacePrivate;
	for (const FVector2D Direction : { FVector2D(1, 0), FVector2D(-1, 0),
		FVector2D(0, 1), FVector2D(0, -1) })
	{
		FPatch Patch;
		Patch.Direction = Direction;
		Patch.Bounds = FMath::Abs(Direction.X) > 0.0
			? FBox2D(FVector2D(100, 200), FVector2D(1600, 1200))
			: FBox2D(FVector2D(100, 200), FVector2D(1100, 1700));
		const FTileFrame Frame = MakeTileFrame(Patch);
		const FTransform Pose(FRotator(0, Frame.Yaw, 0), FVector(Frame.Start.X, Frame.Start.Y, 0));
		const FVector First = Pose.TransformPosition(FVector(0, 0, 0));
		const FVector Seam = Pose.TransformPosition(FVector(0, -500, 0));
		const FVector FarEdge = Pose.TransformPosition(FVector(0, -1000, 0));
		const FVector End = Pose.TransformPosition(FVector(500, 0, 0));
		TestTrue(TEXT("Two stock 500cm lanes fill exactly 1000cm across"),
			FMath::IsNearlyEqual(Frame.Width, 1000.0, 0.0001)
			&& FMath::IsNearlyEqual(FVector::Dist2D(First, Seam), 500.0, 0.0001)
			&& FMath::IsNearlyEqual(FVector::Dist2D(Seam, FarEdge), 500.0, 0.0001));
		TestTrue(TEXT("The stock lane seam sits on the route centerline"),
			FMath::IsNearlyEqual(Seam.X, Patch.Bounds.GetCenter().X, 0.0001)
			|| FMath::IsNearlyEqual(Seam.Y, Patch.Bounds.GetCenter().Y, 0.0001));
		for (const FVector& Point : { First, Seam, FarEdge, End })
		{
			TestTrue(TEXT("Rotated double-road vertices stay within the region bounds"),
				Point.X >= Patch.Bounds.Min.X - 0.0001 && Point.X <= Patch.Bounds.Max.X + 0.0001
				&& Point.Y >= Patch.Bounds.Min.Y - 0.0001 && Point.Y <= Patch.Bounds.Max.Y + 0.0001);
		}
		TestTrue(TEXT("The 500cm tile axis follows the requested road direction"),
			FVector2D::DotProduct(FVector2D(End.X - First.X, End.Y - First.Y),
				Direction) > 499.999);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStreetSurfaceJunctionCurbTest,
	"CGR.CityWalkways.JunctionCurbAndPriority",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCityStreetSurfaceJunctionCurbTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotStreetSurfacePrivate;
	FCityStreetSurfaceRegion Vehicle;
	Vehicle.Bounds = FBox2D(FVector2D(0, 0), FVector2D(1000, 1000));
	Vehicle.bVehicle = true;
	FCityStreetSurfaceRegion Junction;
	Junction.Bounds = FBox2D(FVector2D(0, 0), FVector2D(500, 1000));
	Junction.bVehicle = true;
	Junction.bJunction = true;
	const TArray<FCityStreetSurfaceRegion> Regions = { Vehicle, Junction };
	const TArray<uint8> Renderable = { 1, 1 };
	const TArray<FPatch> Patches = MakePatches(Regions, Renderable);
	if (!TestTrue(TEXT("The junction owns its overlapping XY area before ordinary road"),
		Patches.Num() == 2 && Patches[0].bJunction
		&& Patches[0].Bounds == Junction.Bounds
		&& Patches[1].Bounds == FBox2D(FVector2D(500, 0), FVector2D(1000, 1000))))
	{
		return false;
	}
	FSectionData CurbSection;
	CurbSection.Positions = { FVector(0, -25, 0), FVector(500, -25, 0), FVector(0, 0, 12) };
	CurbSection.Triangles = { 0, 1, 2 };
	CurbSection.Normals = { FVector::UpVector, FVector::UpVector, FVector::UpVector };
	CurbSection.UVs = { FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(0, 1) };
	CurbSection.Tangents = { FProcMeshTangent(), FProcMeshTangent(), FProcMeshTangent() };
	FMeshData JunctionCurbs;
	const TArray<FPatch> JunctionOnly = { Patches[0] };
	AppendExteriorCurbs(JunctionOnly, CurbSection, FVector::ZeroVector, JunctionCurbs);
	TestTrue(TEXT("A junction never generates a curb across its road mouth"),
		JunctionCurbs.Positions.IsEmpty());
	FMeshData OrdinaryCurbs;
	const TArray<FPatch> OrdinaryOnly = { Patches[1] };
	AppendExteriorCurbs(OrdinaryOnly, CurbSection, FVector::ZeroVector, OrdinaryCurbs);
	TestTrue(TEXT("The curb fixture generates geometry for an ordinary road"),
		!OrdinaryCurbs.Positions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStreetSurfaceInternalJunctionSourceTest,
	"CGR.CityWalkways.InternalJunctionSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCityStreetSurfaceInternalJunctionSourceTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotStreetSurfacePrivate;
	FCityStreetSurfaceRegion External;
	External.bVehicle = true;
	External.bJunction = true;
	FCityStreetSurfaceRegion Internal = External;
	Internal.bInternalJunction = true;
	TestTrue(TEXT("External junction cuts a road source only"),
		IsSourceForRegion(External, true) && !IsSourceForRegion(External, false));
	TestTrue(TEXT("Internal junction cuts a sidewalk source only"),
		IsSourceForRegion(Internal, false) && !IsSourceForRegion(Internal, true));
	TestTrue(TEXT("External junction requires an existing road"),
		!IsSourceAvailable(External, true, true, false)
		&& IsSourceAvailable(External, true, false, true));
	TestTrue(TEXT("Internal junction requires an existing sidewalk"),
		!IsSourceAvailable(Internal, true, false, true)
		&& IsSourceAvailable(Internal, true, true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStreetSurfaceInternalJunctionShapeTest,
	"CGR.CityWalkways.InternalJunctionShape",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCityStreetSurfaceInternalJunctionShapeTest::RunTest(const FString& Parameters)
{
	using namespace CityPlotStreetSurfacePrivate;
	FCityStreetSurfaceRegion Junction;
	Junction.Bounds = FBox2D(FVector2D(1000, 1000), FVector2D(2000, 2000));
	Junction.bVehicle = true;
	Junction.bJunction = true;
	Junction.bInternalJunction = true;
	FCityStreetSurfaceRegion Horizontal;
	Horizontal.Bounds = FBox2D(FVector2D(0, 1000), FVector2D(3000, 2000));
	Horizontal.bVehicle = true;
	FCityStreetSurfaceRegion Vertical = Horizontal;
	Vertical.Bounds = FBox2D(FVector2D(1000, 0), FVector2D(2000, 3000));
	Vertical.Direction = FVector2D(0, 1);
	const TArray<FCityStreetSurfaceRegion> Regions = { Horizontal, Vertical, Junction };
	const TArray<uint8> Renderable = { 1, 1, 1 };
	const TArray<FPatch> Patches = MakePatches(Regions, Renderable);
	TestEqual(TEXT("A crossing has one junction and four full-width approaches"), Patches.Num(), 5);
	if (Patches.Num() != 5) { return false; }
	TestTrue(TEXT("Junction takes priority over both roads"),
		Patches[0].bJunction && Patches[0].bInternalJunction && Patches[0].Bounds == Junction.Bounds);
	for (int32 Index = 1; Index < Patches.Num(); ++Index)
	{
		TestTrue(TEXT("Approach remains a complete two-lane road"),
			!Patches[Index].bJunction && Patches[Index].bVehicle
			&& FMath::IsNearlyEqual(MakeTileFrame(Patches[Index]).Width, 1000.0, 0.01));
		for (int32 Other = 0; Other < Index; ++Other)
		{
			TestFalse(TEXT("Junction and approaches have no XY overlap"),
				Overlaps(Patches[Index].Bounds, Patches[Other].Bounds));
		}
	}
	FSectionData CurbSection;
	CurbSection.Positions = { FVector(0, -25, 0), FVector(500, -25, 0), FVector(0, 0, 12) };
	CurbSection.Triangles = { 0, 1, 2 };
	CurbSection.Normals = { FVector::UpVector, FVector::UpVector, FVector::UpVector };
	CurbSection.UVs = { FVector2D::ZeroVector, FVector2D(1, 0), FVector2D(0, 1) };
	CurbSection.Tangents = { FProcMeshTangent(), FProcMeshTangent(), FProcMeshTangent() };
	FMeshData Curb;
	const TArray<FPatch> JunctionOnly = { Patches[0] };
	AppendExteriorCurbs(JunctionOnly, CurbSection, FVector::ZeroVector, Curb);
	TestEqual(TEXT("Isolated internal junction curbs all four outer sides"), Curb.Triangles.Num() / 3, 8);
	Curb = FMeshData();
	AppendExteriorCurbs(Patches, CurbSection, FVector::ZeroVector, Curb);
	TestEqual(TEXT("Connected approach faces have no curb"), Curb.Triangles.Num() / 3, 24);
	FCityStreetSurfaceRegion West = Horizontal;
	West.Bounds = FBox2D(FVector2D(0, 1000), FVector2D(2000, 2000));
	FCityStreetSurfaceRegion South = Vertical;
	South.Bounds = FBox2D(FVector2D(1000, 0), FVector2D(2000, 2000));
	const TArray<FCityStreetSurfaceRegion> CornerRegions = { West, South, Junction };
	const TArray<uint8> CornerRenderable = { 1, 1, 1 };
	const TArray<FPatch> CornerPatches = MakePatches(CornerRegions, CornerRenderable);
	TestEqual(TEXT("A corner has one square and two approach roads"), CornerPatches.Num(), 3);
	for (const FPatch& Patch : CornerPatches)
	{
		if (!Patch.bJunction)
		{
			TestTrue(TEXT("Corner approach retains 1000cm width"),
				FMath::IsNearlyEqual(MakeTileFrame(Patch).Width, 1000.0, 0.01));
		}
	}
	Curb = FMeshData();
	AppendExteriorCurbs(CornerPatches, CurbSection, FVector::ZeroVector, Curb);
	TestEqual(TEXT("Corner curbs only its two unconnected junction sides"),
		Curb.Triangles.Num() / 3, 16);
	const TArray<FCityStreetSurfaceRegion> BranchRegions = { Horizontal, South, Junction };
	const TArray<FPatch> BranchPatches = MakePatches(BranchRegions, CornerRenderable);
	TestEqual(TEXT("A branch has one square and three approach roads"), BranchPatches.Num(), 4);
	for (const FPatch& Patch : BranchPatches)
	{
		if (!Patch.bJunction)
		{
			TestTrue(TEXT("Branch approach retains 1000cm width"),
				FMath::IsNearlyEqual(MakeTileFrame(Patch).Width, 1000.0, 0.01));
		}
	}
	Curb = FMeshData();
	AppendExteriorCurbs(BranchPatches, CurbSection, FVector::ZeroVector, Curb);
	TestEqual(TEXT("Branch curbs only its one unconnected junction side"),
		Curb.Triangles.Num() / 3, 20);
	FCityStreetSurfaceRegion Parallel = Horizontal;
	Parallel.Bounds = FBox2D(FVector2D(0, 1500), FVector2D(2000, 2500));
	const TArray<FCityStreetSurfaceRegion> ParallelRegions = { Horizontal, Parallel };
	const TArray<uint8> ParallelRenderable = { 1, 1 };
	const TArray<FPatch> ParallelPatches = MakePatches(ParallelRegions, ParallelRenderable);
	if (!TestEqual(TEXT("Parallel routes become nonoverlapping patches"), ParallelPatches.Num(), 2))
	{
		return false;
	}
	const FPatch& HalfLane = ParallelPatches[1];
	TestTrue(TEXT("A 500cm visible strip keeps its 1000cm source tile frame"),
		FMath::IsNearlyEqual(MakeTileFrame(HalfLane).Width, 500.0, 0.01)
		&& FMath::IsNearlyEqual(MakeOriginalTileFrame(HalfLane).Width, 1000.0, 0.01));
	auto RoadVertex = [](double X, double Y)
	{
		FVertex Vertex;
		Vertex.Position = FVector(X, Y, 0);
		Vertex.UV = FVector2D(X / 500.0, (2500.0 - Y) / 1000.0);
		return Vertex;
	};
	const TArray<FVertex> RoadTriangle = {
		RoadVertex(0, 2500), RoadVertex(500, 2500), RoadVertex(0, 1500)
	};
	TArray<FVertex> Visible;
	ClipToBounds(RoadTriangle, HalfLane.Bounds, Visible);
	bool bUVPreserved = Visible.Num() >= 3;
	double MaxVisibleV = 0.0;
	for (const FVertex& Vertex : Visible)
	{
		bUVPreserved &= FMath::IsNearlyEqual(Vertex.UV.Y,
			(2500.0 - Vertex.Position.Y) / 1000.0, 0.0001);
		MaxVisibleV = FMath::Max(MaxVisibleV, Vertex.UV.Y);
	}
	TestTrue(TEXT("Clipped lane vertices retain the original UV interpolation"),
		bUVPreserved && FMath::IsNearlyEqual(MaxVisibleV, 0.5, 0.0001));
	return true;
}
#endif
