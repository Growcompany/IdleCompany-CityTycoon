#include "Office/OfficeExteriorLayout.h"

#include "Office/OfficeFootprintGeometry.h"

namespace
{
constexpr float AuthoredTileSizeCm = 400.f;
constexpr float CubeMeshSizeCm = 100.f;
constexpr float SlabEdgeInsetCm = 12.f;
constexpr float SlabClearanceBelowPlayableFloorCm = 4.f;
constexpr float SlabBottomInsetCm = 20.f;
constexpr float FacadeDepthCm = 40.f;
constexpr float FacadeFloorHeightCm = 400.f;
constexpr int32 FacadeFloorCount = 10;
constexpr float MinorBandHeightCm = 6.f;
constexpr float MajorBandHeightCm = 20.f;
constexpr float TopTrimHeightCm = 24.f;
constexpr float TerminalJambWidthCm = 24.f;
constexpr float TransferOverlapCm = 100.f;
constexpr float TransferDepthBelowApronCm = 100.f;
constexpr float TowerTransferOverlapCm = 50.f;
constexpr float BoundsToleranceCm = 0.1f;
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
		|| Input.TowerHeightCm <= 0.f
		|| !FMath::IsFinite(Input.OfficeHeightCm)
		|| Input.OfficeHeightCm < 0.f
		|| !FMath::IsFinite(Input.StructuralFloorZ - Input.OfficeHeightCm))
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
	if (Input.ExteriorFootprintBounds.bIsValid)
	{
		const FBox2D& ExteriorBounds = Input.ExteriorFootprintBounds;
		if (!IsValidBounds(ExteriorBounds)
			|| ExteriorBounds.Min.X > Input.CurrentFloorBounds.Min.X + BoundsToleranceCm
			|| ExteriorBounds.Min.Y > Input.CurrentFloorBounds.Min.Y + BoundsToleranceCm
			|| ExteriorBounds.Max.X < Input.CurrentFloorBounds.Max.X - BoundsToleranceCm
			|| ExteriorBounds.Max.Y < Input.CurrentFloorBounds.Max.Y - BoundsToleranceCm)
		{
			return false;
		}
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

float GetVisibleTopTrimHeight(const float FacadeHeightCm)
{
	return FMath::Min(TopTrimHeightCm, FacadeHeightCm * 0.5f);
}

void AddFacadeBayTransforms(
	const FBox2D& ExteriorBounds,
	const FIntPoint BayColumns,
	const float ApronTopZ,
	const float FacadeHeightCm,
	const int32 VisibleFloorCount,
	FOfficeExteriorLayoutResult& Result)
{
	Result.FacadeBayTransforms.Reserve((BayColumns.X + BayColumns.Y) * VisibleFloorCount);
	const FVector2D BayWidths = ExteriorBounds.GetSize() / FVector2D(BayColumns.X, BayColumns.Y);
	for (int32 FloorIndex = 0; FloorIndex < VisibleFloorCount; ++FloorIndex)
	{
		const float FloorHeightCm = FMath::Min(FacadeFloorHeightCm, FacadeHeightCm - FloorIndex * FacadeFloorHeightCm);
		// 최상단 마감 아래에서 창틀이 끝나도록 하여 같은 높이의 상향 면 중복을 피한다.
		const float TrimHeightCm = FloorIndex == 0 ? GetVisibleTopTrimHeight(FacadeHeightCm) : 0.f;
		const float BayHeightCm = FloorHeightCm - TrimHeightCm;
		const float BayCenterZ = ApronTopZ - FloorIndex * FacadeFloorHeightCm - TrimHeightCm - BayHeightCm * 0.5f;
		for (int32 Column = 0; Column < BayColumns.X; ++Column)
		{
			Result.FacadeBayTransforms.Emplace(FRotator::ZeroRotator,
				FVector(ExteriorBounds.Min.X + (Column + 0.5f) * BayWidths.X,
					ExteriorBounds.Min.Y - FacadeDepthCm * 0.5f, BayCenterZ),
				FVector(BayWidths.X / AuthoredTileSizeCm, 1.f, BayHeightCm / FacadeFloorHeightCm));
		}
		for (int32 Column = 0; Column < BayColumns.Y; ++Column)
		{
			Result.FacadeBayTransforms.Emplace(FRotator(0.f, 90.f, 0.f),
				FVector(ExteriorBounds.Max.X + FacadeDepthCm * 0.5f,
					ExteriorBounds.Max.Y - (Column + 0.5f) * BayWidths.Y, BayCenterZ),
				FVector(BayWidths.Y / AuthoredTileSizeCm, 1.f, BayHeightCm / FacadeFloorHeightCm));
		}
	}
}

void AddBandAndTrimTransforms(
	const FBox2D& ExteriorBounds,
	const float ApronTopZ,
	const float ApronBottomZ,
	const int32 VisibleFloorCount,
	FOfficeExteriorLayoutResult& Result)
{
	if (VisibleFloorCount == 0)
	{
		return;
	}
	const float ApronHeight = ApronTopZ - ApronBottomZ;
	const float VisibleTrimHeightCm = GetVisibleTopTrimHeight(ApronHeight);
	const FVector2D FloorSize = ExteriorBounds.GetSize();
	const FVector2D FloorCenter = ExteriorBounds.GetCenter();
	const float NegativeYCenter = ExteriorBounds.Min.Y - FacadeDepthCm * 0.5f;
	const float PositiveXCenter = ExteriorBounds.Max.X + FacadeDepthCm * 0.5f;

	Result.FloorBandTransforms.Reserve(VisibleFloorCount * 2);
	for (int32 BoundaryIndex = 1; BoundaryIndex <= VisibleFloorCount; ++BoundaryIndex)
	{
		const float BoundaryZ = FMath::Max(ApronBottomZ, ApronTopZ - BoundaryIndex * FacadeFloorHeightCm);
		const float FloorHeightCm = FMath::Min(FacadeFloorHeightCm, ApronHeight - (BoundaryIndex - 1) * FacadeFloorHeightCm);
		const float BandHeightCm = FMath::Min(FloorHeightCm, BoundaryIndex % 3 == 0 ? MajorBandHeightCm : MinorBandHeightCm);
		const float BandCenterZ = Result.bHasOfficeHeight
			? FMath::Max(BoundaryZ, Result.GroundZ + BandHeightCm * 0.5f) : BoundaryZ;
		// 아주 짧은 외벽에서도 지면 쪽 하단을 유지하며 상단 마감 밑으로 층띠를 제한한다.
		const float BandBottomZ = BandCenterZ - BandHeightCm * 0.5f;
		const float VisibleBandHeightCm = FMath::Min(BandHeightCm, ApronTopZ - VisibleTrimHeightCm - BandBottomZ);
		const float VisibleBandCenterZ = BandBottomZ + VisibleBandHeightCm * 0.5f;
		Result.FloorBandTransforms.Add(MakeCubeTransform(
			FVector(FloorCenter.X, NegativeYCenter, VisibleBandCenterZ),
			FVector(FloorSize.X, FacadeDepthCm, VisibleBandHeightCm)));
		Result.FloorBandTransforms.Add(MakeCubeTransform(
			FVector(PositiveXCenter, FloorCenter.Y, VisibleBandCenterZ),
			FVector(FacadeDepthCm, FloorSize.Y, VisibleBandHeightCm)));
	}

	const float ProfileHeightCm = ApronHeight - VisibleTrimHeightCm;
	const float ProfileCenterZ = ApronBottomZ + ProfileHeightCm * 0.5f;
	// 수직 기둥과 상단 마감은 맞닿기만 하며 외측 면을 중복해서 그리지 않는다.
	Result.CornerAndTrimTransforms.Reserve(5);
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(ExteriorBounds.Min.X + TerminalJambWidthCm * 0.5f, NegativeYCenter, ProfileCenterZ),
		FVector(TerminalJambWidthCm, FacadeDepthCm, ProfileHeightCm)));
	// 두 외벽의 깊이가 만나는 바깥 모서리를 채우고 실내 footprint를 침범하지 않는다.
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(PositiveXCenter, NegativeYCenter, ProfileCenterZ),
		FVector(FacadeDepthCm, FacadeDepthCm, ProfileHeightCm)));
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(PositiveXCenter, ExteriorBounds.Max.Y - TerminalJambWidthCm * 0.5f, ProfileCenterZ),
		FVector(FacadeDepthCm, TerminalJambWidthCm, ProfileHeightCm)));
	// 최상층 마감은 아래로만 두껍게 하며, 전면 마감이 코너까지 덮는 L자 접합이다.
	const float TopTrimCenterZ = ApronTopZ - VisibleTrimHeightCm * 0.5f;
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(FloorCenter.X + FacadeDepthCm * 0.5f, NegativeYCenter, TopTrimCenterZ),
		FVector(FloorSize.X + FacadeDepthCm, FacadeDepthCm, VisibleTrimHeightCm)));
	Result.CornerAndTrimTransforms.Add(MakeCubeTransform(
		FVector(PositiveXCenter, FloorCenter.Y, TopTrimCenterZ),
		FVector(FacadeDepthCm, FloorSize.Y, VisibleTrimHeightCm)));
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

void FSkylineClimbBandLayout::Build(
	const FBox2D& FootprintBounds,
	const float TopZ,
	const float GroundZ,
	const int32 ClimbedFloorCount,
	TArray<FTransform>& OutTransforms)
{
	OutTransforms.Reset();
	if (ClimbedFloorCount <= 0 || !FMath::IsFinite(TopZ) || !FMath::IsFinite(GroundZ)) { return; }

	const int32 Count = FMath::Min(ClimbedFloorCount, MaxBands);
	// 탑과 같은 폭이면 외곽면이 동일 평면이라 z-fighting 하거나 아예 묻힌다
	const FVector2D Size = FootprintBounds.GetSize() + FVector2D(2.0f * OvershootCm, 2.0f * OvershootCm);
	OutTransforms.Reserve(Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float BandZ = TopZ - Index * FacadeFloorHeightCm;
		if (BandZ < GroundZ) { break; }
		OutTransforms.Add(MakeCubeTransform(
			MakeBoundsCenter(FootprintBounds, BandZ),
			FVector(Size.X, Size.Y, BandThicknessCm)));
	}
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
	Result.ExteriorFootprintBounds = Input.ExteriorFootprintBounds.bIsValid
		? Input.ExteriorFootprintBounds : Input.CurrentFloorBounds;
	const FVector2D FloorSize = Input.CurrentFloorBounds.GetSize();
	// 창틀 열 수는 바닥 칸 수를 따르고, 배치 폭만 실제 외벽 경계에 맞춘다.
	const FIntPoint BayColumns(
		FMath::CeilToInt(FMath::Min((FloorSize.X - 0.01) / AuthoredTileSizeCm, 128.0)),
		FMath::CeilToInt(FMath::Min((FloorSize.Y - 0.01) / AuthoredTileSizeCm, 128.0)));
	const int32 DetailedFloorCount = FMath::Min(FacadeFloorCount, MaxFacadeBayInstances / (BayColumns.X + BayColumns.Y));
	Result.bHasOfficeHeight = Input.OfficeHeightCm > 0.f;
	Result.GroundZ = Input.StructuralFloorZ - Input.OfficeHeightCm;
	const float DefaultSlabTopZ = Input.StructuralFloorZ
		+ FOfficeFootprintGeometry::FloorTileCenterZ
		- SlabClearanceBelowPlayableFloorCm;
	const float SlabTopZ = Result.bHasOfficeHeight ? FMath::Max(DefaultSlabTopZ, Result.GroundZ) : DefaultSlabTopZ;
	const float DefaultSlabBottomZ = Input.StructuralFloorZ - SlabBottomInsetCm;
	const float SlabBottomZ = Result.bHasOfficeHeight ? FMath::Max(DefaultSlabBottomZ, Result.GroundZ) : DefaultSlabBottomZ;
	const float ApronTopZ = SlabBottomZ;
	const float MaximumFacadeHeightCm = DetailedFloorCount * FacadeFloorHeightCm;
	const float FacadeHeightCm = Result.bHasOfficeHeight
		? FMath::Clamp(ApronTopZ - Result.GroundZ, 0.f, MaximumFacadeHeightCm) : MaximumFacadeHeightCm;
	const int32 VisibleFloorCount = FMath::CeilToInt(FacadeHeightCm / FacadeFloorHeightCm);
	Result.ApronBottomZ = ApronTopZ - FacadeHeightCm;
	Result.TransferTopZ = Result.ApronBottomZ + TransferOverlapCm;
	Result.TransferBottomZ = Result.ApronBottomZ - TransferDepthBelowApronCm;
	Result.TowerTopZ = Result.TransferBottomZ + TowerTransferOverlapCm;
	float VisibleTowerHeightCm = Input.TowerHeightCm;
	if (Result.bHasOfficeHeight)
	{
		Result.bShowTransferLevel = Result.ApronBottomZ > Result.GroundZ;
		Result.bShowTowerBody = Result.TransferBottomZ > Result.GroundZ;
		if (Result.bShowTransferLevel)
		{
			Result.TransferBottomZ = FMath::Max(Result.TransferBottomZ, Result.GroundZ);
		}
		else
		{
			Result.TransferTopZ = Result.GroundZ;
			Result.TransferBottomZ = Result.GroundZ;
		}
		Result.TowerTopZ = Result.bShowTowerBody
			? Result.TransferBottomZ + TowerTransferOverlapCm : Result.GroundZ;
		VisibleTowerHeightCm = Result.bShowTowerBody ? Result.TowerTopZ - Result.GroundZ : 0.f;
	}
	else
	{
		Result.GroundZ = Result.TowerTopZ - VisibleTowerHeightCm;
	}

	const FBox2D InsetSlabBounds(
		Result.ExteriorFootprintBounds.Min + FVector2D(SlabEdgeInsetCm, SlabEdgeInsetCm),
		Result.ExteriorFootprintBounds.Max - FVector2D(SlabEdgeInsetCm, SlabEdgeInsetCm));
	const FVector2D InsetSlabSize = InsetSlabBounds.GetSize();
	Result.ApronSlabTransform = MakeCubeTransform(
		MakeBoundsCenter(InsetSlabBounds, (SlabTopZ + SlabBottomZ) * 0.5f),
		FVector(InsetSlabSize.X, InsetSlabSize.Y, SlabTopZ - SlabBottomZ));

	// 외피 두께를 실제 외벽 경계 안쪽에 두어 하부 외측 면을 맞춘다.
	const FBox2D FacadeInnerBounds(
		Result.ExteriorFootprintBounds.Min + FVector2D(0.f, FacadeDepthCm),
		Result.ExteriorFootprintBounds.Max - FVector2D(FacadeDepthCm, 0.f));
	AddFacadeBayTransforms(FacadeInnerBounds, BayColumns, ApronTopZ, FacadeHeightCm, VisibleFloorCount, Result);
	AddBandAndTrimTransforms(FacadeInnerBounds, ApronTopZ, Result.ApronBottomZ, VisibleFloorCount, Result);

	Result.TowerTopBounds = Result.ExteriorFootprintBounds;
	const FBox2D& TransferBounds = Result.ExteriorFootprintBounds;
	const FVector2D TransferSize = TransferBounds.GetSize();
	Result.TransferLevelTransform = MakeCubeTransform(
		MakeBoundsCenter(
			TransferBounds,
			(Result.TransferTopZ + Result.TransferBottomZ) * 0.5f),
		FVector(
			TransferSize.X,
			TransferSize.Y,
			Result.TransferTopZ - Result.TransferBottomZ));

	const FVector2D TowerSize = Result.TowerTopBounds.GetSize();
	Result.TowerBodyTransform = MakeCubeTransform(
		MakeBoundsCenter(Result.TowerTopBounds, Result.TowerTopZ - VisibleTowerHeightCm * 0.5f),
		FVector(TowerSize.X, TowerSize.Y, VisibleTowerHeightCm));

	OutResult = MoveTemp(Result);
	return true;
}
