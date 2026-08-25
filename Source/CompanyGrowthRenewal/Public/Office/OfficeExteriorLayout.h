#pragma once

#include "CoreMinimal.h"

struct COMPANYGROWTHRENEWAL_API FOfficeExteriorLayoutInput
{
	static constexpr float DefaultTowerHeightCm = 6400.f;

	FIntPoint TileCount = FIntPoint::ZeroValue;
	FIntPoint MaxTileCount = FIntPoint::ZeroValue;
	FBox2D CurrentFloorBounds = FBox2D(ForceInit);
	FBox2D MaxFloorBounds = FBox2D(ForceInit);
	float TileSizeCm = 0.f;
	float StructuralFloorZ = 0.f;
	float TowerHeightCm = DefaultTowerHeightCm;
};

struct COMPANYGROWTHRENEWAL_API FOfficeExteriorLayoutResult
{
	FTransform ApronSlabTransform = FTransform::Identity;
	TArray<FTransform> FacadeBayTransforms;
	TArray<FTransform> FloorBandTransforms;
	TArray<FTransform> CornerAndTrimTransforms;
	FTransform TransferLevelTransform = FTransform::Identity;
	FTransform TowerBodyTransform = FTransform::Identity;
	TArray<FTransform> SkylineTransforms;
	FBox2D TowerTopBounds = FBox2D(ForceInit);
	float ApronBottomZ = 0.f;
	float TransferTopZ = 0.f;
	float TransferBottomZ = 0.f;
	float TowerTopZ = 0.f;

	int32 GetDynamicInstanceCount() const;
};

struct COMPANYGROWTHRENEWAL_API FOfficeExteriorLayoutBuilder
{
	static constexpr int32 CanonicalMaxTileCountX = 5;
	static constexpr int32 CanonicalMaxTileCountY = 6;

	static FIntPoint GetCanonicalMaxTileCount();
	static bool IsCanonicalMaxTileCount(FIntPoint MaxTileCount);

	static bool Build(
		const FOfficeExteriorLayoutInput& Input,
		FOfficeExteriorLayoutResult& OutResult);
};
