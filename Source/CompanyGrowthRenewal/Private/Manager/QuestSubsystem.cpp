#include "Manager/QuestSubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Data/RewardGrant.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/BuildingSkinGachaSaveData.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuest, Log, All);

void UQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UGameClockSubsystem>();
	Collection.InitializeDependency<UResourceItemManager>();
	Collection.InitializeDependency<UItemInventoryManager>();
	Collection.InitializeDependency<URecruitmentManagerSubsystem>();
	Collection.InitializeDependency<UBuildingTraitManagerSubsystem>();
	Collection.InitializeDependency<UBuildingSkinManagerSubsystem>();

	UGameInstance* GI = GetGameInstance();
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UQuestSubsystem::HandleGameDataLoaded);
	}
	if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnDayKeyChanged.AddUObject(this, &UQuestSubsystem::HandleDayKeyChanged);
		Clock->OnWeekKeyChanged.AddUObject(this, &UQuestSubsystem::HandleWeekKeyChanged);
		Clock->OnServerTimeValidityChanged.AddUObject(this, &UQuestSubsystem::HandleServerTimeValidityChanged);
	}
	if (URecruitmentManagerSubsystem* Recruit = GI->GetSubsystem<URecruitmentManagerSubsystem>())
	{
		Recruit->OnGachaPullCompleted.AddUObject(this, &UQuestSubsystem::HandleRecruitGacha);
	}
	if (UBuildingTraitManagerSubsystem* Trait = GI->GetSubsystem<UBuildingTraitManagerSubsystem>())
	{
		Trait->OnTraitGachaCompleted.AddUObject(this, &UQuestSubsystem::HandleTraitGacha);
	}
	if (UBuildingSkinManagerSubsystem* Skin = GI->GetSubsystem<UBuildingSkinManagerSubsystem>())
	{
		Skin->OnSkinGachaCompleted.AddUObject(this, &UQuestSubsystem::HandleSkinGacha);
	}
}

void UQuestSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->OnGameDataLoaded.RemoveAll(this);
		if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>()) { Clock->OnDayKeyChanged.RemoveAll(this); Clock->OnWeekKeyChanged.RemoveAll(this); Clock->OnServerTimeValidityChanged.RemoveAll(this); }
		if (URecruitmentManagerSubsystem* Recruit = GI->GetSubsystem<URecruitmentManagerSubsystem>()) Recruit->OnGachaPullCompleted.RemoveAll(this);
		if (UBuildingTraitManagerSubsystem* Trait = GI->GetSubsystem<UBuildingTraitManagerSubsystem>()) Trait->OnTraitGachaCompleted.RemoveAll(this);
		if (UBuildingSkinManagerSubsystem* Skin = GI->GetSubsystem<UBuildingSkinManagerSubsystem>()) Skin->OnSkinGachaCompleted.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UQuestSubsystem::HandleGameDataLoaded()
{
	// 세이브 핸드오프는 SetQuestData 하나뿐 — 여기서 다시 읽으면 스테일 캐시로 같은 방송의 앞선 쓰기를 덮는다
	// 아래 롤오버가 게이트를 통과해야 하므로 호출 전에 세운다
	bLoaded = true;
	// 로드 직후 즉시 저장 금지(BACKEND_PLAYBOOK §2.6) — 롤오버 결과는 지연 저장으로 나간다
	RolloverIfNeeded(/*bShouldSave=*/false);
	OnQuestBoardChanged.Broadcast();
}

void UQuestSubsystem::HandleDayKeyChanged(const FString&) { RolloverIfNeeded(true); }
void UQuestSubsystem::HandleWeekKeyChanged(const FString&) { RolloverIfNeeded(true); }
// 첫 로그인의 서버 시각 도착 시점 — 여기서만 LoginDay 와 기준 키가 권위 시각으로 잡힌다
void UQuestSubsystem::HandleServerTimeValidityChanged(bool bValid) { if (bValid) RolloverIfNeeded(/*bShouldSave=*/false); }
void UQuestSubsystem::HandleRecruitGacha(const FGachaResultData&) { ReportCounter(EQuestCounter::GachaPull, 1); }
// 마일리지 상자 교환은 뽑기가 아니다 — 같은 델리게이트로 오므로 여기서 가른다
void UQuestSubsystem::HandleTraitGacha(const FBuildingTraitGachaResult& Result) { if (!Result.bFromMileageBox) ReportCounter(EQuestCounter::GachaPull, 1); }
void UQuestSubsystem::HandleSkinGacha(const FBuildingSkinGachaResult& Result) { if (!Result.bFromMileageBox) ReportCounter(EQuestCounter::GachaPull, 1); }

bool UQuestSubsystem::RolloverIfNeeded(bool bShouldSave)
{
	UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!Clock || !TableMgr || !bLoaded) return false;
	// 키는 권위 시각에서만 쓴다 — 로컬 폴백 키로 롤오버하면 남의 날짜로 Claimed 를 지우고 보상을 만료시킨다
	if (!Clock->IsServerTimeValid()) return false;

	const FString DayKey = Clock->GetDayKey();
	const FString WeekKey = Clock->GetWeekKey();
	bool bChanged = false;

	if (Save.DayKey != DayKey)
	{
		TArray<FMissionReward> Expired;
		// 첫 실행(빈 키)은 만료 보상 없이 키만 잡는다
		if (!Save.DayKey.IsEmpty())
		{
			FQuestRules::Rollover(Save, TableMgr->GetAllQuestData(), EQuestPeriod::Daily, DayKey, Expired);
			EmitExpired(FText::FromString(TEXT("어제 완료한 일일 미션 보상")), Expired);
		}
		else
		{
			Save.DayKey = DayKey;
		}
		bChanged = true;
	}
	if (Save.WeekKey != WeekKey)
	{
		TArray<FMissionReward> Expired;
		if (!Save.WeekKey.IsEmpty())
		{
			FQuestRules::Rollover(Save, TableMgr->GetAllQuestData(), EQuestPeriod::Weekly, WeekKey, Expired);
			EmitExpired(FText::FromString(TEXT("지난주 완료한 주간 미션 보상")), Expired);
		}
		else
		{
			Save.WeekKey = WeekKey;
		}
		bChanged = true;
	}
	// 접속일 카운터: 하루 1회 (권위 시각 게이트는 함수 상단이 이미 걸었다)
	if (Save.LastLoginDayKey != DayKey)
	{
		Save.LastLoginDayKey = DayKey;
		ReportCounter(EQuestCounter::LoginDay, 1);
		bChanged = true;
	}
	if (bChanged)
	{
		UE_LOG(LogQuest, Log, TEXT("rollover day=%s week=%s"), *Save.DayKey, *Save.WeekKey);
		// 로드/유효성 전환 경로에서 즉시 저장하면 아직 로드를 소화 못 한 매니저를 스냅샷한다
		if (bShouldSave) SaveNow();
		else if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
		OnQuestBoardChanged.Broadcast();
	}
	return bChanged;
}

void UQuestSubsystem::EmitExpired(const FText& Title, const TArray<FMissionReward>& Rewards)
{
	if (Rewards.Num() == 0) return;
	UE_LOG(LogQuest, Log, TEXT("expired rewards: %s x%d"), *Title.ToString(), Rewards.Num());
	OnQuestRewardsExpired.Broadcast(Title, Rewards);
}

void UQuestSubsystem::ReportCounter(EQuestCounter Counter, int64 Amount)
{
	// 로드 전 누적은 기본값 Save 에 쌓였다가 SetQuestData 로 통째 덮여 사라진다
	if (!bLoaded || Counter == EQuestCounter::None || Amount <= 0) return;
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	bool bChanged = false;
	if (FQuestRules::IsIncrementCounter(Counter))
	{
		for (const auto& Pair : TableMgr->GetAllQuestData())
		{
			const FQuestTable& Row = Pair.Value;
			if (Row.Counter != Counter) continue;
			if (Row.Period == EQuestPeriod::Lifetime) continue; // 업적 증분은 아래에서 카운터 단위로 1회만
			if (Save.Claimed.Contains(Pair.Key)) continue;
			int64& Cur = Save.Progress.FindOrAdd(Pair.Key);
			if (Cur >= Row.Target) continue;
			Cur = FMath::Min(Cur + Amount, Row.Target);
			bChanged = true;
		}
		const FName CounterName = StaticEnum<EQuestCounter>()->GetNameByValue(static_cast<int64>(Counter));
		Save.LifetimeProgress.FindOrAdd(CounterName) += Amount;
		bChanged = true;
	}
	OnCounterReported.Broadcast(Counter, Amount);
	if (bChanged)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
		OnQuestBoardChanged.Broadcast();
	}
}

int64 UQuestSubsystem::ReadAbsolute(EQuestCounter Counter) const
{
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* Data = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!Data) return 0;
	const FGameSaveData& G = Data->GameData;
	switch (Counter)
	{
	case EQuestCounter::TotalRevenue: return G.TotalRevenueEarned;
	case EQuestCounter::BuildingCount: return G.Buildings.Num();
	case EQuestCounter::EmployeeCount:
	{
		const UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>();
		return EmpMgr ? EmpMgr->GetAllEmployeesRef().Num() : G.EmployeeList.Num();
	}
	case EQuestCounter::ProjectsCompleted: return G.TotalProjectsCompleted;
	case EQuestCounter::SGradeProjects: return G.SGradeProjectsCompleted;
	case EQuestCounter::HQLevel: return SaveMgr->GetHQLevel();
	case EQuestCounter::MaxStar:
	{
		// 로스터 주인은 UEmployeeManager — FGameSaveData::EmployeeList 는 런타임에 비어 있는 레거시다
		const UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>();
		if (!EmpMgr) return 0;
		int32 Best = 0;
		for (const FEmployeeInstance& E : EmpMgr->GetAllEmployeesRef()) Best = FMath::Max(Best, E.EnhancementLevel);
		return Best;
	}
	case EQuestCounter::CodexDiscovered: return G.DiscoveredCombos.Num();
	case EQuestCounter::CompanyTitleRank: return static_cast<int64>(G.CompanyTitle);
	case EQuestCounter::OwnedPlots: return G.OwnedPlotIds.Num();
	default: return 0;
	}
}

int64 UQuestSubsystem::CurrentFor(FName QuestID, const FQuestTable& Row) const
{
	if (!FQuestRules::IsIncrementCounter(Row.Counter)) return ReadAbsolute(Row.Counter);
	if (Row.Period == EQuestPeriod::Lifetime)
	{
		const FName CounterName = StaticEnum<EQuestCounter>()->GetNameByValue(static_cast<int64>(Row.Counter));
		return Save.LifetimeProgress.FindRef(CounterName);
	}
	return Save.Progress.FindRef(QuestID);
}

void UQuestSubsystem::GetEntries(EQuestPeriod Period, TArray<FQuestEntry>& OutEntries) const
{
	OutEntries.Reset();
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;
	const TMap<FName, FQuestTable>& Rows = TableMgr->GetAllQuestData();
	for (const FName& ID : TableMgr->GetQuestRowOrder())
	{
		const FQuestTable* Row = Rows.Find(ID);
		if (!Row || Row->Period != Period) continue;
		if (Period == EQuestPeriod::Lifetime && FQuestRules::VisibleLifetimeTier(Row->Track, Rows, Save) != Row->Tier) continue;
		FQuestEntry E;
		E.QuestID = ID; E.Row = *Row; E.Current = CurrentFor(ID, *Row); E.Target = FMath::Max<int64>(1, Row->Target);
		E.State = FQuestRules::StateFor(ID, *Row, E.Current, Save);
		if (E.State == EQuestState::Hidden) continue;
		OutEntries.Add(E);
	}
	// 진행 중끼리는 끝에 가까운 줄을 위로 — 목표가 제각각이라 절대량이 아닌 비율로 비교한다
	OutEntries.StableSort([](const FQuestEntry& A, const FQuestEntry& B)
	{
		auto Rank = [](EQuestState S) { return S == EQuestState::Claimable ? 0 : (S == EQuestState::Claimed ? 2 : 1); };
		const int32 RankA = Rank(A.State);
		const int32 RankB = Rank(B.State);
		if (RankA != RankB) return RankA < RankB;
		if (RankA != 1) return false;
		const double RatioA = static_cast<double>(FMath::Clamp<int64>(A.Current, 0, A.Target)) / static_cast<double>(A.Target);
		const double RatioB = static_cast<double>(FMath::Clamp<int64>(B.Current, 0, B.Target)) / static_cast<double>(B.Target);
		return RatioA > RatioB;
	});
}

bool UQuestSubsystem::HasClaimable(EQuestPeriod Period) const
{
	TArray<FQuestEntry> Entries;
	GetEntries(Period, Entries);
	for (const FQuestEntry& E : Entries) if (E.State == EQuestState::Claimable) return true;
	return false;
}

bool UQuestSubsystem::HasAnyClaimable() const
{
	return HasClaimable(EQuestPeriod::Daily) || HasClaimable(EQuestPeriod::Weekly) || HasClaimable(EQuestPeriod::Lifetime);
}

int32 UQuestSubsystem::GetClaimableCount() const
{
	int32 Count = 0;
	for (const EQuestPeriod Period : { EQuestPeriod::Daily, EQuestPeriod::Weekly, EQuestPeriod::Lifetime })
	{
		TArray<FQuestEntry> Entries;
		GetEntries(Period, Entries);
		for (const FQuestEntry& E : Entries) if (E.State == EQuestState::Claimable) ++Count;
	}
	return Count;
}

bool UQuestSubsystem::CheckClaimableOrNotify() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	if (Clock && Clock->IsServerTimeValid()) return true;
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowRejectNotification(FText::FromString(TEXT("서버 시간을 확인하는 중입니다")), 0.4f, ENotificationType::Warning);
	}
	return false;
}

bool UQuestSubsystem::ClaimQuest(FName QuestID)
{
	return ClaimQuestWithPresentation(QuestID, true);
}

bool UQuestSubsystem::ClaimQuestWithPresentation(FName QuestID, bool bShowRewardToast)
{
	if (!CheckClaimableOrNotify()) return false;
	RolloverIfNeeded(/*bShouldSave=*/false);
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FQuestTable Row;
	if (!TableMgr || !TableMgr->GetQuestData(QuestID, Row)) return false;
	const int64 Current = CurrentFor(QuestID, Row);
	// 업적은 트랙에서 노출 중인 티어만 수령 대상 — 상위 티어를 건너뛰고 받는 경로를 막는다
	const bool bWrongTier = Row.Period == EQuestPeriod::Lifetime
		&& FQuestRules::VisibleLifetimeTier(Row.Track, TableMgr->GetAllQuestData(), Save) != Row.Tier;
	if (bWrongTier || FQuestRules::StateFor(QuestID, Row, Current, Save) != EQuestState::Claimable)
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowRejectNotification(FText::FromString(TEXT("아직 완료되지 않았습니다")), 0.4f, ENotificationType::Warning);
		}
		return false;
	}

	FRewardGrant::Commit(GetGameInstance(), Row.Rewards);
	if (Row.Period == EQuestPeriod::Lifetime)
	{
		Save.ClaimedLifetime.AddUnique(QuestID);
	}
	else
	{
		Save.Claimed.AddUnique(QuestID);
	}
	SaveNow();
	if (bShowRewardToast)
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowRewardToast(Row.Rewards, Row.Title, true);
		}
	}
	UE_LOG(LogQuest, Log, TEXT("claimed %s"), *QuestID.ToString());
	OnQuestBoardChanged.Broadcast();
	return true;
}

void UQuestSubsystem::SaveNow()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
}

void UQuestSubsystem::DevReset()
{
	Save = FQuestSaveData();
	RolloverIfNeeded(/*bShouldSave=*/false);
	SaveNow();
	OnQuestBoardChanged.Broadcast();
	UE_LOG(LogQuest, Log, TEXT("dev reset"));
}
