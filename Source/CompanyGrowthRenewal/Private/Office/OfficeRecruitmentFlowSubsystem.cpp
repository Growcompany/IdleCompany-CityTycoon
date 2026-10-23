#include "Office/OfficeRecruitmentFlowSubsystem.h"
#include "Office/RecruitmentSkipGesture.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Office/WorkstationActorBase.h"
#include "Entity/Door/InteractiveDoorActor.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "GameMode/OfficeGameMode.h"
#include "Core/CGGameInstance.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Player/OfficeCameraPawn.h"
#include "UI/Panel/EmployeeGachaPresentationWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Enum/WidgetType.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

namespace
{
constexpr float PortraitReadyTimeout = 8.f;

// 카드 위의 투명 입력막이 마우스·터치를 한 경로로 소비한다.
class SRecruitmentSkipInput final : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SRecruitmentSkipInput) {}
		SLATE_EVENT(FSimpleDelegate, OnSkip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SkipDelegate = InArgs._OnSkip;
		Gesture.Arm(FPlatformTime::Seconds());
		SBorder::Construct(SBorder::FArguments().BorderBackgroundColor(FLinearColor::Transparent));
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!Event.IsTouchEvent() && Event.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			Gesture.Begin(FPlatformTime::Seconds(), Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), INDEX_NONE);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		return OnMouseButtonDown(Geometry, Event);
	}

	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!Event.IsTouchEvent()) Gesture.Move(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), INDEX_NONE);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!Event.IsTouchEvent() && Event.GetEffectingButton() == EKeys::LeftMouseButton)
			EndTap(Geometry, Event, INDEX_NONE);
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FReply OnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		Gesture.Begin(FPlatformTime::Seconds(), Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Event.GetPointerIndex());
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		Gesture.Move(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Event.GetPointerIndex());
		return FReply::Handled();
	}

	virtual FReply OnTouchEnded(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		EndTap(Geometry, Event, Event.GetPointerIndex());
		return FReply::Handled().ReleaseMouseCapture();
	}

private:
	void EndTap(const FGeometry& Geometry, const FPointerEvent& Event, int32 PointerIndex)
	{
		if (Gesture.End(FPlatformTime::Seconds(), Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), PointerIndex))
			SkipDelegate.ExecuteIfBound();
	}

	FRecruitmentSkipGesture Gesture;
	FSimpleDelegate SkipDelegate;
};
}

bool UOfficeRecruitmentFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* CandidateWorld = Cast<UWorld>(Outer);
	return CandidateWorld && CandidateWorld->IsGameWorld();
}

TStatId UOfficeRecruitmentFlowSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOfficeRecruitmentFlowSubsystem, STATGROUP_Tickables);
}

bool UOfficeRecruitmentFlowSubsystem::IsTickable() const
{
	return bBusy && !IsTemplate();
}

int32 UOfficeRecruitmentFlowSubsystem::GetFreeSeatCount() const
{
	const UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr;
	int32 Count = 0;
	if (OfficeMgr)
	{
		for (const AWorkstationActorBase* Workstation : OfficeMgr->GetPlacedWorkstations())
		{
			if (!IsValid(Workstation)) continue;
			for (int32 SeatIndex = 0; SeatIndex < Workstation->GetChairCount(); ++SeatIndex)
			{
				if (Workstation->GetAssignedEmployeeID(SeatIndex) < 0) ++Count;
			}
		}
	}
	return Count;
}

bool UOfficeRecruitmentFlowSubsystem::RequestRecruitment(EGachaTier Tier, int32 RequestedCount,
	EProductionDiscipline PrimaryDiscipline, AWorkstationActorBase* PreferredWorkstation, bool bHoldEntranceUntilReleased)
{
	UCGGameInstance* GI = GetWorld() ? Cast<UCGGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	URecruitmentManagerSubsystem* RecruitMgr = GI ? GI->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	AOfficeGameMode* OfficeGM = GetWorld() ? GetWorld()->GetAuthGameMode<AOfficeGameMode>() : nullptr;
	UItemInventoryManager* ItemMgr = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	if (!GI || !RecruitMgr || !ItemMgr || !OfficeGM || GI->IsVisitMode() || GI->IsLevelTransitionActive() || bBusy
		|| RequestedCount < 1 || RequestedCount > 5 || PrimaryDiscipline >= EProductionDiscipline::Count)
	{
		return false;
	}
	AOfficeInterior* Interior = OfficeGM->GetOfficeInterior();
	if (!IsValid(Interior) || !IsValid(Interior->GetRecruitmentDoor()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitment] Office entrance door is unavailable; payment not requested"));
		return false;
	}
	BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (RequestedCount > FMath::Min(GetFreeSeatCount(), RecruitMgr->GetFreeCapacity(BuildingIndex))) return false;

	bBusy = true;
	bPaymentPending = true;
	bPreparingAppearance = false;
	bAppearanceWaitTimedOut = false;
	bCommitted = false;
	bCardsStarted = false;
	bSkippingEntrance = false;
	bSaveFailureReported = false;
	CommitRetryElapsed = 0.f;
	CardElapsed = 0.f;
	SkipElapsed = 0.f;
	SelectedTier = Tier;
	Preferred = PreferredWorkstation;
	Elapsed = 0.f;
	PendingResults.Reset();
	Arrivals.Reset();
	EntranceHold.Reset();
	if (bHoldEntranceUntilReleased) EntranceHold.Request();

	// 투명 입력막은 결제 중 중복 채용과 좌석 편집, 연출 중 오피스 조작을 막는다.
	if (UGameViewportClient* ViewportClient = GI->GetGameViewportClient())
	{
		InputBlocker = SNew(SBorder).BorderBackgroundColor(FLinearColor::Transparent)
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			.OnMouseButtonUp_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); });
		ViewportClient->AddViewportWidgetContent(InputBlocker.ToSharedRef(), 990);
	}
	RecruitMgr->RequestGachaPull(Tier, BuildingIndex, RequestedCount,
		URecruitmentManagerSubsystem::FOnGachaPullDone::CreateUObject(this,
			&UOfficeRecruitmentFlowSubsystem::HandlePull), PrimaryDiscipline,
		ItemMgr->GetItemCount(RecruitMgr->GetTicketTypeForTier(Tier)) == 0);
	return true;
}

void UOfficeRecruitmentFlowSubsystem::HandlePull(bool bOk, const TArray<FGachaResultData>& Results)
{
	bPaymentPending = false;
	if (!bOk || Results.IsEmpty())
	{
		FinishEntrance();
		OnRequestFinished.Broadcast(false, bOk ? FName(TEXT("Replayed")) : FName(TEXT("PaymentFailed")));
		return;
	}
	PendingResults = Results;
	Elapsed = 0.f;
	AppearanceAssetPaths.Reset();
	for (const FGachaResultData& Result : Results)
		AFunnyOfficeworker::GatherAppearanceAssets(GetWorld()->GetGameInstance(),
			Result.ResultEmployee.EmployeeID, Result.ResultEmployee.Gender, AppearanceAssetPaths);
	AppearanceLoadHandle = AppearanceAssetPaths.IsEmpty() ? nullptr
		: UAssetManager::GetStreamableManager().RequestAsyncLoad(AppearanceAssetPaths);
	// 결제 콜백 프레임에 파츠 로드/메시 준비/실제 직원 스폰을 함께 몰지 않는다.
	bPreparingAppearance = true;
}

bool UOfficeRecruitmentFlowSubsystem::CommitResults()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OfficeRecruitment_CommitResults);
	UGameInstance* GI = GetWorld()->GetGameInstance();
	UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>();
	URecruitmentManagerSubsystem* RecruitMgr = GI->GetSubsystem<URecruitmentManagerSubsystem>();
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!EmpMgr || !RecruitMgr || !SaveMgr || PendingResults.IsEmpty()) return false;
	int32 SeatedCount = 0;
	int32 BenchedCount = 0;
	bool bAllHired = true;
	for (const FGachaResultData& Result : PendingResults)
	{
		const FEmployeeInstance* Employee = EmpMgr->GetEmployeeData(Result.ResultEmployee.EmployeeID);
		bAllHired &= Employee && Employee->AssignedBuildingIndex == Result.TargetBuildingIndex;
		if (Employee && Employee->bIsAssigned) ++SeatedCount;
		else if (Employee) ++BenchedCount;
	}
	bool bSaved = false;
	if (bAllHired)
	{
		// 이미 생성된 직원은 다시 고용하지 않고 저장만 재시도한다.
		bSaved = SaveMgr->SaveGameData();
		// 로스터에 이미 들어갔으니 이후 어떤 저장이든 함께 실린다 ― 저장 불능(용량 부족·저장 억제)에 입력·전환을 영구히 묶지 않는다.
		if (!bSaved && bSaveFailureReported)
		{
			SaveMgr->RequestDeferredSave();
			bSaved = true;
		}
	}
	else
	{
		TGuardValue<bool> CommitGuard(bCommittingSeats, true);
		bSaved = RecruitMgr->ConfirmGachaHireBatch(PendingResults, Preferred.Get(), SeatedCount, BenchedCount, true);
	}
	if (!bSaved)
	{
		if (!bSaveFailureReported)
		{
			bSaveFailureReported = true;
			UE_LOG(LogTemp, Error, TEXT("[OfficeRecruitment] Hire persistence pending; retrying without another payment"));
			if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
				UIMgr->ShowNotification(NSLOCTEXT("OfficeRecruitment", "SaveRetry", "채용 결과 저장을 다시 시도하고 있습니다"), 4.f, ENotificationType::Failed);
		}
		return false;
	}
	bCommitted = true;
	// 고용 중 지급된 장비까지 준비한 뒤 입장 워커에 외형을 적용한다.
	for (const FGachaResultData& Result : PendingResults)
		AFunnyOfficeworker::GatherAppearanceAssets(GI, Result.ResultEmployee.EmployeeID,
			Result.ResultEmployee.Gender, AppearanceAssetPaths);
	AppearanceLoadHandle = AppearanceAssetPaths.IsEmpty() ? nullptr
		: UAssetManager::GetStreamableManager().RequestAsyncLoad(AppearanceAssetPaths);
	UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitment] Saved %d employees, seated=%d benched=%d"),
		PendingResults.Num(), SeatedCount, BenchedCount);
	OnRequestFinished.Broadcast(true, NAME_None);
	// 모달이 퇴장 연출을 마칠 때까지 문 입장을 미룬다(ReleaseEntrance 또는 안전 시한).
	if (EntranceHold.OnCommitted()) BeginEntrance();
	return true;
}

void UOfficeRecruitmentFlowSubsystem::BeginEntrance()
{
	APlayerController* OfficePlayerController = GetWorld()->GetFirstPlayerController();
	if (AMainMapPlayerController* OfficePC = Cast<AMainMapPlayerController>(OfficePlayerController)) OfficePC->GoToUIMode();
	if (AOfficeCameraPawn* OfficeCamera = OfficePlayerController ? Cast<AOfficeCameraPawn>(OfficePlayerController->GetPawn()) : nullptr)
	{
		OfficeCamera->StopFollowingEmployee();
		OfficeCamera->StopCameraTransition();
		OfficeCamera->ApplyInitialOfficeFraming(true);
	}
	AOfficeGameMode* OfficeGM = GetWorld()->GetAuthGameMode<AOfficeGameMode>();
	AOfficeInterior* Interior = OfficeGM ? OfficeGM->GetOfficeInterior() : nullptr;
	Door = Interior ? Interior->GetRecruitmentDoor() : nullptr;
	if (Door.IsValid()) Door->OpenDoor();
	Elapsed = 0.f;
	UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>();
	for (int32 Index = 0; Index < PendingResults.Num(); ++Index)
	{
		FArrival& Arrival = Arrivals.AddDefaulted_GetRef();
		Arrival.Workstation = OfficeMgr ? OfficeMgr->FindWorkstationByEmployeeID(PendingResults[Index].ResultEmployee.EmployeeID) : nullptr;
		if (Arrival.Workstation.IsValid())
			for (int32 SeatIndex = 0; SeatIndex < Arrival.Workstation->GetChairCount(); ++SeatIndex)
				if (Arrival.Workstation->GetAssignedEmployeeID(SeatIndex) == PendingResults[Index].ResultEmployee.EmployeeID)
					{ Arrival.SeatIndex = SeatIndex; break; }
	}
	BeginCards();
	if (UGameViewportClient* ViewportClient = GetWorld()->GetGameInstance()->GetGameViewportClient())
	{
		if (InputBlocker) ViewportClient->RemoveViewportWidgetContent(InputBlocker.ToSharedRef());
		InputBlocker = SNew(SRecruitmentSkipInput)
			.OnSkip(FSimpleDelegate::CreateUObject(this, &ThisClass::RequestSkipEntrance));
		ViewportClient->AddViewportWidgetContent(InputBlocker.ToSharedRef(), 1001);
	}
}

void UOfficeRecruitmentFlowSubsystem::PrepareArrivalWorker(int32 ArrivalIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OfficeRecruitment_PrepareArrivalWorker);
	FArrival& Arrival = Arrivals[ArrivalIndex];
	Arrival.bWorkerPrepared = true;
	const int32 EmployeeID = PendingResults[ArrivalIndex].ResultEmployee.EmployeeID;
	AOfficeGameMode* OfficeGM = GetWorld()->GetAuthGameMode<AOfficeGameMode>();
	UEmployeeManager* EmpMgr = GetWorld()->GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const FEmployeeInstance* Employee = EmpMgr ? EmpMgr->GetEmployeeData(EmployeeID) : nullptr;
	if (OfficeGM && Employee && Arrival.Workstation.IsValid() && Arrival.SeatIndex != INDEX_NONE)
		OfficeGM->SpawnWorkerForAssignment(*Employee, Arrival.Workstation.Get(), Arrival.SeatIndex);
	AOfficeworker* Worker = OfficeGM ? OfficeGM->FindEmployeeActorByID(EmployeeID) : nullptr;
	Arrival.Worker = Worker;
	UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitment] arrival worker prepared roll=%d employee=%d frame=%llu"),
		ArrivalIndex, EmployeeID, GFrameCounter);
	if (!Worker || !Worker->BehaviorComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitment] Arrival worker unavailable for saved employee=%d"), EmployeeID);
		return;
	}
	Worker->BehaviorComponent->BeginRecruitmentArrival();
	UCapsuleComponent* ArrivalCapsule = Worker->GetCapsuleComponent();
	Arrival.PawnCollisionResponse = ArrivalCapsule->GetCollisionResponseToChannel(ECC_Pawn);
	ArrivalCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Arrival.bPawnCollisionOverridden = true;
	Arrival.Target = Worker->GetWorkstationApproachLocation();
	Worker->SetActorHiddenInGame(true);
	AOfficeInterior* Interior = OfficeGM ? OfficeGM->GetOfficeInterior() : nullptr;
	if (Door.IsValid() && Interior)
	{
		// 문 로컬 -Y가 실내 방향이다. NavMesh에서 이동을 시작한다.
		FVector Entrance = Door->GetActorLocation() - Interior->GetActorRightVector() * 55.f;
		FNavLocation EntranceNav;
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
			if (NavSys->ProjectPointToNavigation(Entrance, EntranceNav, FVector(240.f, 240.f, 300.f)))
				Entrance = EntranceNav.Location;
		Entrance.Z = Interior->GetActorTransform().TransformPosition(FVector(0.f, 0.f,
			Interior->GetStructuralFloorZLocal())).Z + ArrivalCapsule->GetScaledCapsuleHalfHeight();
		Worker->SetActorLocation(Entrance, false, nullptr, ETeleportType::TeleportPhysics);
	}
	Arrival.LastLocation = Worker->GetActorLocation();
	Arrival.MoveTimeout = FMath::Clamp(FVector::Dist2D(Arrival.LastLocation, Arrival.Target) / 450.f + 1.f, 1.f, 4.f);
}

void UOfficeRecruitmentFlowSubsystem::Tick(float DeltaTime)
{
	if (!bBusy || bPaymentPending) return;
	if (bPreparingAppearance)
	{
		if ((AppearanceLoadHandle && !AppearanceLoadHandle->HasLoadCompleted())
			|| !AFunnyOfficeworker::AreAppearanceAssetsReady(AppearanceAssetPaths)) return;
		bPreparingAppearance = false;
		CommitResults();
		return;
	}
	if (!bCommitted)
	{
		CommitRetryElapsed += DeltaTime;
		if (CommitRetryElapsed >= 1.f)
		{
			CommitRetryElapsed = 0.f;
			CommitResults();
		}
		return;
	}
	if (EntranceHold.IsHeld())
	{
		if (EntranceHold.Tick(DeltaTime))
		{
			if (EntranceHold.DidTimeOut())
				UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitment] Modal exit did not release entrance; starting after timeout"));
			BeginEntrance();
		}
		return;
	}
	Elapsed += DeltaTime;
	const UEmployeeManager* EmpMgr = GetWorld()->GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const bool bAppearanceReady = (!AppearanceLoadHandle || AppearanceLoadHandle->HasLoadCompleted())
		&& AFunnyOfficeworker::AreAppearanceAssetsReady(AppearanceAssetPaths);
	if (!bAppearanceReady && !bAppearanceWaitTimedOut && Elapsed >= PortraitReadyTimeout)
	{
		bAppearanceWaitTimedOut = true;
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitment] Appearance preparation timed out; attempting saved workers one at a time"));
	}
	if (bSkippingEntrance)
	{
		SkipElapsed += DeltaTime;
		TickSkippedEntrance(bAppearanceReady || bAppearanceWaitTimedOut);
		return;
	}
	bool bAllComplete = true;
	bool bPreparedThisTick = false;
	int32 PreparedIndex = INDEX_NONE;
	for (int32 ArrivalIndex = 0; ArrivalIndex < Arrivals.Num(); ++ArrivalIndex)
	{
		FArrival& Arrival = Arrivals[ArrivalIndex];
		if (!Arrival.bWorkerPrepared && !bPreparedThisTick)
		{
			if (bAppearanceReady || bAppearanceWaitTimedOut)
			{
				PrepareArrivalWorker(ArrivalIndex);
				bPreparedThisTick = true;
				PreparedIndex = ArrivalIndex;
			}
		}
		if (Arrival.bWorkerPrepared && !Arrival.bStarted && ArrivalIndex != PreparedIndex)
		{
			Arrival.bStarted = true;
			if (AOfficeworker* Worker = Arrival.Worker.Get(); Worker && Worker->BehaviorComponent)
			{
				Worker->SetActorHiddenInGame(false);
				Worker->MoveToWorkstationArea();
			}
			else SettleArrivalWorker(Arrival);
			UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitment] entrance started roll=%d employee=%d frame=%llu"),
				ArrivalIndex, PendingResults[ArrivalIndex].ResultEmployee.EmployeeID, GFrameCounter);
		}
		if (Arrival.bStarted && !Arrival.bSettled)
		{
			AOfficeworker* Worker = Arrival.Worker.Get();
			if (!Worker || !Worker->BehaviorComponent || !Arrival.Workstation.IsValid())
				SettleArrivalWorker(Arrival);
			else
			{
				Arrival.MoveElapsed += DeltaTime;
				const FVector CurrentLocation = Worker->GetActorLocation();
				const float Progress = FVector::Dist2D(CurrentLocation, Arrival.LastLocation);
				Arrival.StillElapsed = Progress > 3.f ? 0.f : Arrival.StillElapsed + DeltaTime;
				if (Progress > 3.f) Arrival.LastLocation = CurrentLocation;
				AAIController* ArrivalController = Cast<AAIController>(Worker->GetController());
				const UPathFollowingComponent* PathFollowing = ArrivalController ? ArrivalController->GetPathFollowingComponent() : nullptr;
				const bool bIdle = !PathFollowing || PathFollowing->GetStatus() == EPathFollowingStatus::Idle;
				if (FVector::Dist2D(CurrentLocation, Arrival.Target) <= 65.f)
					SettleArrivalWorker(Arrival);
				else if (bIdle && Arrival.MoveElapsed >= 0.1f && !Arrival.bFallbackMove && Door.IsValid())
				{
					// 책상 접근점이 다른 NavMesh 섬이면 문 안쪽의 짧은 목표로 한 번 더 걷는다.
					AOfficeGameMode* OfficeGM = GetWorld()->GetAuthGameMode<AOfficeGameMode>();
					AOfficeInterior* Interior = OfficeGM ? OfficeGM->GetOfficeInterior() : nullptr;
					FVector ShortTarget = Interior
						? Door->GetActorLocation() - Interior->GetActorRightVector() * 180.f : CurrentLocation;
					FNavLocation ShortNav;
					if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
						if (NavSys->ProjectPointToNavigation(ShortTarget, ShortNav, FVector(240.f, 240.f, 300.f)))
							ShortTarget = ShortNav.Location;
					Arrival.bFallbackMove = true;
					Arrival.Target = ShortTarget;
					Arrival.MoveElapsed = 0.f;
					Arrival.StillElapsed = 0.f;
					Arrival.MoveTimeout = FMath::Clamp(FVector::Dist2D(CurrentLocation, ShortTarget) / 450.f + 1.f, 1.f, 4.f);
					if (!ArrivalController || ArrivalController->MoveToLocation(ShortTarget, 50.f, false) == EPathFollowingRequestResult::Failed)
						SettleArrivalWorker(Arrival);
				}
				else if ((bIdle && Arrival.MoveElapsed >= 0.1f) || Arrival.StillElapsed >= 0.45f
					|| Arrival.MoveElapsed >= Arrival.MoveTimeout)
					SettleArrivalWorker(Arrival);
			}
		}
		if (Arrival.bSettled && !Arrival.bSeatLogged)
		{
			Arrival.bSeatLogged = true;
			UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitment] seat revealed roll=%d employee=%d frame=%llu"),
				ArrivalIndex, PendingResults[ArrivalIndex].ResultEmployee.EmployeeID, GFrameCounter);
		}
		if (Arrival.bSettled && !Arrival.bCardNotified)
		{
			const bool bPortraitReady = !Presentation || Presentation->IsOfficePortraitReady(ArrivalIndex);
			const bool bPortraitFailed = !EmpMgr || !EmpMgr->IsPortraitCapturing();
			if (bPortraitReady || bPortraitFailed || Elapsed - Arrival.SeatedAt >= PortraitReadyTimeout)
			{
				if (!bPortraitReady)
					UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitment] Portrait unavailable; continuing saved hire employee=%d"),
						PendingResults[ArrivalIndex].ResultEmployee.EmployeeID);
				Arrival.bCardNotified = true;
				if (Presentation) Presentation->NotifyOfficeEmployeeArrived(ArrivalIndex);
			}
		}
		bAllComplete &= Arrival.bSettled && Arrival.bCardNotified;
	}
	if (bAllComplete && bCardsStarted)
	{
		CardElapsed += DeltaTime;
		if (!Presentation || Presentation->IsOfficeArrivalFinished() || CardElapsed > 10.f) FinishEntrance();
	}
}

void UOfficeRecruitmentFlowSubsystem::BeginCards()
{
	bCardsStarted = true;
	UGameInstance* GI = GetWorld()->GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	TSubclassOf<UUserWidget> CardClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::EmployeeGachaPresentation) : nullptr;
	Presentation = CardClass ? CreateWidget<UEmployeeGachaPresentationWidget>(GI, CardClass) : nullptr;
	if (!Presentation) return;
	TArray<FVector> Targets;
	for (const FArrival& Arrival : Arrivals)
	{
		Targets.Add(Arrival.Workstation.IsValid() && Arrival.SeatIndex != INDEX_NONE
			? Arrival.Workstation->GetSeatLocation(Arrival.SeatIndex) : FVector::ZeroVector);
	}
	if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
		MissionMgr->SetRecruitmentPresentationActive(true);
	Presentation->AddToViewport(1000);
	Presentation->SetupOfficeArrival(PendingResults, SelectedTier, BuildingIndex, Targets);
}

void UOfficeRecruitmentFlowSubsystem::RequestSkipEntrance()
{
	if (!bBusy || !bCommitted || !bCardsStarted || EntranceHold.IsHeld() || bSkippingEntrance) return;
	bSkippingEntrance = true;
	SkipElapsed = 0.f;
	if (Presentation) { Presentation->RemoveFromParent(); Presentation = nullptr; }
	for (FArrival& Arrival : Arrivals)
		if (Arrival.bWorkerPrepared) SettleArrivalWorker(Arrival);
	UE_LOG(LogTemp, Log, TEXT("[OfficeRecruitment] skip requested employees=%d frame=%llu"), PendingResults.Num(), GFrameCounter);
}

void UOfficeRecruitmentFlowSubsystem::TickSkippedEntrance(bool bCanPrepareWorker)
{
	for (int32 ArrivalIndex = 0; ArrivalIndex < Arrivals.Num(); ++ArrivalIndex)
	{
		FArrival& Arrival = Arrivals[ArrivalIndex];
		if (!Arrival.bWorkerPrepared)
		{
			if (bCanPrepareWorker) PrepareArrivalWorker(ArrivalIndex);
			// 새 워커는 한 Tick 에 한 명만 준비하고 다음 Tick 에 착석·표시한다.
			return;
		}
		SettleArrivalWorker(Arrival);
	}
	// 스킵 직후 이어지는 짧은 연타가 오피스 버튼으로 새지 않게 한다.
	if (SkipElapsed >= 0.2f) FinishEntrance();
}

void UOfficeRecruitmentFlowSubsystem::SettleArrivalWorker(FArrival& Arrival)
{
	if (Arrival.bSettled) return;
	Arrival.bSettled = true;
	Arrival.SeatedAt = Elapsed;
	if (AOfficeworker* Worker = Arrival.Worker.Get())
	{
		if (AAIController* ArrivalController = Cast<AAIController>(Worker->GetController()))
			ArrivalController->StopMovement();
		if (Worker->BehaviorComponent) Worker->BehaviorComponent->EndRecruitmentArrival();
		Worker->SetActorHiddenInGame(false);
		if (Arrival.bPawnCollisionOverridden)
		{
			Worker->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, Arrival.PawnCollisionResponse);
			Arrival.bPawnCollisionOverridden = false;
		}
	}
}

void UOfficeRecruitmentFlowSubsystem::FinishEntrance()
{
	if (!bBusy) return;
	const bool bWasCommitted = bCommitted;
	if (AppearanceLoadHandle) AppearanceLoadHandle->CancelHandle();
	AppearanceLoadHandle.Reset();
	AppearanceAssetPaths.Reset();
	bPreparingAppearance = false;
	for (FArrival& Arrival : Arrivals) SettleArrivalWorker(Arrival);
	if (Presentation) { Presentation->RemoveFromParent(); Presentation = nullptr; }
	if (Door.IsValid()) Door->CloseDoor();
	Door.Reset();
	if (InputBlocker)
	{
		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameInstance()->GetGameViewportClient())
			ViewportClient->RemoveViewportWidgetContent(InputBlocker.ToSharedRef());
		InputBlocker.Reset();
	}
	bBusy = false;
	bPaymentPending = false;
	bSkippingEntrance = false;
	bCardsStarted = false;
	EntranceHold.Reset();
	Arrivals.Reset();
	PendingResults.Reset();
	if (UWorld* CurrentWorld = GetWorld())
	{
		if (bWasCommitted)
			if (AMainMapPlayerController* OfficePC = Cast<AMainMapPlayerController>(CurrentWorld->GetFirstPlayerController())) OfficePC->GoToNormalMode();
		if (UGameInstance* GI = CurrentWorld->GetGameInstance())
			if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
			{
				if (bWasCommitted) MissionMgr->NotifyRecruitPanelClosed();
				MissionMgr->SetRecruitmentPresentationActive(false);
			}
	}
}

void UOfficeRecruitmentFlowSubsystem::ReleaseEntrance()
{
	EntranceHold.Release();
}

void UOfficeRecruitmentFlowSubsystem::Deinitialize()
{
	FinishEntrance();
	OnRequestFinished.Clear();
	Super::Deinitialize();
}
