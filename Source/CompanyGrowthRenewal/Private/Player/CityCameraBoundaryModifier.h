#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "CityCameraBoundaryModifier.generated.h"

UCLASS()
class COMPANYGROWTHRENEWAL_API UCityCameraBoundaryModifier : public UCameraModifier
{
    GENERATED_BODY()

public:
    UCityCameraBoundaryModifier();
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};
