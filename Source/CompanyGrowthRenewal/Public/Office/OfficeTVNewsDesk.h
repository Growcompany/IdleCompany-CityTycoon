#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/OfficeTVBroadcast.h"
#include "Data/ProjectEventData.h"
#include "Enum/ProjectLifecycle.h"
#include "Manager/LaunchReactionSubsystem.h"
#include "Office/OfficeTVBroadcastSource.h"
#include "OfficeTVNewsDesk.generated.h"

class UEmployeeManager;
class UEventChoicePanelWidget;
class UOfficeStageProgressManager;

/** Listens to real office events and turns them into television news. Owned by the office TV. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeTVNewsDesk : public UObject
{
	GENERATED_BODY()

public:
	void Start(UWorld* InWorld);
	void Stop();

	/** Retries work that needs widgets created after the event, and fills launch records missed by ordering. */
	void Tick();

	/** Drains news in arrival order. Launch sequences always precede tier-up ceremonies. */
	void TakeNews(TArray<FOfficeTVBroadcastFrame>& OutInterrupts, TArray<FOfficeTVBroadcastFrame>& OutPriority);

	const FOfficeTVNewsBuffers& GetBuffers() const { return Buffers; }
	void NoteOperationTick(int32 BuildingID, double TotalRevenue);

private:
	UFUNCTION()
	void HandleScoreOrb(FVector WorldPosition, int32 StepNumber, float Score, bool bIsCritical, int32 OrbCount, bool bPlaySpectacle);

	UFUNCTION()
	void HandleFever(bool bActive);

	UFUNCTION()
	void HandleProjectEvent(const FProjectEventData& EventData);

	UFUNCTION()
	void HandleEventChoice(int32 ChoiceIndex);

	UFUNCTION()
	void HandleReaction(int32 BuildingID, const FLaunchReaction& Reaction);

	UFUNCTION()
	void HandleTierUnlocked(int32 NewTier);

	UFUNCTION()
	void HandleLifecycle(EProjectLifecycle OldState, EProjectLifecycle NewState);

	void HandleLevelUp(int32 EmployeeId, int32 NewLevel);
	void HandleHire(const FString& EmployeeId);

	int32 ResolveWorker(const FVector& OrbPosition) const;
	int32 CurrentBuilding() const;
	bool TryBindChoicePanel();
	void UnbindChoicePanel();
	void RememberLevels();

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UOfficeStageProgressManager> Stage;
	TWeakObjectPtr<ULaunchReactionSubsystem> Reactions;
	TWeakObjectPtr<UEmployeeManager> Employees;
	TWeakObjectPtr<UEventChoicePanelWidget> ChoicePanel;

	FProjectEventData PendingEvent;
	bool bEventPending = false;
	FOfficeTVLaunchRecord LaunchRecord;
	TMap<int32, int32> KnownLevels;
	FOfficeTVNewsBuffers Buffers;

	TArray<FOfficeTVBroadcastFrame> Interrupts;
	TArray<FOfficeTVBroadcastFrame> LaunchSequence;
	TArray<FOfficeTVBroadcastFrame> TierUps;
};
