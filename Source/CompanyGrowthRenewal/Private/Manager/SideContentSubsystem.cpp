#include "Manager/SideContentSubsystem.h"

#include "Data/RewardGrant.h"
#include "Data/SideContentRules.h"
#include "Manager/EmployeeManager.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Table/BusinessTripCardTable.h"
#include "Table/SideDungeonLootTable.h"
#include "Table/SkylineFloorTable.h"
#include "Manager/TableManagerSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Engine/World.h"
#include "GameMode/OfficeGameMode.h"

namespace
{
	constexpr int32 DispatchCardsPerDay = 3;

	/** 같은 날 같은 건물이면 늘 같은 카드 — 재접속으로 다시 뽑지 못하게 한다. */
	int32 MakeCardSeed(const FString& DayKey, int32 BuildingIndex)
	{
		const uint32 Mixed = GetTypeHash(DayKey) ^ (static_cast<uint32>(BuildingIndex) * 2654435761u);
		return static_cast<int32>(Mixed & 0x7FFFFFFFu);
	}
}

void USideContentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnGameDataLoaded.AddUObject(this, &USideContentSubsystem::HandleGameDataLoaded);
		}
		if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnDayKeyChanged.AddUObject(this, &USideContentSubsystem::HandleDayKeyChanged);
		}
	}
}

void USideContentSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->OnGameDataLoaded.RemoveAll(this);
		if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>()) Clock->OnDayKeyChanged.RemoveAll(this);
	}
	Super::Deinitialize();
}

void USideContentSubsystem::SetSideContentData(const FSideContentSaveData& InData)
{
	Save = InData;
	bLoaded = true;
	OnSideContentChanged.Broadcast();
}

void USideContentSubsystem::HandleGameDataLoaded()
{
	// 세이브 핸드오프는 SetSideContentData 하나뿐 — 여기서 다시 읽으면 같은 방송의 앞선 쓰기를 덮는다
	// 아래 따라잡기가 게이트를 통과해야 하므로 호출 전에 세운다
	bLoaded = true;
	// 시계는 첫 폴링에서 방송하지 않는다(구독자가 자기 저장 키와 대조하는 계약) — 재접속만으로는
	// 입장권이 영구히 리필되지 않아 컨텐츠가 잠긴다. QuestSubsystem::RolloverIfNeeded 와 같은 처리.
	if (const UGameClockSubsystem* Clock = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameClockSubsystem>() : nullptr)
	{
		// 로드 직후 즉시 저장 금지(BACKEND_PLAYBOOK §2.6) — 리필 결과는 지연 저장으로 나간다
		RefillEntriesForDay(Clock->GetDayKey(), /*bShouldSave=*/false);
	}
	OnSideContentChanged.Broadcast();
}

void USideContentSubsystem::HandleDayKeyChanged(const FString& NewDayKey)
{
	if (!bLoaded) return;
	RefillEntriesForDay(NewDayKey, /*bShouldSave=*/true);
}

void USideContentSubsystem::RefillEntriesForDay(const FString& NewDayKey, bool bShouldSave)
{
	if (NewDayKey.IsEmpty()) return;
	if (Save.LastRefillDayKey == NewDayKey) return;

	Save.LastRefillDayKey = NewDayKey;

	// 입장권 리필 (건물 축 1/일·최대 5 — 설계 §0-9). 출장은 입장권이 없고 슬롯 1개뿐이다.
	for (FSideBuildingState& State : Save.Buildings)
	{
		State.CourseEntries = FSideContentRules::RefillEntries(State.CourseEntries, 1, 5);
	}
	// 스카이라인 1장 = 등반 1회(층 단위 아님)라 3/10 이 아니라 2/5 다 (DECISION_RECORDS §13.2)
	Save.SkylineEntries = FSideContentRules::RefillEntries(Save.SkylineEntries, SkylineEntriesPerDay, SkylineEntryCap);

	if (bShouldSave)
	{
		RequestSave();
		OnSideContentChanged.Broadcast();
	}
}

int64 USideContentSubsystem::NowUnix() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>()) return Clock->GetServerNowUnix();
	}
	return 0;
}

bool USideContentSubsystem::IsServerTimeValid() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>()) return Clock->IsServerTimeValid();
	}
	return false;
}

int32 USideContentSubsystem::GetHQLevel() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) return SaveMgr->GetHQLevel();
	}
	return 0;
}

void USideContentSubsystem::RequestSave()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	}
}

const FSideBuildingState* USideContentSubsystem::FindBuilding(int32 BuildingIndex) const
{
	return Save.Buildings.FindByPredicate([BuildingIndex](const FSideBuildingState& S) { return S.BuildingIndex == BuildingIndex; });
}

bool USideContentSubsystem::GetCompanyFeatureUnlock(int32 BuildingIndex, FName FeatureKey,
	int32& OutRequiredLevel, FText& OutDisplayName) const
{
	OutRequiredLevel = 0;
	OutDisplayName = FText::GetEmpty();
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return false;

	FCompanyFeatureUnlockTable UnlockRow;
	if (!TableMgr->GetCompanyFeatureUnlock(FeatureKey, UnlockRow)) return false;
	OutRequiredLevel = UnlockRow.RequiredCompanyLevel;
	OutDisplayName = UnlockRow.DisplayName;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData || BuildingIndex == INDEX_NONE) return false;
	const FBuildingEntitySaveData* OwnedBuilding = SaveData->GameData.Buildings.FindByPredicate(
		[BuildingIndex](const FBuildingEntitySaveData& Building) { return Building.BuildingIndex == BuildingIndex; });
	if (!OwnedBuilding || OwnedBuilding->BuildingData.CompanyType == ECompanyType::None
		|| OwnedBuilding->BuildingData.CompanyType == ECompanyType::Max) return false;
	return SaveMgr->GetBuildingTier(BuildingIndex) >= OutRequiredLevel;
}

FSideBuildingState& USideContentSubsystem::FindOrAddBuilding(int32 BuildingIndex)
{
	for (FSideBuildingState& State : Save.Buildings)
	{
		if (State.BuildingIndex == BuildingIndex) return State;
	}
	FSideBuildingState NewState;
	NewState.BuildingIndex = BuildingIndex;
	return Save.Buildings[Save.Buildings.Add(NewState)];
}

void USideContentSubsystem::PruneMissingBuildings(const TArray<int32>& ExistingBuildingIndices)
{
	const int32 Removed = Save.Buildings.RemoveAll([&ExistingBuildingIndices](const FSideBuildingState& S)
	{
		return !ExistingBuildingIndices.Contains(S.BuildingIndex);
	});

	if (Removed > 0)
	{
		RequestSave();
		OnSideContentChanged.Broadcast();
	}
}

void USideContentSubsystem::RefreshDispatchCardsIfNewDay(FSideBuildingState& State)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!Clock || !TableMgr) return;

	const FString DayKey = Clock->GetDayKey();
	if (DayKey.IsEmpty()) return;
	if (State.DispatchCardDayKey == DayKey && State.DispatchCardKeys.Num() > 0) return;

	const TArray<FName>& AllCards = TableMgr->GetBusinessTripCardOrder();
	if (AllCards.Num() == 0) return;

	int32 RequiredLevel = 0;
	FText DisplayName;
	if (!GetCompanyFeatureUnlock(State.BuildingIndex, TEXT("BusinessTrip"), RequiredLevel, DisplayName)) return;
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return;
	const int32 CompanyLevel = SaveMgr->GetBuildingTier(State.BuildingIndex);
	TArray<FName> Eligible;
	for (const FName& CardKey : AllCards)
	{
		FBusinessTripCardTable Row;
		if (TableMgr->GetBusinessTripCard(CardKey, Row) && CompanyLevel >= Row.RequiredCompanyLevel) Eligible.Add(CardKey);
	}
	if (Eligible.Num() == 0) return;

	FRandomStream Rng(MakeCardSeed(DayKey, State.BuildingIndex));
	State.DispatchCardKeys.Reset();
	for (int32 i = 0; i < DispatchCardsPerDay && Eligible.Num() > 0; ++i)
	{
		const int32 Pick = Rng.RandRange(0, Eligible.Num() - 1);
		State.DispatchCardKeys.Add(Eligible[Pick]);
		Eligible.RemoveAt(Pick);
	}
	State.DispatchCardDayKey = DayKey;
}

TArray<FName> USideContentSubsystem::GetDispatchCards(int32 BuildingIndex)
{
	int32 RequiredLevel = 0;
	FText DisplayName;
	if (!GetCompanyFeatureUnlock(BuildingIndex, TEXT("BusinessTrip"), RequiredLevel, DisplayName)) return {};
	FSideBuildingState& State = FindOrAddBuilding(BuildingIndex);
	RefreshDispatchCardsIfNewDay(State);
	return State.DispatchCardKeys;
}

bool USideContentSubsystem::IsDispatchActive(int32 BuildingIndex) const
{
	const FSideBuildingState* State = FindBuilding(BuildingIndex);
	return State && State->DispatchSlot.IsActive();
}

int32 USideContentSubsystem::GetDispatchRemainingSeconds(int32 BuildingIndex) const
{
	const FSideBuildingState* State = FindBuilding(BuildingIndex);
	if (!State || !State->DispatchSlot.IsActive()) return 0;
	return static_cast<int32>(FMath::Max<int64>(0, State->DispatchSlot.EndUnix - NowUnix()));
}

bool USideContentSubsystem::IsDispatchClaimable(int32 BuildingIndex) const
{
	// 서버 시각이 무효면 잠근다 — 로컬 시계를 당겨 조기 수령하는 길을 막는 유일한 가드(설계 §모듈4)
	if (!IsServerTimeValid()) return false;
	const FSideBuildingState* State = FindBuilding(BuildingIndex);
	return State && State->DispatchSlot.IsActive() && NowUnix() >= State->DispatchSlot.EndUnix;
}

TArray<int32> USideContentSubsystem::AutoPickDispatchCrew(int32 BuildingIndex, FName CardKey) const
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return {};

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!TableMgr || !EmployeeMgr) return {};

	FBusinessTripCardTable Row;
	if (!TableMgr->GetBusinessTripCard(CardKey, Row)) return {};

	return FSideContentRules::AutoPickDispatchCrew(
		EmployeeMgr->GetAllEmployeesRef(), BuildingIndex, Row.RequiredDiscipline, Row.CrewCount, NowUnix());
}

int32 USideContentSubsystem::GetDispatchCrewScore(FName CardKey, const TArray<int32>& CrewIDs) const
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return 0;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!TableMgr || !EmployeeMgr) return 0;

	FBusinessTripCardTable Row;
	if (!TableMgr->GetBusinessTripCard(CardKey, Row)) return 0;

	return FSideContentRules::SumDisciplinePoints(EmployeeMgr->GetAllEmployeesRef(), CrewIDs, Row.RequiredDiscipline);
}

EDispatchBlockReason USideContentSubsystem::GetDispatchBlockReason(int32 BuildingIndex, FName CardKey, const TArray<int32>& CrewIDs) const
{
	int32 RequiredLevel = 0;
	FText DisplayName;
	if (!GetCompanyFeatureUnlock(BuildingIndex, TEXT("BusinessTrip"), RequiredLevel, DisplayName)) return EDispatchBlockReason::CompanyLocked;
	UGameInstance* GI = GetGameInstance();
	if (!GI) return EDispatchBlockReason::NoCard;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!TableMgr || !EmployeeMgr) return EDispatchBlockReason::NoCard;

	FBusinessTripCardTable Row;
	if (!TableMgr->GetBusinessTripCard(CardKey, Row)) return EDispatchBlockReason::NoCard;
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr || SaveMgr->GetBuildingTier(BuildingIndex) < Row.RequiredCompanyLevel) return EDispatchBlockReason::CompanyLocked;
	if (IsDispatchActive(BuildingIndex)) return EDispatchBlockReason::SlotBusy;
	if (CrewIDs.Num() != Row.CrewCount) return EDispatchBlockReason::CrewCountMismatch;

	const TArray<FEmployeeInstance>& Roster = EmployeeMgr->GetAllEmployeesRef();
	const int64 Now = NowUnix();
	for (int32 EmployeeID : CrewIDs)
	{
		const FEmployeeInstance* Employee = Roster.FindByPredicate([EmployeeID](const FEmployeeInstance& E) { return E.EmployeeID == EmployeeID; });
		if (!Employee) return EDispatchBlockReason::CrewUnavailable;
		if (FSideContentRules::IsBusy(*Employee, Now)) return EDispatchBlockReason::CrewUnavailable;

		// 벤치이거나 이 건물 배치 팀이어야 한다 — 남의 건물 직원은 못 데려간다
		const bool bBench = (Employee->AssignedBuildingIndex == INDEX_NONE);
		const bool bThisBuilding = (Employee->AssignedBuildingIndex == BuildingIndex);
		if (!bBench && !bThisBuilding) return EDispatchBlockReason::CrewUnavailable;
	}

	if (FSideContentRules::SumDisciplinePoints(Roster, CrewIDs, Row.RequiredDiscipline) < Row.RequiredScoreSum)
	{
		return EDispatchBlockReason::DisciplineShort;
	}

	return EDispatchBlockReason::None;
}

bool USideContentSubsystem::Depart(int32 BuildingIndex, FName CardKey, const TArray<int32>& CrewIDs)
{
	if (!bLoaded) return false;
	if (GetDispatchBlockReason(BuildingIndex, CardKey, CrewIDs) != EDispatchBlockReason::None) return false;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!TableMgr || !EmployeeMgr) return false;

	FBusinessTripCardTable Row;
	if (!TableMgr->GetBusinessTripCard(CardKey, Row)) return false;

	const int64 Start = NowUnix();
	const int64 End = Start + FMath::Max(Row.DurationSeconds, 1);

	FSideBuildingState& State = FindOrAddBuilding(BuildingIndex);
	State.DispatchSlot.CardKey = CardKey;
	State.DispatchSlot.StartUnix = Start;
	State.DispatchSlot.EndUnix = End;
	State.DispatchSlot.EmployeeIDs = CrewIDs;

	for (int32 EmployeeID : CrewIDs)
	{
		if (FEmployeeInstance* Employee = EmployeeMgr->FindEmployee(EmployeeID)) Employee->BusyUntil = End;
	}

	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

bool USideContentSubsystem::ClaimDispatch(int32 BuildingIndex, TArray<FMissionReward>& OutRewards)
{
	if (!bLoaded) return false;
	if (!IsDispatchClaimable(BuildingIndex)) return false;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!TableMgr || !EmployeeMgr) return false;

	FSideBuildingState& State = FindOrAddBuilding(BuildingIndex);
	const FName CardKey = State.DispatchSlot.CardKey;
	const TArray<int32> CrewIDs = State.DispatchSlot.EmployeeIDs;

	FBusinessTripCardTable Row;
	const bool bHasRow = TableMgr->GetBusinessTripCard(CardKey, Row);

	// 드랍 — 슬롯 시작 시각을 시드로 써서 같은 출장이 두 번 정산돼도 값이 흔들리지 않는다
	TArray<FMissionReward> Rewards;
	if (bHasRow && !Row.LootTableKey.IsNone())
	{
		FRandomStream Rng(static_cast<int32>(State.DispatchSlot.StartUnix ^ static_cast<int64>(BuildingIndex)));
		USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
		const int32 CompanyLevel = SaveMgr ? SaveMgr->GetBuildingTier(BuildingIndex) : 0;
		Rewards = FSideContentRules::RollLoot(
			TableMgr->GetSideDungeonLootRows(Row.LootTableKey), Row.LootStage, Row.LootRolls, GetHQLevel(), Rng, CompanyLevel);
	}

	if (Rewards.Num() > 0) FRewardGrant::Commit(GI, Rewards);

	// 확정 보상 = 보낸 직원 각자의 경험치. AddExperience 가 ExpGain 스탯 배율을 적용해 각 직원에게 적립한다.
	if (bHasRow && Row.ExperienceReward > 0.0f)
	{
		for (int32 EmployeeID : CrewIDs) EmployeeMgr->AddExperience(EmployeeID, Row.ExperienceReward);
	}

	for (int32 EmployeeID : CrewIDs)
	{
		if (FEmployeeInstance* Employee = EmployeeMgr->FindEmployee(EmployeeID)) Employee->BusyUntil = 0;
	}

	State.DispatchSlot = FSideDispatchSlot();

	OutRewards = Rewards;
	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

// ──────────────────────────────── 기술 연구소 ────────────────────────────────
// 판정이 즉시·결정적이라 UI 는 누르기 전에 조회만으로 화면을 다 그린다(설계 §0-4).

float USideContentSubsystem::GetCourseRequirement(int32 Course) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 0.0f;

	const FTrainingCourseRow* Band = TableMgr->FindTrainingCourseBand(Course);
	if (!Band) return 0.0f;

	return Band->RequiredBase * FMath::Pow(Band->RequiredMul, static_cast<float>(Course - 1));
}

int32 USideContentSubsystem::GetCourseBest(int32 BuildingIndex) const
{
	const FSideBuildingState* State = FindBuilding(BuildingIndex);
	return State ? State->CourseBest : 0;
}

int32 USideContentSubsystem::GetCurrentCourse(int32 BuildingIndex) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const int32 MaxCourse = TableMgr ? TableMgr->GetTrainingMaxCourse() : 0;

	const int32 Next = GetCourseBest(BuildingIndex) + 1;
	return (MaxCourse > 0) ? FMath::Min(Next, MaxCourse) : Next;
}

int32 USideContentSubsystem::GetTrainingEntries(int32 BuildingIndex) const
{
	const FSideBuildingState* State = FindBuilding(BuildingIndex);
	return State ? State->CourseEntries : 0;
}

float USideContentSubsystem::GetTrainingTeamPower(int32 BuildingIndex) const
{
	UGameInstance* GI = GetGameInstance();
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmployeeMgr) return 0.0f;

	// 기술 연구소 팀 = 그 건물 **배치 전원**(= 책상을 받은 인원). 선발이 없다는 뜻이지 소속 명부 전체는 아니다 —
	// 벤치(bIsAssigned=false)는 빠진다(사용자 확정 2026-09-25). 출장자는 빼지 않는다(설계 §0-7).
	// GetActiveEmployeesInBuilding 은 기여 계열 전용이다. 표시(배치 전원 N)와 판정의 모집단이 갈리면 안 된다.
	const TArray<FEmployeeInstance> Team = EmployeeMgr->GetEmployeesInBuilding(BuildingIndex);
	TArray<int32> TeamIDs;
	TeamIDs.Reserve(Team.Num());
	for (const FEmployeeInstance& Employee : Team) TeamIDs.Add(Employee.EmployeeID);

	TArray<float> Power;
	FSideContentRules::ComputeTeamPower(Team, TeamIDs, Power);

	// 보는 숫자는 하나다(설계 §0-1). 직능 가중은 DT 컬럼으로 남겨 두고 지금은 총합만 쓴다.
	float Sum = 0.0f;
	for (float P : Power) Sum += P;
	return Sum;
}

ETrainingBlockReason USideContentSubsystem::GetTrainingBlockReason(int32 BuildingIndex) const
{
	int32 RequiredLevel = 0;
	FText DisplayName;
	if (!GetCompanyFeatureUnlock(BuildingIndex, TEXT("TrainingCenter"), RequiredLevel, DisplayName)) return ETrainingBlockReason::CompanyLocked;

	UGameInstance* GI = GetGameInstance();
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmployeeMgr || !EmployeeMgr->HasAnyEmployeeInBuilding(BuildingIndex)) return ETrainingBlockReason::NoCrew;

	if (GetTrainingEntries(BuildingIndex) <= 0) return ETrainingBlockReason::NoEntry;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const int32 MaxCourse = TableMgr ? TableMgr->GetTrainingMaxCourse() : 0;
	if (MaxCourse > 0 && GetCourseBest(BuildingIndex) >= MaxCourse) return ETrainingBlockReason::MaxCourse;

	return ETrainingBlockReason::None;
}

bool USideContentSubsystem::CanAdvance(int32 BuildingIndex) const
{
	const float Required = GetCourseRequirement(GetCurrentCourse(BuildingIndex));
	if (Required <= 0.0f) return false;   // 밴드가 없으면 판정 자체가 성립하지 않는다
	return GetTrainingTeamPower(BuildingIndex) >= Required;
}

TArray<FMissionReward> USideContentSubsystem::RollTrainingLoot(int32 BuildingIndex, int32 Course) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return TArray<FMissionReward>();

	const FTrainingCourseRow* Band = TableMgr->FindTrainingCourseBand(Course);
	if (!Band) return TArray<FMissionReward>();

	// 출장과 달리 반복 실행이 전제라 매번 달라야 한다. 남은 입장권을 시드에 섞어
	// 같은 초에 두 번 눌러도 같은 드랍이 안 나오게 한다(서버 시각은 1초 해상도).
	const int64 Seed = NowUnix()
		^ (static_cast<int64>(BuildingIndex) << 8)
		^ (static_cast<int64>(GetTrainingEntries(BuildingIndex)) << 16)
		^ Course;
	FRandomStream Rng(static_cast<int32>(Seed));
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	const int32 CompanyLevel = SaveMgr ? SaveMgr->GetBuildingTier(BuildingIndex) : 0;

	return FSideContentRules::RollLoot(
		TableMgr->GetSideDungeonLootRows(TEXT("Training")), Course, Band->Rolls, GetHQLevel(), Rng, CompanyLevel);
}

bool USideContentSubsystem::Challenge(int32 BuildingIndex, TArray<FMissionReward>& OutRewards)
{
	OutRewards.Reset();
	if (!bLoaded) return false;
	if (GetTrainingBlockReason(BuildingIndex) != ETrainingBlockReason::None) return false;

	// 결과가 미리 보이므로 실패는 아무것도 소모하지 않는다 — 입장권 불변(설계 §0-4)
	if (!CanAdvance(BuildingIndex)) return false;

	const int32 Course = GetCurrentCourse(BuildingIndex);
	OutRewards = RollTrainingLoot(BuildingIndex, Course);
	if (OutRewards.Num() > 0) FRewardGrant::Commit(GetGameInstance(), OutRewards);

	FSideBuildingState& State = FindOrAddBuilding(BuildingIndex);
	State.CourseBest = Course;   // 방금 깬 과정이 최고 과정이 된다
	State.CourseEntries = FMath::Max(0, State.CourseEntries - 1);

	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

bool USideContentSubsystem::SweepTraining(int32 BuildingIndex, TArray<FMissionReward>& OutRewards)
{
	OutRewards.Reset();
	if (!bLoaded) return false;

	// 최종 과정에 닿아도 소탕은 계속 된다 — 그때는 소탕이 유일한 행동이다
	const ETrainingBlockReason Reason = GetTrainingBlockReason(BuildingIndex);
	if (Reason != ETrainingBlockReason::None && Reason != ETrainingBlockReason::MaxCourse) return false;

	const int32 Best = GetCourseBest(BuildingIndex);
	if (Best <= 0) return false;   // 아직 깬 과정이 없으면 줄 보상 자체가 없다

	OutRewards = RollTrainingLoot(BuildingIndex, Best);
	if (OutRewards.Num() > 0) FRewardGrant::Commit(GetGameInstance(), OutRewards);

	FSideBuildingState& State = FindOrAddBuilding(BuildingIndex);
	State.CourseEntries = FMath::Max(0, State.CourseEntries - 1);

	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

// ─────────────────────────────── 스카이라인 ───────────────────────────────

float USideContentSubsystem::GetFloorRequirement(int32 Floor) const
{
	if (Floor <= 0) return 0.0f;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 0.0f;

	const FSkylineFloorRow* Band = TableMgr->FindSkylineFloorBand(Floor);
	if (!Band) return 0.0f;

	const float Base = Band->RequiredBase * FMath::Pow(Band->RequiredMul, static_cast<float>(Floor - 1));
	return IsBossFloor(Floor) ? Base * Band->BossMul : Base;
}

bool USideContentSubsystem::IsBossFloor(int32 Floor) const
{
	if (Floor <= 0) return false;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return false;

	const FSkylineFloorRow* Band = TableMgr->FindSkylineFloorBand(Floor);
	return Band && Band->BossEvery > 0 && (Floor % Band->BossEvery) == 0;
}

int32 USideContentSubsystem::GetNextBossFloor() const
{
	const int32 MaxFloor = GetSkylineMaxFloor();
	for (int32 Floor = Save.BestFloor + 1; Floor <= MaxFloor; ++Floor)
	{
		if (IsBossFloor(Floor)) return Floor;
	}
	return 0;
}

int32 USideContentSubsystem::GetSkylineMaxFloor() const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	return TableMgr ? TableMgr->GetSkylineMaxFloor() : 0;
}

FText USideContentSubsystem::GetFloorTenantName(int32 Floor) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return FText::GetEmpty();

	const FSkylineFloorRow* Band = TableMgr->FindSkylineFloorBand(Floor);
	return Band ? Band->TenantName : FText::GetEmpty();
}

float USideContentSubsystem::GetSkylineRevenueMultiplier() const
{
	return 1.0f + 0.01f * static_cast<float>(Save.BestFloor / 10);
}

float USideContentSubsystem::GetSkylineTeamPower(int32 BuildingIndex) const
{
	// 기술 연구소와 같은 모집단·같은 계산이다. 두 화면이 같은 건물에 다른 파워를 보이면 그 자체가 결함이다.
	return GetTrainingTeamPower(BuildingIndex);
}

TArray<int32> USideContentSubsystem::GetSkylineCandidateBuildings() const
{
	TArray<int32> Result;

	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData) return Result;

	// 소유 권위는 Buildings 다 — OfficeDataMap 은 오피스 진입 시점에 지연 생성돼 소유 판정에 쓸 수 없다.
	// 파워를 비교자 안에서 부르면 GetEmployeesInBuilding 이 명부를 값으로 돌려주므로 비교 한 번마다 배열이 복사된다 — 미리 뽑아 둔다.
	TArray<TPair<int32, float>> Scored;
	Scored.Reserve(SaveData->GameData.Buildings.Num());
	for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
	{
		if (Building.BuildingIndex == INDEX_NONE) continue;
		Scored.Emplace(Building.BuildingIndex, GetSkylineTeamPower(Building.BuildingIndex));
	}

	Scored.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) { return A.Value > B.Value; });

	Result.Reserve(Scored.Num());
	for (const TPair<int32, float>& Entry : Scored) Result.Add(Entry.Key);
	return Result;
}

int32 USideContentSubsystem::GetSkylineBuildingIndex() const
{
	const TArray<int32> Candidates = GetSkylineCandidateBuildings();
	if (Candidates.Num() == 0) return INDEX_NONE;

	// 건물이 팔리거나 철거되면 저장된 인덱스가 남는다 — 목록에 없으면 자동 선택으로 되돌린다
	if (Candidates.Contains(Save.SkylineBuildingIndex)) return Save.SkylineBuildingIndex;
	return Candidates[0];   // 정렬이 파워 내림차순이라 이게 곧 "제일 센 회사"
}

void USideContentSubsystem::SetSkylineBuildingIndex(int32 BuildingIndex)
{
	if (Save.SkylineBuildingIndex == BuildingIndex) return;

	Save.SkylineBuildingIndex = BuildingIndex;
	RequestSave();
	OnSideContentChanged.Broadcast();
}

int32 USideContentSubsystem::ComputeSkylineReach(int32 BuildingIndex) const
{
	const int32 MaxFloor = GetSkylineMaxFloor();
	if (MaxFloor <= 0) return Save.BestFloor;

	const float Power = GetSkylineTeamPower(BuildingIndex);

	int32 Floor = Save.BestFloor;
	while (Floor < MaxFloor)
	{
		const float Required = GetFloorRequirement(Floor + 1);
		if (Required <= 0.0f || Power < Required) break;   // 밴드가 없으면 판정 자체가 성립하지 않는다
		++Floor;
	}
	return Floor;
}

ESkylineBlockReason USideContentSubsystem::GetSkylineBlockReason(int32 BuildingIndex) const
{
	if (GetHQLevel() < 2) return ESkylineBlockReason::HQLocked;
	if (BuildingIndex == INDEX_NONE) return ESkylineBlockReason::NoBuilding;

	UGameInstance* GI = GetGameInstance();
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!EmployeeMgr || !EmployeeMgr->HasAnyEmployeeInBuilding(BuildingIndex)) return ESkylineBlockReason::NoCrew;

	const int32 MaxFloor = GetSkylineMaxFloor();
	if (MaxFloor > 0 && Save.BestFloor >= MaxFloor) return ESkylineBlockReason::MaxFloor;

	if (Save.SkylineEntries <= 0) return ESkylineBlockReason::NoEntry;

	// 입장권보다 뒤에 본다 — 표가 있어도 못 오르는 상태와 표가 없는 상태는 문구가 다르다
	if (ComputeSkylineReach(BuildingIndex) <= Save.BestFloor) return ESkylineBlockReason::PowerShort;

	return ESkylineBlockReason::None;
}

TArray<FMissionReward> USideContentSubsystem::RollSkylineLoot(int32 Floor) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return TArray<FMissionReward>();

	const FSkylineFloorRow* Band = TableMgr->FindSkylineFloorBand(Floor);
	if (!Band) return TArray<FMissionReward>();

	// 층 보상은 1회성이라 같은 층이면 언제 깨든 같아야 한다 — 시드에 시각을 섞지 않는다(기술 연구소와 반대)
	FRandomStream Rng(static_cast<int32>((static_cast<int64>(Floor) * 2654435761ll) & 0x7FFFFFFFll));

	TArray<FMissionReward> Rewards = FSideContentRules::RollLoot(
		TableMgr->GetSideDungeonLootRows(TEXT("Skyline")), Floor, Band->Rolls, GetHQLevel(), Rng);

	if (IsBossFloor(Floor) && !Band->BossLootKey.IsNone())
	{
		Rewards.Append(FSideContentRules::RollLoot(
			TableMgr->GetSideDungeonLootRows(Band->BossLootKey), Floor, 1, GetHQLevel(), Rng));
	}

	// 마일스톤은 해당 층의 각 행을 확정 지급한다. 한 풀로 굴리면 다이아와 스킨이 서로를 대체한다.
	for (const FSideDungeonLootRow& Milestone : TableMgr->GetSideDungeonLootRows(TEXT("SkylineMilestone")))
	{
		Rewards.Append(FSideContentRules::RollLoot({Milestone}, Floor, 1, GetHQLevel(), Rng));
	}
	return Rewards;
}

bool USideContentSubsystem::Climb(int32 BuildingIndex, FSkylineRunResult& OutResult)
{
	OutResult = FSkylineRunResult();
	OutResult.BuildingIndex = BuildingIndex;
	OutResult.FromFloor = Save.BestFloor;
	OutResult.ReachedFloor = Save.BestFloor;

	if (!bLoaded) return false;

	const ESkylineBlockReason Reason = GetSkylineBlockReason(BuildingIndex);

	// 못 오르는 것도 화면에는 그려야 한다 — 막힌 층과 요구치를 채워 돌려주고 false 로 "소모 없음"을 알린다
	OutResult.TeamPower = GetSkylineTeamPower(BuildingIndex);
	OutResult.StopFloor = (Save.BestFloor < GetSkylineMaxFloor()) ? Save.BestFloor + 1 : 0;
	OutResult.StopRequirement = GetFloorRequirement(OutResult.StopFloor);
	if (Reason != ESkylineBlockReason::None) return false;

	const int32 Reached = ComputeSkylineReach(BuildingIndex);

	for (int32 Floor = Save.BestFloor + 1; Floor <= Reached; ++Floor)
	{
		OutResult.ClearedFloors.Add(Floor);
		if (IsBossFloor(Floor)) OutResult.ClearedBossFloors.Add(Floor);

		// 1회성이므로 이미 받은 층은 건너뛴다. BestFloor 가 아니라 이 목록이 지급 권위다.
		if (Save.ClaimedFloorRewards.Contains(Floor)) continue;

		OutResult.Rewards.Append(RollSkylineLoot(Floor));
		Save.ClaimedFloorRewards.Add(Floor);
	}

	if (OutResult.Rewards.Num() > 0) FRewardGrant::Commit(GetGameInstance(), OutResult.Rewards);

	OutResult.ReachedFloor = Reached;
	OutResult.StopFloor = (Reached < GetSkylineMaxFloor()) ? Reached + 1 : 0;
	OutResult.StopRequirement = GetFloorRequirement(OutResult.StopFloor);

	Save.BestFloor = Reached;
	Save.SkylineEntries = FMath::Max(0, Save.SkylineEntries - 1);
	Save.SkylineBuildingIndex = BuildingIndex;   // 다음 진입의 기본 선택이 된다

	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

bool USideContentSubsystem::PrepareSkylineOfficeRun(int32 BuildingIndex)
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!bLoaded || !GI || GI->GetCurrentMapType() != ECurrentMapType::MainMap
		|| SkylineOfficeBuildingIndex != INDEX_NONE
		|| !GetSkylineCandidateBuildings().Contains(BuildingIndex)
		|| GetSkylineBlockReason(BuildingIndex) != ESkylineBlockReason::None)
	{
		return false;
	}
	SkylineOfficeBuildingIndex = BuildingIndex;
	SkylineOfficeRun = FSkylineRunResult();
	bSkylineOfficeRunStarted = false;
	bReturnToSkylineLobby = false;
	return true;
}

bool USideContentSubsystem::StartSkylineOfficeRun(FSkylineRunResult& OutResult)
{
	OutResult = FSkylineRunResult();
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	const AOfficeGameMode* Office = GetWorld() ? GetWorld()->GetAuthGameMode<AOfficeGameMode>() : nullptr;
	if (!GI || GI->GetOfficeMode() != EOfficeMode::Skyline || !GI->IsInOfficeMap()
		|| SkylineOfficeBuildingIndex == INDEX_NONE
		|| GI->GetCurrentManagedBuildingIndex() != SkylineOfficeBuildingIndex
		|| !Office || !Office->CanStartSkylineDevelopment())
	{
		return false;
	}
	if (!bSkylineOfficeRunStarted)
	{
		bSkylineOfficeRunStarted = true;
		Climb(SkylineOfficeBuildingIndex, SkylineOfficeRun);
	}
	OutResult = SkylineOfficeRun;
	return OutResult.DidClimb();
}

void USideContentSubsystem::FinishSkylineOfficeRun()
{
	SkylineOfficeBuildingIndex = INDEX_NONE;
	SkylineOfficeRun = FSkylineRunResult();
	bSkylineOfficeRunStarted = false;
	bReturnToSkylineLobby = true;
}

bool USideContentSubsystem::ConsumeSkylineLobbyReturn()
{
	const bool bReturn = bReturnToSkylineLobby;
	bReturnToSkylineLobby = false;
	return bReturn;
}

void USideContentSubsystem::DebugSetSkyline(int32 BestFloor, int32 Entries)
{
	if (BestFloor >= 0)
	{
		Save.BestFloor = BestFloor;
		// 건너뛴 층의 보상까지 준 것으로 쳐야 치트로 올린 뒤 내려와도 같은 층을 두 번 받지 않는다
		for (int32 Floor = 1; Floor <= BestFloor; ++Floor) Save.ClaimedFloorRewards.AddUnique(Floor);
	}
	if (Entries >= 0) { Save.SkylineEntries = Entries; }

	RequestSave();
	OnSideContentChanged.Broadcast();
}

void USideContentSubsystem::DebugSetTraining(int32 BuildingIndex, int32 CourseBest, int32 Entries)
{
	FSideBuildingState& State = FindOrAddBuilding(BuildingIndex);
	// 상한은 GetCurrentCourse 가 이미 잡는다 — 여기서 또 자르면 「50 을 넣었는데 49」 같은 혼선이 생긴다
	if (CourseBest >= 0) { State.CourseBest = CourseBest; }
	if (Entries >= 0) { State.CourseEntries = Entries; }

	RequestSave();
	OnSideContentChanged.Broadcast();
}
