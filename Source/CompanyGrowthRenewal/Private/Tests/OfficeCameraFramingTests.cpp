#include "Misc/AutomationTest.h"
#include "Player/OfficeCameraFraming.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraHeroBoundsTest,
	"CGR.Office.Exterior.CameraFraming.HeroBounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraProjectionTest,
	"CGR.Office.Exterior.CameraFraming.Projection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraAspectFitTest,
	"CGR.Office.Exterior.CameraFraming.AspectFit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraClosestFitTest,
	"CGR.Office.Exterior.CameraFraming.ClosestFit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraInvalidHeroBoundsTest,
	"CGR.Office.Exterior.CameraFraming.InvalidHeroBounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraSearchLimitsTest,
	"CGR.Office.Exterior.CameraFraming.SearchLimits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeCameraHeroBoundsTest::RunTest(const FString& Parameters)
{
	const FBox2D FloorBounds(FVector2D(4.f, -2409.f), FVector2D(2004.f, -9.f));
	const FBox HeroBounds = FOfficeCameraFraming::MakeHeroBounds(FloorBounds, 0.f, 400.f);
	TestEqual(TEXT("hero min"), HeroBounds.Min, FVector(1004.f, -2409.f, -400.f));
	TestEqual(TEXT("hero max"), HeroBounds.Max, FVector(2004.f, -1209.f, 0.f));

	const TStaticArray<FVector, 8> Corners = FOfficeCameraFraming::MakeBoxCorners(HeroBounds);
	TestEqual(TEXT("hero produces eight corners"), Corners.Num(), 8);
	TestEqual(TEXT("first corner is minimum"), Corners[0], HeroBounds.Min);
	TestEqual(TEXT("last corner is maximum"), Corners[7], HeroBounds.Max);

	return true;
}

bool FCGROfficeCameraInvalidHeroBoundsTest::RunTest(const FString& Parameters)
{
	const float Infinity = std::numeric_limits<float>::infinity();
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const FBox2D ValidFloorBounds(FVector2D(4.f, -2409.f), FVector2D(2004.f, -9.f));

	TestFalse(
		TEXT("infinite visible depth is rejected"),
		FOfficeCameraFraming::MakeHeroBounds(ValidFloorBounds, 0.f, Infinity).IsValid != 0);
	TestFalse(
		TEXT("NaN structural floor is rejected"),
		FOfficeCameraFraming::MakeHeroBounds(ValidFloorBounds, NaN, 400.f).IsValid != 0);
	TestFalse(
		TEXT("zero visible depth is rejected without constructing a box"),
		FOfficeCameraFraming::MakeHeroBounds(ValidFloorBounds, 0.f, 0.f).IsValid != 0);
	TestFalse(
		TEXT("invalid floor bounds are rejected"),
		FOfficeCameraFraming::MakeHeroBounds(FBox2D(ForceInit), 0.f, 400.f).IsValid != 0);

	const FBox2D InfiniteFloorBounds(FVector2D(4.f, -2409.f), FVector2D(Infinity, -9.f));
	TestFalse(
		TEXT("infinite floor coordinate is rejected"),
		FOfficeCameraFraming::MakeHeroBounds(InfiniteFloorBounds, 0.f, 400.f).IsValid != 0);

	const FBox2D NaNFloorBounds(FVector2D(4.f, NaN), FVector2D(2004.f, -9.f));
	TestFalse(
		TEXT("NaN floor coordinate is rejected"),
		FOfficeCameraFraming::MakeHeroBounds(NaNFloorBounds, 0.f, 400.f).IsValid != 0);

	return true;
}

bool FCGROfficeCameraProjectionTest::RunTest(const FString& Parameters)
{
	FVector2D ProjectedPoint = FVector2D::ZeroVector;
	TestTrue(
		TEXT("forward point projects"),
		FOfficeCameraFraming::ProjectWorldPoint(
			FVector(1000.f, 0.f, 0.f),
			FTransform::Identity,
			90.f,
			16.f / 9.f,
			ProjectedPoint));
	TestTrue(TEXT("forward point projects to center"), ProjectedPoint.Equals(FVector2D(0.5f, 0.5f), KINDA_SMALL_NUMBER));

	TestTrue(
		TEXT("right-offset point projects"),
		FOfficeCameraFraming::ProjectWorldPoint(
			FVector(1000.f, 500.f, 0.f),
			FTransform::Identity,
			90.f,
			16.f / 9.f,
			ProjectedPoint));
	TestTrue(TEXT("right-offset point uses horizontal FOV"), ProjectedPoint.Equals(FVector2D(0.75f, 0.5f), KINDA_SMALL_NUMBER));

	TestFalse(
		TEXT("point behind camera is rejected"),
		FOfficeCameraFraming::ProjectWorldPoint(
			FVector(-100.f, 0.f, 0.f),
			FTransform::Identity,
			90.f,
			16.f / 9.f,
			ProjectedPoint));
	TestFalse(
		TEXT("non-finite world point is rejected"),
		FOfficeCameraFraming::ProjectWorldPoint(
			FVector(std::numeric_limits<float>::infinity(), 0.f, 0.f),
			FTransform::Identity,
			90.f,
			16.f / 9.f,
			ProjectedPoint));

	return true;
}

bool FCGROfficeCameraAspectFitTest::RunTest(const FString& Parameters)
{
	const FBox ViewBounds(FVector(1000.f, -800.f, -400.f), FVector(1100.f, 800.f, 400.f));
	const TStaticArray<FVector, 8> ViewCorners = FOfficeCameraFraming::MakeBoxCorners(ViewBounds);
	const TConstArrayView<FVector> CornerView(ViewCorners.GetData(), ViewCorners.Num());
	const FVector2D SafeMin(0.05f, 0.05f);
	const FVector2D SafeMax(0.95f, 0.95f);

	TestTrue(
		TEXT("view fits at 16:9"),
		FOfficeCameraFraming::DoesViewFit(
			CornerView,
			FTransform::Identity,
			90.f,
			16.f / 9.f,
			SafeMin,
			SafeMax));
	TestTrue(
		TEXT("view fits at 2.10 aspect"),
		FOfficeCameraFraming::DoesViewFit(
			CornerView,
			FTransform::Identity,
			90.f,
			2.10f,
			SafeMin,
			SafeMax));

	const TArray<FVector> NonFinitePoints = {
		FVector(std::numeric_limits<float>::infinity(), 0.f, 0.f)
	};
	TestFalse(
		TEXT("view fit rejects a non-finite point"),
		FOfficeCameraFraming::DoesViewFit(
			MakeArrayView(NonFinitePoints),
			FTransform::Identity,
			90.f,
			16.f / 9.f,
			SafeMin,
			SafeMax));

	return true;
}

bool FCGROfficeCameraClosestFitTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> SearchPoints = {FVector(1000.f, 500.f, 0.f)};
	const FVector2D SafeMin(0.05f, 0.05f);
	const FVector2D SafeMax(0.95f, 0.95f);
	int32 SampleProviderCallCount = 0;

	const FOfficeCameraFitResult ClosestFit = FOfficeCameraFraming::FindClosestFit(
		[&SampleProviderCallCount](const float ZoomValue)
		{
			++SampleProviderCallCount;
			FOfficeCameraSample Sample;
			Sample.ZoomValue = ZoomValue;
			Sample.ArmLength = 1000.f + ZoomValue * 4000.f;
			Sample.CameraTransform = FTransform::Identity;
			Sample.HorizontalFOVDegrees = ZoomValue >= 0.4f && ZoomValue <= 0.55f ? 90.f : 20.f;
			Sample.AspectRatio = 16.f / 9.f;
			return Sample;
		},
		MakeArrayView(SearchPoints),
		SafeMin,
		SafeMax,
		32,
		8);

	TestTrue(TEXT("first passing interval is found"), ClosestFit.bFits);
	TestTrue(TEXT("closest passing zoom is refined"), ClosestFit.ZoomValue >= 0.4f && ClosestFit.ZoomValue < 0.4002f);
	TestEqual(TEXT("result arm belongs to refined sample"), ClosestFit.ArmLength, 1000.f + ClosestFit.ZoomValue * 4000.f);
	TestEqual(TEXT("coarse samples through first fit plus eight refinements"), SampleProviderCallCount, 22);
	TestEqual(TEXT("result reports every sample"), ClosestFit.SampleCount, SampleProviderCallCount);

	SampleProviderCallCount = 0;
	const FOfficeCameraFitResult NoFit = FOfficeCameraFraming::FindClosestFit(
		[&SampleProviderCallCount](const float ZoomValue)
		{
			++SampleProviderCallCount;
			FOfficeCameraSample Sample;
			Sample.ZoomValue = ZoomValue;
			Sample.ArmLength = 1000.f + ZoomValue * 4000.f;
			Sample.CameraTransform = FTransform::Identity;
			Sample.HorizontalFOVDegrees = 5.f;
			Sample.AspectRatio = 16.f / 9.f;
			return Sample;
		},
		MakeArrayView(SearchPoints),
		SafeMin,
		SafeMax,
		32,
		8);

	TestFalse(TEXT("farthest sample can still fail"), NoFit.bFits);
	TestEqual(TEXT("no-fit fallback reports farthest zoom"), NoFit.ZoomValue, 1.f);
	TestEqual(TEXT("no-fit fallback carries farthest arm"), NoFit.ArmLength, 5000.f);
	TestEqual(TEXT("32 coarse intervals include both endpoints"), SampleProviderCallCount, 33);
	TestEqual(TEXT("no-fit result reports every coarse sample"), NoFit.SampleCount, SampleProviderCallCount);

	return true;
}

bool FCGROfficeCameraSearchLimitsTest::RunTest(const FString& Parameters)
{
	constexpr int32 ExpectedMaxCoarseSteps = FOfficeCameraFraming::MaxCoarseSteps;
	constexpr int32 ExpectedMaxRefineIterations = FOfficeCameraFraming::MaxRefineIterations;
	const TArray<FVector> SearchPoints = {FVector(1000.f, 500.f, 0.f)};
	const FVector2D SafeMin(0.05f, 0.05f);
	const FVector2D SafeMax(0.95f, 0.95f);

	auto TestRejectedSettings = [this, &SearchPoints, SafeMin, SafeMax](
		const TCHAR* Label,
		const int32 CoarseSteps,
		const int32 RefineIterations)
	{
		int32 ProviderCallCount = 0;
		const FOfficeCameraFitResult Result = FOfficeCameraFraming::FindClosestFit(
			[&ProviderCallCount](const float ZoomValue)
			{
				++ProviderCallCount;
				FOfficeCameraSample Sample;
				Sample.ZoomValue = ZoomValue;
				Sample.ArmLength = 1000.f;
				Sample.CameraTransform = FTransform::Identity;
				Sample.HorizontalFOVDegrees = 90.f;
				Sample.AspectRatio = 16.f / 9.f;
				return Sample;
			},
			MakeArrayView(SearchPoints),
			SafeMin,
			SafeMax,
			CoarseSteps,
			RefineIterations);

		TestFalse(FString::Printf(TEXT("%s returns no fit"), Label), Result.bFits);
		TestEqual(FString::Printf(TEXT("%s keeps default zoom"), Label), Result.ZoomValue, 1.f);
		TestEqual(FString::Printf(TEXT("%s keeps default arm"), Label), Result.ArmLength, 0.f);
		TestEqual(FString::Printf(TEXT("%s reports zero samples"), Label), Result.SampleCount, 0);
		TestEqual(FString::Printf(TEXT("%s never calls provider"), Label), ProviderCallCount, 0);
	};

	TestRejectedSettings(TEXT("zero coarse steps"), 0, 8);
	TestRejectedSettings(TEXT("negative coarse steps"), -1, 8);
	TestRejectedSettings(TEXT("coarse steps above maximum"), ExpectedMaxCoarseSteps + 1, 8);
	TestRejectedSettings(TEXT("INT32_MAX coarse steps"), TNumericLimits<int32>::Max(), 8);
	TestRejectedSettings(TEXT("negative refine iterations"), 32, -1);
	TestRejectedSettings(TEXT("refine iterations above maximum"), 32, ExpectedMaxRefineIterations + 1);
	TestRejectedSettings(TEXT("INT32_MAX refine iterations"), 32, TNumericLimits<int32>::Max());

	int32 ProviderCallCount = 0;
	const FOfficeCameraFitResult MaxCoarseResult = FOfficeCameraFraming::FindClosestFit(
		[&ProviderCallCount](const float ZoomValue)
		{
			++ProviderCallCount;
			FOfficeCameraSample Sample;
			Sample.ZoomValue = ZoomValue;
			Sample.ArmLength = 1000.f + 4000.f * ZoomValue;
			Sample.CameraTransform = FTransform::Identity;
			Sample.HorizontalFOVDegrees = 5.f;
			Sample.AspectRatio = 16.f / 9.f;
			return Sample;
		},
		MakeArrayView(SearchPoints),
		SafeMin,
		SafeMax,
		ExpectedMaxCoarseSteps,
		0);
	TestFalse(TEXT("maximum coarse steps are accepted"), MaxCoarseResult.bFits);
	TestEqual(TEXT("maximum coarse steps sample both endpoints"), ProviderCallCount, ExpectedMaxCoarseSteps + 1);

	ProviderCallCount = 0;
	const FOfficeCameraFitResult MaxRefineResult = FOfficeCameraFraming::FindClosestFit(
		[&ProviderCallCount](const float ZoomValue)
		{
			++ProviderCallCount;
			FOfficeCameraSample Sample;
			Sample.ZoomValue = ZoomValue;
			Sample.ArmLength = 1000.f + 4000.f * ZoomValue;
			Sample.CameraTransform = FTransform::Identity;
			Sample.HorizontalFOVDegrees = ZoomValue >= (1.f / ExpectedMaxCoarseSteps) ? 90.f : 5.f;
			Sample.AspectRatio = 16.f / 9.f;
			return Sample;
		},
		MakeArrayView(SearchPoints),
		SafeMin,
		SafeMax,
		ExpectedMaxCoarseSteps,
		ExpectedMaxRefineIterations);
	TestTrue(TEXT("maximum refine iterations are accepted"), MaxRefineResult.bFits);
	TestEqual(
		TEXT("maximum refine iterations stay within the accepted budget"),
		ProviderCallCount,
		2 + ExpectedMaxRefineIterations);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
