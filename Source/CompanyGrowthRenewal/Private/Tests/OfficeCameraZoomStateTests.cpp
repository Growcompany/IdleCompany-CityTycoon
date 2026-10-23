#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Entity/Officeworker/Officeworker.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Misc/AutomationTest.h"
#include "Office/OfficeInterior.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/OfficeCameraPawn.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

struct FMovementInputHandlerTestAccess
{
	static void ApplyZoomDirection(UMovementInputHandler& Handler, float Direction)
	{
		Handler.ZoomDirection = Direction;
		Handler.UpdateZoom();
		Handler.ZoomDirection = 0.f;
	}
};

struct FOfficeCameraPawnTestAccess
{
	static void RecordAutomaticPose(AOfficeCameraPawn& CameraPawn)
	{
		CameraPawn.LastAutomaticFramingPose = CameraPawn.GetActorTransform();
		CameraPawn.LastAutomaticFramingZoom = CameraPawn.MovementInputHandler->GetZoomValue();
	}

	static bool IsAtAutomaticFramingPose(const AOfficeCameraPawn& CameraPawn)
	{
		return CameraPawn.IsAtAutomaticFramingPose();
	}

	static void BindOfficeInterior(AOfficeCameraPawn& CameraPawn, AOfficeInterior* Interior)
	{
		CameraPawn.BindOfficeBoundaryInterior(Interior);
	}

	static bool ResolveZoomValueForArmLength(
		AOfficeCameraPawn& CameraPawn,
		const float DesiredArmLength,
		float& OutZoomValue,
		float& OutClampedArmLength)
	{
		return CameraPawn.ResolveZoomValueForArmLength(
			DesiredArmLength,
			OutZoomValue,
			OutClampedArmLength);
	}

	static bool SettleFocusOnLocationImmediately(
		AOfficeCameraPawn& CameraPawn,
		const FVector& Location,
		const float DesiredArmLength,
		const float MinimumArmLength,
		const float MaximumArmLength)
	{
		return CameraPawn.SettleFocusOnLocationImmediately(
			Location,
			DesiredArmLength,
			MinimumArmLength,
			MaximumArmLength);
	}

	static void RefreshCameraRigTransforms(AOfficeCameraPawn& CameraPawn)
	{
		CameraPawn.RefreshCameraRigTransforms();
	}

	static bool TrySettleCalibratedInitialPose(
		AOfficeCameraPawn& CameraPawn,
		AOfficeInterior* Interior,
		const FBox2D* FramingBounds = nullptr)
	{
		return CameraPawn.TrySettleCalibratedInitialPose(Interior, FramingBounds);
	}

	static void SeedChildZoomInterpolation(
		AOfficeCameraPawn& CameraPawn,
		const float InTargetZoomValue)
	{
		CameraPawn.TargetZoomValue = InTargetZoomValue;
		CameraPawn.bIsZooming = true;
	}

	static bool IsChildZoomInterpolationActive(const AOfficeCameraPawn& CameraPawn)
	{
		return CameraPawn.bIsZooming;
	}

	static void RequestInitialOfficeFraming(AOfficeCameraPawn& CameraPawn)
	{
		CameraPawn.HandleGameDataLoaded();
	}

	static bool IsInitialOfficeFramingPending(const AOfficeCameraPawn& CameraPawn)
	{
		return CameraPawn.bInitialOfficeFramingPending;
	}

	static bool IsInitialOfficeFramingApplied(const AOfficeCameraPawn& CameraPawn)
	{
		return CameraPawn.bInitialOfficeFramingApplied;
	}
};

namespace OfficeCameraZoomStateTests
{
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
	FCGROfficeCameraSharedZoomTargetTest,
	"CGR.Office.Exterior.Camera.SharedZoomTarget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraStopTransitionCancelsZoomTest,
	"CGR.Office.Exterior.Camera.StopTransitionCancelsZoom",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraFocusDistanceHardUpperBoundTest,
	"CGR.Office.Exterior.Camera.FocusDistanceHardUpperBound",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraFocusDistanceRangeClampTest,
	"CGR.Office.Exterior.Camera.FocusDistanceRangeClamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraCurveCorrectFallbackArmTest,
	"CGR.Office.Exterior.Camera.CurveCorrectFallbackArm",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraZoomCurveArmMonotonicityTest,
	"CGR.Office.Exterior.Camera.ZoomCurveArmMonotonicity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraInitialFramingWaitsForViewportTest,
	"CGR.Office.Exterior.Camera.InitialFramingWaitsForViewport",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraInitialFocusSettlesImmediatelyTest,
	"CGR.Office.Exterior.Camera.InitialFocusSettlesImmediately",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraInitialSettleRespectsApprovedArmRangeTest,
	"CGR.Office.Exterior.Camera.InitialSettleRespectsApprovedArmRange",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraFailedInitialSettlePreservesZoomStateTest,
	"CGR.Office.Exterior.Camera.FailedInitialSettlePreservesZoomState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraPoseLogCapturesReproducibleReferenceFramesTest,
	"CGR.Office.Exterior.Camera.PoseLogCapturesReproducibleReferenceFrames",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraCalibratedTwoByTwoPoseSettlesExactlyTest,
	"CGR.Office.Exterior.Camera.CalibratedTwoByTwoPoseSettlesExactly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraCalibratedDefaultFootprintUsesBaselineTest,
	"CGR.Office.Exterior.Camera.CalibratedDefaultFootprintUsesBaseline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraCalibratedWallCornerCenteredTest,
	"CGR.Office.Exterior.Camera.CalibratedWallCornerCentered",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraCalibratedArmScalesMonotonicallyTest,
	"CGR.Office.Exterior.Camera.CalibratedArmScalesMonotonically",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeCameraExpansionPreservesPendingInputTest,
	"CGR.Office.Exterior.Camera.ExpansionPreservesPendingInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeCameraExpansionPreservesPendingInputTest::RunTest(const FString&)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get())) return false;
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera"), CameraPawn)) return false;
	FOfficeCameraPawnTestAccess::RecordAutomaticPose(*CameraPawn);
	TestTrue(TEXT("untouched initial framing may adapt"), FOfficeCameraPawnTestAccess::IsAtAutomaticFramingPose(*CameraPawn));
	CameraPawn->AddMovementInput(FVector::ForwardVector, 1.f, true);
	TestFalse(TEXT("queued movement before the next tick is preserved"), FOfficeCameraPawnTestAccess::IsAtAutomaticFramingPose(*CameraPawn));
	CameraPawn->ConsumeMovementInputVector();
	CameraPawn->FocusOnLocation(CameraPawn->GetActorLocation() + FVector(100.f, 0.f, 0.f));
	TestTrue(TEXT("focus transition has begun before pose moves"), CameraPawn->IsTransitioningCamera());
	TestFalse(TEXT("new focus target is preserved before its first tick"), FOfficeCameraPawnTestAccess::IsAtAutomaticFramingPose(*CameraPawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraInitialPhotoViewOffsetPreservesBaseRigTest,
	"CGR.Office.Exterior.Camera.InitialPhotoViewOffsetPreservesBaseRig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraRegularFocusStillGlidesTest,
	"CGR.Office.Exterior.Camera.RegularFocusStillGlides",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraLocalPanBoundaryTest,
	"CGR.Office.Exterior.Camera.LocalPanBoundary",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraLocalZoomExpansionTest,
	"CGR.Office.Exterior.Camera.LocalZoomExpansion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfficeCameraLocalEmployeeEdgeFocusTest,
	"CGR.Office.Exterior.Camera.LocalEmployeeEdgeFocus",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfficeCameraLocalEmployeeEdgeFocusTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get())) { return false; }
	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeworker* Employee = TestWorld.Get()->SpawnActor<AOfficeworker>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("employee"), Employee)) { return false; }
	Interior->SetActorScale3D(FVector(1.2f));
	const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
	for (const bool bUseLocalBounds : {false, true})
	{
		AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
		if (!TestNotNull(TEXT("office camera"), CameraPawn)) { return false; }
		if (bUseLocalBounds) { FOfficeCameraPawnTestAccess::BindOfficeInterior(*CameraPawn, Interior); }
		for (const float Yaw : {90.f, 135.f, 150.f, 180.f})
		{
			for (const double EdgeX : {FloorBounds.Min.X + 60.0, FloorBounds.Max.X - 60.0})
			{
				for (const double EdgeY : {FloorBounds.Min.Y + 60.0, FloorBounds.Max.Y - 60.0})
				{
					CameraPawn->StopFollowingEmployee();
					CameraPawn->StopCameraTransition();
					CameraPawn->SetActorLocation(Interior->GetActorTransform().TransformPosition(FVector(FloorBounds.GetCenter(), 0.f)));
					CameraPawn->SetActorRotation(FRotator(0.f, Yaw, 0.f));
					FMovementInputHandlerTestAccess::ApplyZoomDirection(*CameraPawn->MovementInputHandler, 100.f);
					FOfficeCameraPawnTestAccess::RefreshCameraRigTransforms(*CameraPawn);
					Employee->SetActorLocation(Interior->GetActorTransform().TransformPosition(FVector(EdgeX, EdgeY, 100.f)));
					CameraPawn->FocusOnEmployee(Employee, 500.f, true);
					for (int32 Frame = 0; Frame < 600; ++Frame)
					{
						CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
						CameraPawn->SpringArm->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
						CameraPawn->CameraComponent->UpdateComponentToWorld();
					}
					const FVector CameraLocation = CameraPawn->CameraComponent->GetComponentLocation();
					const FVector ViewDirection = CameraPawn->CameraComponent->GetForwardVector();
					const FVector EmployeeCenter = Employee->GetCenterLocation();
					const FVector LookPoint = CameraLocation + ViewDirection * ((EmployeeCenter.Z - CameraLocation.Z) / ViewDirection.Z);
					TestTrue(FString::Printf(TEXT("employee at floor edge %.0f,%.0f stays centered at yaw %.0f (bounds %d, error %.2fcm, transition %d)"),
						EdgeX, EdgeY, Yaw, bUseLocalBounds, FVector2D(LookPoint - EmployeeCenter).Size(), CameraPawn->IsTransitioningCamera()),
						FVector2D(LookPoint - EmployeeCenter).Size() < 1.0);
					TestFalse(TEXT("employee focus transition completes while follow remains active"), CameraPawn->IsTransitioningCamera());
				}
			}
		}
	}
	return true;
}

bool FCGROfficeCameraLocalPanBoundaryTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get())) { return false; }
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office camera"), CameraPawn)
		|| !TestNotNull(TEXT("office interior"), Interior)) { return false; }

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 2;
	OfficeData.TileCountY = 3;
	OfficeData.StarterTileCountX = 2;
	OfficeData.StarterTileCountY = 3;
	Interior->ApplyOfficeSaveData(OfficeData);
	const FTransform InteriorTransform(FRotator(0.f, 37.f, 0.f), FVector(5104.f, -3209.f, 275.f), FVector(1.2f));
	Interior->SetActorTransform(InteriorTransform);
	FOfficeCameraPawnTestAccess::BindOfficeInterior(*CameraPawn, Interior);
	const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
	const double TileMargin = FloorBounds.GetSize().X / Interior->GetTileCount().X;
	const FVector OutsideLocal(FloorBounds.Max.X + 6000.f, FloorBounds.Min.Y - 6000.f, 450.f);
	CameraPawn->SetActorLocation(InteriorTransform.TransformPosition(OutsideLocal));
	CameraPawn->MovementComponent->Velocity = InteriorTransform.TransformVectorNoScale(FVector(700.f, -800.f, 90.f));
	const float OriginalWorldZ = CameraPawn->GetActorLocation().Z;
	CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	const FVector ConstrainedLocal = InteriorTransform.InverseTransformPosition(CameraPawn->GetActorLocation());
	TestEqual(TEXT("long pan stops one tile past the right floor edge"), ConstrainedLocal.X, FloorBounds.Max.X + TileMargin, 0.02);
	TestEqual(TEXT("long pan stops one tile past the lower floor edge"), ConstrainedLocal.Y, FloorBounds.Min.Y - TileMargin, 0.02);
	TestEqual(TEXT("pan constraint preserves camera height"), CameraPawn->GetActorLocation().Z, static_cast<double>(OriginalWorldZ), 0.02);
	const FVector RemainingLocalVelocity = InteriorTransform.InverseTransformVectorNoScale(CameraPawn->MovementComponent->Velocity);
	TestEqual(TEXT("outward keyboard momentum is removed at both edges"), FVector2D(RemainingLocalVelocity).Size(), 0.0, 0.02);
	TestEqual(TEXT("vertical movement remains available"), RemainingLocalVelocity.Z, 90.0, 0.02);

	const FVector InwardLocal = ConstrainedLocal + FVector(-80.f, 80.f, 0.f);
	CameraPawn->SetActorLocation(InteriorTransform.TransformPosition(InwardLocal));
	CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	TestTrue(TEXT("moving back inside responds immediately"), InteriorTransform.InverseTransformPosition(CameraPawn->GetActorLocation()).Equals(InwardLocal, 0.02));
	return true;
}

bool FCGROfficeCameraLocalZoomExpansionTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get())) { return false; }
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office camera"), CameraPawn)
		|| !TestNotNull(TEXT("office interior"), Interior)) { return false; }

	Interior->SetActorScale3D(FVector(1.2f));
	FOfficeCameraPawnTestAccess::BindOfficeInterior(*CameraPawn, Interior);
	FMovementInputHandlerTestAccess::ApplyZoomDirection(*CameraPawn->MovementInputHandler, 100.f);
	const float SmallRoomMaximumArm = CameraPawn->SpringArm->TargetArmLength;
	TestTrue(TEXT("starter office cannot zoom out to the distant city"), SmallRoomMaximumArm > 1726.f && SmallRoomMaximumArm < 2500.f);
	const FVector PreservedLocation = CameraPawn->GetActorLocation();
	const float PreservedZoom = CameraPawn->MovementInputHandler->GetZoomValue();
	FOfficeSaveData ExpandedData;
	ExpandedData.TileCountX = 5;
	ExpandedData.TileCountY = 6;
	ExpandedData.StarterTileCountX = 1;
	ExpandedData.StarterTileCountY = 2;
	Interior->ApplyOfficeSaveData(ExpandedData);
	TestEqual(TEXT("expanding the floor preserves the selected zoom"), CameraPawn->MovementInputHandler->GetZoomValue(), PreservedZoom);
	TestEqual(TEXT("expanding the floor preserves the current position"), CameraPawn->GetActorLocation(), PreservedLocation);
	FMovementInputHandlerTestAccess::ApplyZoomDirection(*CameraPawn->MovementInputHandler, 100.f);
	TestTrue(TEXT("large offices receive a larger view without exceeding the city ceiling"), CameraPawn->SpringArm->TargetArmLength > SmallRoomMaximumArm && CameraPawn->SpringArm->TargetArmLength <= 5200.f);

	const FBox2D ExpandedFloor = Interior->GetCurrentFloorBoundsLocal();
	const double TileMargin = ExpandedFloor.GetSize().X / Interior->GetTileCount().X;
	const FVector NewEdgeLocal(ExpandedFloor.Max.X + TileMargin - 20.f, ExpandedFloor.Max.Y + TileMargin - 20.f, 0.f);
	CameraPawn->SetActorLocation(Interior->GetActorTransform().TransformPosition(NewEdgeLocal));
	CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	TestTrue(TEXT("newly expanded floor and its margin remain reachable"), Interior->GetActorTransform().InverseTransformPosition(CameraPawn->GetActorLocation()).Equals(NewEdgeLocal, 0.02));
	return true;
}

bool FCGROfficeCameraSharedZoomTargetTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get()))
	{
		return false;
	}

	CameraPawn->MovementInputHandler->SetZoomValue(1.f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();

	constexpr float DesiredArmLength = 143.f;
	CameraPawn->FocusOnLocation(FVector(1000.f, -1000.f, 0.f), DesiredArmLength);
	for (int32 FrameIndex = 0; FrameIndex < 240; ++FrameIndex)
	{
		CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	}

	const float FinalArmLength = CameraPawn->SpringArm->TargetArmLength;
	TestTrue(
		FString::Printf(
			TEXT("shared zoom target converges near requested arm (requested %.1f, actual %.1f)"),
			DesiredArmLength,
			FinalArmLength),
		FMath::Abs(FinalArmLength - DesiredArmLength) <= 15.f);

	return true;
}

bool FCGROfficeCameraStopTransitionCancelsZoomTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get()))
	{
		return false;
	}

	CameraPawn->MovementInputHandler->SetZoomValue(1.f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	CameraPawn->FocusOnLocation(FVector(1000.f, -1000.f, 0.f), 143.f);
	CameraPawn->StopCameraTransition();

	const float ZoomBeforeTick = CameraPawn->MovementInputHandler->GetZoomValue();
	CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);

	TestEqual(
		TEXT("stopping a location focus keeps zoom unchanged on the following tick"),
		CameraPawn->MovementInputHandler->GetZoomValue(),
		ZoomBeforeTick,
		KINDA_SMALL_NUMBER);

	return true;
}

bool FCGROfficeCameraFocusDistanceHardUpperBoundTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get()))
	{
		return false;
	}

	CameraPawn->MovementInputHandler->SetZoomValue(1.f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();

	constexpr float RequestedUpperBoundCm = 4000.f;
	CameraPawn->FocusOnLocation(FVector(1000.f, -1000.f, -500.f), RequestedUpperBoundCm);
	for (int32 FrameIndex = 0; FrameIndex < 240; ++FrameIndex)
	{
		CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	}

	const float FinalArmLength = CameraPawn->SpringArm->TargetArmLength;
	// 상한 내부의 요청도 양방향 오차로 요청 거리보다 멀어지지 않아야 한다.
	TestTrue(
		FString::Printf(TEXT("focus distance is a hard upper bound (actual %.6f)"), FinalArmLength),
		FinalArmLength <= RequestedUpperBoundCm);
	TestTrue(
		FString::Printf(TEXT("one-sided solve selects the closest lower arm (actual %.6f)"), FinalArmLength),
		RequestedUpperBoundCm - FinalArmLength <= 1.f);

	return true;
}

bool FCGROfficeCameraFocusDistanceRangeClampTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get()))
	{
		return false;
	}

	CameraPawn->FocusOnLocation(FVector::ZeroVector, -100.f);
	for (int32 FrameIndex = 0; FrameIndex < 240; ++FrameIndex)
	{
		CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	}
	TestEqual(
		TEXT("distance below the zoom range clamps to the minimum arm"),
		CameraPawn->SpringArm->TargetArmLength,
		CameraPawn->MovementInputHandler->MinZoomDistance,
		0.1f);

	CameraPawn->FocusOnLocation(FVector::ZeroVector, 50000.f);
	for (int32 FrameIndex = 0; FrameIndex < 240; ++FrameIndex)
	{
		CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	}
	TestEqual(
		TEXT("distance above the zoom range clamps to the manual maximum arm"),
		CameraPawn->SpringArm->TargetArmLength,
		5200.f,
		0.1f);
	TestFalse(TEXT("overrange focus finishes instead of chasing an unreachable zoom target"), CameraPawn->IsTransitioningCamera());

	return true;
}

bool FCGROfficeCameraCurveCorrectFallbackArmTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get()))
	{
		return false;
	}

	constexpr float RequestedArmLengthCm = 1200.f;
	float ZoomValue = -1.f;
	float ClampedArmLength = -1.f;
	TestTrue(
		TEXT("fallback arm resolves through the configured zoom curve"),
		FOfficeCameraPawnTestAccess::ResolveZoomValueForArmLength(
			*CameraPawn,
			RequestedArmLengthCm,
			ZoomValue,
			ClampedArmLength));
	CameraPawn->MovementInputHandler->SetZoomValue(ZoomValue);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();

	TestEqual(TEXT("fallback requested arm remains in range"), ClampedArmLength, RequestedArmLengthCm);
	TestTrue(
		FString::Printf(
			TEXT("curve-correct fallback arm stays below the request (actual %.6f)"),
			CameraPawn->SpringArm->TargetArmLength),
		CameraPawn->SpringArm->TargetArmLength <= RequestedArmLengthCm);
	TestTrue(
		TEXT("curve-correct fallback arm selects the closest lower sample"),
		RequestedArmLengthCm - CameraPawn->SpringArm->TargetArmLength <= 1.f);

	return true;
}

bool FCGROfficeCameraZoomCurveArmMonotonicityTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
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

	constexpr int32 SegmentCount = 256;
	constexpr float ArmToleranceCm = 0.01f;
	float FirstArmLength = 0.f;
	float PreviousArmLength = 0.f;
	for (int32 SampleIndex = 0; SampleIndex <= SegmentCount; ++SampleIndex)
	{
		const float ZoomValue = static_cast<float>(SampleIndex) / static_cast<float>(SegmentCount);
		CameraPawn->MovementInputHandler->SetZoomValue(ZoomValue);
		CameraPawn->MovementInputHandler->ApplyZoomSettings();
		const float CurrentArmLength = CameraPawn->SpringArm->TargetArmLength;
		TestEqual(TEXT("Office pitch stays fixed throughout applied zoom"),
			CameraPawn->SpringArm->GetRelativeRotation().Pitch, -35.0, 0.001);
		TestEqual(TEXT("Office horizontal FOV stays fixed throughout applied zoom"),
			CameraPawn->CameraComponent->FieldOfView, 35.f, 0.001f);
		if (!FMath::IsFinite(CurrentArmLength))
		{
			AddError(FString::Printf(
				TEXT("zoom curve produced a non-finite arm at ZoomValue %.6f"),
				ZoomValue));
			return false;
		}
		if (SampleIndex == 0)
		{
			FirstArmLength = CurrentArmLength;
		}
		else if (CurrentArmLength + ArmToleranceCm < PreviousArmLength)
		{
			AddError(FString::Printf(
				TEXT("zoom curve arm decreased at ZoomValue %.6f: %.6f -> %.6f"),
				ZoomValue,
				PreviousArmLength,
				CurrentArmLength));
			return false;
		}
		PreviousArmLength = CurrentArmLength;
	}

	TestEqual(
		TEXT("ZoomValue 0 resolves to the configured minimum arm"),
		FirstArmLength,
		CameraPawn->MovementInputHandler->MinZoomDistance,
		ArmToleranceCm);
	TestEqual(
		TEXT("ZoomValue 1 is limited to the Office visible maximum arm"),
		PreviousArmLength,
		5200.f,
		0.02f);
	TestEqual(TEXT("original curve distance mapping remains intact"), CameraPawn->MovementInputHandler->MaxZoomDistance, 12500.f);
	const float LimitedZoom = CameraPawn->MovementInputHandler->GetMaximumZoomValue();
	FMovementInputHandlerTestAccess::ApplyZoomDirection(*CameraPawn->MovementInputHandler, 100.f);
	TestEqual(TEXT("shared wheel/pinch update cannot exceed the normalized ceiling"), CameraPawn->MovementInputHandler->GetZoomValue(), LimitedZoom);
	TestTrue(TEXT("shared input update cannot expose the distant shell"), CameraPawn->SpringArm->TargetArmLength <= 5200.f);
	FMovementInputHandlerTestAccess::ApplyZoomDirection(*CameraPawn->MovementInputHandler, -1.f);
	TestTrue(TEXT("reversing zoom responds immediately without saturated hidden range"), CameraPawn->MovementInputHandler->GetZoomValue() < LimitedZoom);
	CameraPawn->MovementInputHandler->SetZoomValue(0.209888f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	TestEqual(TEXT("manual approved entry zoom keeps its exact distance mapping"), CameraPawn->SpringArm->TargetArmLength, 1438.554f, 0.25f);
	TestEqual(TEXT("approved entry FOV remains fixed"), CameraPawn->CameraComponent->FieldOfView, 35.f, 0.02f);
	APlayerCamera* CityCamera = TestWorld.Get()->SpawnActor<APlayerCamera>();
	if (TestNotNull(TEXT("shared city camera"), CityCamera))
	{
		CityCamera->MovementInputHandler->SetZoomValue(1.f);
		CityCamera->MovementInputHandler->ApplyZoomSettings();
		TestEqual(TEXT("other maps keep the full normalized zoom range"), CityCamera->MovementInputHandler->GetZoomValue(), 1.f);
		TestEqual(TEXT("city arm remains unrestricted by Office policy"), CityCamera->SpringArm->TargetArmLength, CityCamera->MovementInputHandler->MaxZoomDistance, 0.01f);
	}

	return true;
}

bool FCGROfficeCameraInitialFramingWaitsForViewportTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn))
	{
		return false;
	}

	FOfficeCameraPawnTestAccess::RequestInitialOfficeFraming(*CameraPawn);
	TestTrue(
		TEXT("data load keeps initial framing pending while no viewport exists"),
		FOfficeCameraPawnTestAccess::IsInitialOfficeFramingPending(*CameraPawn));
	TestFalse(
		TEXT("initial framing is not marked applied without a viewport"),
		FOfficeCameraPawnTestAccess::IsInitialOfficeFramingApplied(*CameraPawn));

	CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
	TestTrue(
		TEXT("a zero-sized viewport keeps the retry pending after Tick"),
		FOfficeCameraPawnTestAccess::IsInitialOfficeFramingPending(*CameraPawn));
	TestFalse(
		TEXT("Tick does not apply fallback framing before viewport readiness"),
		FOfficeCameraPawnTestAccess::IsInitialOfficeFramingApplied(*CameraPawn));

	return true;
}

bool FCGROfficeCameraInitialFocusSettlesImmediatelyTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
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

	CameraPawn->SetActorLocation(FVector(574.906541f, -538.555362f, 0.f));
	CameraPawn->SetActorRotation(FRotator(0.f, 150.f, 0.f));
	CameraPawn->MovementInputHandler->SetZoomValue(1.f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	CameraPawn->SpringArm->bEnableCameraLag = false;
	CameraPawn->SpringArm->bEnableCameraRotationLag = false;
	FOfficeCameraPawnTestAccess::RefreshCameraRigTransforms(*CameraPawn);
	CameraPawn->SpringArm->bEnableCameraLag = true;
	CameraPawn->SpringArm->bEnableCameraRotationLag = true;

	CameraPawn->FocusOnLocation(FVector(-2000.f, 1800.f, 200.f), 5200.f);
	TestTrue(
		TEXT("test precondition starts a regular parent camera glide"),
		CameraPawn->IsTransitioningCamera());

	const FVector InitialPawnLocation = CameraPawn->GetActorLocation();
	const FVector TargetFocusLocation(404.f, -409.f, -500.f);
	constexpr float RequestedArmLengthCm = 1428.9f;
	TestTrue(
		TEXT("initial focus pose settles successfully"),
		FOfficeCameraPawnTestAccess::SettleFocusOnLocationImmediately(
			*CameraPawn,
			TargetFocusLocation,
			RequestedArmLengthCm,
			1200.f,
			5200.f));

	TestFalse(TEXT("initial focus cancels the active parent camera glide"), CameraPawn->IsTransitioningCamera());
	TestFalse(
		TEXT("initial focus changes the pawn pose before the loading screen is removed"),
		CameraPawn->GetActorLocation().Equals(InitialPawnLocation, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("camera location lag setting is preserved"), CameraPawn->SpringArm->bEnableCameraLag);
	TestTrue(
		TEXT("camera rotation lag setting is preserved"),
		CameraPawn->SpringArm->bEnableCameraRotationLag);
	TestTrue(
		TEXT("initial focus uses the closest curve sample below the arm request"),
		CameraPawn->SpringArm->TargetArmLength <= RequestedArmLengthCm
			&& RequestedArmLengthCm - CameraPawn->SpringArm->TargetArmLength <= 1.f);

	const FVector SettledPawnLocation = CameraPawn->GetActorLocation();
	const FVector SettledCameraLocation = CameraPawn->CameraComponent->GetComponentLocation();
	CameraPawn->SpringArm->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	CameraPawn->CameraComponent->UpdateComponentToWorld();
	TestTrue(
		TEXT("the pawn remains fixed on the next frame"),
		CameraPawn->GetActorLocation().Equals(SettledPawnLocation, KINDA_SMALL_NUMBER));
	TestTrue(
		FString::Printf(
			TEXT("spring-arm lag cache is settled before the next frame (delta %.6f)"),
			FVector::Dist(SettledCameraLocation, CameraPawn->CameraComponent->GetComponentLocation())),
		CameraPawn->CameraComponent->GetComponentLocation().Equals(SettledCameraLocation, 0.1f));

	return true;
}

bool FCGROfficeCameraInitialSettleRespectsApprovedArmRangeTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
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

	TestTrue(
		TEXT("lower-bound initial focus pose settles successfully"),
		FOfficeCameraPawnTestAccess::SettleFocusOnLocationImmediately(
			*CameraPawn,
			FVector(404.f, -409.f, -300.f),
			1200.f,
			1200.f,
			5200.f));

	const float SettledArmLengthCm = CameraPawn->SpringArm->TargetArmLength;
	// Mutation caught: keeping only below-request zoom candidates lets a 1200cm request settle below 1200cm.
	TestTrue(
		FString::Printf(
			TEXT("settled initial arm stays at or above 1200cm (actual %.6f)"),
			SettledArmLengthCm),
		SettledArmLengthCm >= 1200.f);
	TestTrue(
		FString::Printf(
			TEXT("settled initial arm stays at or below 5200cm (actual %.6f)"),
			SettledArmLengthCm),
		SettledArmLengthCm <= 5200.f);

	return true;
}

bool FCGROfficeCameraFailedInitialSettlePreservesZoomStateTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
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

	if (!TestTrue(
		TEXT("test range begins above the configured Office zoom curve"),
		CameraPawn->MovementInputHandler->MaxZoomDistance < 20000.f))
	{
		return false;
	}

	CameraPawn->MovementInputHandler->SetZoomValue(0.37f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	const float InitialZoomValue = CameraPawn->MovementInputHandler->GetZoomValue();
	const float InitialArmLength = CameraPawn->SpringArm->TargetArmLength;
	const double InitialPitch = CameraPawn->SpringArm->GetRelativeRotation().Pitch;
	const float InitialFOV = CameraPawn->CameraComponent->FieldOfView;

	// Mutation caught: sampling an unrepresentable range leaves the live camera at a zoom endpoint on false.
	TestFalse(
		TEXT("an ordered range above the Office zoom curve cannot settle"),
		FOfficeCameraPawnTestAccess::SettleFocusOnLocationImmediately(
			*CameraPawn,
			FVector(404.f, -409.f, -300.f),
			20000.f,
			20000.f,
			21000.f));
	TestEqual(
		TEXT("failed initial settle preserves ZoomValue"),
		CameraPawn->MovementInputHandler->GetZoomValue(),
		InitialZoomValue,
		KINDA_SMALL_NUMBER);
	TestEqual(
		TEXT("failed initial settle preserves the represented arm"),
		CameraPawn->SpringArm->TargetArmLength,
		InitialArmLength,
		0.01f);
	TestEqual(
		TEXT("failed initial settle preserves spring-arm pitch"),
		CameraPawn->SpringArm->GetRelativeRotation().Pitch,
		InitialPitch,
		0.0001);
	TestEqual(
		TEXT("failed initial settle preserves camera FOV"),
		CameraPawn->CameraComponent->FieldOfView,
		InitialFOV,
		KINDA_SMALL_NUMBER);

	return true;
}

bool FCGROfficeCameraPoseLogCapturesReproducibleReferenceFramesTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	TestEqual(TEXT("test interior uses the default footprint"), Interior->GetTileCount(), FIntPoint(1, 2));
	Interior->SetActorLocationAndRotation(
		FVector(1000.f, 2000.f, 300.f),
		FRotator(0.f, 90.f, 0.f));
	CameraPawn->SetActorLocationAndRotation(
		FVector(1700.f, 1900.f, 650.f),
		FRotator(0.f, 137.5f, 0.f));
	CameraPawn->MovementInputHandler->SetZoomValue(0.37f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	FOfficeCameraPawnTestAccess::RefreshCameraRigTransforms(*CameraPawn);

	const FTransform InitialPawnTransform = CameraPawn->GetActorTransform();
	const float InitialZoomValue = CameraPawn->MovementInputHandler->GetZoomValue();
	const float InitialArmLength = CameraPawn->SpringArm->TargetArmLength;
	const FRotator InitialArmRotation = CameraPawn->SpringArm->GetRelativeRotation();
	const FVector InitialSocketOffset = CameraPawn->SpringArm->SocketOffset;
	const FVector InitialCameraWorldLocation = CameraPawn->CameraComponent->GetComponentLocation();
	const FRotator InitialCameraWorldRotation = CameraPawn->CameraComponent->GetComponentRotation();
	const FTransform InitialCameraTransform = CameraPawn->CameraComponent->GetComponentTransform();
	const float InitialHFOV = CameraPawn->CameraComponent->FieldOfView;
	const auto TestCameraStatePreserved = [
		this,
		CameraPawn,
		InitialPawnTransform,
		InitialZoomValue,
		InitialArmLength,
		InitialArmRotation,
		InitialSocketOffset,
		InitialCameraTransform,
		InitialHFOV](const TCHAR* Scenario)
	{
		const FRotator CurrentArmRotation = CameraPawn->SpringArm->GetRelativeRotation();
		TestTrue(
			FString::Printf(TEXT("%s preserves the pawn transform"), Scenario),
			CameraPawn->GetActorTransform().Equals(InitialPawnTransform, 0.0));
		TestTrue(
			FString::Printf(TEXT("%s preserves ZoomValue"), Scenario),
			CameraPawn->MovementInputHandler->GetZoomValue() == InitialZoomValue);
		TestTrue(
			FString::Printf(TEXT("%s preserves actual arm length"), Scenario),
			CameraPawn->SpringArm->TargetArmLength == InitialArmLength);
		TestTrue(
			FString::Printf(TEXT("%s preserves arm rotation"), Scenario),
			CurrentArmRotation.Equals(InitialArmRotation, 0.0));
		TestTrue(
			FString::Printf(TEXT("%s preserves arm pitch"), Scenario),
			CurrentArmRotation.Pitch == InitialArmRotation.Pitch);
		TestTrue(
			FString::Printf(TEXT("%s preserves socket offset"), Scenario),
			CameraPawn->SpringArm->SocketOffset == InitialSocketOffset);
		TestTrue(
			FString::Printf(TEXT("%s preserves the camera transform"), Scenario),
			CameraPawn->CameraComponent->GetComponentTransform().Equals(InitialCameraTransform, 0.0));
		TestTrue(
			FString::Printf(TEXT("%s preserves HFOV"), Scenario),
			CameraPawn->CameraComponent->FieldOfView == InitialHFOV);
	};

	// Mutation caught: a rejected read-only pose query must not clear output or normalize live camera state.
	FString InvalidViewportLine = TEXT("invalid-viewport-sentinel");
	TestFalse(
		TEXT("a non-positive viewport is rejected"),
		CameraPawn->TryBuildCameraPoseLog(Interior, FIntPoint(0, 1080), InvalidViewportLine));
	TestEqual(
		TEXT("invalid viewport preserves the output sentinel"),
		InvalidViewportLine,
		FString(TEXT("invalid-viewport-sentinel")));
	TestCameraStatePreserved(TEXT("invalid viewport"));

	FString NullInteriorLine = TEXT("null-interior-sentinel");
	TestFalse(
		TEXT("a null Office interior is rejected"),
		CameraPawn->TryBuildCameraPoseLog(nullptr, FIntPoint(1920, 1080), NullInteriorLine));
	TestEqual(
		TEXT("null interior preserves the output sentinel"),
		NullInteriorLine,
		FString(TEXT("null-interior-sentinel")));
	TestCameraStatePreserved(TEXT("null interior"));

	FString PoseLine;
	TestTrue(
		TEXT("a valid Office camera pose builds one calibration line"),
		CameraPawn->TryBuildCameraPoseLog(Interior, FIntPoint(1920, 1080), PoseLine));
	TestTrue(
		TEXT("calibration output begins with the stable logger prefix"),
		PoseLine.StartsWith(TEXT("[OfficeCameraCalibration]"), ESearchCase::CaseSensitive));
	TestFalse(TEXT("calibration output contains no line feed"), PoseLine.Contains(TEXT("\n")));
	TestFalse(TEXT("calibration output contains no carriage return"), PoseLine.Contains(TEXT("\r")));

	// Mutation caught: omitting any reference-frame field makes the captured entry pose irreproducible.
	TestTrue(TEXT("log identifies the default footprint"), PoseLine.Contains(TEXT("Tile=1x2")));
	TestTrue(TEXT("log identifies the capture viewport"), PoseLine.Contains(TEXT("Viewport=1920x1080")));
	TestTrue(
		TEXT("log includes the transformed floor center in world space"),
		PoseLine.Contains(TEXT("FloorCenterWorld=(1409.000,2204.000,300.000)")));
	TestTrue(
		TEXT("log includes the pawn offset from the floor center"),
		PoseLine.Contains(TEXT("PawnRelativeToFloorCenter=(291.000,-304.000,350.000)")));
	TestTrue(
		TEXT("log includes the pawn world location"),
		PoseLine.Contains(TEXT("PawnWorldLocation=(1700.000,1900.000,650.000)")));
	TestTrue(TEXT("log includes the pawn yaw"), PoseLine.Contains(TEXT("PawnYaw=137.500")));
	TestTrue(
		TEXT("log includes the pawn world rotation"),
		PoseLine.Contains(TEXT("PawnWorldRotation=(0.000,137.500,0.000)")));
	TestTrue(TEXT("log includes the seeded zoom state"), PoseLine.Contains(TEXT("ZoomValue=0.370000")));

	// Mutation caught: rig fields must serialize the live component state, not constants or stale samples.
	TestTrue(
		TEXT("log includes the exact actual arm length"),
		PoseLine.Contains(FString::Printf(
			TEXT("ActualArmLength=%.3f"),
			InitialArmLength)));
	TestTrue(
		TEXT("log includes the exact arm pitch"),
		PoseLine.Contains(FString::Printf(TEXT("ArmPitch=%.3f"), InitialArmRotation.Pitch)));
	TestTrue(
		TEXT("log includes the exact arm rotation"),
		PoseLine.Contains(FString::Printf(
			TEXT("ArmRotation=(%.3f,%.3f,%.3f)"),
			InitialArmRotation.Pitch,
			InitialArmRotation.Yaw,
			InitialArmRotation.Roll)));
	TestTrue(
		TEXT("log includes the exact socket offset"),
		PoseLine.Contains(FString::Printf(
			TEXT("SocketOffset=(%.3f,%.3f,%.3f)"),
			InitialSocketOffset.X,
			InitialSocketOffset.Y,
			InitialSocketOffset.Z)));
	TestTrue(
		TEXT("log includes the exact camera world location"),
		PoseLine.Contains(FString::Printf(
			TEXT("CameraWorldLocation=(%.3f,%.3f,%.3f)"),
			InitialCameraWorldLocation.X,
			InitialCameraWorldLocation.Y,
			InitialCameraWorldLocation.Z)));
	TestTrue(
		TEXT("log includes the exact camera world rotation"),
		PoseLine.Contains(FString::Printf(
			TEXT("CameraWorldRotation=(%.3f,%.3f,%.3f)"),
			InitialCameraWorldRotation.Pitch,
			InitialCameraWorldRotation.Yaw,
			InitialCameraWorldRotation.Roll)));
	TestTrue(
		TEXT("log includes the exact horizontal field of view"),
		PoseLine.Contains(FString::Printf(
			TEXT("HFOV=%.3f"),
			InitialHFOV)));

	return true;
}

bool FCGROfficeCameraCalibratedTwoByTwoPoseSettlesExactlyTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 2;
	OfficeData.TileCountY = 2;
	OfficeData.StarterTileCountX = 2;
	OfficeData.StarterTileCountY = 2;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	Interior->ApplyOfficeSaveData(OfficeData);
	Interior->SetActorTransform(FTransform::Identity);

	TestEqual(TEXT("calibration fixture uses the approved footprint"), Interior->GetTileCount(), FIntPoint(2, 2));
	const FVector2D FloorCenterLocal = Interior->GetCurrentFloorBoundsLocal().GetCenter();
	const FVector FloorCenterWorld = Interior->GetActorTransform().TransformPosition(
		FVector(FloorCenterLocal.X, FloorCenterLocal.Y, Interior->GetStructuralFloorZLocal()));
	TestTrue(
		TEXT("calibration fixture reproduces the captured floor center"),
		FloorCenterWorld.Equals(FVector(404.f, -409.f, 0.f), 0.001));

	CameraPawn->SetActorLocationAndRotation(
		FVector(-800.f, 1200.f, 75.f),
		FRotator(0.f, 95.f, 0.f));
	CameraPawn->MovementInputHandler->SetZoomValue(0.82f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	CameraPawn->SpringArm->bEnableCameraLag = true;
	CameraPawn->SpringArm->bEnableCameraRotationLag = true;
	CameraPawn->SpringArm->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	CameraPawn->CameraComponent->UpdateComponentToWorld();
	CameraPawn->FocusOnLocation(FVector(-2500.f, 1800.f, 300.f), 5000.f);
	FOfficeCameraPawnTestAccess::SeedChildZoomInterpolation(*CameraPawn, 0.95f);
	TestTrue(TEXT("fixture starts from an active camera transition"), CameraPawn->IsTransitioningCamera());
	TestTrue(
		TEXT("fixture starts from active child zoom interpolation"),
		FOfficeCameraPawnTestAccess::IsChildZoomInterpolationActive(*CameraPawn));

	// Mutation caught: falling back to dynamic framing, retaining an old transition/child zoom, or leaving
	// stale lag history changes the approved 2x2 entry pose on the first full Pawn/component tick.
	TestTrue(
		TEXT("the approved 2x2 calibration pose settles"),
		FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(*CameraPawn, Interior));
	TestFalse(TEXT("calibrated settle cancels the previous transition"), CameraPawn->IsTransitioningCamera());
	TestFalse(
		TEXT("calibrated settle cancels child zoom interpolation"),
		FOfficeCameraPawnTestAccess::IsChildZoomInterpolationActive(*CameraPawn));
	TestTrue(TEXT("calibrated settle preserves camera lag"), CameraPawn->SpringArm->bEnableCameraLag);
	TestTrue(TEXT("calibrated settle preserves rotation lag"), CameraPawn->SpringArm->bEnableCameraRotationLag);

	TestTrue(
		TEXT("calibrated pawn location matches the approved capture"),
		CameraPawn->GetActorLocation().Equals(FVector(515.3345f, -522.3345f, 0.f), 0.25));
	TestTrue(
		TEXT("calibrated pawn yaw matches the approved capture"),
		FMath::IsNearlyEqual(CameraPawn->GetActorRotation().Yaw, 135.0, 0.02));
	TestTrue(
		TEXT("calibrated entry is closer on the unchanged zoom curve"),
		CameraPawn->MovementInputHandler->GetZoomValue() > 0.f
			&& CameraPawn->MovementInputHandler->GetZoomValue() < 0.209888f);
	TestTrue(
		TEXT("calibrated actual arm length matches the approved capture"),
		FMath::IsNearlyEqual(CameraPawn->SpringArm->TargetArmLength, 1000.f, 0.25f));
	TestTrue(
		TEXT("calibrated arm pitch matches the approved capture"),
		FMath::IsNearlyEqual(CameraPawn->SpringArm->GetRelativeRotation().Pitch, -35.0, 0.02));
	TestTrue(
		TEXT("calibrated arm rotation matches the approved capture"),
		CameraPawn->SpringArm->GetRelativeRotation().Equals(FRotator(-35.0, 0.0, 0.0), 0.02));
	TestTrue(
		TEXT("calibrated socket offset matches the approved capture"),
		CameraPawn->SpringArm->SocketOffset.Equals(FVector(-1200.f, 0.f, 320.f), 0.001));
	TestTrue(
		TEXT("calibrated HFOV matches the approved capture"),
		FMath::IsNearlyEqual(CameraPawn->CameraComponent->FieldOfView, 35.f, 0.02f));
	TestTrue(
		TEXT("calibrated camera world location matches the approved capture"),
		CameraPawn->CameraComponent->GetComponentLocation().Equals(
			FVector(1659.8505f, -1666.8505f, 1523.9968f),
			0.5));
	TestTrue(
		TEXT("calibrated camera world rotation matches the approved capture"),
		CameraPawn->CameraComponent->GetComponentRotation().Equals(
			FRotator(-35.0, 135.0, 0.0),
			0.02));

	const float SettledZoomValue = CameraPawn->MovementInputHandler->GetZoomValue();
	const FTransform SettledPawnTransform = CameraPawn->GetActorTransform();
	const FVector SettledCameraLocation = CameraPawn->CameraComponent->GetComponentLocation();
	const FRotator SettledCameraRotation = CameraPawn->CameraComponent->GetComponentRotation();
	CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);

	TestTrue(
		TEXT("the first full Pawn tick preserves the calibrated ZoomValue"),
		CameraPawn->MovementInputHandler->GetZoomValue() == SettledZoomValue);
	TestTrue(
		TEXT("the first full Pawn tick preserves the calibrated pawn transform"),
		CameraPawn->GetActorTransform().Equals(SettledPawnTransform, 0.0));
	TestTrue(
		TEXT("the first full Pawn tick preserves the calibrated camera location"),
		CameraPawn->CameraComponent->GetComponentLocation().Equals(SettledCameraLocation, 0.1));
	TestTrue(
		TEXT("the first full Pawn tick preserves the calibrated camera rotation"),
		CameraPawn->CameraComponent->GetComponentRotation().Equals(SettledCameraRotation, 0.01));

	CameraPawn->SpringArm->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
	CameraPawn->CameraComponent->UpdateComponentToWorld();

	TestTrue(
		TEXT("the first spring-arm tick preserves the calibrated pawn transform"),
		CameraPawn->GetActorTransform().Equals(SettledPawnTransform, 0.001));
	TestTrue(
		TEXT("the first spring-arm tick does not pop the camera location"),
		CameraPawn->CameraComponent->GetComponentLocation().Equals(SettledCameraLocation, 0.1));
	TestTrue(
		TEXT("the first spring-arm tick does not pop the camera rotation"),
		CameraPawn->CameraComponent->GetComponentRotation().Equals(SettledCameraRotation, 0.01));

	return true;
}

bool FCGROfficeCameraCalibratedDefaultFootprintUsesBaselineTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 1;
	OfficeData.TileCountY = 2;
	OfficeData.StarterTileCountX = 1;
	OfficeData.StarterTileCountY = 2;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	Interior->ApplyOfficeSaveData(OfficeData);
	Interior->SetActorLocationAndRotation(
		FVector(120.f, -80.f, 35.f),
		FRotator(0.f, 25.f, 0.f));

	TestEqual(TEXT("baseline fixture uses the default footprint"), Interior->GetTileCount(), FIntPoint(1, 2));
	CameraPawn->SetActorLocationAndRotation(
		FVector(-321.f, 654.f, 87.f),
		FRotator(4.f, 111.f, -2.f));
	CameraPawn->MovementInputHandler->SetZoomValue(0.63f);
	CameraPawn->MovementInputHandler->ApplyZoomSettings();
	FOfficeCameraPawnTestAccess::RefreshCameraRigTransforms(*CameraPawn);

	// Mutation caught: retaining the exact 2x2 equality gate rejects the default footprint, while
	// scaling below the approved baseline makes a 1x2 Office enter closer than the reference pose.
	if (!TestTrue(
		TEXT("the default 1x2 footprint settles to the approved baseline"),
		FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(*CameraPawn, Interior)))
	{
		return false;
	}

	const FVector2D FloorCenterLocal = Interior->GetCurrentFloorBoundsLocal().GetCenter();
	const FVector FloorCenterWorld = Interior->GetActorTransform().TransformPosition(
		FVector(FloorCenterLocal.X, FloorCenterLocal.Y, Interior->GetStructuralFloorZLocal()));
	const FVector PlanarViewForward = FRotator(0.f, 135.f, 0.f).Vector();
	const FVector ApprovedFloorOffset(181.892f, -42.777f, 0.f);
	const FVector SettledFloorOffset = CameraPawn->GetActorLocation() - FloorCenterWorld;
	TestEqual(TEXT("rotated room centering preserves the approved floor-relative depth"),
		FVector::DotProduct(SettledFloorOffset, PlanarViewForward),
		FVector::DotProduct(ApprovedFloorOffset, PlanarViewForward), 0.25);
	TestEqual(TEXT("rotated room centering preserves floor-relative height"),
		SettledFloorOffset.Z, 0.0, 0.25);
	TestTrue(
		TEXT("default footprint preserves the approved yaw"),
		FMath::IsNearlyEqual(CameraPawn->GetActorRotation().Yaw, 135.0, 0.02));
	TestTrue(
		TEXT("default footprint uses the closer baseline on the unchanged curve"),
		CameraPawn->MovementInputHandler->GetZoomValue() > 0.f
			&& CameraPawn->MovementInputHandler->GetZoomValue() < 0.209888f);
	TestTrue(
		TEXT("default footprint actual arm equals the 1000cm baseline"),
		FMath::IsNearlyEqual(CameraPawn->SpringArm->TargetArmLength, 1000.f, 0.25f));

	Interior->ApplyPresentationScale();
	TestTrue(TEXT("scaled default office settles"),
		FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(*CameraPawn, Interior));
	const FVector ScaledFloorCenter = Interior->GetActorTransform().TransformPosition(
		FVector(FloorCenterLocal.X, FloorCenterLocal.Y, Interior->GetStructuralFloorZLocal()));
	const FVector ScaledFloorOffset = CameraPawn->GetActorLocation() - ScaledFloorCenter;
	TestEqual(TEXT("presentation scales the approved world depth once"),
		FVector::DotProduct(ScaledFloorOffset, PlanarViewForward),
		FVector::DotProduct(ApprovedFloorOffset * 1.2, PlanarViewForward), 0.25);
	TestEqual(TEXT("presentation preserves floor-relative height"), ScaledFloorOffset.Z, 0.0, 0.25);
	TestEqual(TEXT("presentation scales baseline arm once"), CameraPawn->SpringArm->TargetArmLength, 1200.f, 0.25f);

	const FBox2D ActualFloor = Interior->GetCurrentFloorBoundsLocal();
	const FBox2D VirtualFramingBounds(ActualFloor.Min - FVector2D(0.0, 400.0),
		ActualFloor.Max + FVector2D(400.0, 0.0));
	const FVector BaselinePawnLocation = CameraPawn->GetActorLocation();
	const float BaselineArmLength = CameraPawn->SpringArm->TargetArmLength;
	if (!TestTrue(TEXT("virtual expansion bounds settle with the actual room"),
		FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(
			*CameraPawn, Interior, &VirtualFramingBounds)))
	{
		return false;
	}
	TestEqual(TEXT("unowned expansion tiles do not increase the initial arm"),
		CameraPawn->SpringArm->TargetArmLength, BaselineArmLength, 0.25f);
	const FVector VirtualCenterShift = Interior->GetActorTransform().TransformVector(FVector(200.0, -200.0, 0.0));
	const FVector ExpectedDepthShift = PlanarViewForward * FVector::DotProduct(VirtualCenterShift, PlanarViewForward);
	TestTrue(TEXT("virtual expansion shifts depth while preserving wall-corner centering"),
		(CameraPawn->GetActorLocation() - BaselinePawnLocation).Equals(ExpectedDepthShift, 0.25));

	return true;
}

bool FCGROfficeCameraCalibratedWallCornerCenteredTest::RunTest(const FString&)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get())) return false;
	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("office camera"), CameraPawn)
		|| !TestNotNull(TEXT("camera arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	FOfficeSaveData OfficeData;
	OfficeData.TileCountX = 1;
	OfficeData.TileCountY = 2;
	OfficeData.StarterTileCountX = 1;
	OfficeData.StarterTileCountY = 2;
	OfficeData.CurrentFloorTileRowName = NAME_None;
	Interior->ApplyOfficeSaveData(OfficeData);
	Interior->ApplyPresentationScale();
	FVector LeftMin, LeftMax, RightMin, RightMax;
	Interior->GetWallPlacementBoundsLocal(EWallSide::Left, LeftMin, LeftMax);
	Interior->GetWallPlacementBoundsLocal(EWallSide::Right, RightMin, RightMax);
	const FVector CornerLocal(RightMin.X, LeftMin.Y, Interior->GetStructuralFloorZLocal());
	const FVector CornerWorld = Interior->GetActorTransform().TransformPosition(CornerLocal);

	const FBox2D ActualFloor = Interior->GetCurrentFloorBoundsLocal();
	const FBox2D VirtualFramingBounds(ActualFloor.Min - FVector2D(0.0, 400.0),
		ActualFloor.Max + FVector2D(400.0, 0.0));
	const FBox2D* FramingCases[] = {nullptr, &VirtualFramingBounds};
	for (const FBox2D* FramingBounds : FramingCases)
	{
		if (!TestTrue(TEXT("wall corner pose settles"),
			FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(*CameraPawn, Interior, FramingBounds)))
		{
			return false;
		}
		TestEqual(TEXT("virtual expansion keeps the actual starter arm"),
			CameraPawn->SpringArm->TargetArmLength, 1200.f, 0.25f);
		const FVector InCameraSpace = CameraPawn->CameraComponent->GetComponentTransform()
			.InverseTransformPosition(CornerWorld);
		TestTrue(TEXT("wall corner is in front of the camera"), InCameraSpace.X > 0.0);
		TestTrue(TEXT("wall corner is centered horizontally"), FMath::Abs(InCameraSpace.Y) < 1.0);
		const double Offset = 100.0;
		const FVector LeftWallPoint = Interior->GetActorTransform().TransformPosition(CornerLocal + FVector(Offset, 0.0, 0.0));
		const FVector RightWallPoint = Interior->GetActorTransform().TransformPosition(CornerLocal + FVector(0.0, -Offset, 0.0));
		const FTransform CameraTransform = CameraPawn->CameraComponent->GetComponentTransform();
		const FVector LeftCameraSpace = CameraTransform.InverseTransformPosition(LeftWallPoint);
		const FVector RightCameraSpace = CameraTransform.InverseTransformPosition(RightWallPoint);
		TestTrue(TEXT("equal wall offsets appear on opposite sides"),
			LeftCameraSpace.Y * RightCameraSpace.Y < 0.0);
		TestTrue(TEXT("equal wall offsets have equal camera depth"),
			FMath::IsNearlyEqual(LeftCameraSpace.X, RightCameraSpace.X, 1.0));
		CameraPawn->TickActor(1.f / 60.f, LEVELTICK_All, CameraPawn->PrimaryActorTick);
		CameraPawn->SpringArm->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		CameraPawn->CameraComponent->UpdateComponentToWorld();
		const FVector AfterTick = CameraPawn->CameraComponent->GetComponentTransform()
			.InverseTransformPosition(CornerWorld);
		TestTrue(TEXT("first full tick keeps the wall corner horizontally centered"),
			AfterTick.X > 0.0 && FMath::Abs(AfterTick.Y) < 1.0);
	}
	return true;
}

bool FCGROfficeCameraInitialPhotoViewOffsetPreservesBaseRigTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get())) { return false; }
	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	const FVector Sentinel(11.f, 22.f, 33.f);
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	const struct
	{
		FVector2D FloorSize;
		float Scale;
	} InvalidCases[] = {
		{FVector2D::ZeroVector, 1.f},
		{FVector2D(-1.f, 800.f), 1.f},
		{FVector2D(Infinity, 800.f), 1.f},
		{FVector2D(800.f, NaN), 1.f},
		{FVector2D(800.f, 800.f), 0.f},
		{FVector2D(800.f, 800.f), NaN},
		{FVector2D(800.f, 800.f), Infinity}
	};
	for (const auto& InvalidCase : InvalidCases)
	{
		FVector CameraOffset = Sentinel;
		TestFalse(TEXT("invalid photo view input is rejected"),
			AOfficeCameraPawn::TryGetInitialPhotoViewOffset(
				InvalidCase.FloorSize, InvalidCase.Scale, CameraOffset));
		TestEqual(TEXT("invalid photo view input preserves output"), CameraOffset, Sentinel);
	}

	const struct
	{
		FIntPoint Tiles;
		float Scale;
	} Cases[] = {
		{FIntPoint(1, 2), 1.f},
		{FIntPoint(2, 2), 1.f},
		{FIntPoint(1, 2), 1.2f},
		{FIntPoint(2, 3), 1.2f},
		{FIntPoint(5, 6), 1.2f}
	};
	for (const auto& Case : Cases)
	{
		FOfficeSaveData OfficeData;
		OfficeData.TileCountX = Case.Tiles.X;
		OfficeData.TileCountY = Case.Tiles.Y;
		OfficeData.StarterTileCountX = Case.Tiles.X;
		OfficeData.StarterTileCountY = Case.Tiles.Y;
		OfficeData.CurrentFloorTileRowName = NAME_None;
		Interior->ApplyOfficeSaveData(OfficeData);
		Interior->SetActorTransform(FTransform(FRotator(0.f, 25.f, 0.f), FVector(500.f, -300.f, 75.f), FVector(Case.Scale)));

		CameraPawn->SetActorLocationAndRotation(FVector(-600.f, 900.f, 120.f), FRotator(0.f, 95.f, 0.f));
		CameraPawn->MovementInputHandler->SetZoomValue(0.3f);
		CameraPawn->MovementInputHandler->ApplyZoomSettings();
		FOfficeCameraPawnTestAccess::RefreshCameraRigTransforms(*CameraPawn);
		const FTransform PreviousPawnTransform = CameraPawn->GetActorTransform();
		const float PreviousZoom = CameraPawn->MovementInputHandler->GetZoomValue();
		const float PreviousArm = CameraPawn->SpringArm->TargetArmLength;
		const FTransform PreviousCameraTransform = CameraPawn->CameraComponent->GetComponentTransform();

		FVector CameraOffset = Sentinel;
		const FVector2D FloorSize = Interior->GetCurrentFloorBoundsLocal().GetSize();
		if (!TestTrue(FString::Printf(TEXT("%dx%d photo offset is available"), Case.Tiles.X, Case.Tiles.Y),
			AOfficeCameraPawn::TryGetInitialPhotoViewOffset(FloorSize, Case.Scale, CameraOffset)))
		{
			return false;
		}
		TestTrue(TEXT("photo query preserves pawn pose"), CameraPawn->GetActorTransform().Equals(PreviousPawnTransform, 0.0));
		TestEqual(TEXT("photo query preserves zoom"), CameraPawn->MovementInputHandler->GetZoomValue(), PreviousZoom);
		TestEqual(TEXT("photo query preserves arm"), CameraPawn->SpringArm->TargetArmLength, PreviousArm);
		TestTrue(TEXT("photo query preserves camera pose"),
			CameraPawn->CameraComponent->GetComponentTransform().Equals(PreviousCameraTransform, 0.0));

		if (!TestTrue(FString::Printf(TEXT("%dx%d live calibrated pose settles"), Case.Tiles.X, Case.Tiles.Y),
			FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(*CameraPawn, Interior)))
		{
			return false;
		}
		const FVector2D FloorCenterLocal = Interior->GetCurrentFloorBoundsLocal().GetCenter();
		const FVector FloorCenterWorld = Interior->GetActorTransform().TransformPosition(
			FVector(FloorCenterLocal.X, FloorCenterLocal.Y, Interior->GetStructuralFloorZLocal()));
		const FVector ActualOffset = CameraPawn->CameraComponent->GetComponentLocation() - FloorCenterWorld;
		const FVector OffsetDifference = ActualOffset - CameraOffset;
		const FVector CameraForward = CameraPawn->CameraComponent->GetForwardVector();
		const FVector CameraUp = CameraPawn->CameraComponent->GetUpVector();
		TestTrue(
			FString::Printf(TEXT("%dx%d scale %.1f photo offset preserves camera depth (error %.3fcm)"),
				Case.Tiles.X, Case.Tiles.Y, Case.Scale, FVector::DotProduct(OffsetDifference, CameraForward)),
			FMath::Abs(FVector::DotProduct(OffsetDifference, CameraForward)) < 1.0);
		TestTrue(
			FString::Printf(TEXT("%dx%d scale %.1f photo offset preserves camera height (error %.3fcm)"),
				Case.Tiles.X, Case.Tiles.Y, Case.Scale, FVector::DotProduct(OffsetDifference, CameraUp)),
			FMath::Abs(FVector::DotProduct(OffsetDifference, CameraUp)) < 1.0);
	}
	return true;
}

bool FCGROfficeCameraCalibratedArmScalesMonotonicallyTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeInterior* Interior = TestWorld.Get()->SpawnActor<AOfficeInterior>();
	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office interior"), Interior)
		|| !TestNotNull(TEXT("office camera pawn"), CameraPawn)
		|| !TestNotNull(TEXT("movement input handler"), CameraPawn->MovementInputHandler.Get())
		|| !TestNotNull(TEXT("spring arm"), CameraPawn->SpringArm.Get())
		|| !TestNotNull(TEXT("camera component"), CameraPawn->CameraComponent.Get()))
	{
		return false;
	}

	struct FFootprintArmCase
	{
		FIntPoint TileCount;
		float ExpectedArmLengthCm;
	};

	// Hand-derived literals: 1000 * max(1, sqrt(X^2 + Y^2) / sqrt(8)).
	// The 400cm tile size cancels against the 2x2 reference diagonal 800*sqrt(2).
	const FFootprintArmCase ArmCases[] = {
		{ FIntPoint(2, 2), 1000.000000f },
		{ FIntPoint(2, 3), 1274.754878f },
		{ FIntPoint(2, 4), 1581.138830f },
		{ FIntPoint(3, 4), 1767.766953f },
		{ FIntPoint(5, 6), 2761.340254f }
	};

	float PreviousArmLengthCm = 0.f;
	for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(ArmCases); ++CaseIndex)
	{
		const FFootprintArmCase& ArmCase = ArmCases[CaseIndex];
		FOfficeSaveData OfficeData;
		OfficeData.TileCountX = ArmCase.TileCount.X;
		OfficeData.TileCountY = ArmCase.TileCount.Y;
		OfficeData.StarterTileCountX = ArmCase.TileCount.X;
		OfficeData.StarterTileCountY = ArmCase.TileCount.Y;
		OfficeData.CurrentFloorTileRowName = NAME_None;
		Interior->ApplyOfficeSaveData(OfficeData);

		TestEqual(
			TEXT("save data applies the requested footprint"),
			Interior->GetTileCount(),
			ArmCase.TileCount);
		if (!TestTrue(
			FString::Printf(
				TEXT("%dx%d footprint settles through the calibrated initial path"),
				ArmCase.TileCount.X,
				ArmCase.TileCount.Y),
			FOfficeCameraPawnTestAccess::TrySettleCalibratedInitialPose(*CameraPawn, Interior)))
		{
			return false;
		}

		const float ActualArmLengthCm = CameraPawn->SpringArm->TargetArmLength;
		TestTrue(
			FString::Printf(
				TEXT("%dx%d actual arm %.6fcm matches hand-derived %.6fcm"),
				ArmCase.TileCount.X,
				ArmCase.TileCount.Y,
				ActualArmLengthCm,
				ArmCase.ExpectedArmLengthCm),
			FMath::IsNearlyEqual(ActualArmLengthCm, ArmCase.ExpectedArmLengthCm, 0.5f));
		if (CaseIndex > 0)
		{
			TestTrue(
				FString::Printf(
					TEXT("%dx%d actual arm %.6fcm is strictly farther than %.6fcm"),
					ArmCase.TileCount.X,
					ArmCase.TileCount.Y,
					ActualArmLengthCm,
					PreviousArmLengthCm),
				ActualArmLengthCm > PreviousArmLengthCm);
		}

		const FVector2D FloorCenterLocal = Interior->GetCurrentFloorBoundsLocal().GetCenter();
		const FVector FloorCenterWorld = Interior->GetActorTransform().TransformPosition(
			FVector(FloorCenterLocal.X, FloorCenterLocal.Y, Interior->GetStructuralFloorZLocal()));
		const FVector PawnOffset = CameraPawn->GetActorLocation() - FloorCenterWorld;
		const FVector GroundViewDirection = FRotator(0.f, 135.f, 0.f).Vector();
		TestTrue(
			TEXT("wall centering preserves the approved floor-relative depth"),
			FMath::IsNearlyEqual(FVector::DotProduct(PawnOffset, GroundViewDirection),
				FVector::DotProduct(FVector(181.892f, -42.777f, 0.f), GroundViewDirection), 0.25));
		TestTrue(TEXT("wall centering preserves the approved floor-relative height"),
			FMath::IsNearlyZero(PawnOffset.Z, 0.25));
		TestTrue(
			TEXT("scaled framing preserves the approved yaw"),
			FMath::IsNearlyEqual(CameraPawn->GetActorRotation().Yaw, 135.0, 0.02));
		PreviousArmLengthCm = ActualArmLengthCm;
	}

	return true;
}

bool FCGROfficeCameraRegularFocusStillGlidesTest::RunTest(const FString& Parameters)
{
	OfficeCameraZoomStateTests::FScopedTestWorld TestWorld;
	if (!TestNotNull(TEXT("test world"), TestWorld.Get()))
	{
		return false;
	}

	AOfficeCameraPawn* CameraPawn = TestWorld.Get()->SpawnActor<AOfficeCameraPawn>();
	if (!TestNotNull(TEXT("office camera pawn"), CameraPawn))
	{
		return false;
	}

	const FVector InitialPawnLocation = CameraPawn->GetActorLocation();
	CameraPawn->FocusOnLocation(FVector(1000.f, -1000.f, -500.f), 1200.f);

	TestTrue(TEXT("regular location focus still starts a smooth transition"), CameraPawn->IsTransitioningCamera());
	TestTrue(
		TEXT("regular location focus does not snap the pawn before Tick"),
		CameraPawn->GetActorLocation().Equals(InitialPawnLocation, KINDA_SMALL_NUMBER));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
