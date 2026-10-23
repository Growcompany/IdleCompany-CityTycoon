#pragma once

#include "CoreMinimal.h"

struct COMPANYGROWTHRENEWAL_API FOfficeExteriorLayoutInput
{
	static constexpr float DefaultTowerHeightCm = 6400.f;

	FIntPoint TileCount = FIntPoint::ZeroValue;
	FIntPoint MaxTileCount = FIntPoint::ZeroValue;
	FBox2D CurrentFloorBounds = FBox2D(ForceInit);
	FBox2D MaxFloorBounds = FBox2D(ForceInit);
	FBox2D ExteriorFootprintBounds = FBox2D(ForceInit);
	float TileSizeCm = 0.f;
	float StructuralFloorZ = 0.f;
	float TowerHeightCm = DefaultTowerHeightCm;
	float OfficeHeightCm = 0.f;
};

struct COMPANYGROWTHRENEWAL_API FOfficeExteriorLayoutResult
{
	FTransform ApronSlabTransform = FTransform::Identity;
	TArray<FTransform> FacadeBayTransforms;
	TArray<FTransform> FloorBandTransforms;
	TArray<FTransform> CornerAndTrimTransforms;
	FTransform TransferLevelTransform = FTransform::Identity;
	FTransform TowerBodyTransform = FTransform::Identity;
	FBox2D TowerTopBounds = FBox2D(ForceInit);
	FBox2D ExteriorFootprintBounds = FBox2D(ForceInit);
	float ApronBottomZ = 0.f;
	float TransferTopZ = 0.f;
	float TransferBottomZ = 0.f;
	float TowerTopZ = 0.f;
	float GroundZ = 0.f;
	bool bHasOfficeHeight = false;
	bool bShowTransferLevel = true;
	bool bShowTowerBody = true;

	int32 GetDynamicInstanceCount() const;
};

/**
 * 등반으로 쌓인 층의 표시 밴드. 상세 파사드는 FacadeFloorCount(10층 · 4000cm)에서 포화하므로
 * 그와 무관하게 층 수만큼 따로 그린다 ― 높은 건물에서도 쌓임이 보여야 한다.
 */
struct COMPANYGROWTHRENEWAL_API FSkylineClimbBandLayout
{
	static constexpr int32 MaxBands = 40;
	static constexpr float BandThicknessCm = 26.f;

	/** 탑 표면 밖으로 내미는 양. 풋프린트와 같은 폭이면 TowerBody 와 동일 평면이라 묻힌다
	 *  ― 파사드 베이가 FacadeDepthCm 의 절반만큼 빼는 것과 같은 이유다. */
	static constexpr float OvershootCm = 30.f;

	/** TopZ 부터 층 높이 간격으로 아래로. GroundZ 아래는 만들지 않는다.
	 *  TopZ 는 전달 레벨 **아래**를 받아야 한다 ― 그 박스 안에 들어가면 안 보인다. */
	static void Build(
		const FBox2D& FootprintBounds,
		float TopZ,
		float GroundZ,
		int32 ClimbedFloorCount,
		TArray<FTransform>& OutTransforms);
};

struct COMPANYGROWTHRENEWAL_API FOfficeExteriorLayoutBuilder
{
	static constexpr int32 CanonicalMaxTileCountX = 5;
	static constexpr int32 CanonicalMaxTileCountY = 6;
	static constexpr int32 MaxFacadeBayInstances = 256;

	static FIntPoint GetCanonicalMaxTileCount();
	static bool IsCanonicalMaxTileCount(FIntPoint MaxTileCount);

	static bool Build(
		const FOfficeExteriorLayoutInput& Input,
		FOfficeExteriorLayoutResult& OutResult);
};
