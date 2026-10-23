#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Office/OfficeFootprintGeometry.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/OfficeCameraFraming.h"
#include "Player/OfficeCameraPawn.h"
#include "UObject/UnrealType.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace OfficeCameraExpansionFramingTests
{
constexpr float InitialFramingSafeInset = 0.06f;
constexpr float ApprovedMaximumArmLengthCm = 9000.f;
constexpr float ArmLengthToleranceCm = 0.1f;

class FScopedTestWorld
{
public:
	FScopedTestWorld()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (TestWorld && GEngine)
		{
			FWorldContext& TestWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			TestWorldContext.SetCurrentWorld(TestWorld);
		}
	}

	~FScopedTestWorld()
	{
		if (!TestWorld)
		{
			return;
		}

		if (GEngine)
		{
			GEngine->DestroyWorldContext(TestWorld);
		}
		TestWorld->DestroyWorld(false);
	}

	UWorld* Get() const
	{
		return TestWorld;
	}

private:
	UWorld* TestWorld = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraCanonicalExpansionAuthoredRigRangeTest,
	"CGR.Office.Exterior.CameraFraming.CanonicalExpansionAuthoredRigRange",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraApprovedHeroDepthDefaultTest,
	"CGR.Office.Exterior.CameraFraming.ApprovedHeroDepthDefault",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraApprovedZoomBandTest,
	"CGR.Office.Exterior.CameraFraming.ApprovedZoomBand",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraInitialEntryArmLimitTest,
	"CGR.Office.Exterior.CameraFraming.InitialEntryArmLimit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeCameraInitialEntryArmLimitTest::RunTest(const FString& Parameters)
{
	// Mutation caught: retaining the max-only clamp leaves small footprints as extreme close-ups.
	// Mutation caught: accepting reversed bounds writes an arm for an invalid automatic-entry range.
	float SelectedArmLength = -1.f;
	TestTrue(
		TEXT("arm below the automatic-entry range is valid"),
		FOfficeCameraFraming::TryClampInitialArmLength(153.f, 1200.f, 5200.f, SelectedArmLength));
	TestEqual(TEXT("arm below the range is raised to its minimum"), SelectedArmLength, 1200.f);

	TestTrue(
		TEXT("arm inside the automatic-entry range is valid"),
		FOfficeCameraFraming::TryClampInitialArmLength(1428.9f, 1200.f, 5200.f, SelectedArmLength));
	TestEqual(TEXT("arm inside the range is preserved"), SelectedArmLength, 1428.9f);

	TestTrue(
		TEXT("arm above the automatic-entry range is valid"),
		FOfficeCameraFraming::TryClampInitialArmLength(7764.f, 1200.f, 5200.f, SelectedArmLength));
	TestEqual(TEXT("arm above the range is capped at its maximum"), SelectedArmLength, 5200.f);

	SelectedArmLength = 4321.f;
	TestFalse(
		TEXT("reversed automatic-entry range is rejected"),
		FOfficeCameraFraming::TryClampInitialArmLength(2000.f, 5200.f, 1200.f, SelectedArmLength));
	TestEqual(TEXT("reversed range preserves output"), SelectedArmLength, 4321.f);
	TestFalse(
		TEXT("non-finite calculated arm is rejected"),
		FOfficeCameraFraming::TryClampInitialArmLength(
			std::numeric_limits<float>::quiet_NaN(),
			1200.f,
			6000.f,
			SelectedArmLength));
	TestEqual(TEXT("invalid calculated arm preserves output"), SelectedArmLength, 4321.f);
	TestFalse(
		TEXT("non-positive entry limit is rejected"),
		FOfficeCameraFraming::TryClampInitialArmLength(4109.67578125f, 1200.f, 0.f, SelectedArmLength));
	TestEqual(TEXT("invalid entry limit preserves output"), SelectedArmLength, 4321.f);

	return true;
}

bool FCGROfficeCameraApprovedHeroDepthDefaultTest::RunTest(const FString& Parameters)
{
	// Mutation caught: omitting 300/1200 or regressing 800/5200 restores the rejected initial-entry composition.
	const FFloatProperty* FocusDepthProperty = FindFProperty<FFloatProperty>(
		AOfficeCameraPawn::StaticClass(),
		TEXT("InitialFocusBelowFloorCm"));
	const FFloatProperty* HeroDepthProperty = FindFProperty<FFloatProperty>(
		AOfficeCameraPawn::StaticClass(),
		TEXT("InitialFacadeVisibleDepthCm"));
	const FFloatProperty* InitialArmMinimumProperty = FindFProperty<FFloatProperty>(
		AOfficeCameraPawn::StaticClass(),
		TEXT("InitialMinimumArmLengthCm"));
	const FFloatProperty* InitialArmCapProperty = FindFProperty<FFloatProperty>(
		AOfficeCameraPawn::StaticClass(),
		TEXT("InitialMaximumArmLengthCm"));
	const AOfficeCameraPawn* CameraPawnCDO = GetDefault<AOfficeCameraPawn>();
	const bool bHasFocusDepthProperty =
		TestNotNull(TEXT("InitialFocusBelowFloorCm property"), FocusDepthProperty);
	const bool bHasHeroDepthProperty =
		TestNotNull(TEXT("InitialFacadeVisibleDepthCm property"), HeroDepthProperty);
	const bool bHasInitialArmMinimumProperty =
		TestNotNull(TEXT("InitialMinimumArmLengthCm property"), InitialArmMinimumProperty);
	const bool bHasInitialArmCapProperty =
		TestNotNull(TEXT("InitialMaximumArmLengthCm property"), InitialArmCapProperty);
	const bool bHasCameraPawnCDO = TestNotNull(TEXT("office camera pawn CDO"), CameraPawnCDO);
	if (bHasFocusDepthProperty && bHasCameraPawnCDO)
	{
		TestEqual(
			TEXT("initial focus aims 300cm below the structural floor"),
			FocusDepthProperty->GetPropertyValue_InContainer(CameraPawnCDO),
			300.f);
	}
	if (bHasHeroDepthProperty && bHasCameraPawnCDO)
	{
		TestEqual(
			TEXT("initial hero depth keeps two 400cm facade floors prominent"),
			HeroDepthProperty->GetPropertyValue_InContainer(CameraPawnCDO),
			800.f);
	}
	if (bHasInitialArmMinimumProperty && bHasCameraPawnCDO)
	{
		TestEqual(
			TEXT("automatic Office entry arm keeps at least 1200cm distance"),
			InitialArmMinimumProperty->GetPropertyValue_InContainer(CameraPawnCDO),
			1200.f);
	}
	if (bHasInitialArmCapProperty && bHasCameraPawnCDO)
	{
		TestEqual(
			TEXT("automatic Office entry arm is capped at 5200cm"),
			InitialArmCapProperty->GetPropertyValue_InContainer(CameraPawnCDO),
			5200.f);
	}

	return bHasFocusDepthProperty
		&& bHasHeroDepthProperty
		&& bHasInitialArmMinimumProperty
		&& bHasInitialArmCapProperty
		&& bHasCameraPawnCDO;
}

bool FCGROfficeCameraApprovedZoomBandTest::RunTest(const FString& Parameters)
{
	// Zoom changes the Office viewing distance while preserving its framing angle and lens.
	const AOfficeCameraPawn* CameraPawnCDO = GetDefault<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn CDO"), CameraPawnCDO)
		|| !TestNotNull(
			TEXT("office movement input handler"),
			CameraPawnCDO ? CameraPawnCDO->MovementInputHandler.Get() : nullptr))
	{
		return false;
	}

	const UMovementInputHandler* MovementHandler = CameraPawnCDO->MovementInputHandler.Get();
	TestEqual(TEXT("Office zoom-in pitch"), MovementHandler->ZoomInPitch, -35.f);
	TestEqual(TEXT("Office zoom-out pitch"), MovementHandler->ZoomOutPitch, -35.f);
	TestEqual(TEXT("Office zoom-in horizontal FOV"), MovementHandler->ZoomInFOV, 35.f);
	TestEqual(TEXT("Office zoom-out horizontal FOV"), MovementHandler->ZoomOutFOV, 35.f);
	TestEqual(TEXT("Office authored rig mapping remains unchanged"), MovementHandler->MaxZoomDistance, 12500.f);

	return true;
}

bool FCGROfficeCameraCanonicalExpansionAuthoredRigRangeTest::RunTest(const FString& Parameters)
{
	// 전체 hero bounds의 raw-fit 수학 검증은 저작 리그 전체를 사용한다.
	// 실제 Office 입력 상한과 5x6 진입 구도는 OfficeCameraZoomStateTests에서 별도로 검증한다.
	using namespace OfficeCameraExpansionFramingTests;

	FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	CameraPawn->MovementInputHandler->SetMaximumZoomValue(1.f);
	CameraPawn->SetActorLocation(FVector::ZeroVector);
	CameraPawn->SetActorRotation(FRotator(0.f, 135.f, 0.f));
	CameraPawn->SpringArm->bEnableCameraLag = false;
	CameraPawn->SpringArm->bEnableCameraRotationLag = false;

	auto RefreshCameraTransforms = [CameraPawn]()
	{
		CameraPawn->SpringArm->UpdateComponentToWorld();
		CameraPawn->SpringArm->TickComponent(0.f, LEVELTICK_All, nullptr);
		CameraPawn->CameraComponent->UpdateComponentToWorld();
	};

	auto MakeTargetAlignedSample = [CameraPawn, &RefreshCameraTransforms](
		const FVector& FloorFocusPoint,
		const float ZoomValue,
		const float AspectRatio)
	{
		CameraPawn->MovementInputHandler->SetZoomValue(ZoomValue);
		CameraPawn->MovementInputHandler->ApplyZoomSettings();
		RefreshCameraTransforms();

		FOfficeCameraSample Sample;
		Sample.ZoomValue = ZoomValue;
		Sample.ArmLength = CameraPawn->SpringArm->TargetArmLength;
		Sample.CameraTransform = CameraPawn->CameraComponent->GetComponentTransform();
		Sample.HorizontalFOVDegrees = CameraPawn->CameraComponent->FieldOfView;
		Sample.AspectRatio = AspectRatio;

		const FVector CameraForward = CameraPawn->CameraComponent->GetForwardVector();
		if (FMath::IsNearlyZero(CameraForward.Z))
		{
			Sample.HorizontalFOVDegrees = 0.f;
			return Sample;
		}

		const FVector CameraLocation = CameraPawn->CameraComponent->GetComponentLocation();
		const float RayDistance = (FloorFocusPoint.Z - CameraLocation.Z) / CameraForward.Z;
		const FVector CenterRayFloorPoint = CameraLocation + CameraForward * RayDistance;
		Sample.CameraTransform.AddToTranslation(FloorFocusPoint - CenterRayFloorPoint);
		return Sample;
	};

	auto FindFootprintFit = [CameraPawn, &MakeTargetAlignedSample](
		const FIntPoint TileCount,
		const float YawDegrees,
		const float AspectRatio,
		TStaticArray<FVector, 8>& OutHeroCorners,
		FVector& OutFloorFocusPoint)
	{
		CameraPawn->SetActorRotation(FRotator(0.f, YawDegrees, 0.f));
		const FBox2D FloorBounds = FOfficeFootprintGeometry::MakeFloorBounds(TileCount, 400.f);
		FOfficeCameraFraming::TryMakeFloorFocusPoint(
			FloorBounds,
			FOfficeFootprintGeometry::StructuralFloorZ,
			300.f,
			OutFloorFocusPoint);
		OutHeroCorners = FOfficeCameraFraming::MakeBoxCorners(
			FOfficeCameraFraming::MakeHeroBounds(
				FloorBounds,
				FOfficeFootprintGeometry::StructuralFloorZ,
				800.f));

		return FOfficeCameraFraming::FindClosestFit(
			[&MakeTargetAlignedSample, OutFloorFocusPoint, AspectRatio](const float ZoomValue)
			{
				return MakeTargetAlignedSample(OutFloorFocusPoint, ZoomValue, AspectRatio);
			},
			TConstArrayView<FVector>(OutHeroCorners.GetData(), OutHeroCorners.Num()),
			FVector2D(InitialFramingSafeInset, InitialFramingSafeInset),
			FVector2D(1.f - InitialFramingSafeInset, 1.f - InitialFramingSafeInset),
			32,
			8);
	};

	const TArray<float> SupportedYawDegrees = {90.f, 135.f, 150.f, 180.f};
	const TArray<float> SupportedAspectRatios = {16.f / 9.f, 21.f / 10.f};
	for (const float YawDegrees : SupportedYawDegrees)
	{
		for (const float AspectRatio : SupportedAspectRatios)
		{
			const FString Context = FString::Printf(
				TEXT("yaw %.0f aspect %.3f"),
				YawDegrees,
				AspectRatio);
			TStaticArray<FVector, 8> StarterHeroCorners;
			FVector StarterFloorFocusPoint = FVector::ZeroVector;
			const FOfficeCameraFitResult StarterFit = FindFootprintFit(
				FIntPoint(1, 2),
				YawDegrees,
				AspectRatio,
				StarterHeroCorners,
				StarterFloorFocusPoint);

			TStaticArray<FVector, 8> MaximumHeroCorners;
			FVector MaximumFloorFocusPoint = FVector::ZeroVector;
			const FOfficeCameraFitResult MaximumFit = FindFootprintFit(
				FIntPoint(5, 6),
				YawDegrees,
				AspectRatio,
				MaximumHeroCorners,
				MaximumFloorFocusPoint);

			const FOfficeCameraSample MaximumZoomSample = MakeTargetAlignedSample(
				MaximumFloorFocusPoint,
				1.f,
				AspectRatio);
			TestTrue(
				*FString::Printf(TEXT("%s maximum zoom frames 5x6 two-floor hero bounds"), *Context),
				FOfficeCameraFraming::DoesViewFit(
					TConstArrayView<FVector>(MaximumHeroCorners.GetData(), MaximumHeroCorners.Num()),
					MaximumZoomSample.CameraTransform,
					MaximumZoomSample.HorizontalFOVDegrees,
					MaximumZoomSample.AspectRatio,
					FVector2D(InitialFramingSafeInset, InitialFramingSafeInset),
					FVector2D(1.f - InitialFramingSafeInset, 1.f - InitialFramingSafeInset)));
			TestTrue(
				*FString::Printf(TEXT("%s canonical 5x6 hero bounds find a closest fit"), *Context),
				MaximumFit.bFits);
			TestTrue(
				*FString::Printf(TEXT("%s canonical 5x6 closest fit stays at or below 9000cm"), *Context),
				MaximumFit.ArmLength
					<= ApprovedMaximumArmLengthCm + ArmLengthToleranceCm);
			TestTrue(
				*FString::Printf(TEXT("%s starter 1x2 footprint still finds a closest fit"), *Context),
				StarterFit.bFits);
			TestTrue(
				*FString::Printf(TEXT("%s starter 1x2 fit remains closer than canonical 5x6"), *Context),
				StarterFit.ArmLength < MaximumFit.ArmLength);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
