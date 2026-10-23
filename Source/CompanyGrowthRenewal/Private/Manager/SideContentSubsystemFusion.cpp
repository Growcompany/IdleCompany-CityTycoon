#include "Manager/SideContentSubsystem.h"

#include "Data/RewardGrant.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/FusionProjectTable.h"

#include <cmath>

bool FusionRules::IsValidProject(const FFusionProjectTable& Row)
{
	if (Row.DisplayName.IsEmpty() || Row.RequiredIndustries.Num() < 2 || Row.RequiredIndustries.Num() > 3
		|| Row.DurationSeconds <= 0 || Row.BaseMarketCap <= 0 || Row.RequiredHQLevel < 1
		|| !FMath::IsFinite(Row.BaseRequirement) || Row.BaseRequirement <= 0.0f
		|| !FMath::IsFinite(Row.RequirementMultiplier) || Row.RequirementMultiplier <= 0.0f
		|| !FMath::IsFinite(Row.MarketCapMultiplier) || Row.MarketCapMultiplier <= 0.0f
		|| Row.FirstSuccessDiamonds < 0 || Row.MilestoneEvery <= 0 || Row.MilestoneTicketAmount < 0)
	{
		return false;
	}
	for (ECompanyType Industry : Row.RequiredIndustries)
	{
		if (Industry <= ECompanyType::None || Industry >= ECompanyType::Max) return false;
	}
	return true;
}

bool FusionRules::IsProjectAvailable(const FFusionProjectTable& Row, TFunctionRef<bool(ECompanyType)> IsIndustryAvailable)
{
	for (ECompanyType Industry : Row.RequiredIndustries)
	{
		if (!IsIndustryAvailable(Industry)) return false;
	}
	return true;
}

TArray<FName> FusionRules::FilterAvailableKeys(const TArray<FName>& Keys,
	TFunctionRef<bool(FName, FFusionProjectTable&)> FindRow, TFunctionRef<bool(ECompanyType)> IsIndustryAvailable)
{
	TArray<FName> Result;
	Result.Reserve(Keys.Num());
	for (FName Key : Keys)
	{
		FFusionProjectTable Row;
		// 행 결손으로 판정을 못 하면 남긴다 — 숨기는 쪽으로 실패하면 DT 누락이 컨텐츠를 조용히 지운다
		if (!FindRow(Key, Row) || IsProjectAvailable(Row, IsIndustryAvailable)) Result.Add(Key);
	}
	return Result;
}

float FusionRules::Requirement(const FFusionProjectTable& Row, int32 Level)
{
	if (Level < 0 || !IsValidProject(Row)) return 0.0f;
	const double Value = Row.BaseRequirement * std::pow(static_cast<double>(Row.RequirementMultiplier), Level);
	return std::isfinite(Value) && Value > 0.0 && Value <= MAX_flt ? static_cast<float>(Value) : 0.0f;
}

TArray<FMissionReward> FusionRules::Rewards(const FFusionProjectTable& Row, int32 Level)
{
	TArray<FMissionReward> Result;
	if (Level < 0 || Level == MAX_int32 || !IsValidProject(Row)) return Result;
	const double Value = std::round(static_cast<double>(Row.BaseMarketCap)
		* std::pow(static_cast<double>(Row.MarketCapMultiplier), Level));
	// int64 경계를 넘어간 double 을 정수로 캐스팅하지 않는다. 상한 레벨을 게임 규칙으로 만들지 않는다.
	if (!std::isfinite(Value) || Value < 1.0 || Value >= static_cast<double>(MAX_int64)) return Result;

	FMissionReward MarketCap;
	MarketCap.ResourceType = EResourceType::MarketCap;
	MarketCap.Amount = static_cast<int64>(Value);
	Result.Add(MarketCap);
	if (Level == 0 && Row.FirstSuccessDiamonds > 0)
	{
		FMissionReward FirstSuccess;
		FirstSuccess.ResourceType = EResourceType::Diamond;
		FirstSuccess.Amount = Row.FirstSuccessDiamonds;
		Result.Add(FirstSuccess);
	}
	if ((Level + 1) % Row.MilestoneEvery == 0 && Row.MilestoneTicketAmount > 0)
	{
		FMissionReward Milestone;
		Milestone.ItemType = EItemType::BuildingTraitTicketAdvanced;
		Milestone.ItemAmount = Row.MilestoneTicketAmount;
		Result.Add(Milestone);
	}
	return Result;
}

EFusionBlockReason FusionRules::ValidateSelection(const TArray<ECompanyType>& Industries,
	const TArray<FFusionBuildingCandidate>& OwnedBuildings, const TArray<int32>& Selection, float RequiredPower)
{
	if (Industries.IsEmpty() || !FMath::IsFinite(RequiredPower) || RequiredPower <= 0.0f) return EFusionBlockReason::NoProject;
	if (Selection.Num() != Industries.Num()) return EFusionBlockReason::SlotCountMismatch;
	TSet<int32> Selected;
	for (int32 BuildingIndex : Selection)
	{
		if (BuildingIndex == INDEX_NONE) return EFusionBlockReason::EmptySlot;
		if (Selected.Contains(BuildingIndex)) return EFusionBlockReason::DuplicateBuilding;
		Selected.Add(BuildingIndex);
	}
	for (int32 SlotIndex = 0; SlotIndex < Selection.Num(); ++SlotIndex)
	{
		const FFusionBuildingCandidate* Candidate = OwnedBuildings.FindByPredicate([&Selection, SlotIndex](const FFusionBuildingCandidate& Entry)
		{
			return Entry.BuildingIndex == Selection[SlotIndex];
		});
		if (!Candidate) return EFusionBlockReason::InvalidBuilding;
		if (Candidate->Industry != Industries[SlotIndex]) return EFusionBlockReason::IndustryMismatch;
		if (!Candidate->bHasCrew) return EFusionBlockReason::NoCrew;
		if (!FMath::IsFinite(Candidate->TeamPower) || Candidate->TeamPower < RequiredPower) return EFusionBlockReason::PowerShort;
	}
	return EFusionBlockReason::None;
}

bool FusionRules::ConsumeClaim(FSideContentSaveData& State, const FFusionProjectTable& Row,
	int64 CurrentUnix, bool bServerTimeValid, FFusionClaimResult& OutResult)
{
	OutResult = FFusionClaimResult();
	if (!bServerTimeValid || State.FusionActiveKey.IsNone() || State.FusionEndUnix <= 0 || CurrentUnix < State.FusionEndUnix) return false;
	const int32 Level = FMath::Max(0, State.FusionLevels.FindRef(State.FusionActiveKey));
	const TArray<FMissionReward> ClaimRewards = Rewards(Row, Level);
	if (ClaimRewards.IsEmpty()) return false;

	OutResult.FusionKey = State.FusionActiveKey;
	OutResult.PreviousLevel = Level;
	OutResult.NewLevel = Level + 1;
	OutResult.Rewards = ClaimRewards;
	State.FusionLevels.Add(State.FusionActiveKey, OutResult.NewLevel);
	State.FusionActiveKey = NAME_None;
	State.FusionEndUnix = 0;
	State.FusionBuildingIndices.Reset();
	return true;
}

int32 USideContentSubsystem::GetFusionLevel(FName FusionKey) const
{
	return FMath::Max(0, Save.FusionLevels.FindRef(FusionKey));
}

TArray<FName> USideContentSubsystem::GetAvailableFusionKeys() const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return {};
	const TArray<FName>& Keys = TableMgr->GetFusionProjectOrder();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	// 캐시를 로드 시점에 거르지 않고 매 조회마다 판정한다 — DT_CompanyInfo 가 아직 안 올라온
	// 순서에서 캐시를 걸렀다면 그 상태가 영구히 굳는다. 판정자가 없으면 거르지 않는다(여는 쪽 실패).
	if (!SaveMgr) return Keys;
	return FusionRules::FilterAvailableKeys(Keys,
		[TableMgr](FName Key, FFusionProjectTable& OutRow) { return TableMgr->GetFusionProject(Key, OutRow); },
		[SaveMgr](ECompanyType Industry) { return SaveMgr->IsIndustryAvailable(Industry); });
}

float USideContentSubsystem::GetFusionRequirement(FName FusionKey, int32 LevelOverride) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row)) return 0.0f;
	return FusionRules::Requirement(Row, LevelOverride == INDEX_NONE ? GetFusionLevel(FusionKey) : LevelOverride);
}

TArray<FMissionReward> USideContentSubsystem::GetFusionRewardPreview(FName FusionKey, int32 LevelOverride) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row)) return {};
	return FusionRules::Rewards(Row, LevelOverride == INDEX_NONE ? GetFusionLevel(FusionKey) : LevelOverride);
}

TArray<FFusionBuildingCandidate> USideContentSubsystem::GetFusionOwnedBuildings() const
{
	TArray<FFusionBuildingCandidate> Result;
	const UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	UEmployeeManager* EmployeeMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	const USaveGame_GameData* Saved = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!Saved) return Result;
	for (const FBuildingEntitySaveData& Building : Saved->GameData.Buildings)
	{
		if (Building.BuildingIndex < 0) continue;
		FFusionBuildingCandidate Candidate;
		Candidate.BuildingIndex = Building.BuildingIndex;
		Candidate.Industry = SaveMgr->GetBuildingCompanyType(Building.BuildingIndex);
		Candidate.TeamPower = GetTrainingTeamPower(Building.BuildingIndex);
		Candidate.bHasCrew = EmployeeMgr && !EmployeeMgr->GetEmployeesInBuilding(Building.BuildingIndex).IsEmpty();
		Result.Add(Candidate);
	}
	return Result;
}

TArray<int32> USideContentSubsystem::GetFusionCandidateBuildings(ECompanyType Industry) const
{
	TArray<FFusionBuildingCandidate> Candidates = GetFusionOwnedBuildings();
	Candidates.RemoveAll([Industry](const FFusionBuildingCandidate& Candidate) { return Candidate.Industry != Industry; });
	Candidates.Sort([](const FFusionBuildingCandidate& A, const FFusionBuildingCandidate& B)
	{
		if (A.TeamPower != B.TeamPower) return A.TeamPower > B.TeamPower;
		return A.BuildingIndex < B.BuildingIndex;
	});
	TArray<int32> Result;
	for (const FFusionBuildingCandidate& Candidate : Candidates) Result.AddUnique(Candidate.BuildingIndex);
	return Result;
}

TArray<int32> USideContentSubsystem::AutoPickFusionBuildings(FName FusionKey) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	TArray<int32> Result;
	if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row)) return Result;
	for (ECompanyType Industry : Row.RequiredIndustries)
	{
		const TArray<int32> Candidates = GetFusionCandidateBuildings(Industry);
		const int32* Pick = Candidates.FindByPredicate([&Result](int32 BuildingIndex) { return !Result.Contains(BuildingIndex); });
		Result.Add(Pick ? *Pick : INDEX_NONE);
	}
	return Result;
}

ECompanyType USideContentSubsystem::GetFusionMissingIndustry(FName FusionKey) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row)) return ECompanyType::None;
	for (ECompanyType Industry : Row.RequiredIndustries)
	{
		if (GetFusionCandidateBuildings(Industry).IsEmpty()) return Industry;
	}
	return ECompanyType::None;
}

EFusionBlockReason USideContentSubsystem::GetFusionBlockReason(FName FusionKey, const TArray<int32>& BuildingIndices) const
{
	if (!bLoaded) return EFusionBlockReason::NotLoaded;
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row) || !FusionRules::IsValidProject(Row)
		|| GetFusionRewardPreview(FusionKey).IsEmpty()) return EFusionBlockReason::NoProject;
	// 패널은 미가용 콜라보를 숨기지만 키는 세이브·치트로 직접 올 수 있다 — 표시는 fail-open 이고 착수는 fail-closed 다.
	// SaveLoadManager 가 없으면 아래 GetHQLevel() 이 0 을 내 HQLocked 로 이미 닫힌다.
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	if (SaveMgr && !FusionRules::IsProjectAvailable(Row,
		[SaveMgr](ECompanyType Industry) { return SaveMgr->IsIndustryAvailable(Industry); }))
	{
		return EFusionBlockReason::NoProject;
	}
	if (IsFusionActive()) return EFusionBlockReason::ActiveProject;
	if (GetHQLevel() < Row.RequiredHQLevel) return EFusionBlockReason::HQLocked;
	if (GetFusionMissingIndustry(FusionKey) != ECompanyType::None) return EFusionBlockReason::MissingIndustry;
	return FusionRules::ValidateSelection(Row.RequiredIndustries, GetFusionOwnedBuildings(), BuildingIndices, GetFusionRequirement(FusionKey));
}

bool USideContentSubsystem::StartFusion(FName FusionKey, const TArray<int32>& BuildingIndices)
{
	if (GetFusionBlockReason(FusionKey, BuildingIndices) != EFusionBlockReason::None) return false;
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row)) return false;
	const int64 Start = NowUnix();
	if (Start <= 0 || Start > MAX_int64 - Row.DurationSeconds) return false;

	// 배치 파워는 착수 때 확정한다. 참여 건물의 일반 프로젝트와 직원 배치는 점유하지 않는다.
	Save.FusionActiveKey = FusionKey;
	Save.FusionEndUnix = Start + Row.DurationSeconds;
	Save.FusionBuildingIndices = BuildingIndices;
	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

bool USideContentSubsystem::IsFusionActive() const
{
	return !Save.FusionActiveKey.IsNone() && Save.FusionEndUnix > 0;
}

bool USideContentSubsystem::IsFusionClaimable() const
{
	return bLoaded && IsFusionActive() && IsServerTimeValid() && NowUnix() >= Save.FusionEndUnix;
}

int32 USideContentSubsystem::GetFusionRemainingSeconds() const
{
	if (!IsFusionActive()) return 0;
	return static_cast<int32>(FMath::Clamp<int64>(Save.FusionEndUnix - NowUnix(), 0, MAX_int32));
}

float USideContentSubsystem::GetFusionProgress() const
{
	if (!IsFusionActive()) return 0.0f;
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFusionProjectTable Row;
	if (!TableMgr || !TableMgr->GetFusionProject(Save.FusionActiveKey, Row) || Row.DurationSeconds <= 0) return 0.0f;
	return FMath::Clamp(1.0f - static_cast<float>(GetFusionRemainingSeconds()) / Row.DurationSeconds, 0.0f, 1.0f);
}

bool USideContentSubsystem::ClaimFusion(FFusionClaimResult& OutResult)
{
	OutResult = FFusionClaimResult();
	if (!IsFusionClaimable()) return false;
	UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	const UItemInventoryManager* ItemMgr = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	FFusionProjectTable Row;
	if (!ResourceMgr || !ItemMgr || !TableMgr || !TableMgr->GetFusionProject(Save.FusionActiveKey, Row)) return false;
	const TArray<FMissionReward> Preview = GetFusionRewardPreview(Save.FusionActiveKey);
	if (Preview.IsEmpty()) return false;
	for (const FMissionReward& Reward : Preview)
	{
		if (Reward.ResourceType != EResourceType::None && ResourceMgr->GetResourceAmount(Reward.ResourceType) > MAX_int64 - Reward.Amount) return false;
		if (Reward.ItemType != EItemType::None && ItemMgr->GetItemCount(Reward.ItemType) > MAX_int32 - Reward.ItemAmount) return false;
	}
	// 시작 뒤 건물/직원이 바뀌어도 이미 완료한 개발을 취소하지 않는다.
	if (!FusionRules::ConsumeClaim(Save, Row, NowUnix(), IsServerTimeValid(), OutResult)) return false;
	FRewardGrant::Commit(GI, OutResult.Rewards);
	RequestSave();
	OnSideContentChanged.Broadcast();
	return true;
}

void USideContentSubsystem::DebugSetFusion(FName FusionKey, int32 Level, int32 RemainingSeconds)
{
#if !UE_BUILD_SHIPPING
	if (!bLoaded) return;
	if (FusionKey.IsNone())
	{
		Save.FusionActiveKey = NAME_None;
		Save.FusionEndUnix = 0;
		Save.FusionBuildingIndices.Reset();
	}
	else
	{
		const UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		FFusionProjectTable Row;
		if (!TableMgr || !TableMgr->GetFusionProject(FusionKey, Row)) return;
		if (Level >= 0) Save.FusionLevels.Add(FusionKey, Level);
		if (RemainingSeconds >= 0)
		{
			Save.FusionActiveKey = FusionKey;
			Save.FusionEndUnix = NowUnix() + RemainingSeconds;
			Save.FusionBuildingIndices = AutoPickFusionBuildings(FusionKey);
		}
	}
	RequestSave();
	OnSideContentChanged.Broadcast();
#endif
}
