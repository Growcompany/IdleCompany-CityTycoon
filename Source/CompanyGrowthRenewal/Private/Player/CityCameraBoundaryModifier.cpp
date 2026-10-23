#include "Player/CityCameraBoundaryModifier.h"

#include "Camera/PlayerCameraManager.h"
#include "GameMode/CGGameModeBase.h"
#include "Player/PlayerCamera.h"
#include "Player/Components/MovementInputHandler.h"

UCityCameraBoundaryModifier::UCityCameraBoundaryModifier()
{
    Priority = 255;
    AlphaInTime = 0.f;
    AlphaOutTime = 0.f;
}

bool UCityCameraBoundaryModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    const APlayerCamera* CameraPawn = CameraOwner ? Cast<APlayerCamera>(CameraOwner->GetViewTarget()) : nullptr;
    if (!CameraPawn || !CameraPawn->MovementInputHandler || !CameraPawn->MovementInputHandler->bConstrainCameraView)
        return false;

    if (const ACGGameModeBase* CityMode = CameraPawn->GetWorld()->GetAuthGameMode<ACGGameModeBase>())
    {
        FMinimalViewInfo OpeningView;
        if (CityMode->TryGetCityOpeningView(OpeningView)) return false;
    }
    // Camera shakes run after the pawn's CalcCamera; constrain the modified view as well.
    CameraPawn->MovementInputHandler->ConstrainCameraView(InOutPOV);
    return false;
}
