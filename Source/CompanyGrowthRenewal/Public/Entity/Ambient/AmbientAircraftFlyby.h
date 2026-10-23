#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Entity/Ambient/AmbientSplineMover.h"
#include "Interfaces/IInputHandler.h"
#include "AmbientAircraftFlyby.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// 레벨에 작도한 닫힌 경로를 도는 항공기. 플레이어가 구역 근처에 있을 때만 이동한다.
UCLASS()
class COMPANYGROWTHRENEWAL_API AAmbientAircraftFlyby : public AAmbientSplineMover, public IInputHandler
{
	GENERATED_BODY()

public:
	AAmbientAircraftFlyby();
	virtual void Tick(float DeltaTime) override;
	virtual void OnInteract_Implementation(APlayerController* InstigatingPC) override;
	virtual void OnEndInteract_Implementation(APlayerController* InstigatingPC) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Aircraft")
	TObjectPtr<UStaticMeshComponent> RotorMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Aircraft")
	TObjectPtr<UBoxComponent> ClickBoxComp;

	UPROPERTY(EditAnywhere, Category = "Ambient|Aircraft", meta = (ClampMin = "0.0"))
	float RotorDegreesPerSecond = 2280.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Aircraft")
	TObjectPtr<UStaticMeshComponent> TailRotorMeshComp;

	UPROPERTY(EditAnywhere, Category = "Ambient|Aircraft", meta = (ClampMin = "0.0"))
	float TailRotorDegreesPerSecond = 4560.f;

	UPROPERTY(EditAnywhere, Category = "Ambient|Aircraft", meta = (ClampMin = "100.0"))
	float ActivationRadiusCm = 35000.f;

	UPROPERTY(EditAnywhere, Category = "Ambient|Aircraft", meta = (ClampMin = "100.0"))
	float DeactivationRadiusCm = 45000.f;

private:
	FTimerHandle ActivationTimer;
	bool bIsActive = false;

	void UpdateActivation();
};
