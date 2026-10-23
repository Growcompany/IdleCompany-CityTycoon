#include "Office/OfficeFootprintGeometry.h"

namespace
{
bool IsMaximumTileCountValid(const FIntPoint Maximum)
{
	return Maximum.X >= FOfficeFootprintGeometry::MinTileCountX
		&& Maximum.Y >= FOfficeFootprintGeometry::MinTileCountY;
}
}

FIntPoint FOfficeFootprintGeometry::NormalizeTileCount(const FIntPoint Requested, const FIntPoint Maximum)
{
	if (!ensureMsgf(
		IsMaximumTileCountValid(Maximum),
		TEXT("Office footprint maximum must be at least %dx%d, but was %dx%d."),
		MinTileCountX,
		MinTileCountY,
		Maximum.X,
		Maximum.Y))
	{
		return FIntPoint::ZeroValue;
	}

	return FIntPoint(
		FMath::Clamp(Requested.X, MinTileCountX, Maximum.X),
		FMath::Clamp(Requested.Y, MinTileCountY, Maximum.Y));
}

bool FOfficeFootprintGeometry::IsTileCountValid(const FIntPoint Count, const FIntPoint Maximum)
{
	if (!ensureMsgf(
		IsMaximumTileCountValid(Maximum),
		TEXT("Office footprint maximum must be at least %dx%d, but was %dx%d."),
		MinTileCountX,
		MinTileCountY,
		Maximum.X,
		Maximum.Y))
	{
		return false;
	}

	return Count.X >= MinTileCountX
		&& Count.X <= Maximum.X
		&& Count.Y >= MinTileCountY
		&& Count.Y <= Maximum.Y;
}

FBox2D FOfficeFootprintGeometry::MakeFloorBounds(const FIntPoint Count, const float TileSizeCm)
{
	if (!ensureMsgf(
		Count.X >= MinTileCountX && Count.Y >= MinTileCountY,
		TEXT("Office footprint count must be at least %dx%d, but was %dx%d."),
		MinTileCountX,
		MinTileCountY,
		Count.X,
		Count.Y)
		|| !ensureMsgf(TileSizeCm > 0.f, TEXT("Office tile size must be positive, but was %f."), TileSizeCm))
	{
		return FBox2D(ForceInit);
	}

	const float HalfTileSize = TileSizeCm * 0.5f;
	const FVector2D FloorMin(
		FirstTileCenterX - HalfTileSize,
		FirstTileCenterY + HalfTileSize - Count.Y * TileSizeCm);
	const FVector2D FloorMax(
		FirstTileCenterX - HalfTileSize + Count.X * TileSizeCm,
		FirstTileCenterY + HalfTileSize);

	return FBox2D(FloorMin, FloorMax);
}

FVector FOfficeFootprintGeometry::MakeTileCenter(const int32 TileX, const int32 TileY, const float TileSizeCm)
{
	if (!ensureMsgf(
		TileX >= 0 && TileY >= 0,
		TEXT("Office tile indices must be non-negative, but were (%d, %d)."),
		TileX,
		TileY)
		|| !ensureMsgf(TileSizeCm > 0.f, TEXT("Office tile size must be positive, but was %f."), TileSizeCm))
	{
		return FVector::ZeroVector;
	}

	return FVector(
		FirstTileCenterX + TileX * TileSizeCm,
		FirstTileCenterY - TileY * TileSizeCm,
		FloorTileCenterZ);
}

FBox2D FOfficeFootprintGeometry::MakeExpansionCellBounds(
	const FIntPoint Count,
	const FIntPoint Maximum,
	const EOfficeExpandDirection Dir,
	const float TileSizeCm)
{
	if (!IsTileCountValid(Count, Maximum) || TileSizeCm <= 0.f)
	{
		return FBox2D(ForceInit);
	}

	const FIntPoint Next(
		Dir == EOfficeExpandDirection::Left ? Count.X + 1 : Count.X,
		Dir == EOfficeExpandDirection::Left ? Count.Y : Count.Y + 1);
	if (!IsTileCountValid(Next, Maximum))
	{
		return FBox2D(ForceInit);
	}

	const FBox2D Current = MakeFloorBounds(Count, TileSizeCm);
	const FBox2D Grown = MakeFloorBounds(Next, TileSizeCm);
	if (!Current.bIsValid || !Grown.bIsValid)
	{
		return FBox2D(ForceInit);
	}

	// 자란 바닥에서 기존 바닥을 뺀 띠가 곧 새 칸이다.
	return Dir == EOfficeExpandDirection::Left
		? FBox2D(FVector2D(Current.Max.X, Grown.Min.Y), FVector2D(Grown.Max.X, Grown.Max.Y))
		: FBox2D(FVector2D(Grown.Min.X, Grown.Min.Y), FVector2D(Grown.Max.X, Current.Min.Y));
}

FVector FOfficeFootprintGeometry::MakeExpansionCellCenter(
	const FIntPoint Count,
	const FIntPoint Maximum,
	const EOfficeExpandDirection Dir,
	const float TileSizeCm)
{
	const FBox2D Cell = MakeExpansionCellBounds(Count, Maximum, Dir, TileSizeCm);
	if (!Cell.bIsValid)
	{
		return FVector::ZeroVector;
	}

	const FVector2D Center = Cell.GetCenter();
	return FVector(Center.X, Center.Y, StructuralFloorZ);
}

FTransform FOfficeFootprintGeometry::MakeExpansionSegmentTransform(
	const FTransform& Source, const EAxis::Type LocalAxis, const float SignedLocalSpan, const float AxisScale)
{
	const FVector SourceScale = Source.GetScale3D();

	FVector AxisUnit = FVector::ZeroVector;
	float SourceAxisScale = 1.f;
	switch (LocalAxis)
	{
	case EAxis::X: AxisUnit = FVector::XAxisVector; SourceAxisScale = static_cast<float>(SourceScale.X); break;
	case EAxis::Y: AxisUnit = FVector::YAxisVector; SourceAxisScale = static_cast<float>(SourceScale.Y); break;
	default:       AxisUnit = FVector::ZAxisVector; SourceAxisScale = static_cast<float>(SourceScale.Z); break;
	}

	// 복제본은 원본이 이미 차지한 끝에서 시작하고, 그 축 길이만 AxisScale 배다.
	// 밀어 놓는 거리는 AxisScale 과 무관하다 ― 띠를 줄여도 시작점은 경계에 붙어 있어야 한다.
	FVector SegmentScale = SourceScale;
	SegmentScale[static_cast<int32>(LocalAxis) - 1] = AxisScale;

	const FVector LocalOffset = AxisUnit * (SignedLocalSpan * SourceAxisScale);
	return FTransform(
		Source.GetRotation(),
		Source.GetLocation() + Source.GetRotation().RotateVector(LocalOffset),
		SegmentScale);
}
