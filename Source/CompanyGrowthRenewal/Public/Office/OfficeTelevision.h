#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/OfficeTVBroadcast.h"
#include "Data/OperationData.h"
#include "Enum/ProjectLifecycle.h"
#include "Office/OfficeTVPlayback.h"
#include "OfficeTelevision.generated.h"

class AOfficeInterior;
class UStaticMeshComponent;
class UWidgetComponent;
class UOfficeStageProgressManager;
class UOfficeTVNewsDesk;

/** Fixed office fixture. Furniture saves do not own its lifetime or transform. */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeTelevision : public AActor
{

	GENERATED_BODY()

public:
	AOfficeTelevision();
	void Initialize(AOfficeInterior* InInterior);
	static FBox GetScreenBoundsLocal(const AOfficeInterior* InInterior);
	static FBox GetExclusionBoundsLocal(const AOfficeInterior* InInterior);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Bezel;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> Screen;

	UPROPERTY(Transient)
	TObjectPtr<UOfficeTVNewsDesk> NewsDesk;

	TWeakObjectPtr<AOfficeInterior> Interior;
	TWeakObjectPtr<UOfficeStageProgressManager> StageSource;
	FTimerHandle BroadcastTimer;
	FTimerHandle MotionTimer;
	FOfficeTVPlayback Playback;
	double LastBroadcastTime = 0.0;
	bool bScreenInitialized = false;
	bool bShowingLiveReadout = false;

	void UpdateLayout(FIntPoint TileCount);
	void RefreshBroadcast();

	/** The office top bar redraws on this tick; the TV follows it so the shared figures never trail by a second. */
	UFUNCTION()
	void HandleOperationUpdated(int32 BuildingID, const FOperationData& Data);
	void PumpMotion();
	bool CanWatchBroadcast() const;
};
