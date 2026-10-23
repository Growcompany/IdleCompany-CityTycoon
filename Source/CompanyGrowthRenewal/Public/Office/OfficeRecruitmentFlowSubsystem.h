#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/ProductionDiscipline.h"
#include "Office/RecruitEntranceHold.h"
#include "OfficeRecruitmentFlowSubsystem.generated.h"

class AWorkstationActorBase;
class AOfficeworker;
class AInteractiveDoorActor;
class UEmployeeGachaPresentationWidget;
class SWidget;
struct FStreamableHandle;

/** 빈 좌석 확보, 실제 채용 저장, 좌석별 직원과 카드 공개를 관리한다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRecruitmentFlowSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	int32 GetFreeSeatCount() const;
	bool IsBusy() const { return bBusy; }
	bool IsSeatMutationAllowed() const { return !bBusy || bCommittingSeats; }
	bool ShouldDeferAssignedWorkerSpawns() const { return bCommittingSeats; }
	bool RequestRecruitment(EGachaTier Tier, int32 RequestedCount, EProductionDiscipline PrimaryDiscipline,
		AWorkstationActorBase* PreferredWorkstation = nullptr, bool bHoldEntranceUntilReleased = false);
	// 보류 요청한 쪽(채용 모달)이 퇴장 연출을 마쳤을 때 부른다. 다음 Tick 에 입장을 시작한다.
	void ReleaseEntrance();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRequestFinished, bool, const FName&);
	FOnRequestFinished OnRequestFinished;

private:
	struct FArrival
	{
		TWeakObjectPtr<AOfficeworker> Worker;
		TWeakObjectPtr<AWorkstationActorBase> Workstation;
		int32 SeatIndex = INDEX_NONE;
		FVector Target = FVector::ZeroVector;
		FVector LastLocation = FVector::ZeroVector;
		float MoveElapsed = 0.f;
		float StillElapsed = 0.f;
		float MoveTimeout = 4.f;
		float SeatedAt = 0.f;
		ECollisionResponse PawnCollisionResponse = ECR_Block;
		bool bWorkerPrepared = false;
		bool bStarted = false;
		bool bSettled = false;
		bool bSeatLogged = false;
		bool bCardNotified = false;
		bool bFallbackMove = false;
		bool bPawnCollisionOverridden = false;
	};
	void HandlePull(bool bOk, const TArray<FGachaResultData>& Results);
	bool CommitResults();
	void BeginEntrance();
	void PrepareArrivalWorker(int32 ArrivalIndex);
	void BeginCards();
	void RequestSkipEntrance();
	void TickSkippedEntrance(bool bCanPrepareWorker);
	void SettleArrivalWorker(FArrival& Arrival);
	void FinishEntrance();

	bool bBusy = false;
	bool bPaymentPending = false;
	bool bPreparingAppearance = false;
	bool bAppearanceWaitTimedOut = false;
	bool bCommitted = false;
	bool bCardsStarted = false;
	bool bSkippingEntrance = false;
	bool bCommittingSeats = false;
	bool bSaveFailureReported = false;
	float CommitRetryElapsed = 0;
	float Elapsed = 0;
	float CardElapsed = 0;
	float SkipElapsed = 0;
	int32 BuildingIndex = INDEX_NONE;
	EGachaTier SelectedTier = EGachaTier::Normal;
	TArray<FGachaResultData> PendingResults;
	TArray<FSoftObjectPath> AppearanceAssetPaths;
	TSharedPtr<FStreamableHandle> AppearanceLoadHandle;
	TArray<FArrival> Arrivals;
	FRecruitEntranceHold EntranceHold;
	TWeakObjectPtr<AWorkstationActorBase> Preferred;
	TWeakObjectPtr<AInteractiveDoorActor> Door;
	UPROPERTY(Transient) TObjectPtr<UEmployeeGachaPresentationWidget> Presentation;
	TSharedPtr<SWidget> InputBlocker;
};
