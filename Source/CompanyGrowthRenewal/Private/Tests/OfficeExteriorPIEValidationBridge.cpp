#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogOfficeExteriorPIEValidation, Log, All);

namespace
{
struct FOfficeExteriorPIESettingsSnapshot
{
	EPlayNetMode OriginalPlayNetMode = PIE_Standalone;
	int32 OriginalNewWindowWidth = 0;
	int32 OriginalNewWindowHeight = 0;
	FIntPoint OriginalNewWindowPosition = FIntPoint::ZeroValue;
	FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
	TArray<FIntPoint> OriginalMultipleInstancePositions;
	bool bOriginalCenterNewWindow = false;
	EPlayModeType OriginalPlayModeType = PlayMode_InEditorFloating;
	EPlayModeLocations OriginalPlayModeLocation = PlayLocation_DefaultPlayerStart;

	static FOfficeExteriorPIESettingsSnapshot Capture(const ULevelEditorPlaySettings& PlaySettings)
	{
		FOfficeExteriorPIESettingsSnapshot Snapshot;
		PlaySettings.GetPlayNetMode(Snapshot.OriginalPlayNetMode);
		Snapshot.OriginalNewWindowWidth = PlaySettings.NewWindowWidth;
		Snapshot.OriginalNewWindowHeight = PlaySettings.NewWindowHeight;
		Snapshot.OriginalNewWindowPosition = PlaySettings.NewWindowPosition;
		Snapshot.OriginalLastSize = PlaySettings.LastSize;
		Snapshot.OriginalMultipleInstancePositions = PlaySettings.MultipleInstancePositions;
		Snapshot.bOriginalCenterNewWindow = PlaySettings.CenterNewWindow != 0;
		Snapshot.OriginalPlayModeType = PlaySettings.LastExecutedPlayModeType.GetValue();
		Snapshot.OriginalPlayModeLocation = PlaySettings.LastExecutedPlayModeLocation.GetValue();
		return Snapshot;
	}

	void Restore(ULevelEditorPlaySettings& PlaySettings) const
	{
		PlaySettings.SetPlayNetMode(OriginalPlayNetMode);
		PlaySettings.NewWindowWidth = OriginalNewWindowWidth;
		PlaySettings.NewWindowHeight = OriginalNewWindowHeight;
		PlaySettings.NewWindowPosition = OriginalNewWindowPosition;
		PlaySettings.LastSize = OriginalLastSize;
		PlaySettings.MultipleInstancePositions = OriginalMultipleInstancePositions;
		PlaySettings.CenterNewWindow = bOriginalCenterNewWindow;
		PlaySettings.LastExecutedPlayModeType = OriginalPlayModeType;
		PlaySettings.LastExecutedPlayModeLocation = OriginalPlayModeLocation;
	}
};

TOptional<FOfficeExteriorPIESettingsSnapshot> ActiveSettingsSnapshot;
FDelegateHandle EndPIEDelegateHandle;
FDelegateHandle CancelPIEDelegateHandle;
bool bSawEndPIE = false;

void UnbindOfficeExteriorPIESettingsDelegates()
{
	if (EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
		EndPIEDelegateHandle.Reset();
	}
	if (CancelPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::CancelPIE.Remove(CancelPIEDelegateHandle);
		CancelPIEDelegateHandle.Reset();
	}
}

bool RestoreOfficeExteriorPIESettings(bool bFinalize)
{
	if (!ActiveSettingsSnapshot.IsSet())
	{
		return false;
	}

	ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
	ActiveSettingsSnapshot->Restore(*PlaySettings);

	if (bFinalize)
	{
		// EndPlayMap saves window state after EndPIE. Re-save the original values after
		// its final CancelPIE notification so the validation window cannot leak to config.
		if (bSawEndPIE)
		{
			PlaySettings->SaveConfig();
		}
		ActiveSettingsSnapshot.Reset();
		bSawEndPIE = false;
		UnbindOfficeExteriorPIESettingsDelegates();
	}

	return true;
}

void HandleOfficeExteriorEndPIE(bool bWasSimulating)
{
	(void)bWasSimulating;
	bSawEndPIE = true;
	if (RestoreOfficeExteriorPIESettings(false))
	{
		UE_LOG(LogOfficeExteriorPIEValidation, Display,
			TEXT("CGR.OfficeExterior.StartPIE restored editor play settings at EndPIE."));
	}
}

void HandleOfficeExteriorCancelPIE()
{
	if (RestoreOfficeExteriorPIESettings(true))
	{
		UE_LOG(LogOfficeExteriorPIEValidation, Display,
			TEXT("CGR.OfficeExterior.StartPIE finalized editor play settings restoration."));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOfficeExteriorPIESettingsSnapshotRoundTripTest,
	"CGR.Office.Exterior.PIEValidationBridge.SettingsRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeExteriorPIESettingsSnapshotRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (ActiveSettingsSnapshot.IsSet())
	{
		AddError(TEXT("Cannot test the settings snapshot while an Office exterior PIE request owns it."));
		return false;
	}

	ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
	const FOfficeExteriorPIESettingsSnapshot Snapshot =
		FOfficeExteriorPIESettingsSnapshot::Capture(*PlaySettings);

	PlaySettings->SetPlayNetMode(
		Snapshot.OriginalPlayNetMode == PIE_Standalone ? PIE_Client : PIE_Standalone);
	PlaySettings->NewWindowWidth = Snapshot.OriginalNewWindowWidth == 1234 ? 1235 : 1234;
	PlaySettings->NewWindowHeight = Snapshot.OriginalNewWindowHeight == 678 ? 679 : 678;
	PlaySettings->NewWindowPosition =
		Snapshot.OriginalNewWindowPosition == FIntPoint(246, 810)
			? FIntPoint(247, 811)
			: FIntPoint(246, 810);
	PlaySettings->LastSize =
		Snapshot.OriginalLastSize == FIntPoint(1357, 913)
			? FIntPoint(1358, 914)
			: FIntPoint(1357, 913);
	PlaySettings->MultipleInstancePositions = Snapshot.OriginalMultipleInstancePositions;
	PlaySettings->MultipleInstancePositions.Add(FIntPoint(111, 222));
	PlaySettings->CenterNewWindow = !Snapshot.bOriginalCenterNewWindow;
	PlaySettings->LastExecutedPlayModeType =
		Snapshot.OriginalPlayModeType == PlayMode_InViewPort
			? PlayMode_InEditorFloating
			: PlayMode_InViewPort;
	PlaySettings->LastExecutedPlayModeLocation =
		Snapshot.OriginalPlayModeLocation == PlayLocation_DefaultPlayerStart
			? PlayLocation_CurrentCameraLocation
			: PlayLocation_DefaultPlayerStart;

	Snapshot.Restore(*PlaySettings);

	EPlayNetMode RestoredPlayNetMode = PIE_Standalone;
	PlaySettings->GetPlayNetMode(RestoredPlayNetMode);
	TestEqual(TEXT("PlayNetMode"), RestoredPlayNetMode, Snapshot.OriginalPlayNetMode);
	TestEqual(TEXT("NewWindowWidth"), PlaySettings->NewWindowWidth, Snapshot.OriginalNewWindowWidth);
	TestEqual(TEXT("NewWindowHeight"), PlaySettings->NewWindowHeight, Snapshot.OriginalNewWindowHeight);
	TestEqual(
		TEXT("NewWindowPosition"),
		PlaySettings->NewWindowPosition,
		Snapshot.OriginalNewWindowPosition);
	TestEqual(TEXT("LastSize"), PlaySettings->LastSize, Snapshot.OriginalLastSize);
	TestEqual(
		TEXT("MultipleInstancePositions"),
		PlaySettings->MultipleInstancePositions,
		Snapshot.OriginalMultipleInstancePositions);
	TestEqual(
		TEXT("CenterNewWindow"),
		PlaySettings->CenterNewWindow != 0,
		Snapshot.bOriginalCenterNewWindow);
	TestEqual(
		TEXT("LastExecutedPlayModeType"),
		PlaySettings->LastExecutedPlayModeType.GetValue(),
		Snapshot.OriginalPlayModeType);
	TestEqual(
		TEXT("LastExecutedPlayModeLocation"),
		PlaySettings->LastExecutedPlayModeLocation.GetValue(),
		Snapshot.OriginalPlayModeLocation);
	return true;
}

void StartOfficeExteriorPIE()
{
	if (!GUnrealEd)
	{
		UE_LOG(LogOfficeExteriorPIEValidation, Warning,
			TEXT("CGR.OfficeExterior.StartPIE ignored because the editor engine is unavailable."));
		return;
	}

	if (GUnrealEd->IsPlaySessionInProgress())
	{
		UE_LOG(LogOfficeExteriorPIEValidation, Display,
			TEXT("CGR.OfficeExterior.StartPIE ignored because a PIE session is active or already requested."));
		return;
	}

	ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
	if (ActiveSettingsSnapshot.IsSet())
	{
		UE_LOG(LogOfficeExteriorPIEValidation, Warning,
			TEXT("CGR.OfficeExterior.StartPIE found stale validation settings and restored them before starting."));
		RestoreOfficeExteriorPIESettings(true);
	}

	ActiveSettingsSnapshot = FOfficeExteriorPIESettingsSnapshot::Capture(*PlaySettings);
	bSawEndPIE = false;
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddStatic(&HandleOfficeExteriorEndPIE);
	CancelPIEDelegateHandle = FEditorDelegates::CancelPIE.AddStatic(&HandleOfficeExteriorCancelPIE);

	PlaySettings->SetPlayNetMode(PIE_Standalone);
	PlaySettings->LastExecutedPlayModeType = PlayMode_InEditorFloating;
	PlaySettings->LastExecutedPlayModeLocation = PlayLocation_DefaultPlayerStart;
	PlaySettings->NewWindowWidth = 1920;
	PlaySettings->NewWindowHeight = 1080;
	PlaySettings->CenterNewWindow = true;

	GUnrealEd->RequestPlaySession(FRequestPlaySessionParams{});
	UE_LOG(LogOfficeExteriorPIEValidation, Display,
		TEXT("CGR.OfficeExterior.StartPIE requested a 1920x1080 floating PIE session at the default player start."));
}

FAutoConsoleCommand StartOfficeExteriorPIECommand(
	TEXT("CGR.OfficeExterior.StartPIE"),
	TEXT("Starts the Office exterior validation PIE session in a centered 1920x1080 floating window."),
	FConsoleCommandDelegate::CreateStatic(&StartOfficeExteriorPIE));
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
