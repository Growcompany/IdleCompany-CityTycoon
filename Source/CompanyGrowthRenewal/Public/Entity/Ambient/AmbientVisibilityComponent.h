#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "AmbientVisibilityComponent.generated.h"

class UPrimitiveComponent;

// 메시가 플레이어의 가까운 화면 안에 있을 때만 소유 액터의 움직임을 갱신한다.
UCLASS(ClassGroup = (Ambient))
class COMPANYGROWTHRENEWAL_API UAmbientVisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAmbientVisibilityComponent();
	void StartMonitoring(UPrimitiveComponent* Target);
	void StopMonitoring();

	UPROPERTY(EditAnywhere, Category = "Ambient|Visibility", meta = (ClampMin = "100.0"))
	float ActivationRadiusCm = 45000.f;

	UPROPERTY(EditAnywhere, Category = "Ambient|Visibility", meta = (ClampMin = "100.0"))
	float DeactivationRadiusCm = 50000.f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> ObservedMesh;

	FTimerHandle VisibilityTimer;
	bool bMotionActive = false;
	void UpdateVisibility();
};
