#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IInputHandler.h"
#include "CityClickProxyActor.generated.h"

class UBoxComponent;
class UCityAcquisitionManager;

UCLASS()
class COMPANYGROWTHRENEWAL_API ACityClickProxyActor : public AActor, public IInputHandler
{
    GENERATED_BODY()
public:
    ACityClickProxyActor();

    // 스폰 직후 호출 — BuildingKey 저장 + BoxExtent(건물 bounds) 설정
    void InitProxy(int32 InKey, FVector BoxExtent);

    virtual void OnEndInteract_Implementation(APlayerController* PC) override;

    int32 GetCompanyKey() const { return CompanyKey; }

private:
    UPROPERTY(VisibleAnywhere)
    UBoxComponent* BoxComponent;

    int32 CompanyKey = 0;
};
