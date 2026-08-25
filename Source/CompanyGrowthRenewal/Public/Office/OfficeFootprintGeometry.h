#pragma once

#include "CoreMinimal.h"

struct COMPANYGROWTHRENEWAL_API FOfficeFootprintGeometry
{
	static constexpr int32 MinTileCountX = 1;
	static constexpr int32 MinTileCountY = 2;
	static constexpr float FirstTileCenterX = 204.f;
	static constexpr float FirstTileCenterY = -209.f;
	static constexpr float FloorTileCenterZ = -6.f;
	static constexpr float StructuralFloorZ = 0.f;

	static FIntPoint NormalizeTileCount(FIntPoint Requested, FIntPoint Maximum);
	static bool IsTileCountValid(FIntPoint Count, FIntPoint Maximum);
	static FBox2D MakeFloorBounds(FIntPoint Count, float TileSizeCm);
	static FVector MakeTileCenter(int32 TileX, int32 TileY, float TileSizeCm);
};
