#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/OfficeCameraPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

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

#endif // WITH_DEV_AUTOMATION_TESTS
