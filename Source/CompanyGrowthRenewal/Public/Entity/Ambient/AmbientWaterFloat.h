#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "AmbientWaterFloat.generated.h"

class UAmbientVisibilityComponent;

// 배치된 수면 소품의 원래 자세를 기준으로 잔잔한 부유 움직임을 적용한다.
UCLASS()
class COMPANYGROWTHRENEWAL_API AAmbientWaterFloat : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	AAmbientWaterFloat();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient")
	TObjectPtr<UAmbientVisibilityComponent> Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BobAmplitude = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float BobPeriod = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PitchAmplitude = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PitchPeriod = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RollAmplitude = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float RollPeriod = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Float")
	float PhaseOffsetRadians = 0.f;

private:
	FTransform InitialWorldTransform;
	double ElapsedActiveSeconds = 0.0;
	bool bHasValidMesh = false;
};
