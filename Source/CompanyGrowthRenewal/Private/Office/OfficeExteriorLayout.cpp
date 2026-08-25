#include "Office/OfficeExteriorLayout.h"

#include "Office/OfficeFootprintGeometry.h"

namespace
{
constexpr float AuthoredTileSizeCm = 400.f;
constexpr float CubeMeshSizeCm = 100.f;
constexpr float SlabThicknessCm = 20.f;
constexpr float FacadeDepthCm = 40.f;
constexpr float FacadeFloorHeightCm = 400.f;
constexpr int32 FacadeFloorCount = 4;
constexpr float BandHeightCm = 20.f;
constexpr float TransferOverlapCm = 100.f;
constexpr float TransferDepthBelowApronCm = 300.f;
constexpr float TowerTransferOverlapCm = 50.f;
constexpr float BoundsToleranceCm = 0.1f;

struct FSkylineBlockSpec
{
	float OffsetX;
	float OffsetY;
	float SizeX;
	float SizeY;
	float TopZ;
	float BottomZ;
};

constexpr FSkylineBlockSpec SkylineBlockSpecs[] = {
	{-3200.f, -600.f, 800.f, 900.f, -2200.f, -9500.f},
	{-3800.f, 1500.f, 1200.f, 700.f, -2350.f, -10200.f},
	{-4500.f, -2300.f, 1000.f, 1200.f, -2500.f, -9800.f},
	{3400.f, -800.f, 900.f, 1000.f, -2650.f, -11000.f},
	{4100.f, 1500.f, 1400.f, 800.f, -2800.f, -9700.f},
	{5000.f, -2600.f, 1000.f, 1500.f, -2950.f, -10500.f},
	{-500.f, 3500.f, 800.f, 900.f, -3100.f, -9200.f},
	{1400.f, 4200.f, 1000.f, 1200.f, -3250.f, -9900.f},
	{-1600.f, 5000.f, 1500.f, 1000.f, -3400.f, -10800.f},
	{-600.f, -3800.f, 900.f, 1000.f, -3550.f, -9600.f},
	{1700.f, -4500.f, 1200.f, 1400.f, -2300.f, -10100.f},
	{-2200.f, -5200.f, 1000.f, 1200.f, -3000.f, -11200.f}};

bool IsFiniteVector2D(const FVector2D& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
}

bool IsValidBounds(const FBox2D& Bounds)
{
	return Bounds.bIsValid
		&& IsFiniteVector2D(Bounds.Min)
		&& IsFiniteVector2D(Bounds.Max)
		&& Bounds.Max.X > Bounds.Min.X
		&& Bounds.Max.Y > Bounds.Min.Y;
}

bool AreBoundsNearlyEqual(const FBox2D& A, const FBox2D& B)
{
	return A.Min.Equals(B.Min, BoundsToleranceCm)
		&& A.Max.Equals(B.Max, BoundsToleranceCm);
}

bool IsInputValid(const FOfficeExteriorLayoutInput& Input)
{
	if (!FMath::IsFinite(Input.TileSizeCm)
		|| !FMath::IsNearlyEqual(Input.TileSizeCm, AuthoredTileSizeCm, KINDA_SMALL_NUMBER)
		|| !FMath::IsFinite(Input.StructuralFloorZ)
		|| !FMath::IsFinite(Input.TowerHeightCm)
		|| Input.TowerHeightCm <= 0.f)
	{
		return false;
	}

	if (!FOfficeExteriorLayoutBuilder::IsCanonicalMaxTileCount(Input.MaxTileCount)
		|| !FOfficeFootprintGeometry::IsTileCountValid(Input.TileCount, Input.MaxTileCount))
	{
		return false;
	}

	if (!IsValidBounds(Input.CurrentFloorBounds) || !IsValidBounds(Input.MaxFloorBounds))
	{
		return false;
	}

	const FBox2D ExpectedCurrentBounds =
		FOfficeFootprintGeometry::MakeFloorBounds(Input.TileCount, Input.TileSizeCm);
	const FBox2D ExpectedMaxBounds =
		FOfficeFootprintGeometry::MakeFloorBounds(Input.MaxTileCount, Input.TileSizeCm);
	if (!AreBoundsNearlyEqual(Input.CurrentFloorBounds, ExpectedCurrentBounds)
		|| !AreBoundsNearlyEqual(Input.MaxFloorBounds, ExpectedMaxBounds))
	{
		return false;
	}

	return Input.CurrentFloorBounds.Min.X >= Input.MaxFloorBounds.Min.X - BoundsToleranceCm
		&& Input.CurrentFloorBounds.Min.Y >= Input.MaxFloorBounds.Min.Y - BoundsToleranceCm
		&& Input.CurrentFloorBounds.Max.X <= Input.MaxFloorBounds.Max.X + BoundsToleranceCm
		&& Input.CurrentFloorBounds.Max.Y <= Input.MaxFloorBounds.Max.Y + BoundsToleranceCm;
}

FTransform MakeCubeTransform(
	const FVector& Location,
	const FVector& SizeCm,
	const float YawDegrees = 0.f)
{
	return FTransform(
		FRotator(0.f, YawDegrees, 0.f),
		Location,
		SizeCm / CubeMeshSizeCm);
}

FVector MakeBoundsCenter(const FBox2D& Bounds, const float CenterZ)
{
	const FVector2D CenterXY = Bounds.GetCenter();
	return FVector(CenterXY.X, CenterXY.Y, CenterZ);
}

void AddFacadeBayTransforms(
	const FOfficeExteriorLayoutInput& Input,
	const float ApronTopZ,
	FOfficeExteriorLayoutResult& Result)
{
	Result.FacadeBayTransforms.Reserve(
		(Input.TileCount.X + Input.TileCount.Y) * FacadeFloorCount);

	for (int32 FloorIndex = 0; FloorIndex < FacadeFloorCount; ++FloorIndex)
	{
		const float BayCenterZ =
			ApronTopZ - FloorIndex * FacadeFloorHeightCm - FacadeFloorHeightCm * 0.5f;

		for (int32 TileX = 0; TileX < Input.TileCount.X; ++TileX)
		{
			const float BayCenterX =
				Input.CurrentFloorBounds.Min.X + (TileX + 0.5f) * Input.TileSizeCm;
			Result.FacadeBayTransforms.Emplace(
				FRotator::ZeroRotator,
				FVector(
					BayCenterX,
					Input.CurrentFloorBounds.Min.Y - FacadeDepthCm * 0.5f,
					BayCenterZ),
				FVector::OneVector);
		}

		for (int32 TileY = 0; TileY < Input.TileCount.Y; ++TileY)
		{
			const float BayCenterY =
				Input.CurrentFloorBounds.Max.Y - (TileY + 0.5f) * Input.TileSizeCm;
			Result.FacadeBayTransforms.Emplace(
				FRotator(0.f, 90.f, 0.f),
				FVector(
					Input.CurrentFloorBounds.Max.X + FacadeDepthCm * 0.5f,
					BayCenterY,
					BayCenterZ),
				FVector::OneVector);
		}
	}
}

void AddBandAndTrimTransforms(
	const FOfficeExteriorLayoutInput& Input,
	const float ApronTopZ,
	const float ApronBottomZ,
	FOfficeExteriorLayoutResult& Result)
{
	const FVector2D FloorSize = Input.CurrentFloorBounds.GetSize();
	const FVector2D FloorCenter = Input.CurrentFloorBounds.GetCenter();
	const float NegativeYCenter = Input.CurrentFloorBounds.Min.Y - FacadeDepthCm * 0.5f;
	const float PositiveXCenter = Input.CurrentFloorBounds.Max.X + FacadeDepthCm * 0.5f;

	Result.FloorBandTransforms.Reserve(FacadeFloorCount * 2);
	for (int32 BoundaryIndex = 1; BoundaryIndex <= FacadeFloorCount; ++BoundaryIndex)
	{
		const float BoundaryZ = ApronTopZ - BoundaryIndex * FacadeFloorHeightCm;
		Result.FloorBandTransforms.Add(MakeCubeTransform(
			FVector(FloorCenter.X, NegativeYCenter, BoundaryZ),
			FVector(FloorSize.X, FacadeDepthCm, BandHeightCm)));
		Result.FloorBandTransforms.Add(MakeCubeTransform(
			FVector(PositiveXCenter, FloorCenter.Y, BoundaryZ),
			FVector(FacadeDepthCm, FloorSize.Y, BandHeightCm)));
	}

	const float ApronCenterZ = (ApronTopZ + ApronBottomZ) * 0.5f;
	const float ApronHeight = ApronTopZ - ApronBottomZ;
	Result.CornerAndTrimTransforms.Reserve(5);
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(Input.CurrentFloorBounds.Min.X, Input.CurrentFloorBounds.Min.Y, ApronCenterZ),
		FVector(FacadeDepthCm, FacadeDepthCm, ApronHeight)));
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(Input.CurrentFloorBounds.Max.X, Input.CurrentFloorBounds.Min.Y, ApronCenterZ),
		FVector(FacadeDepthCm, FacadeDepthCm, ApronHeight)));
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(Input.CurrentFloorBounds.Max.X, Input.CurrentFloorBounds.Max.Y, ApronCenterZ),
		FVector(FacadeDepthCm, FacadeDepthCm, ApronHeight)));
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(FloorCenter.X, NegativeYCenter, ApronTopZ),
		FVector(FloorSize.X, FacadeDepthCm, BandHeightCm)));
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(PositiveXCenter, FloorCenter.Y, ApronTopZ),
		FVector(FacadeDepthCm, FloorSize.Y, BandHeightCm)));
}

void AddSkylineTransforms(
	const FBox2D& MaxFloorBounds,
	FOfficeExteriorLayoutResult& Result)
{
	const FVector2D MaxFloorCenter = MaxFloorBounds.GetCenter();
	Result.SkylineTransforms.Reserve(UE_ARRAY_COUNT(SkylineBlockSpecs));
	for (const FSkylineBlockSpec& Spec : SkylineBlockSpecs)
	{
		const float Height = Spec.TopZ - Spec.BottomZ;
		Result.SkylineTransforms.Add(MakeCubeTransform(
			FVector(
				MaxFloorCenter.X + Spec.OffsetX,
				MaxFloorCenter.Y + Spec.OffsetY,
				(Spec.TopZ + Spec.BottomZ) * 0.5f),
			FVector(Spec.SizeX, Spec.SizeY, Height)));
	}
}
}

int32 FOfficeExteriorLayoutResult::GetDynamicInstanceCount() const
{
	return FacadeBayTransforms.Num()
		+ FloorBandTransforms.Num()
		+ CornerAndTrimTransforms.Num();
}

FIntPoint FOfficeExteriorLayoutBuilder::GetCanonicalMaxTileCount()
{
	return FIntPoint(CanonicalMaxTileCountX, CanonicalMaxTileCountY);
}

bool FOfficeExteriorLayoutBuilder::IsCanonicalMaxTileCount(const FIntPoint MaxTileCount)
{
	return MaxTileCount == GetCanonicalMaxTileCount();
}

bool FOfficeExteriorLayoutBuilder::Build(
	const FOfficeExteriorLayoutInput& Input,
	FOfficeExteriorLayoutResult& OutResult)
{
	if (!IsInputValid(Input))
	{
		return false;
	}

	FOfficeExteriorLayoutResult Result;
	const float SlabTopZ = Input.StructuralFloorZ;
	const float SlabBottomZ = SlabTopZ - SlabThicknessCm;
	const float ApronTopZ = SlabBottomZ;
	Result.ApronBottomZ = ApronTopZ - FacadeFloorCount * FacadeFloorHeightCm;
	Result.TransferTopZ = Result.ApronBottomZ + TransferOverlapCm;
	Result.TransferBottomZ = Result.ApronBottomZ - TransferDepthBelowApronCm;
	Result.TowerTopZ = Result.TransferBottomZ + TowerTransferOverlapCm;

	const FVector2D CurrentFloorSize = Input.CurrentFloorBounds.GetSize();
	Result.ApronSlabTransform = MakeCubeTransform(
		MakeBoundsCenter(Input.CurrentFloorBounds, (SlabTopZ + SlabBottomZ) * 0.5f),
		FVector(CurrentFloorSize.X, CurrentFloorSize.Y, SlabThicknessCm));

	AddFacadeBayTransforms(Input, ApronTopZ, Result);
	AddBandAndTrimTransforms(Input, ApronTopZ, Result.ApronBottomZ, Result);

	const FVector2D MaxFloorSize = Input.MaxFloorBounds.GetSize();
	Result.TransferLevelTransform = MakeCubeTransform(
		MakeBoundsCenter(
			Input.MaxFloorBounds,
			(Result.TransferTopZ + Result.TransferBottomZ) * 0.5f),
		FVector(
			MaxFloorSize.X,
			MaxFloorSize.Y,
			Result.TransferTopZ - Result.TransferBottomZ));

	Result.TowerTopBounds = FBox2D(
		FVector2D(
			Input.MaxFloorBounds.Min.X,
			Input.MaxFloorBounds.Min.Y + Input.TileSizeCm),
		FVector2D(
			Input.MaxFloorBounds.Max.X - Input.TileSizeCm,
			Input.MaxFloorBounds.Max.Y));
	const FVector2D TowerSize = Result.TowerTopBounds.GetSize();
	Result.TowerBodyTransform = MakeCubeTransform(
		MakeBoundsCenter(Result.TowerTopBounds, Result.TowerTopZ - Input.TowerHeightCm * 0.5f),
		FVector(TowerSize.X, TowerSize.Y, Input.TowerHeightCm));

	AddSkylineTransforms(Input.MaxFloorBounds, Result);

	OutResult = MoveTemp(Result);
	return true;
}
