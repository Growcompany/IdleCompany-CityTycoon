#include "Manager/GoalBoardSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "UI/Panel/RewardRevealPresentationWidget.h"
#include "Enum/WidgetType.h"
#include "Data/CountedGoalProgressRules.h"
#include "Data/GameSaveData.h"
#include "Data/GoalBoardLoadEvaluationRules.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UGoalBoardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 구독 대상은 InitializeDependency 로 강제 — 없으면 init 순서상 null 구독 스킵 (GDD_TUTORIAL §4 함정)
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UMissionManagerSubsystem>();
	Collection.InitializeDependency<UResourceItemManager>();
	Collection.InitializeDependency<UItemInventoryManager>();

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->OnConditionSignal.AddUObject(this, &UGoalBoardSubsystem::HandleConditionSignal);
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UGoalBoardSubsystem::HandleGameDataLoaded);
	}
}

void UGoalBoardSubsystem::Deinitialize()
{
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
	ClaimedGoalIDs = SaveData->GameData.ClaimedGoalIDs;
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
			if (State == EGoalState::Claimed || State == EGoalState::Locked)
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
	OutData.bGoalBoardUnlocked = bUnlocked;
	OutData.CompletedGoalIDs = CompletedGoalIDs;
	OutData.GoalEventProgressByID = GoalEventProgressByID;
	OutData.ClaimedGoalIDs = ClaimedGoalIDs;
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
	ClaimedGoalIDsBeforePreparedUnlock = ClaimedGoalIDs;
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
	ClaimedGoalIDs = MoveTemp(ClaimedGoalIDsBeforePreparedUnlock);
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
	ClaimedGoalIDsBeforePreparedUnlock.Reset();
	OnGoalBoardChanged.Broadcast();
}

void UGoalBoardSubsystem::HandleConditionSignal(EMissionConditionType Type)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}
	bool bChanged = false;
	bool bCompleted = false;
	for (const auto& Pair : TableMgr->GetAllGoalData())
	{
		if (Pair.Value.ConditionType != Type || CompletedGoalIDs.Contains(Pair.Key))
		{
			continue;
		}

		if (Type == EMissionConditionType::RaiseBuildingFloor)
		{
			const FCountedGoalProgressDecision Decision = FCountedGoalProgressRules::ApplySignal(
				GoalEventProgressByID.FindRef(Pair.Key), Pair.Value.ConditionAmount, /*bAlreadyCompleted=*/false);
			if (!Decision.bChanged)
			{
				continue;
			}

			GoalEventProgressByID.Add(Pair.Key, Decision.Current);
			bChanged = true;
			if (Decision.bReachedThisSignal)
			{
				CompletedGoalIDs.Add(Pair.Key);
				bCompleted = true;
				UE_LOG(LogTemp, Log, TEXT("[GoalBoard] 미션 충족: %s"), *Pair.Key.ToString());
			}
		}
		else
		{
			// 단발 이벤트는 Claimable 로 래치만 한다. 보상은 보드에서 플레이어가 직접 수령한다.
			const bool bGoalCompleted = EvaluateGoal(
				Pair.Key, Pair.Value, /*bFromEvent=*/true, /*bShouldBroadcast=*/false);
			bChanged |= bGoalCompleted;
			bCompleted |= bGoalCompleted;
		}
	}
	if (!bChanged)
	{
		return;
	}

	OnGoalBoardChanged.Broadcast();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (bCompleted)
		{
			SaveMgr->SaveGameData();
		}
		else
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
	if (CompletedGoalIDs.Contains(GoalID))
	{
		return false;
	}
	int64 Current = 0, Target = 0;
	const bool bProgressType = GetConditionProgress(GoalID, Row, Current, Target);
	const bool bSatisfied = bProgressType ? (Current >= Target) : bFromEvent;
	if (!bSatisfied)
	{
		return false;
	}
	CompletedGoalIDs.Add(GoalID);
	if (bShouldBroadcast)
	{
		OnGoalBoardChanged.Broadcast();
	}
	UE_LOG(LogTemp, Log, TEXT("[GoalBoard] 미션 충족: %s"), *GoalID.ToString());
	return true;
}

bool UGoalBoardSubsystem::GetConditionProgress(
	FName GoalID,
	const FGoalTable& Row,
	int64& OutCurrent,
	int64& OutTarget) const
{
	switch (Row.ConditionType)
	{
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
	if (ClaimedGoalIDs.Contains(GoalID))
	{
		return EGoalState::Claimed;
	}
	if (!Row.PrereqGoalID.IsNone() && !ClaimedGoalIDs.Contains(Row.PrereqGoalID))
	{
		return EGoalState::Locked;
	}
	return CompletedGoalIDs.Contains(GoalID) ? EGoalState::Claimable : EGoalState::InProgress;
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
		GetConditionProgress(GoalID, Row, Entry.ProgressCurrent, Entry.ProgressTarget);
		OutEntries.Add(MoveTemp(Entry));
	}
}

bool UGoalBoardSubsystem::ClaimGoal(FName GoalID)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FGoalTable Row;
	if (!TableMgr || !TableMgr->GetGoalData(GoalID, Row))
	{
		return false;
	}
	if (GetGoalState(GoalID, Row) != EGoalState::Claimable)
	{
		return false;
	}

	// 지급 — CompleteActiveMission 의 보상 루프와 동형 (자원 StoreResource / 아이템 AddItem)
	// bShouldSave=false — 각 지급 호출의 기본 즉시저장을 끄고 아래 수령 플래그 저장 1회로 통합 (보상 N개=세이브 N+1회 방지, 지급됐지만 미수령인 상태가 디스크에 남는 창 제거)
	UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	for (const FMissionReward& R : Row.Rewards)
	{
		if (ResMgr && R.ResourceType != EResourceType::None && R.Amount > 0)
		{
			ResMgr->StoreResource(R.ResourceType, R.Amount, /*bShouldSave=*/false);
		}
		if (ItemMgr && R.ItemType != EItemType::None && R.ItemAmount > 0)
		{
			ItemMgr->AddItem(R.ItemType, R.ItemAmount, /*bShouldSave=*/false);
		}
	}
	ClaimedGoalIDs.Add(GoalID);
	OnGoalBoardChanged.Broadcast();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}

	// 미션판 보상은 RewardToast로 표시한다. 튜토리얼 피날레의 RewardReveal과는 별도 경로다.
	// 빈 보상이면 생략 (ShowRewardSplash :932-935 미러)
	if (Row.Rewards.Num() > 0)
	{
		if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
		{
			TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::RewardToast);
			if (!Cls)
			{
				// loud failure — DT_WidgetClass 미등록을 조용히 삼키지 않는다
				UE_LOG(LogTemp, Warning, TEXT("[GoalBoard] 보상 토스트 위젯 미등록 — 표시 생략"));
			}
			else if (URewardRevealPresentationWidget* Toast = CreateWidget<URewardRevealPresentationWidget>(PC, Cls))
			{
				Toast->AddToViewport(10000);
				Toast->SetupRewards(Row.Rewards, Row.Title, /*bToastMode=*/true);
			}
		}
	}
	// 수령한 미션의 안내만 내린다 — 다른 미션을 안내 중일 때 그걸 꺼뜨리면 안 된다
	if (TrackedGoalID == GoalID)
	{
		SetTrackedGoal(NAME_None);
	}

	UE_LOG(LogTemp, Log, TEXT("[GoalBoard] 미션 수령: %s"), *GoalID.ToString());
	return true;
}

void UGoalBoardSubsystem::DevCompleteGoal(FName GoalID)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FGoalTable Row;
	if (TableMgr && TableMgr->GetGoalData(GoalID, Row) && !CompletedGoalIDs.Contains(GoalID))
	{
		if (Row.ConditionType == EMissionConditionType::RaiseBuildingFloor)
		{
			GoalEventProgressByID.Add(GoalID, FMath::Max<int64>(1, Row.ConditionAmount));
		}
		CompletedGoalIDs.Add(GoalID);
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
	ClaimedGoalIDs.Reset();
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
		if (State == EGoalState::Locked || State == EGoalState::Claimed)
		{
			return;
		}
	}

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

bool UGoalBoardSubsystem::ConsumeEntranceOnce()
{
	if (bEntrancePlayed || !bUnlocked)
	{
		return false;
	}
	bEntrancePlayed = true;
	return true;
}

bool UGoalBoardSubsystem::HasAnyUnclaimedGoal() const
{
	TArray<FGoalBoardEntry> Entries;
	GetBoardEntries(Entries);
	for (const FGoalBoardEntry& Entry : Entries)
	{
		if (Entry.State != EGoalState::Claimed)
		{
			return true;
		}
	}
	return false;
}
