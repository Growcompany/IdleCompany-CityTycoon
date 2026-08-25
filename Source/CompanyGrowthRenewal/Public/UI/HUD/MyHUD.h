// MyHUD.h
#pragma once
#include "GameFramework/HUD.h"
#include "MyHUD.generated.h"

UCLASS()
class COMPANYGROWTHRENEWAL_API AMyHUD : public AHUD
{
    GENERATED_BODY()

    public:
    virtual void DrawHUD() override;
};
