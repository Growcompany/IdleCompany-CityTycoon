#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Templates/Function.h"

struct COMPANYGROWTHRENEWAL_API FOfficeCameraSample
{
	float ZoomValue = 0.f;
	float ArmLength = 0.f;
	FTransform CameraTransform = FTransform::Identity;
	float HorizontalFOVDegrees = 90.f;
	float AspectRatio = 16.f / 9.f;
};

struct COMPANYGROWTHRENEWAL_API FOfficeCameraFitResult
{
	bool bFits = false;
	float ZoomValue = 1.f;
	float ArmLength = 0.f;
	int32 SampleCount = 0;
};

struct COMPANYGROWTHRENEWAL_API FOfficeCameraFraming
{
	static constexpr int32 MaxCoarseSteps = 256;
	static constexpr int32 MaxRefineIterations = 32;

	static FIntPoint ResolveFramingSlack(bool bCanExpandX, bool bCanExpandY,
		TFunctionRef<bool(FIntPoint SlackTiles, bool bAxisX)> IsCellInside);

	static FBox MakeHeroBounds(
		const FBox2D& FloorBounds,
		float StructuralFloorZ,
		float VisibleDepthCm);

	static bool TryMakeFloorFocusPoint(
		const FBox2D& FloorBounds,
		float StructuralFloorZ,
		float FocusBelowFloorCm,
		FVector& OutFocusPoint);

	static bool TryClampInitialArmLength(
		float CalculatedArmLength,
		float MinimumInitialArmLength,
		float MaximumInitialArmLength,
		float& OutArmLength);

	static TStaticArray<FVector, 8> MakeBoxCorners(const FBox& Bounds);

	static bool ProjectWorldPoint(
		const FVector& Point,
		const FTransform& CameraTransform,
		float HorizontalFOVDegrees,
		float AspectRatio,
		FVector2D& OutNormalizedPosition);

	static bool DoesViewFit(
		TConstArrayView<FVector> Points,
		const FTransform& CameraTransform,
		float HorizontalFOVDegrees,
		float AspectRatio,
		FVector2D SafeMin,
		FVector2D SafeMax);

	static FOfficeCameraFitResult FindClosestFit(
		TFunctionRef<FOfficeCameraSample(float)> SampleProvider,
		TConstArrayView<FVector> Points,
		FVector2D SafeMin,
		FVector2D SafeMax,
		int32 CoarseSteps,
		int32 RefineIterations);
};
