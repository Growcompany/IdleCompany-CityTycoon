#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityPlayableArea.generated.h"

class APlayerCamera;
class UBoxComponent;

UCLASS()
class COMPANYGROWTHRENEWAL_API ACityPlayableArea : public AActor
{
    GENERATED_BODY()

public:
    ACityPlayableArea();

    bool ApplyToCamera(APlayerCamera* Camera) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "City|Camera")
    TObjectPtr<UBoxComponent> Bounds;
};
