#include "Manager/GoalBoardSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Data/GameSaveData.h"
#include "Data/GoalBoardLoadEvaluationRules.h"
#include "Data/GrowthGoalRules.h"
#include "Data/WorldMapTypes.h"
#include "Manager/EmployeeManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SpawnManager.h"
#include "Manager/EntityManager.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Entity/Factory/BrickFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Core/CGGameInstance.h"
#include "Entity/Plot/CityPlotActor.h"
#include "UI/Panel/BuildModalWidget.h"
#include "Office/WorkstationUpgradeRules.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"

void UGoalBoardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 구독 대상은 InitializeDependency 로 강제 — 없으면 init 순서상 null 구독 스킵 (GDD_TUTORIAL §4 함정)
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UMissionManagerSubsystem>();
	Collection.InitializeDependency<UResourceItemManager>();
	Collection.InitializeDependency<UEmployeeManager>();
	Collection.InitializeDependency<UItemInventoryManager>();
	ReadinessTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UGoalBoardSubsystem::TickReadiness), 1.0f);

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->OnConditionSignal.AddUObject(this, &UGoalBoardSubsystem::HandleConditionSignal);
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UGoalBoardSubsystem::HandleGameDataLoaded);
	}
	if (UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		Resources->OnResourceChanged.AddUObject(this, &UGoalBoardSubsystem::HandleResourceChanged);
	}
	if (UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		Employees->OnEmployeeExperienceChanged.AddWeakLambda(this, [this](int32) { RefreshRecommendation(); });
		Employees->OnEmployeeRosterChanged.AddUObject(this, &UGoalBoardSubsystem::RefreshRecommendation);
	}
	if (UItemInventoryManager* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
	{
		Items->OnItemChanged.AddWeakLambda(this, [this](EItemType, int32, int32) { RefreshRecommendation(); });
	}
}

void UGoalBoardSubsystem::Deinitialize()
{
	if (UItemInventoryManager* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
	{
		Items->OnItemChanged.RemoveAll(this);
	}
	FTSTicker::GetCoreTicker().RemoveTicker(ReadinessTickerHandle);
	if (UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		Employees->OnEmployeeExperienceChanged.RemoveAll(this);
		Employees->OnEmployeeRosterChanged.RemoveAll(this);
	}
	if (UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		Resources->OnResourceChanged.RemoveAll(this);
	}
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->OnConditionSignal.RemoveAll(this);
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UGoalBoardSubsystem::HandleGameDataLoaded()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData)
	{
		return;
	}
	bUnlocked = SaveData->GameData.bGoalBoardUnlocked;
	CompletedGoalIDs = SaveData->GameData.CompletedGoalIDs;
	GoalEventProgressByID = SaveData->GameData.GoalEventProgressByID;
	GoalSuccessCountsByID = SaveData->GameData.GoalSuccessCountsByID;
	ReadyGoalIDs = SaveData->GameData.ReadyGoalIDs;
	ActiveGuideGoalID = SaveData->GameData.ActiveGuideGoalID;
	bLoadedFromSave = true;
	bool bEvaluationDirty = false;
	if (bUnlocked)
	{
		bEvaluationDirty = EvaluateAllGoals(/*bShouldSave=*/false, /*bShouldBroadcast=*/false);
	}

	// 로드로 상태가 바뀌었을 수 있다 — 더 이상 안내할 수 없는 대상이면 그때만 해제한다.
	// ⚠ 무조건 해제하면 안 된다: 이 함수는 맵 전환마다 도는 LoadGameAfterLevelStart 경로에 있어,
	//    사무실을 오가야 하는 미션(G7 등)의 안내가 전환 때마다 꺼진다
	if (!TrackedGoalID.IsNone())
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		FGoalTable Row;
		if (!TableMgr || !TableMgr->GetGoalData(TrackedGoalID, Row))
		{
			SetTrackedGoal(NAME_None);
		}
		else
		{
			const EGoalState State = GetGoalState(TrackedGoalID, Row);
			if (State != EGoalState::InProgress)
			{
				SetTrackedGoal(NAME_None);
			}
		}
	}

	const FGoalBoardLoadEvaluationDecision Decision = FGoalBoardLoadEvaluationRules::Resolve(
		bEvaluationDirty, /*bSaveAttempted=*/false, /*bSaveSucceeded=*/false);
	if (Decision.bScheduleSaveForNextTick)
	{
		if (UWorld* CurrentWorld = GetWorld())
		{
			CurrentWorld->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &UGoalBoardSubsystem::HandlePostLoadEvaluationSave));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GoalBoard] 로드 재평가 저장 예약 실패: World 없음"));
		}
		return;
	}

	if (Decision.bBroadcastBoardChanged)
	{
		OnGoalBoardChanged.Broadcast();
	}
}

void UGoalBoardSubsystem::HandlePostLoadEvaluationSave()
{
	UGameInstance* CurrentGameInstance = GetGameInstance();
	USaveLoadManager* SaveMgr = CurrentGameInstance
		? CurrentGameInstance->GetSubsystem<USaveLoadManager>()
		: nullptr;
	const bool bSaveSucceeded = SaveMgr && SaveMgr->SaveGameData();
	const FGoalBoardLoadEvaluationDecision Decision = FGoalBoardLoadEvaluationRules::Resolve(
		/*bEvaluationDirty=*/true, /*bSaveAttempted=*/true, bSaveSucceeded);
	if (Decision.bBroadcastBoardChanged)
	{
		OnGoalBoardChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GoalBoard] 로드 재평가 저장 실패: UI 게시 보류"));
	}
}

void UGoalBoardSubsystem::CollectSaveData(FGameSaveData& OutData) const
{
	// 로드 핸드오프 전에는 멤버가 전부 기본값이다 ― 그대로 내보내면 수령 완료 목표와
	// 층 상승 카운터를 빈 값으로 덮는다(형제 SideContentSubsystem 의 bLoaded 선례).
	if (!bLoadedFromSave)
	{
		return;
	}
	OutData.bGoalBoardUnlocked = bUnlocked;
	OutData.CompletedGoalIDs = CompletedGoalIDs;
	OutData.GoalEventProgressByID = GoalEventProgressByID;
	OutData.GoalSuccessCountsByID = GoalSuccessCountsByID;
	OutData.ReadyGoalIDs = ReadyGoalIDs;
	OutData.ActiveGuideGoalID = ActiveGuideGoalID;
}

void UGoalBoardSubsystem::UnlockBoard()
{
	if (!PrepareUnlockForAtomicSave())
	{
		return;
	}
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr || !SaveMgr->SaveGameData())
	{
		RollbackPreparedUnlock();
		UE_LOG(LogTemp, Error, TEXT("[GoalBoard] 언락 저장 실패 — 준비 상태 롤백"));
		return;
	}
	PublishPreparedUnlock();
	UE_LOG(LogTemp, Log, TEXT("[GoalBoard] 언락 — 미션판 활성"));
}

bool UGoalBoardSubsystem::PrepareUnlockForAtomicSave()
{
	if (bUnlocked)
	{
		return false;
	}

	bUnlockedBeforePreparedUnlock = bUnlocked;
	CompletedGoalIDsBeforePreparedUnlock = CompletedGoalIDs;
	GoalEventProgressBeforePreparedUnlock = GoalEventProgressByID;
	GoalSuccessCountsBeforePreparedUnlock = GoalSuccessCountsByID;
	ReadyGoalIDsBeforePreparedUnlock = ReadyGoalIDs;
	ActiveGuideGoalBeforePreparedUnlock = ActiveGuideGoalID;
	bPreparedUnlockRollbackAvailable = true;

	bUnlocked = true;
	EvaluateAllGoals(/*bShouldSave=*/false, /*bShouldBroadcast=*/false);
	bUnlockNotificationPending = true;
	return true;
}

void UGoalBoardSubsystem::RollbackPreparedUnlock()
{
	if (!bPreparedUnlockRollbackAvailable)
	{
		return;
	}

	bUnlocked = bUnlockedBeforePreparedUnlock;
	CompletedGoalIDs = MoveTemp(CompletedGoalIDsBeforePreparedUnlock);
	GoalEventProgressByID = MoveTemp(GoalEventProgressBeforePreparedUnlock);
	GoalSuccessCountsByID = MoveTemp(GoalSuccessCountsBeforePreparedUnlock);
	ReadyGoalIDs = MoveTemp(ReadyGoalIDsBeforePreparedUnlock);
	ActiveGuideGoalID = ActiveGuideGoalBeforePreparedUnlock;
	bUnlockNotificationPending = false;
	bPreparedUnlockRollbackAvailable = false;
}

void UGoalBoardSubsystem::PublishPreparedUnlock()
{
	if (!bUnlockNotificationPending)
	{
		return;
	}

	bUnlockNotificationPending = false;
	bPreparedUnlockRollbackAvailable = false;
	CompletedGoalIDsBeforePreparedUnlock.Reset();
	GoalEventProgressBeforePreparedUnlock.Reset();
	GoalSuccessCountsBeforePreparedUnlock.Reset();
	ReadyGoalIDsBeforePreparedUnlock.Reset();
	OnGoalBoardChanged.Broadcast();
}

void UGoalBoardSubsystem::HandleConditionSignal(EMissionConditionType Type, int64 Amount)
{
	if (!bUnlocked || bClaimInProgress || Amount <= 0) { return; }
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) { return; }
	if (Type == EMissionConditionType::ReachFactoryProductionLevel)
	{
		const bool bChanged = EvaluateAllGoals(false, false);
		RefreshRecommendation();
		OnGoalBoardChanged.Broadcast();
		if (bChanged)
		{
			if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
			{
				SaveMgr->RequestDeferredSave();
			}
		}
		return;
	}
	bool bChanged = false;
	for (const auto& Pair : TableMgr->GetAllGoalData())
	{
		const FGoalTable& Row = Pair.Value;
		if (Row.ConditionType != Type || !IsGoalAvailable(Row)
			|| !GrowthGoalRules::CanRecordSuccess(Row, GoalSuccessCountsByID.FindRef(Pair.Key))) { continue; }
		// 선행 퀘스트를 수령하기 전 실제 행동도 보존해 같은 행동을 다시 요구하지 않는다.
		const int64 Previous = GoalEventProgressByID.FindRef(Pair.Key);
		const int64 Next = GrowthGoalRules::AddProgress(Previous, Row.bCountActions ? Amount : 1);
		GoalEventProgressByID.Add(Pair.Key, Next);
		bChanged |= Next != Previous;
		bChanged |= EvaluateGoal(Pair.Key, Row, true, false);
	}
	bChanged |= EvaluateAllGoals(false, false);
	RefreshRecommendation();
	if (bChanged)
	{
		OnGoalBoardChanged.Broadcast();
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->RequestDeferredSave();
		}
	}
}

bool UGoalBoardSubsystem::EvaluateAllGoals(bool bShouldSave, bool bShouldBroadcast)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return false;
	}
	bool bDirty = false;
	for (const auto& Pair : TableMgr->GetAllGoalData())
	{
		// 도달형만 — 이벤트형은 실제 행동 신호로만 래치 (과거 행동을 소급하지 않는다)
		bDirty |= EvaluateGoal(Pair.Key, Pair.Value, /*bFromEvent=*/false, /*bShouldBroadcast=*/false);
	}
	// 미션당 저장이 아니라 전수 재평가 1회당 최대 1저장 (저장 폭주 방지)
	if (bDirty && bShouldSave)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
	if (bDirty && bShouldBroadcast)
	{
		OnGoalBoardChanged.Broadcast();
	}
	return bDirty;
}

bool UGoalBoardSubsystem::EvaluateGoal(FName GoalID, const FGoalTable& Row, bool bFromEvent, bool bShouldBroadcast)
{
	if (!bUnlocked || ReadyGoalIDs.Contains(GoalID)
		|| !GrowthGoalRules::CanRecordSuccess(Row, GoalSuccessCountsByID.FindRef(GoalID))) { return false; }
	int64 Current = 0, Target = 0;
	const bool bHasProgress = GetConditionProgress(GoalID, Row, Current, Target);
	if (!(bHasProgress ? Current >= Target : (bFromEvent || GoalEventProgressByID.FindRef(GoalID) > 0)))
	{
		return false;
	}
	ReadyGoalIDs.AddUnique(GoalID);
	if (TrackedGoalID == GoalID && !bClaimInProgress) { SetTrackedGoal(NAME_None); }
	if (bShouldBroadcast) { OnGoalBoardChanged.Broadcast(); }
	UE_LOG(LogTemp, Log, TEXT("[GoalBoard] Ready to claim: %s"), *GoalID.ToString());
	return true;
}

bool UGoalBoardSubsystem::GetConditionProgress(
	FName GoalID,
	const FGoalTable& Row,
	int64& OutCurrent,
	int64& OutTarget) const
{
	if (Row.bCountActions)
	{
		OutCurrent = GoalEventProgressByID.FindRef(GoalID);
		OutTarget = GrowthGoalRules::TargetForClaim(Row, GoalSuccessCountsByID.FindRef(GoalID));
		return true;
	}

	USaveLoadManager* GrowthSaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* GrowthSave = GrowthSaveMgr ? GrowthSaveMgr->GetCurrentSaveData() : nullptr;
	const UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (GrowthSave && Employees && GrowthGoalRules::GetProgress(
		Row, GrowthSave->GameData, Employees->GetAllEmployeesRef(), OutCurrent, OutTarget))
	{
		if (Row.ConditionType == EMissionConditionType::EnhanceFirstEmployee)
		{
			OutCurrent = FMath::Max(OutCurrent, GoalEventProgressByID.FindRef(GoalID));
		}
		return true;
	}
	switch (Row.ConditionType)
	{
	case EMissionConditionType::ReserveManufacturingProduct:
	{
		OutCurrent = 0;
		OutTarget = FMath::Max<int64>(1, Row.ConditionAmount);
		const UProductionOrderManager* Orders = GetGameInstance()->GetSubsystem<UProductionOrderManager>();
		if (Orders)
		{
			for (const FProductionOrder& Order : Orders->GetAllOrders())
			{
				if (Order.CompanyType == Row.CompanyType && Order.ProjectIndex == Row.TargetProjectIndex)
				{
					OutCurrent = GrowthGoalRules::AddProgress(OutCurrent, Order.Quantity);
				}
			}
		}
		return true;
	}
	case EMissionConditionType::LevelUpFirstEmployee:
	case EMissionConditionType::EnhanceFirstEmployee:
	case EMissionConditionType::UpgradeFirstWorkstation:
	case EMissionConditionType::PromoteFirstWorkstation:
	case EMissionConditionType::RecruitGoalDiscipline:
	case EMissionConditionType::LaunchGoalProject:
	case EMissionConditionType::BuildIndustry:
		OutCurrent = 0;
		OutTarget = FMath::Max<int64>(1, Row.ConditionAmount);
		return true;
	case EMissionConditionType::RaiseBuildingFloor:
		OutCurrent = GoalEventProgressByID.FindRef(GoalID);
		OutTarget = FMath::Max<int64>(1, Row.ConditionAmount);
		return true;
	case EMissionConditionType::ReachHQLevel:
	{
		// 구 NotifyHQLevelChanged 판정식 이관 — SaveLoadManager 가 보유한 현재 HQ 레벨 절대값
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		OutCurrent = SaveMgr ? SaveMgr->GetHQLevel() : 0;
		OutTarget = Row.ConditionAmount;
		return true;
	}
	case EMissionConditionType::ReachFactoryProductionLevel:
	{
		OutCurrent = 0;
		OutTarget = FMath::Max<int64>(1, Row.ConditionAmount);
		if (GrowthSave)
		{
			for (const auto& FactoryPair : GrowthSave->GameData.FactoryData)
			{
				if (const FFactoryUpgradeData* Production = FactoryPair.Value.UpgradeData.Find(EFactoryUpgradeType::HoldProductionAmount))
				{
					OutCurrent = FMath::Max<int64>(OutCurrent, Production->Level);
				}
			}
		}
		if (UWorld* CurrentWorld = GetWorld())
		{
			for (TActorIterator<ABrickFactory> It(CurrentWorld); It; ++It)
			{
				OutCurrent = FMath::Max<int64>(OutCurrent, It->GetUpgradeLevel(EFactoryUpgradeType::HoldProductionAmount));
			}
		}
		return true;
	}
	case EMissionConditionType::BuildSecondBuilding:
	case EMissionConditionType::BuildOnClearedPlot:
	{
		// 구 NotifyBuildingPlaced M17/M21 판정식 이관 — 보유 빌딩 채수
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		OutCurrent = SaveData ? SaveData->GameData.Buildings.Num() : 0;
		OutTarget = (Row.ConditionType == EMissionConditionType::BuildSecondBuilding) ? 2 : 3;
		return true;
	}
	default:
		return false; // 단발 이벤트형 (AcquireFirstCompany / DemolishFirstCompany 등)
	}
}

EGoalState UGoalBoardSubsystem::GetGoalState(FName GoalID, const FGoalTable& Row) const
{
	const int32 SuccessCount = GoalSuccessCountsByID.FindRef(GoalID);
	if (!GrowthGoalRules::CanRecordSuccess(Row, SuccessCount))
	{
		return EGoalState::Completed;
	}
	if (!GrowthGoalRules::PrerequisiteMet(Row.PrereqGoalID, CompletedGoalIDs))
	{
		return EGoalState::Locked;
	}
	if (!IsGoalAvailable(Row)) { return EGoalState::Locked; }
	return ReadyGoalIDs.Contains(GoalID) ? EGoalState::ReadyToClaim : EGoalState::InProgress;
}

void UGoalBoardSubsystem::GetBoardEntries(TArray<FGoalBoardEntry>& OutEntries) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr || !bUnlocked)
	{
		OutEntries.Reset();
		return;
	}

	BuildBoardEntriesFromTableManager(*TableMgr, OutEntries);
}

void UGoalBoardSubsystem::BuildBoardEntriesFromTableManager(
	const UTableManagerSubsystem& TableMgr,
	TArray<FGoalBoardEntry>& OutEntries) const
{
	OutEntries.Reset();
	for (const FName GoalID : TableMgr.GetGoalRowOrder())
	{
		FGoalTable Row;
		if (!TableMgr.GetGoalData(GoalID, Row))
		{
			continue;
		}

		FGoalBoardEntry Entry;
		Entry.GoalID = GoalID;
		Entry.Row = Row;
		Entry.State = GetGoalState(GoalID, Row);
		Entry.ClaimCount = GoalSuccessCountsByID.FindRef(GoalID);
		Entry.bPracticed = Entry.ClaimCount > 0;
		if (!GetConditionProgress(GoalID, Row, Entry.ProgressCurrent, Entry.ProgressTarget))
		{
			Entry.ProgressCurrent = GoalEventProgressByID.FindRef(GoalID);
			Entry.ProgressTarget = 1;
		}
		if (Entry.State == EGoalState::ReadyToClaim)
		{
			Entry.ProgressCurrent = FMath::Max(Entry.ProgressCurrent, Entry.ProgressTarget);
		}
		Entry.bRecommendable = Entry.State == EGoalState::ReadyToClaim
			|| (Entry.State == EGoalState::InProgress && !DismissedGoalIDs.Contains(GoalID)
				&& IsGoalReady(GoalID, Row));
		OutEntries.Add(MoveTemp(Entry));
	}
}

namespace CGRGrowthRecommendation
{
FName Select(const TArray<FGoalBoardEntry>& Entries)
{
	const FGoalBoardEntry* Best = nullptr;
	for (const FGoalBoardEntry& Entry : Entries)
	{
		if ((Entry.State != EGoalState::InProgress && Entry.State != EGoalState::ReadyToClaim)
			|| !Entry.bRecommendable) { continue; }
		if (!Best) { Best = &Entry; continue; }
		const bool bReady = Entry.State == EGoalState::ReadyToClaim;
		const bool bBestReady = Best->State == EGoalState::ReadyToClaim;
		const bool bRepeat = GrowthGoalRules::RepeatCap(Entry.Row) > 1;
		const bool bBestRepeat = GrowthGoalRules::RepeatCap(Best->Row) > 1;
		const bool bPrefer = bReady != bBestReady ? bReady
			: bRepeat != bBestRepeat ? !bRepeat
			: bRepeat && Entry.ClaimCount != Best->ClaimCount ? Entry.ClaimCount < Best->ClaimCount
			: Entry.Row.RecommendationPriority < Best->Row.RecommendationPriority;
		if (bPrefer) { Best = &Entry; }
	}
	return Best ? Best->GoalID : NAME_None;
}

FName SelectAfterClaim(const TArray<FGoalBoardEntry>& Entries, FName ClaimedGoalID, int32 PreviousClaimCount)
{
	if (PreviousClaimCount == 0 && !ClaimedGoalID.IsNone())
	{
		TArray<FGoalBoardEntry> DirectSuccessors;
		for (const FGoalBoardEntry& Entry : Entries)
		{
			if (Entry.Row.PrereqGoalID == ClaimedGoalID && GrowthGoalRules::RepeatCap(Entry.Row) == 1
				&& Entry.bRecommendable
				&& (Entry.State == EGoalState::InProgress || Entry.State == EGoalState::ReadyToClaim))
			{
				DirectSuccessors.Add(Entry);
			}
		}
		const FName SuccessorID = Select(DirectSuccessors);
		if (!SuccessorID.IsNone()) { return SuccessorID; }
	}
	return Select(Entries);
}

FName SelectForRefresh(const TArray<FGoalBoardEntry>& Entries, FName ActiveGoalID,
	const TArray<FName>& LastAvailableGoalIDs)
{
	const FGoalBoardEntry* Active = Entries.FindByPredicate([ActiveGoalID](const FGoalBoardEntry& Entry)
		{ return Entry.GoalID == ActiveGoalID && Entry.bRecommendable
			&& (Entry.State == EGoalState::InProgress || Entry.State == EGoalState::ReadyToClaim); });
	if (!Active) { return Select(Entries); }
	if (Active->State == EGoalState::ReadyToClaim || GrowthGoalRules::RepeatCap(Active->Row) <= 1
		|| LastAvailableGoalIDs.IsEmpty()) { return ActiveGoalID; }

	TArray<FGoalBoardEntry> NewMilestones;
	for (const FGoalBoardEntry& Entry : Entries)
	{
		if (Entry.bRecommendable && (Entry.State == EGoalState::InProgress || Entry.State == EGoalState::ReadyToClaim)
			&& GrowthGoalRules::RepeatCap(Entry.Row) == 1 && !LastAvailableGoalIDs.Contains(Entry.GoalID))
		{
			NewMilestones.Add(Entry);
		}
	}
	const FName NewMilestoneID = Select(NewMilestones);
	return NewMilestoneID.IsNone() ? ActiveGoalID : NewMilestoneID;
}
}

FName UGoalBoardSubsystem::GetRecommendedGoalID() const
{
	TArray<FGoalBoardEntry> Entries;
	GetBoardEntries(Entries);
	const FGoalBoardEntry* Active = Entries.FindByPredicate([this](const FGoalBoardEntry& Entry)
		{ return Entry.GoalID == ActiveGuideGoalID && Entry.bRecommendable; });
	return Active ? Active->GoalID : CGRGrowthRecommendation::Select(Entries);
}

bool UGoalBoardSubsystem::SetActiveGoal(FName GoalID)
{
	TArray<FGoalBoardEntry> Entries;
	GetBoardEntries(Entries);
	const FGoalBoardEntry* Entry = Entries.FindByPredicate([GoalID](const FGoalBoardEntry& Candidate)
		{ return Candidate.GoalID == GoalID && Candidate.bRecommendable; });
	if (!Entry) { return false; }
	ActiveGuideGoalID = GoalID;
	if (TrackedGoalID != GoalID) { SetTrackedGoal(NAME_None); }
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->RequestDeferredSave();
	}
	OnGoalBoardChanged.Broadcast();
	return true;
}

bool UGoalBoardSubsystem::ClaimGoal(FName GoalID, int32 ExpectedClaimCount)
{
	if (!bUnlocked || bClaimInProgress || ExpectedClaimCount < 0) { return false; }
	UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	USaveGame_GameData* CachedSave = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	FGoalTable Row;
	const int32 Count = GoalSuccessCountsByID.FindRef(GoalID);
	if (!Tables || !Resources || !CachedSave || !Tables->GetGoalData(GoalID, Row)
		|| Count != ExpectedClaimCount || GetGoalState(GoalID, Row) != EGoalState::ReadyToClaim
		|| Row.RewardMoney <= 0) { return false; }
	const int64 MoneyBefore = Resources->GetResourceAmount(EResourceType::Money);
	if (MoneyBefore < 0 || Row.RewardMoney > MAX_int64 - MoneyBefore) { return false; }

	// 자연 생산 진행은 보상 트랜잭션의 롤백 범위 밖에서 먼저 정산한다.
	if (UManufacturingDevelopmentManager* Development = GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>())
	{
		Development->AccrueOnline();
	}
	FGameSaveData Candidate;
	if (!SaveMgr->BuildCurrentSnapshot(Candidate))
	{
		UE_LOG(LogTemp, Error, TEXT("[GoalBoard] Claim save failed; restored %s"), *GoalID.ToString());
		return false;
	}
	const TMap<EResourceType, int64> ResourcesBefore = Resources->GetAllResources();
	const TArray<FName> CompletedBefore = CompletedGoalIDs;
	const TArray<FName> ReadyBefore = ReadyGoalIDs;
	const TMap<FName, int64> ProgressBefore = GoalEventProgressByID;
	const TMap<FName, int32> CountsBefore = GoalSuccessCountsByID;
	const FName ActiveBefore = ActiveGuideGoalID;
	bClaimInProgress = true;

	GoalSuccessCountsByID.Add(GoalID, Count + 1);
	CompletedGoalIDs.AddUnique(GoalID);
	ReadyGoalIDs.Remove(GoalID);
	if (Row.bCountActions)
	{
		const int64 Remaining = FMath::Max<int64>(0,
			GoalEventProgressByID.FindRef(GoalID) - GrowthGoalRules::TargetForClaim(Row, Count));
		GoalEventProgressByID.Add(GoalID, Remaining);
	}
	else { GoalEventProgressByID.Remove(GoalID); }
	TMap<EResourceType, int64> RewardResources = ResourcesBefore;
	RewardResources.FindOrAdd(EResourceType::Money) += Row.RewardMoney;
	Resources->SetAllResources(RewardResources, false);
	EvaluateAllGoals(false, false);
	TArray<FGoalBoardEntry> Entries;
	GetBoardEntries(Entries);
	ActiveGuideGoalID = CGRGrowthRecommendation::SelectAfterClaim(Entries, GoalID, Count);

	// 보상과 수령 회차를 같은 스냅샷에 저장한다. 저장 실패 때 UI에는 지급을 게시하지 않는다.
	Candidate.ResourceBank = Resources->GetAllResources();
	CollectSaveData(Candidate);
	if (!SaveMgr->CommitPreparedSnapshot(Candidate))
	{
		CompletedGoalIDs = CompletedBefore;
		ReadyGoalIDs = ReadyBefore;
		GoalEventProgressByID = ProgressBefore;
		GoalSuccessCountsByID = CountsBefore;
		ActiveGuideGoalID = ActiveBefore;
		Resources->SetAllResources(ResourcesBefore, false);
		bClaimInProgress = false;
		UE_LOG(LogTemp, Error, TEXT("[GoalBoard] Claim save failed; restored %s"), *GoalID.ToString());
		return false;
	}
	bClaimInProgress = false;
	if (TrackedGoalID == GoalID) { SetTrackedGoal(NAME_None); }
	Resources->OnResourceChanged.Broadcast(EResourceType::Money, Resources->GetResourceAmount(EResourceType::Money));
	RefreshRecommendation();
	OnGoalBoardChanged.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("[GoalBoard] Claimed %s cycle=%d reward=%lld next=%s"),
		*GoalID.ToString(), Count + 1, Row.RewardMoney, *ActiveGuideGoalID.ToString());
	return true;
}

void UGoalBoardSubsystem::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	RefreshRecommendation();
}

void UGoalBoardSubsystem::RefreshRecommendation()
{
	if (!bLoadedFromSave || !bUnlocked || bClaimInProgress) { return; }
	const FName PreviousActive = ActiveGuideGoalID;
	TArray<FGoalBoardEntry> Entries;
	GetBoardEntries(Entries);
	ActiveGuideGoalID = CGRGrowthRecommendation::SelectForRefresh(Entries, ActiveGuideGoalID, LastAvailableGoalIDs);
	if (PreviousActive != ActiveGuideGoalID)
	{
		if (!TrackedGoalID.IsNone() && TrackedGoalID != ActiveGuideGoalID) { SetTrackedGoal(NAME_None); }
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->RequestDeferredSave();
		}
	}
	TArray<FName> Available;
	for (const FGoalBoardEntry& Entry : Entries)
	{
		if (Entry.bRecommendable) { Available.Add(Entry.GoalID); }
	}
	if (LastRecommendedGoalID != ActiveGuideGoalID || LastAvailableGoalIDs != Available)
	{
		LastRecommendedGoalID = ActiveGuideGoalID;
		LastAvailableGoalIDs = MoveTemp(Available);
		OnGoalBoardChanged.Broadcast();
	}
}

bool UGoalBoardSubsystem::IsGoalAvailable(const FGoalTable& Row) const
{
	if (Row.RequiredHQLevel <= 1 && Row.RequiredProjectIndex <= 0
		&& Row.ConditionType != EMissionConditionType::BuildIndustry) { return true; }
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData || SaveMgr->GetHQLevel() < Row.RequiredHQLevel) { return false; }
	if (Row.ConditionType == EMissionConditionType::BuildIndustry
		&& (!SaveMgr->IsIndustryAvailable(Row.CompanyType) || !SaveMgr->IsIndustryUnlocked(Row.CompanyType)))
	{
		return false;
	}
	return Row.RequiredProjectIndex <= 0
		|| GrowthGoalRules::HasShipped(SaveData->GameData, Row.CompanyType, Row.RequiredProjectIndex);
}

namespace CGRGrowthReadiness
{
bool IsNextHQLevelGoal(int32 CurrentHQLevel, int64 TargetHQLevel, bool bHasNextHQLevel)
{
	return bHasNextHQLevel && TargetHQLevel == static_cast<int64>(CurrentHQLevel) + 1;
}

bool IsFullAtPositiveCapacity(int32 EmployeeCount, int32 Capacity)
{
	return Capacity > 0 && EmployeeCount >= Capacity;
}

bool FloorReady(const FBuildingEntitySaveData& Building,
	const UTableManagerSubsystem& Tables, const UResourceItemManager& Resources, bool bRequireFunds)
{
	FBuildingEnhancementDefinition Definition;
	if (!Tables.GetEnhancementDefinition(EBuildingEnhancementType::BuildingFloor, Definition)) { return false; }
	const int32 Level = Building.BuildingData.Body_Module_Copies;
	if (Definition.MaxLevel > 0 && Level >= Definition.MaxLevel) { return false; }
	FBuildableCardTable Card;
	if (!Tables.GetBuildableInfo(Building.InteractableName, Card)) { return false; }
	int64 BaseBuildCost = 0;
	for (const FConstructionCost& Cost : Card.ConstructionCosts)
	{
		if (Cost.ResourceType == EResourceType::Brick) { BaseBuildCost = Cost.Cost; break; }
	}
	if (BaseBuildCost <= 0) { return false; }
	const int64 UpgradeCost = UBuildingEnhancementHelper::CalculateUpgradeCost(
		EBuildingEnhancementType::BuildingFloor, Level, BaseBuildCost);
	return UpgradeCost > 0 && (!bRequireFunds
		|| Resources.GetResourceAmount(Definition.CostResourceType) >= UpgradeCost);
}

bool DeskReady(const FWorkstationSaveData& Desk, bool bPromotion,
	const UTableManagerSubsystem& Tables, int64 Money)
{
	if (bPromotion)
	{
		EComputerSetupLevel Next;
		if (!TryGetNextComputerSetupLevel(Desk.ComputerSetupLevel, Next)
			|| !Tables.FindWorkstationSetupLevelData(Next)) { return false; }
	}
	for (uint8 Index = 0; Index < static_cast<uint8>(EWorkstationUpgradeType::Count); ++Index)
	{
		const EWorkstationUpgradeType Type = static_cast<EWorkstationUpgradeType>(Index);
		const FWorkstationUpgradeDefinition* Definition = Tables.FindWorkstationUpgradeDefinition(Type);
		if (!Definition || !Definition->IsValidConfiguration()) { return false; }
		const int32 Level = FMath::Max(1, Desk.UpgradeLevels.FindRef(Type));
		if (bPromotion)
		{
			if (Level != Definition->MaxLevel) { return false; }
		}
		else
		{
			FWorkstationUpgradeQuote Quote;
			if (WorkstationUpgradeRules::BuildQuote(*Definition, Desk.ComputerSetupLevel, Level, 1, false, Money, Quote)
				&& Quote.bValid && Quote.Count > 0 && Quote.TotalCost <= Money) { return true; }
		}
	}
	return bPromotion;
}

bool HasBuildablePlot(UWorld* World, bool bRequireClearedCompany)
{
	if (!World) { return false; }
	const USpawnManager* Spawn = World->GetSubsystem<USpawnManager>();
	const UEntityManager* Entities = World->GetSubsystem<UEntityManager>();
	const UCityAcquisitionManager* Acquisitions = World->GetSubsystem<UCityAcquisitionManager>();
	if (!Spawn || !Entities || (bRequireClearedCompany && !Acquisitions)) { return false; }
	TArray<FName> OwnedPlotIDs;
	Spawn->GatherOwnedPlotIds(OwnedPlotIDs);
	for (const FName PlotID : OwnedPlotIDs)
	{
		const ACityPlotActor* Plot = Spawn->GetSpawnedPlotById(PlotID);
		if (!IsValid(Plot) || !Plot->IsOwned()
			|| !Entities->HasFreeCapacityOnPlot(PlotID, Plot->GetBuildingCapacity())) { continue; }
		if (bRequireClearedCompany)
		{
			TArray<int32> Occupants;
			Acquisitions->GetOccupantKeysForPlot(PlotID, Occupants);
			if (Occupants.IsEmpty() || !Acquisitions->ArePlotOccupantsCleared(PlotID)) { continue; }
		}
		return true;
	}
	return false;
}

bool HasAffordableBuilding(const UTableManagerSubsystem& Tables,
	const UResourceItemManager& Resources, int32 HQLevel, int32 OwnedBuildings)
{
	TArray<FBuildableCardTable> Cards = Tables.GetBuildableInfos();
	UBuildModalWidget::ApplyBuildCostProgression(Cards, OwnedBuildings);
	for (const FBuildableCardTable& Card : Cards)
	{
		bool bHasBuildingData = false;
		const FBuildingData Definition = Tables.GetBuildingData(Card.RowName, bHasBuildingData);
		if (!bHasBuildingData || HQLevel < Definition.RequiredHQLevel) { continue; }
		EResourceType Missing = EResourceType::None;
		if (Resources.CanAffordCosts(Card.ConstructionCosts, Missing)) { return true; }
	}
	return false;
}

bool HasActionableCityCompany(UWorld* World, const UTableManagerSubsystem& Tables,
	bool bDemolish)
{
	if (!World) { return false; }
	const UCityAcquisitionManager* Acquisitions = World->GetSubsystem<UCityAcquisitionManager>();
	if (!Acquisitions) { return false; }
	TArray<FName> PlotIDs;
	Tables.GetAllCityPlotRows(PlotIDs);
	TSet<int32> SeenKeys;
	for (const FName PlotID : PlotIDs)
	{
		TArray<int32> Keys;
		Acquisitions->GetOccupantKeysForPlot(PlotID, Keys);
		for (const int32 Key : Keys)
		{
			if (SeenKeys.Contains(Key)) { continue; }
			SeenKeys.Add(Key);
			if (!IsValid(Acquisitions->GetCompanyActor(Key))) { continue; }
			if (bDemolish)
			{
				const EAcqState State = Acquisitions->GetState(Key);
				if (State == EAcqState::Milking || State == EAcqState::Depleted) { return true; }
			}
			else if (Acquisitions->CanAcquire(Key)) { return true; }
		}
	}
	return false;
}

bool HasAffordablePainting(const UTableManagerSubsystem& Tables,
	const UResourceItemManager& Resources)
{
	for (const FDecorationCardTable& Card : Tables.GetDecorationCardsByCategory(EDecorationCategory::Picture))
	{
		if (Card.AllowedSurface == EDecorationSurface::Wall && Card.Price >= 0
			&& Resources.HasResource(EResourceType::Money, Card.Price)) { return true; }
	}
	return false;
}

bool HasRecruitmentNextStep(int32 BuildingIndex, const FGameSaveData& Data,
	const UEmployeeManager& Employees, const UTableManagerSubsystem& Tables,
	const UResourceItemManager& Resources)
{
	const FBuildingEntitySaveData* Building = Data.Buildings.FindByPredicate(
		[BuildingIndex](const FBuildingEntitySaveData& Candidate)
		{ return Candidate.BuildingIndex == BuildingIndex; });
	if (!Building) { return false; }
	if (!Employees.CanHireIntoBuilding(BuildingIndex))
	{
		return IsFullAtPositiveCapacity(
			Employees.GetEmployeeCountInBuilding(BuildingIndex),
			Employees.GetBuildingEmployeeCapacity(BuildingIndex, false))
			&& FloorReady(*Building, Tables, Resources, false);
	}
	if (const FOfficeSaveData* Office = Data.OfficeDataMap.Find(BuildingIndex))
	{
		for (const FWorkstationSaveData& Desk : Office->OfficeWorkstations)
		{
			if (Desk.ChairStates.ContainsByPredicate([](const FChairSlotState& Chair)
				{ return Chair.OccupantEmployeeID < 0; })) { return true; }
		}
	}
	// 책상은 구매비가 없다. 카드가 없으면 도크의 배치 경로를 시작할 수 없다.
	return Tables.GetWorkstationCardInfos().ContainsByPredicate([](const FWorkstationCardTable& Card)
		{ return !Card.BlueprintClass.IsNull(); });
}

}

EProductionDiscipline UGoalBoardSubsystem::GetGuidedRecruitDiscipline(int32 BuildingIndex) const
{
	if (!bUnlocked || TrackedGoalID.IsNone() || BuildingIndex == INDEX_NONE)
	{
		return EProductionDiscipline::Count;
	}
	const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FGoalTable Row;
	if (!Tables || !Tables->GetGoalData(TrackedGoalID, Row)
		|| Row.ConditionType != EMissionConditionType::RecruitGoalDiscipline
		|| Row.RequiredDiscipline >= EProductionDiscipline::Count
		|| GetGoalState(TrackedGoalID, Row) != EGoalState::InProgress
		|| GetGuideBuildingIndex(TrackedGoalID) != BuildingIndex)
	{
		return EProductionDiscipline::Count;
	}
	return Row.RequiredDiscipline;
}

int32 UGoalBoardSubsystem::GetGuideBuildingIndex(FName GoalID) const
{
	const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	const UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	FGoalTable Row;
	if (!Tables || !SaveData || !Employees || !Resources || !Tables->GetGoalData(GoalID, Row)) { return INDEX_NONE; }
	// 진행 중인 채용 안내는 채용권·정원 변동으로 다른 회사로 옮기지 않는다.
	if (GoalID == TrackedGoalID && Row.ConditionType == EMissionConditionType::RecruitGoalDiscipline)
	{
		const FBuildingEntitySaveData* TrackedBuilding = SaveData->GameData.Buildings.FindByPredicate(
			[this](const FBuildingEntitySaveData& Building)
			{ return Building.BuildingIndex == TrackedRecruitBuildingIndex; });
		if (TrackedBuilding && GrowthGoalRules::MatchesCompany(Row, *TrackedBuilding, SaveData->GameData))
		{
			return TrackedRecruitBuildingIndex;
		}
	}
	const int64 Money = Resources->GetResourceAmount(EResourceType::Money);
	if (Row.ConditionType == EMissionConditionType::UpgradeFirstWorkstation
		|| Row.ConditionType == EMissionConditionType::PromoteFirstWorkstation)
	{
		const UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
		if (Game && Game->IsInOfficeMap())
		{
			const int32 CurrentBuildingIndex = Game->GetCurrentManagedBuildingIndex();
			if (const FOfficeSaveData* CurrentOffice = SaveData->GameData.OfficeDataMap.Find(CurrentBuildingIndex))
			{
				for (const FWorkstationSaveData& Desk : CurrentOffice->OfficeWorkstations)
				{
					if (CGRGrowthReadiness::DeskReady(Desk,
						Row.ConditionType == EMissionConditionType::PromoteFirstWorkstation, *Tables, Money))
					{
						return CurrentBuildingIndex;
					}
				}
			}
		}
	}
	for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
	{
		const int32 BuildingIndex = Building.BuildingIndex;
		switch (Row.ConditionType)
		{
		case EMissionConditionType::ReserveManufacturingProduct:
			if (GrowthGoalRules::MatchesCompany(Row, Building, SaveData->GameData)) { return BuildingIndex; }
			break;
		case EMissionConditionType::EnhanceBuildingStat:
		{
			const TArray<EBuildingEnhancementType> Unlocked = Tables->GetUnlockedEnhancementsUpToTier(SaveMgr->GetBuildingTier(BuildingIndex));
			for (const FBuildingEnhancementDefinition& Definition : Tables->GetEnhancementsForCompanyType(Building.BuildingData.CompanyType))
			{
				const EBuildingEnhancementType Type = Definition.EnhancementType;
				if (Type == EBuildingEnhancementType::BuildingFloor || !Unlocked.Contains(Type)) { continue; }
				const int32 Level = Building.BuildingData.EnhancementLevels.FindRef(Type);
				if (Definition.MaxLevel > 0 && Level >= Definition.MaxLevel) { continue; }
				const int64 Cost = UBuildingEnhancementHelper::CalculateUpgradeCost(Type, Level);
				if (Cost > 0 && Resources->GetResourceAmount(Definition.CostResourceType) >= Cost) { return BuildingIndex; }
			}
			break;
		}
		case EMissionConditionType::RaiseBuildingFloor:
			if (CGRGrowthReadiness::IsFullAtPositiveCapacity(
				Employees->GetEmployeeCountInBuilding(BuildingIndex),
				Employees->GetBuildingEmployeeCapacity(BuildingIndex, false))
				&& CGRGrowthReadiness::FloorReady(Building, *Tables, *Resources, true)) { return BuildingIndex; }
			break;
		case EMissionConditionType::LevelUpFirstEmployee:
		case EMissionConditionType::AllocateEmployeeStatPoint:
		case EMissionConditionType::EnhanceFirstEmployee:
			for (const FEmployeeInstance& Employee : Employees->GetAllEmployeesRef())
			{
				if (Employee.AssignedBuildingIndex != BuildingIndex) { continue; }
				if ((Row.ConditionType == EMissionConditionType::LevelUpFirstEmployee
					&& Employees->CanLevelUpEmployee(Employee.EmployeeID))
					|| (Row.ConditionType == EMissionConditionType::AllocateEmployeeStatPoint
						&& Employees->CanAllocateAnyStatPoint(Employee.EmployeeID))
					|| (Row.ConditionType == EMissionConditionType::EnhanceFirstEmployee
						&& Employee.EnhancementLevel < UEmployeeManager::MaxEnhancementLevel
						&& Money >= UEmployeeManager::GetEnhanceCost(Employee.EnhancementLevel))) { return BuildingIndex; }
			}
			break;
		case EMissionConditionType::UpgradeFirstWorkstation:
		case EMissionConditionType::PromoteFirstWorkstation:
			if (const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex))
			{
				for (const FWorkstationSaveData& Desk : Office->OfficeWorkstations)
				{
					if (CGRGrowthReadiness::DeskReady(Desk,
						Row.ConditionType == EMissionConditionType::PromoteFirstWorkstation, *Tables, Money)) { return BuildingIndex; }
				}
			}
			break;
		case EMissionConditionType::RecruitGoalDiscipline:
		{
			const URecruitmentManagerSubsystem* Recruitment = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();
			if (Employees->CanHireIntoBuilding(BuildingIndex)
				&& (!Recruitment || !Recruitment->CanExecuteGachaPull(EGachaTier::Normal))) { break; }
			if (GrowthGoalRules::MatchesCompany(Row, Building, SaveData->GameData)
				&& CGRGrowthReadiness::HasRecruitmentNextStep(BuildingIndex,
					SaveData->GameData, *Employees, *Tables, *Resources)) { return BuildingIndex; }
			break;
		}
		case EMissionConditionType::LaunchGoalProject:
		{
			if (!GrowthGoalRules::MatchesCompany(Row, Building, SaveData->GameData)
				|| Row.TargetProjectIndex <= 0) { break; }
			bool bProjectFound = false;
			const FProjectData Project = Tables->GetProjectData(Row.CompanyType, Row.TargetProjectIndex, bProjectFound);
			const FOfficeSaveData* Office = SaveData->GameData.OfficeDataMap.Find(BuildingIndex);
			if (!bProjectFound || !Office || Office->bHasPendingReport) { break; }
			if (!GrowthGoalRules::HasProjectStaff(Project, BuildingIndex, Employees->GetAllEmployeesRef())) { break; }
			const int32 Tier = FTierLayout::GetTierForProject(Row.TargetProjectIndex);
			if (Tier <= 0 || !Office->TierProgress.IsTierUnlocked(Tier)) { break; }
			const UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
			if (Game && Game->IsInOfficeMap()
				&& Game->GetCurrentManagedBuildingIndex() == BuildingIndex)
			{
				const UOfficeStageProgressManager* Stage = GetWorld()
					? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
				if (!Stage || (Stage->GetLifecycle() != EProjectLifecycle::Idle
					&& Stage->GetProgressData().ProjectID != Row.TargetProjectIndex)) { break; }
			}
			else if (Office->StageProgress.CurrentStep != 0
				&& Office->StageProgress.ProjectID != Row.TargetProjectIndex) { break; }
			return BuildingIndex;
		}
		default:
			break;
		}
	}
	return INDEX_NONE;
}

bool UGoalBoardSubsystem::IsGoalReady(FName GoalID, const FGoalTable& Row) const
{
	// 안내할 대상과 추천 준비 여부가 같은 판정을 사용한다.
	switch (Row.ConditionType)
	{
	case EMissionConditionType::LevelUpFirstEmployee:
	case EMissionConditionType::AllocateEmployeeStatPoint:
	case EMissionConditionType::EnhanceFirstEmployee:
	case EMissionConditionType::UpgradeFirstWorkstation:
	case EMissionConditionType::PromoteFirstWorkstation:
	case EMissionConditionType::RecruitGoalDiscipline:
	case EMissionConditionType::LaunchGoalProject:
	case EMissionConditionType::EnhanceBuildingStat:
	case EMissionConditionType::ReserveManufacturingProduct:
		return GetGuideBuildingIndex(GoalID) != INDEX_NONE;
	case EMissionConditionType::UpgradeFactoryStats:
	{
		const ABrickFactory* Factory = Cast<ABrickFactory>(UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass()));
		return Factory && Factory->CanUpgradeAny();
	}
	case EMissionConditionType::ReachFactoryProductionLevel:
	{
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		if (SaveData && !SaveData->GameData.FactoryData.IsEmpty()) { return true; }
		const ABrickFactory* Factory = Cast<ABrickFactory>(UGameplayStatics::GetActorOfClass(GetWorld(), ABrickFactory::StaticClass()));
		return Factory && Factory->GetAllowedUpgradeCount(EFactoryUpgradeType::HoldProductionAmount) > 0;
	}
	case EMissionConditionType::CollectMineResource:
	case EMissionConditionType::CollectFactoryProduct:
	case EMissionConditionType::SellManufacturedProduct:
	{
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		return SaveMgr && SaveMgr->GetHQLevel() >= WorldMapConstants::WorldMapRequiredHQLevel;
	}
	case EMissionConditionType::ReachHQLevel:
	{
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		return SaveMgr && CGRGrowthReadiness::IsNextHQLevelGoal(
			SaveMgr->GetHQLevel(), Row.ConditionAmount, SaveMgr->HasNextHQLevel());
	}
	case EMissionConditionType::RaiseBuildingFloor:
		return GetGuideBuildingIndex(GoalID) != INDEX_NONE;
	case EMissionConditionType::BuildSecondBuilding:
	case EMissionConditionType::BuildOnClearedPlot:
	case EMissionConditionType::BuildIndustry:
	{
		UWorld* CurrentWorld = GetWorld();
		if (!CGRGrowthReadiness::HasBuildablePlot(CurrentWorld,
			Row.ConditionType == EMissionConditionType::BuildOnClearedPlot)) { return false; }
		const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		const UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>();
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		return Tables && Resources && SaveData && CGRGrowthReadiness::HasAffordableBuilding(
			*Tables, *Resources, SaveMgr->GetHQLevel(), SaveData->GameData.Buildings.Num());
	}
	case EMissionConditionType::AcquireFirstCompany:
	case EMissionConditionType::DemolishFirstCompany:
	{
		const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		return Tables && CGRGrowthReadiness::HasActionableCityCompany(GetWorld(), *Tables,
			Row.ConditionType == EMissionConditionType::DemolishFirstCompany);
	}
	case EMissionConditionType::AcquireFirstPlot:
	{
		UWorld* CurrentWorld = GetWorld();
		const USpawnManager* Spawn = CurrentWorld ? CurrentWorld->GetSubsystem<USpawnManager>() : nullptr;
		if (!Spawn) { return false; }
		TArray<ACityPlotActor*> Plots;
		Spawn->GetAdjacentUnownedPlots(Plots);
		return Plots.ContainsByPredicate([](const ACityPlotActor* Plot)
			{ return IsValid(Plot) && Plot->CanBePurchasedNow(); });
	}
	case EMissionConditionType::PlaceFirstPainting:
	{
		const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		const UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>();
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		return Tables && Resources && SaveData && !SaveData->GameData.Buildings.IsEmpty()
			&& CGRGrowthReadiness::HasAffordablePainting(*Tables, *Resources);
	}
	case EMissionConditionType::EquipFirstSkin:
	{
		const UBuildingSkinManagerSubsystem* Skins = GetGameInstance()->GetSubsystem<UBuildingSkinManagerSubsystem>();
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		return Skins && SaveData && !SaveData->GameData.Buildings.IsEmpty()
			&& (Skins->HasAnyGachaSkin() || Skins->CanExecuteGachaPull(false)
				|| Skins->CanExecuteGachaPull(true));
	}
	case EMissionConditionType::EquipFirstTrait:
	{
		const UBuildingTraitManagerSubsystem* Traits = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
		USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
		const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
		if (!Traits || !SaveData) { return false; }
		bool bHasEquipSlot = false;
		for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
		{
			if (Traits->GetUnlockedSlotCount(Building.BuildingIndex) <= 0) { continue; }
			bHasEquipSlot = true;
			for (const TPair<FName, int32>& Owned : Traits->GetAllOwnedTraits())
			{
				if (Owned.Value > 0 && Traits->CanEquipTraitToBuilding(Building.BuildingIndex, Owned.Key))
				{
					return true;
				}
			}
		}
		return bHasEquipSlot
			&& (Traits->CanExecuteGachaPull(false) || Traits->CanExecuteGachaPull(true));
	}
	default:
		return false;
	}
}

void UGoalBoardSubsystem::DismissGoal(FName GoalID)
{
	if (GoalID.IsNone()) { return; }
	if (DismissedGoalIDs.Contains(GoalID)) { return; }
	DismissedGoalIDs.Add(GoalID);
	if (TrackedGoalID == GoalID) { SetTrackedGoal(NAME_None); }
	LastRecommendedGoalID = GetRecommendedGoalID();
	OnGoalBoardChanged.Broadcast();
}

void UGoalBoardSubsystem::DevCompleteGoal(FName GoalID)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FGoalTable Row;
	if (TableMgr && TableMgr->GetGoalData(GoalID, Row)
		&& GrowthGoalRules::CanRecordSuccess(Row, GoalSuccessCountsByID.FindRef(GoalID)))
	{
		if (Row.bCountActions)
		{
			GoalEventProgressByID.Add(GoalID, GrowthGoalRules::TargetForClaim(Row, GoalSuccessCountsByID.FindRef(GoalID)));
		}
		ReadyGoalIDs.AddUnique(GoalID);
		if (TrackedGoalID == GoalID) { SetTrackedGoal(NAME_None); }
		OnGoalBoardChanged.Broadcast();
		// 래치만 하면 치트 직후 종료 시 유실 — 정규 판정 경로와 동일하게 즉시 영속화
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
}

void UGoalBoardSubsystem::DevResetGoals()
{
	bUnlocked = false;
	CompletedGoalIDs.Reset();
	GoalEventProgressByID.Reset();
	GoalSuccessCountsByID.Reset();
	ReadyGoalIDs.Reset();
	ActiveGuideGoalID = NAME_None;
	DismissedGoalIDs.Reset();
	// 리셋 후 stale 추적이 남으면 잠긴 미션을 계속 점등한다
	SetTrackedGoal(NAME_None);
	OnGoalBoardChanged.Broadcast();
	// 저장하지 않으면 디스크에 구 상태가 남아 다음 로드에서 되살아난다
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}

void UGoalBoardSubsystem::SetTrackedGoal(FName GoalID)
{
	if (TrackedGoalID == GoalID)
	{
		return;
	}

	int32 RecruitBuildingIndex = INDEX_NONE;
	if (!GoalID.IsNone())
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		FGoalTable Row;
		if (!TableMgr || !TableMgr->GetGoalData(GoalID, Row))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GoalBoard] 안내 지정 실패 — DT_Goal 에 없는 행: %s"), *GoalID.ToString());
			return;
		}
		// 잠금/수령완료는 안내 대상이 아니다 (버튼 자체가 없지만 치트/재진입 방어)
		const EGoalState State = GetGoalState(GoalID, Row);
		if (State != EGoalState::InProgress)
		{
			return;
		}
		if (Row.ConditionType == EMissionConditionType::RecruitGoalDiscipline)
		{
			RecruitBuildingIndex = GetGuideBuildingIndex(GoalID);
		}
	}

	TrackedRecruitBuildingIndex = RecruitBuildingIndex;
	TrackedGoalID = GoalID;
	OnTrackedGoalChanged.Broadcast(TrackedGoalID);
}

void UGoalBoardSubsystem::SetTrackerFolded(bool bInFolded)
{
	if (bTrackerFolded == bInFolded)
	{
		return;
	}
	bTrackerFolded = bInFolded;
	OnGoalBoardChanged.Broadcast();
}

void UGoalBoardSubsystem::SetRowsExpanded(bool bInExpanded)
{
	if (bRowsExpanded == bInExpanded)
	{
		return;
	}
	bRowsExpanded = bInExpanded;
	OnGoalBoardChanged.Broadcast();
}

bool UGoalBoardSubsystem::HasAvailableGuide() const
{
	TArray<FGoalBoardEntry> Entries;
	GetBoardEntries(Entries);
	for (const FGoalBoardEntry& Entry : Entries)
	{
		if (Entry.bRecommendable)
		{
			return true;
		}
	}
	return false;
}

bool UGoalBoardSubsystem::TickReadiness(float /*DeltaTime*/)
{
	RefreshRecommendation();
	return true;
}
