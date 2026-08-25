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
