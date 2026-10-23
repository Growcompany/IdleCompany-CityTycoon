#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"
#include "CityOpeningSequence.generated.h"

class USplineComponent;
class UStaticMeshComponent;
class UWorldPartitionStreamingSourceComponent;

// 레벨에 저장한 헬기 동선과 촬영 오프셋을 하나의 재생 시각으로 평가한다.
UCLASS()
class COMPANYGROWTHRENEWAL_API ACityOpeningSequence : public AActor
{
    GENERATED_BODY()
public:
    ACityOpeningSequence();
    bool Start(const FMinimalViewInfo& LandingView);
    FMinimalViewInfo Evaluate(float ElapsedSeconds);
    void Stop();
    bool IsAuthored() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    TObjectPtr<USplineComponent> HelicopterPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    TObjectPtr<USceneComponent> HelicopterTravel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    TObjectPtr<UStaticMeshComponent> HelicopterBody;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    TObjectPtr<UStaticMeshComponent> MainRotor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    TObjectPtr<UStaticMeshComponent> TailRotor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;

    UPROPERTY(EditAnywhere, Category="Opening|Camera")
    FVector HelicopterCameraOffset = FVector(-1800., 1100., 900.);
    UPROPERTY(EditAnywhere, Category="Opening|Camera")
    FVector HelicopterLookAtOffset = FVector(1600., 0., -1700.);
    UPROPERTY(EditAnywhere, Category="Opening|Camera")
    float HelicopterFOV = 58.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    FMinimalViewInfo Landing;
    bool bPlaying = false;
    FQuat MainRotorRest = FQuat::Identity;
    FQuat TailRotorRest = FQuat::Identity;
    TArray<TWeakObjectPtr<AActor>> AmbientActors;
    TArray<bool> AmbientHiddenStates;
};
