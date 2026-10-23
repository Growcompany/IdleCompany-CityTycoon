// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"  // ResolveGachaTier (코스메틱 등급)
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Enum/BuildingTraitTarget.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Data/GameSaveData.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeePotentialData.h"
#include "Data/RecruitmentCapacityTicketRules.h"
#include "Table/BuildingData.h"
#include "Table/MissionTable.h"
#include "Utils/GachaBatchMath.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Data/CloudEconomyData.h"
#include "Dom/JsonObject.h"

namespace
{
	// 서버 카탈로그 reason ― `consumeRules.json`/`spendRules.json` 과 바이트 단위로 같아야 한다(다르면 400)
	const FName ConsumeReason_RecruitPull(TEXT("recruit.pull"));
	const FName SpendReason_RecruitAdvanced(TEXT("gacha.recruit.advanced"));
	const FName SpendReason_RecruitPremium(TEXT("gacha.recruit.premium"));

	// `/wallet/spend` 프리미엄은 meta.count 만큼 마일리지를 준다. 다이아는 단일 전용이라 항상 1
	const int32 PremiumSpendMileageCount = 1;
}

void URecruitmentManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Initialized (Gacha System)"));
}

void URecruitmentManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ========== 캐시된 서브시스템 ==========

UEmployeeManager* URecruitmentManagerSubsystem::GetEmployeeManager() const
{
	if (!EmployeeManager)
	{
		EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	}
	return EmployeeManager;
}

UTableManagerSubsystem* URecruitmentManagerSubsystem::GetTableManager() const
{
	if (!TableManager)
	{
		TableManager = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	}
	return TableManager;
}

bool URecruitmentManagerSubsystem::SaveGameData()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		return SaveMgr->SaveGameData();
	}
	return false;
}

int32 URecruitmentManagerSubsystem::GetTotalPermanentEmployeeCapacity(bool& bOutComplete) const
{
	bOutComplete = false;

	UWorld* CurrentWorld = GetWorld();
	UEntityManager* EntityMgr = CurrentWorld ? CurrentWorld->GetSubsystem<UEntityManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GetTableManager();
	if (!EntityMgr || !TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitCapacity] EntityManager or TableManager is unavailable"));
		return 0;
	}

	static const FName CompletedBuildingTag(TEXT("Building"));
	int64 TotalCapacity = 0;
	for (ABuildingBaseActor* Building : EntityMgr->GetBuildings())
	{
		if (!IsValid(Building)
			|| Building->GetCompanyType() == ECompanyType::None
			|| !Building->Tags.Contains(CompletedBuildingTag))
		{
			continue;
		}

		bool bFoundBuildingData = false;
		const FBuildingData BuildingData = TableMgr->GetBuildingData(Building->GetBuildingID(), bFoundBuildingData);
		if (!bFoundBuildingData)
		{
			UE_LOG(LogTemp, Warning, TEXT("[RecruitCapacity] Missing BuildingData row: %s"),
				*Building->GetBuildingID().ToString());
			return 0;
		}

		const int32 AddedFloors = Building->GetEnhancementLevel(EBuildingEnhancementType::BuildingFloor);
		TotalCapacity += FMath::Max(0, BuildingData.GetEmployeeCapacity(AddedFloors));
		if (TotalCapacity > MAX_int32)
		{
			UE_LOG(LogTemp, Error, TEXT("[RecruitCapacity] Total permanent capacity exceeds int32"));
			return 0;
		}
	}

	bOutComplete = true;
	return static_cast<int32>(TotalCapacity);
}

int32 URecruitmentManagerSubsystem::ReconcilePermanentCapacityTickets(bool bShouldSave, bool bShowRewardToast)
{
	UGameInstance* GameInstance = GetGameInstance();
	USaveLoadManager* SaveMgr = GameInstance ? GameInstance->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	UItemInventoryManager* ItemMgr = GameInstance ? GameInstance->GetSubsystem<UItemInventoryManager>() : nullptr;
	if (!SaveMgr || !SaveData || !ItemMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitCapacity] SaveData or ItemInventoryManager is unavailable"));
		return 0;
	}

	bool bCapacityComplete = false;
	const int32 CurrentCapacity = GetTotalPermanentEmployeeCapacity(bCapacityComplete);
	if (!bCapacityComplete)
	{
		return 0;
	}

	const int32 OldCreditedCapacity = SaveData->GameData.CreditedPermanentEmployeeCapacity;
	const FRecruitmentCapacityTicketDecision Decision =
		FRecruitmentCapacityTicketRules::Evaluate(CurrentCapacity, OldCreditedCapacity);
	if (Decision.GrantCount <= 0)
	{
		return 0;
	}

	SaveData->GameData.CreditedPermanentEmployeeCapacity = Decision.NewCreditedCapacity;
	ItemMgr->AddItem(EItemType::RecruitTicketNormal, Decision.GrantCount, false);

	if (bShouldSave)
	{
		SaveGameData();
	}

	if (bShowRewardToast)
	{
		FMissionReward TicketReward;
		TicketReward.ItemType = EItemType::RecruitTicketNormal;
		TicketReward.ItemAmount = Decision.GrantCount;
		const TArray<FMissionReward> Rewards{TicketReward};
		const FText ToastTitle = NSLOCTEXT("RecruitCapacity", "GrantToast", "일반 채용권 획득");
		if (UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>())
		{
			UIManager->ShowRewardToast(Rewards, ToastTitle, true);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[RecruitCapacity] Current=%d, Credited=%d->%d, Granted=%d"),
		CurrentCapacity, OldCreditedCapacity, Decision.NewCreditedCapacity, Decision.GrantCount);
	return Decision.GrantCount;
}

// ========================================================================
// 가챠 뽑기 시스템
// ========================================================================

int32 URecruitmentManagerSubsystem::GetDiamondCost(EGachaTier Tier) const
{
	switch (Tier)
	{
	case EGachaTier::Advanced: return AdvancedDiamondCost;
	case EGachaTier::Premium:  return PremiumDiamondCost;
	default:                   return 0;   // 일반은 다이아 폴백 없음
	}
}

EItemType URecruitmentManagerSubsystem::GetTicketTypeForTier(EGachaTier Tier)
{
	switch (Tier)
	{
	case EGachaTier::Normal:   return EItemType::RecruitTicketNormal;
	case EGachaTier::Advanced: return EItemType::RecruitTicketAdvanced;
	default:                   return EItemType::RecruitTicketPremium;
	}
}

bool URecruitmentManagerSubsystem::CanExecuteGachaPull(EGachaTier Tier) const
{
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();

	// 티어별 채용권 타입 매핑
	EItemType TicketType = EItemType::None;
	int32 DiamondCost = 0;

	switch (Tier)
	{
	case EGachaTier::Normal:
		TicketType = EItemType::RecruitTicketNormal;
		break;
	case EGachaTier::Advanced:
		TicketType = EItemType::RecruitTicketAdvanced;
		DiamondCost = AdvancedDiamondCost;
		break;
	case EGachaTier::Premium:
		TicketType = EItemType::RecruitTicketPremium;
		DiamondCost = PremiumDiamondCost;
		break;
	}

	// 채용권이 있으면 OK
	if (ItemMgr && ItemMgr->HasItem(TicketType, 1))
	{
		return true;
	}

	// 고급/프리미엄은 Diamond 폴백
	if (DiamondCost > 0 && ResourceMgr && ResourceMgr->HasResource(EResourceType::Diamond, DiamondCost))
	{
		return true;
	}

	// 일반은 채용권 전용 (Diamond 폴백 없음)
	return false;
}

bool URecruitmentManagerSubsystem::DeductGachaCost(EGachaTier Tier, bool bAllowDiamondFallback)
{
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();

	EItemType TicketType = EItemType::None;
	int32 DiamondCost = 0;

	switch (Tier)
	{
	case EGachaTier::Normal:
		TicketType = EItemType::RecruitTicketNormal;
		break;
	case EGachaTier::Advanced:
		TicketType = EItemType::RecruitTicketAdvanced;
		DiamondCost = AdvancedDiamondCost;
		break;
	case EGachaTier::Premium:
		TicketType = EItemType::RecruitTicketPremium;
		DiamondCost = PremiumDiamondCost;
		break;
	}

	// 채용권 우선 사용
	if (ItemMgr && ItemMgr->HasItem(TicketType, 1))
	{
		return ItemMgr->SpendItem(TicketType, 1, false);
	}

	// Diamond 폴백 (고급/프리미엄만)
	if (bAllowDiamondFallback && DiamondCost > 0 && ResourceMgr && ResourceMgr->HasResource(EResourceType::Diamond, DiamondCost))
	{
		ResourceMgr->SpendResource(EResourceType::Diamond, DiamondCost, false);
		return true;
	}

	return false;
}

bool URecruitmentManagerSubsystem::ExecuteGachaPull(EGachaTier Tier, int32 BuildingIndex, FGachaResultData& OutResult,
	bool bShouldSave, bool bAllowDiamondFallback)
{
	// 1. 뽑기 가능 여부 확인
	if (!CanExecuteGachaPull(Tier))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] Cannot execute gacha pull - insufficient resources. Tier=%d"), static_cast<int32>(Tier));
		return false;
	}

	// 2. 재화 차감
	if (!DeductGachaCost(Tier, bAllowDiamondFallback))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] Failed to deduct gacha cost. Tier=%d"), static_cast<int32>(Tier));
		return false;
	}

	ApplyLocalPullEffect(Tier, BuildingIndex, OutResult, /*bGrantMileage=*/true, EProductionDiscipline::Count);

	// 세이브는 로컬 효과 뒤 1회
	if (bShouldSave)
	{
		SaveGameData();
	}

	return true;
}

void URecruitmentManagerSubsystem::ApplyLocalPullEffect(EGachaTier Tier, int32 BuildingIndex, FGachaResultData& OutResult,
	bool bGrantMileage, EProductionDiscipline PrimaryDiscipline)
{
	// 3. HR 파워 계산
	int32 HRPower = GetHRPower(BuildingIndex);

	// 4. 잠재능력 등급 롤 (확률 테이블 + Pity 보정)
	ELootBoxRarity PotentialRarity = RollPotentialRarity(Tier, HRPower);

	// 5. 직원 인스턴스 생성 (+0 인턴)
	FEmployeeInstance NewEmployee = GenerateGachaEmployee(PotentialRarity, BuildingIndex, PrimaryDiscipline);

	// 6. Pity 카운터 업데이트 — Legendary 획득 시에만 리셋 — RECRUITMENT_SYSTEM v2.2 정합
	bool bWasPity = false;

	if (Tier == EGachaTier::Advanced)
	{
		GachaData.AdvancedPity.IncrementPull();
		if (PotentialRarity >= ELootBoxRarity::Legendary)
		{
			bWasPity = (GachaData.AdvancedPity.PullsSinceLastGuaranteed >= FGachaPityData::HardPity);
			GachaData.AdvancedPity.ResetPity();
		}
	}
	else if (Tier == EGachaTier::Premium)
	{
		GachaData.PremiumPity.IncrementPull();
		if (PotentialRarity >= ELootBoxRarity::Legendary)
		{
			bWasPity = (GachaData.PremiumPity.PullsSinceLastGuaranteed >= FGachaPityData::HardPity);
			GachaData.PremiumPity.ResetPity();
		}

		// 7. 마일리지 적립 (프리미엄 전용)
		if (bGrantMileage)
		{
			GachaData.Mileage.CurrentPoints++;
			OnMileageChanged.Broadcast(GachaData.Mileage.CurrentPoints);
		}
	}

	// 8. 결과 구성
	OutResult.ResultEmployee = NewEmployee;
	OutResult.PotentialRarity = PotentialRarity;
	OutResult.UsedTier = Tier;
	OutResult.bWasPityGuaranteed = bWasPity;
	OutResult.TargetBuildingIndex = BuildingIndex;

	// 뽑기 시점에 초상화 미리 촬영 → 가챠 연출(수 초) 동안 {ID}.png 준비 완료 → [확인] 시 지연/깜빡임 없음.
	// (확인 안 하고 다시 뽑으면 안 쓰는 PNG 가 남지만 사소함. 확정 캡처는 HireEmployeeFromCard 에서 제거.)
	if (UEmployeeManager* EmpMgr = GetEmployeeManager())
	{
		EmpMgr->CaptureEmployeePortrait(
			FString::FromInt(NewEmployee.EmployeeID),
			UEmployeeTypeHelper::GetRankFromEnhancementLevel(NewEmployee.EnhancementLevel),
			NewEmployee.Gender,
			NewEmployee.Department,
			AFunnyOfficeworker::ResolveGachaTier(NewEmployee)
		);
	}

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Gacha Pull - Tier=%d, Potential=%d, Building=%d, Pity=%s"),
		static_cast<int32>(Tier), static_cast<int32>(PotentialRarity), BuildingIndex,
		bWasPity ? TEXT("YES") : TEXT("NO"));

	// 9. 델리게이트 브로드캐스트
	OnGachaPullCompleted.Broadcast(OutResult);
}

// ===== 서버 권위 뽑기 =====

UCloudEconomySubsystem* URecruitmentManagerSubsystem::GetCloudEconomy() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCloudEconomySubsystem>() : nullptr;
}

EGachaPullRoute URecruitmentManagerSubsystem::ResolvePullRoute(bool bHasTicket, bool bAllowDiamond)
{
	if (bHasTicket)
	{
		return EGachaPullRoute::Ticket;
	}
	return bAllowDiamond ? EGachaPullRoute::Diamond : EGachaPullRoute::None;
}

int32 URecruitmentManagerSubsystem::ResolveRollCount(int32 Requested, int32 ServerAmount)
{
	return FMath::Clamp(ServerAmount, 0, FMath::Max(0, Requested));
}

bool URecruitmentManagerSubsystem::ShouldFallbackToDiamond(int32 HttpStatus, FName Code, bool bAllowDiamondFallback)
{
	return bAllowDiamondFallback && HttpStatus == 409 && Code == CloudErr::InsufficientItems;
}

bool URecruitmentManagerSubsystem::ShouldSkipLocalEffect(bool bReplayed)
{
	return bReplayed;
}

bool URecruitmentManagerSubsystem::ShouldGrantLocalMileage(bool bServerAuthoritative)
{
	return !bServerAuthoritative;
}

bool URecruitmentManagerSubsystem::AllowsDiamondFallback(EGachaTier Tier, int32 RequestedCount)
{
	return RequestedCount == 1 && !GachaSpendReason(Tier).IsNone();
}

FName URecruitmentManagerSubsystem::GachaActionKey(EGachaTier Tier)
{
	return FName(*FString::Printf(TEXT("gacha.recruit.tier%d"), static_cast<int32>(Tier)));
}

FName URecruitmentManagerSubsystem::GachaSpendReason(EGachaTier Tier)
{
	switch (Tier)
	{
	case EGachaTier::Advanced:
		return SpendReason_RecruitAdvanced;
	case EGachaTier::Premium:
		return SpendReason_RecruitPremium;
	default:
		// 일반은 채용권 전용 ― 다이아 경로가 없다
		return NAME_None;
	}
}

void URecruitmentManagerSubsystem::RequestGachaPull(EGachaTier Tier, int32 BuildingIndex, int32 RequestedCount,
	FOnGachaPullDone Done, EProductionDiscipline PrimaryDiscipline, bool bAllowDiamondFallback)
{
	if (static_cast<uint8>(PrimaryDiscipline) > static_cast<uint8>(EProductionDiscipline::Count))
	{
		UE_LOG(LogTemp, Error, TEXT("[RecruitmentManager] Invalid primary discipline for gacha pull: %d"),
			static_cast<int32>(PrimaryDiscipline));
		Done.ExecuteIfBound(false, TArray<FGachaResultData>());
		return;
	}

	const int32 Count = FMath::Clamp(RequestedCount, 1, GachaBatchMath::MaxBatchPull);
	const FName ActionKey = GachaActionKey(Tier);

	UCloudEconomySubsystem* Cloud = GetCloudEconomy();
	if (!Cloud)
	{
		UE_LOG(LogTemp, Error, TEXT("[RecruitmentManager] 뽑기 실패 - CloudEconomySubsystem 없음. Tier=%d"),
			static_cast<int32>(Tier));
		Done.ExecuteIfBound(false, TArray<FGachaResultData>());
		return;
	}

	// 중복 클릭 ― 내가 잡은 pending 이 아니므로 풀지 않는다
	if (!Cloud->BeginPendingAction(ActionKey))
	{
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] 뽑기 요청 중복 - 진행 중. Tier=%d"), static_cast<int32>(Tier));
		Done.ExecuteIfBound(false, TArray<FGachaResultData>());
		return;
	}

	// ① 로컬 캐시로 경로만 고른다 ― 차감은 서버가 한다
	const bool bAllowDiamond = bAllowDiamondFallback && AllowsDiamondFallback(Tier, Count);
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	const bool bHasTicket = ItemMgr && ItemMgr->HasItem(GetTicketTypeForTier(Tier), Count);

	switch (ResolvePullRoute(bHasTicket, bAllowDiamond))
	{
	case EGachaPullRoute::Ticket:
		SendConsumeThenRoll(Tier, BuildingIndex, Count, ActionKey, bAllowDiamond, Done, PrimaryDiscipline);
		break;
	case EGachaPullRoute::Diamond:
		SendSpendThenRoll(Tier, BuildingIndex, ActionKey, Done, PrimaryDiscipline);
		break;
	default:
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] 뽑기 요청 미전송 - 티켓·다이아 모두 불가. Tier=%d Count=%d"),
			static_cast<int32>(Tier), Count);
		FinishServerPull(Tier, BuildingIndex, /*bOk=*/false, /*Rolls=*/0, /*bReplayed=*/false, ActionKey, Done, PrimaryDiscipline);
		break;
	}
}

void URecruitmentManagerSubsystem::SendConsumeThenRoll(EGachaTier Tier, int32 BuildingIndex, int32 RequestedCount,
	FName ActionKey, bool bAllowDiamond, FOnGachaPullDone Done, EProductionDiscipline PrimaryDiscipline)
{
	UCloudEconomySubsystem* Cloud = GetCloudEconomy();
	if (!Cloud)
	{
		FinishServerPull(Tier, BuildingIndex, /*bOk=*/false, /*Rolls=*/0, /*bReplayed=*/false, ActionKey, Done, PrimaryDiscipline);
		return;
	}

	const FName ItemName = FCloudEconomyData::ItemTypeToName(GetTicketTypeForTier(Tier));

	// 응답 전에 서브시스템이 내려가면 CreateLambda 는 죽은 this 를 만진다
	Cloud->RequestConsume(ConsumeReason_RecruitPull, ItemName, RequestedCount,
		UCloudEconomySubsystem::FOnCloudOp::CreateWeakLambda(this,
			[this, Tier, BuildingIndex, RequestedCount, ActionKey, bAllowDiamond, Done, PrimaryDiscipline](const FCloudOpResult& OpResult)
	{
		if (!OpResult.bOk)
		{
			// 409 만 폴백한다 ― 타임아웃에서 폴백하면 티켓과 다이아를 둘 다 낸다(새 opId 라 멱등이 못 막는다)
			if (ShouldFallbackToDiamond(OpResult.HttpStatus, OpResult.Code, bAllowDiamond))
			{
				// pending 을 이어받는다 ― 풀고 다시 잡으면 그 틈에 두 번째 클릭이 들어온다
				SendSpendThenRoll(Tier, BuildingIndex, ActionKey, Done, PrimaryDiscipline);
				return;
			}
			FinishServerPull(Tier, BuildingIndex, /*bOk=*/false, /*Rolls=*/0, OpResult.bReplayed, ActionKey, Done, PrimaryDiscipline);
			return;
		}

		// 요청값이 아니라 서버가 인정한 수량만큼 돌린다
		int32 ServerAmount = 0;
		if (OpResult.Result.IsValid())
		{
			OpResult.Result->TryGetNumberField(TEXT("amount"), ServerAmount);
		}

		FinishServerPull(Tier, BuildingIndex, /*bOk=*/true, ResolveRollCount(RequestedCount, ServerAmount),
			OpResult.bReplayed, ActionKey, Done, PrimaryDiscipline);
	}));
}

void URecruitmentManagerSubsystem::SendSpendThenRoll(EGachaTier Tier, int32 BuildingIndex, FName ActionKey,
	FOnGachaPullDone Done, EProductionDiscipline PrimaryDiscipline)
{
	UCloudEconomySubsystem* Cloud = GetCloudEconomy();
	const FName Reason = GachaSpendReason(Tier);
	const int32 Cost = GetDiamondCost(Tier);
	if (!Cloud || Reason.IsNone() || Cost <= 0)
	{
		FinishServerPull(Tier, BuildingIndex, /*bOk=*/false, /*Rolls=*/0, /*bReplayed=*/false, ActionKey, Done, PrimaryDiscipline);
		return;
	}

	// 프리미엄만 부수효과가 있다 ― meta.count 가 없으면 서버가 400 을 준다
	FCloudMeta Meta;
	if (Reason == SpendReason_RecruitPremium)
	{
		Meta.Add(TEXT("count"), FCloudMetaValue::FromInt(PremiumSpendMileageCount));
	}

	Cloud->RequestSpend(CloudCurrency::Diamond, Cost, Reason, Meta,
		UCloudEconomySubsystem::FOnCloudOp::CreateWeakLambda(this,
			[this, Tier, BuildingIndex, ActionKey, Done, PrimaryDiscipline](const FCloudOpResult& OpResult)
	{
		if (!OpResult.bOk)
		{
			FinishServerPull(Tier, BuildingIndex, /*bOk=*/false, /*Rolls=*/0, OpResult.bReplayed, ActionKey, Done, PrimaryDiscipline);
			return;
		}

		// 다이아 경로는 단일 뽑기 전용이다(배치는 bAllowDiamondFallback=false)
		FinishServerPull(Tier, BuildingIndex, /*bOk=*/true, /*Rolls=*/1, OpResult.bReplayed, ActionKey, Done, PrimaryDiscipline);
	}));
}

void URecruitmentManagerSubsystem::FinishServerPull(EGachaTier Tier, int32 BuildingIndex, bool bOk, int32 Rolls,
	bool bReplayed, FName ActionKey, FOnGachaPullDone Done, EProductionDiscipline PrimaryDiscipline)
{
	TArray<FGachaResultData> Results;

	if (bOk && !ShouldSkipLocalEffect(bReplayed) && Rolls > 0)
	{
		// 마일리지는 서버가 적립하고 이 콜백 전에 지갑이 적용됐다 ― 로컬이 또 올리면 이중 계상이다
		const bool bGrantMileage = ShouldGrantLocalMileage(/*bServerAuthoritative=*/true);

		Results.Reserve(Rolls);
		for (int32 i = 0; i < Rolls; ++i)
		{
			FGachaResultData Rolled;
			ApplyLocalPullEffect(Tier, BuildingIndex, Rolled, bGrantMileage, PrimaryDiscipline);
			Results.Add(Rolled);
		}

		SaveGameData();
	}

	if (UCloudEconomySubsystem* Cloud = GetCloudEconomy())
	{
		Cloud->EndPendingAction(ActionKey);
	}

	Done.ExecuteIfBound(bOk, Results);
}

int32 URecruitmentManagerSubsystem::GetFreeCapacity(int32 BuildingIndex) const
{
	UEmployeeManager* EmpMgr = GetEmployeeManager();
	if (!EmpMgr)
	{
		return 0;
	}

	const int32 Capacity = EmpMgr->GetBuildingEmployeeCapacity(BuildingIndex, /*bLogIfZero=*/false);
	return FMath::Max(0, Capacity - EmpMgr->GetEmployeeCountInBuilding(BuildingIndex));
}

bool URecruitmentManagerSubsystem::ExecuteGachaPullBatch(EGachaTier Tier, int32 RequestedCount, int32 BuildingIndex,
	TArray<FGachaResultData>& OutResults)
{
	OutResults.Reset();

	UItemInventoryManager* ItemMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UItemInventoryManager>() : nullptr;
	if (!ItemMgr)
	{
		return false;
	}

	// 실행 시점 재클램프 — 라벨 계산과 클릭 사이의 상태 변화 방어
	const int32 Count = GachaBatchMath::ComputeBatchPullCount(
		ItemMgr->GetItemCount(GetTicketTypeForTier(Tier)), GetFreeCapacity(BuildingIndex),
		FMath::Min(RequestedCount, GachaBatchMath::MaxBatchPull));

	for (int32 i = 0; i < Count; ++i)
	{
		FGachaResultData Result;
		// 티켓 전용(다이아 폴백 차단) — 클램프가 있어 정상 경로에선 폴백 조건이 안 되지만 이중 방어
		if (!ExecuteGachaPull(Tier, BuildingIndex, Result, /*bShouldSave=*/false, /*bAllowDiamondFallback=*/false))
		{
			break;   // 성공분만 반환
		}
		OutResults.Add(Result);
	}

	if (OutResults.Num() > 0)
	{
		SaveGameData();
	}
	return OutResults.Num() > 0;
}

bool URecruitmentManagerSubsystem::ConfirmGachaHire(const FGachaResultData& ResultData, AWorkstationActorBase* PreferredWorkstation,
	bool bShouldSave)
{
	return ConfirmGachaHireInternal(ResultData, PreferredWorkstation, bShouldSave, false);
}

bool URecruitmentManagerSubsystem::ConfirmGachaHireInternal(const FGachaResultData& ResultData,
	AWorkstationActorBase* PreferredWorkstation, bool bShouldSave, bool bBatchHire)
{
	UEmployeeManager* EmpManager = GetEmployeeManager();
	if (!EmpManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] ConfirmGachaHire failed - EmployeeManager is null"));
		return false;
	}

	const int32 BuildingIndex = ResultData.TargetBuildingIndex;

	// pull-to-pool: 좌석을 점유하지 않고 건물 미배치 풀에 적립.
	// (좌석 배정은 책상 패널(WorkstationInfoWidget)에서 별도로 수행 → 좌석수 카운터도 거기서 관리)
	if (!EmpManager->HireEmployeeFromCardInternal(ResultData.ResultEmployee, BuildingIndex, bShouldSave, bBatchHire))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] ConfirmGachaHire failed - HireEmployeeFromCard returned false"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Gacha hire confirmed - Employee %d benched in building %d (pool)"),
		ResultData.ResultEmployee.EmployeeID, BuildingIndex);

	// 사원증 사번 — 건물별 누적 채용 순번 (단조 증가, SaveGame)
	if (FOfficeRecruitmentData* OfficeData = GetOrCreateOfficeData(BuildingIndex))
	{
		++OfficeData->TotalHiredCount;
	}

	// M4 RecruitEmployees 진행 신호 (튜토리얼 가이드). bCurrentBanked 가드로 정확히 1회.
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyHireConfirmed(ResultData);
	}

	// 자동 착석 — 미션 페이즈 전이(NotifyHireConfirmed) 후에 착석 신호가 나가도록 순서 고정.
	// 실패(만석/오피스 밖)는 벤치 유지 = 기존 pull-to-pool 폴백.
	if (UWorld* CurrentWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		if (UOfficeManager* OfficeMgr = CurrentWorld->GetSubsystem<UOfficeManager>())
		{
			OfficeMgr->AutoSeatEmployee(ResultData.ResultEmployee.EmployeeID, PreferredWorkstation, bShouldSave);
		}
	}

	return true;
}

bool URecruitmentManagerSubsystem::ConfirmGachaHireBatch(const TArray<FGachaResultData>& Results,
	AWorkstationActorBase* PreferredWorkstation, int32& OutSeatedCount, int32& OutBenchedCount,
	bool bAllowPortraitCaptureInProgress)
{
	OutSeatedCount = 0;
	OutBenchedCount = 0;

	UEmployeeManager* EmpMgr = GetEmployeeManager();
	if (!EmpMgr || Results.IsEmpty())
	{
		return false;
	}

	// 사진 큐는 ID와 외형값을 복사해 보관한다. 오피스는 사진 완료보다 채용 저장을 우선한다.
	if (!bAllowPortraitCaptureInProgress && EmpMgr->IsPortraitCapturing())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] Batch hire blocked: portrait capture in progress"));
		return false;
	}

	TMap<int32, int32> RequestedByBuilding;
	TSet<int32> EmployeeIDs;
	for (const FGachaResultData& Result : Results)
	{
		const int32 EmployeeID = Result.ResultEmployee.EmployeeID;
		if (EmployeeIDs.Contains(EmployeeID) || EmpMgr->GetEmployeeData(EmployeeID))
		{
			UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] Batch hire blocked: duplicate employee ID %d"), EmployeeID);
			return false;
		}
		EmployeeIDs.Add(EmployeeID);
		++RequestedByBuilding.FindOrAdd(Result.TargetBuildingIndex);
	}
	for (const TPair<int32, int32>& Request : RequestedByBuilding)
	{
		const int32 CurrentCount = EmpMgr->GetEmployeeCountInBuilding(Request.Key);
		const int32 Capacity = EmpMgr->GetBuildingEmployeeCapacity(Request.Key);
		if (Capacity <= 0 || Request.Value > Capacity - CurrentCount)
		{
			UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] Batch hire blocked: building %d capacity %d/%d, requested %d"),
				Request.Key, CurrentCount, Capacity, Request.Value);
			if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
			{
				UIMgr->ShowNotification(UEmployeeManager::GetCapacityFullMessage(), 3.0f, ENotificationType::Failed);
			}
			return false;
		}
	}

	bool bAll = true;

	for (int32 i = 0; i < Results.Num(); ++i)
	{
		// 뽑기 순서 = 사번 순서 (카드 ID NO 표시와 일치해야 함)
		if (!ConfirmGachaHireInternal(Results[i], i == 0 ? PreferredWorkstation : nullptr,
			/*bShouldSave=*/false, /*bBatchHire=*/true))
		{
			bAll = false;
			continue;
		}

		const FEmployeeInstance* HiredEmployee = EmpMgr ? EmpMgr->GetEmployeeData(Results[i].ResultEmployee.EmployeeID) : nullptr;
		if (HiredEmployee && HiredEmployee->bIsAssigned)
		{
			++OutSeatedCount;
		}
		else
		{
			++OutBenchedCount;
		}
	}

	if (OutSeatedCount + OutBenchedCount > 0)
	{
		// 저장 실패에도 이미 바뀐 메모리 로스터는 즉시 갱신한다.
		EmpMgr->OnEmployeeRosterChanged.Broadcast();
		return SaveGameData() && bAll;
	}
	return false;
}

int32 URecruitmentManagerSubsystem::GetTotalHiredCount(int32 BuildingIndex) const
{
	const FOfficeRecruitmentData* Found = OfficeRecruitmentMap.Find(BuildingIndex);
	return Found ? Found->TotalHiredCount : 0;
}

void URecruitmentManagerSubsystem::DiscardGachaResult()
{
	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Gacha result discarded"));
}

// ========================================================================
// 천장(Pity) / 마일리지
// ========================================================================

int32 URecruitmentManagerSubsystem::GetPityCount(EGachaTier Tier) const
{
	switch (Tier)
	{
	case EGachaTier::Advanced:
		return GachaData.AdvancedPity.PullsSinceLastGuaranteed;
	case EGachaTier::Premium:
		return GachaData.PremiumPity.PullsSinceLastGuaranteed;
	default:
		return 0;
	}
}

int32 URecruitmentManagerSubsystem::GetMileagePoints() const
{
	return GachaData.Mileage.CurrentPoints;
}

bool URecruitmentManagerSubsystem::ExchangeMileage(int32 BuildingIndex, FGachaResultData& OutResult)
{
	if (GachaData.Mileage.CurrentPoints < FGachaMileageData::ExchangeCost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] ExchangeMileage failed - Not enough points (%d/%d)"),
			GachaData.Mileage.CurrentPoints, FGachaMileageData::ExchangeCost);
		return false;
	}

	// 마일리지 차감
	GachaData.Mileage.CurrentPoints -= FGachaMileageData::ExchangeCost;

	// Legendary 확정 직원 생성
	FEmployeeInstance NewEmployee = GenerateGachaEmployee(ELootBoxRarity::Legendary, BuildingIndex);

	// 결과 구성
	OutResult.ResultEmployee = NewEmployee;
	OutResult.PotentialRarity = ELootBoxRarity::Legendary;
	OutResult.UsedTier = EGachaTier::Premium;
	OutResult.bWasPityGuaranteed = false;
	OutResult.TargetBuildingIndex = BuildingIndex;

	OnMileageChanged.Broadcast(GachaData.Mileage.CurrentPoints);

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Mileage exchanged - Legendary employee generated. Remaining points: %d"),
		GachaData.Mileage.CurrentPoints);

	SaveGameData();
	return true;
}

bool URecruitmentManagerSubsystem::SpendMileage(int32 Points, bool bShouldSave)
{
	if (Points <= 0 || GachaData.Mileage.CurrentPoints < Points)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] SpendMileage failed - Not enough points (%d/%d)"),
			GachaData.Mileage.CurrentPoints, Points);
		return false;
	}

	GachaData.Mileage.CurrentPoints -= Points;
	OnMileageChanged.Broadcast(GachaData.Mileage.CurrentPoints);

	if (bShouldSave)
	{
		SaveGameData();
	}
	return true;
}

// ========================================================================
// HR 파워
// ========================================================================

int32 URecruitmentManagerSubsystem::GetHRPower(int32 BuildingIndex) const
{
	UEmployeeManager* EmpManager = GetEmployeeManager();
	if (!EmpManager)
	{
		return 0;
	}

	TArray<FEmployeeInstance> Employees = EmpManager->GetEmployeesInBuilding(BuildingIndex);

	int32 TotalHRPower = 0;
	for (const FEmployeeInstance& Employee : Employees)
	{
		if (Employee.Department == EEmployeeDepartment::HR)
		{
			TotalHRPower += Employee.EnhancementLevel;
		}
	}

	// HR 특성 — 개수 가산(FlatCount). 배율이 아니므로 GetTraitFactor 를 쓰지 않는다.
	if (UBuildingTraitManagerSubsystem* HRTraitMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr)
	{
		TotalHRPower += FMath::RoundToInt(HRTraitMgr->GetAggregatedTraitPercent(BuildingIndex, EBuildingTraitTarget::HRPower));
	}

	return TotalHRPower;
}

// ========================================================================
// 확률 테이블 & 등급 롤
// ========================================================================

TArray<TPair<ELootBoxRarity, float>> URecruitmentManagerSubsystem::GetProbabilityTable(EGachaTier Tier, int32 HRPower) const
{
	TArray<TPair<ELootBoxRarity, float>> Table;

	switch (Tier)
	{
	case EGachaTier::Normal:
	{
		// HR 파워별 일반 뽑기 확률 (Legendary 절대 불가, Epic이 최고 천장)
		if (HRPower >= 50)
		{
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Common,  0.55f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Unusual, 0.30f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Rare,    0.13f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Epic,    0.02f));
		}
		else if (HRPower >= 30)
		{
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Common,  0.63f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Unusual, 0.25f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Rare,    0.11f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Epic,    0.01f));
		}
		else if (HRPower >= 20)
		{
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Common,  0.73f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Unusual, 0.20f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Rare,    0.07f));
		}
		else if (HRPower >= 10)
		{
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Common,  0.85f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Unusual, 0.12f));
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Rare,    0.03f));
		}
		else
		{
			// HR 파워 0 (기본) → Common만 나옴
			Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Common, 1.00f));
		}
		break;
	}
	case EGachaTier::Advanced:
	{
		// 고급 뽑기 (천장=Legendary, RECRUITMENT_SYSTEM.md 표 일치)
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Common,    0.10f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Unusual,   0.40f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Rare,      0.35f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Epic,      0.13f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Legendary, 0.02f));
		break;
	}
	case EGachaTier::Premium:
	{
		// 프리미엄 뽑기 (천장=Legendary, Common 없음, Mythic 불가 → 큐브 전용, RECRUITMENT_SYSTEM.md 표 일치)
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Unusual,   0.15f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Rare,      0.40f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Epic,      0.30f));
		Table.Add(TPair<ELootBoxRarity, float>(ELootBoxRarity::Legendary, 0.15f));
		break;
	}
	}

	return Table;
}

TArray<FGachaRarityChance> URecruitmentManagerSubsystem::GetProbabilityTableForUI(EGachaTier Tier, int32 HRPower) const
{
	TArray<FGachaRarityChance> Result;
	const TArray<TPair<ELootBoxRarity, float>> Internal = GetProbabilityTable(Tier, HRPower);
	for (const TPair<ELootBoxRarity, float>& Pair : Internal)
	{
		FGachaRarityChance Row;
		Row.Rarity = Pair.Key;
		Row.Percent = Pair.Value * 100.f;   // 0~1 → 0~100
		Result.Add(Row);
	}
	return Result;
}

ELootBoxRarity URecruitmentManagerSubsystem::RollPotentialRarity(EGachaTier Tier, int32 HRPower)
{
	// 고급/프리미엄 모두 Legendary
	ELootBoxRarity GuaranteedRarity = ELootBoxRarity::Common;
	FGachaPityData* PityDataPtr = nullptr;

	if (Tier == EGachaTier::Advanced)
	{
		GuaranteedRarity = ELootBoxRarity::Legendary;
		PityDataPtr = &GachaData.AdvancedPity;
	}
	else if (Tier == EGachaTier::Premium)
	{
		GuaranteedRarity = ELootBoxRarity::Legendary;
		PityDataPtr = &GachaData.PremiumPity;
	}

	// Hard Pity 확인 (고급/프리미엄만)
	if (PityDataPtr)
	{
		if (PityDataPtr->PullsSinceLastGuaranteed + 1 >= FGachaPityData::HardPity)
		{
			UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Hard Pity triggered - Tier=%d, Pull=%d, Guaranteed=%d"),
				static_cast<int32>(Tier), PityDataPtr->PullsSinceLastGuaranteed + 1, static_cast<int32>(GuaranteedRarity));
			return GuaranteedRarity;
		}
	}

	// 확률 테이블 가져오기
	TArray<TPair<ELootBoxRarity, float>> Table = GetProbabilityTable(Tier, HRPower);

	// Soft Pity 보정 (고급/프리미엄, 40회 이후 → 보장등급 확률 증가)
	if (PityDataPtr)
	{
		int32 PullCount = PityDataPtr->PullsSinceLastGuaranteed + 1;

		if (PullCount > FGachaPityData::SoftPityStart)
		{
			float BonusChance = static_cast<float>(PullCount - FGachaPityData::SoftPityStart) * FGachaPityData::SoftPityBonusPerPull;

			// 보장등급 확률 찾아서 증가
			int32 TargetIdx = INDEX_NONE;

			for (int32 i = 0; i < Table.Num(); ++i)
			{
				if (Table[i].Key == GuaranteedRarity)
				{
					TargetIdx = i;
					break;
				}
			}

			if (TargetIdx == INDEX_NONE)
			{
				Table.Add(TPair<ELootBoxRarity, float>(GuaranteedRarity, 0.f));
				TargetIdx = Table.Num() - 1;
			}

			Table[TargetIdx].Value += BonusChance;

			// 하위 등급에서 비례적으로 감산
			for (int32 i = 0; i < Table.Num(); ++i)
			{
				if (i != TargetIdx && Table[i].Key < GuaranteedRarity)
				{
					float Reduce = FMath::Min(Table[i].Value * 0.5f, BonusChance * Table[i].Value);
					Table[i].Value -= Reduce;
				}
			}

			UE_LOG(LogTemp, Verbose, TEXT("[RecruitmentManager] Soft Pity active: Pull %d, Bonus %.2f%% for rarity %d"),
				PullCount, BonusChance * 100.f, static_cast<int32>(GuaranteedRarity));
		}
	}

	// 확률 합계 정규화
	float TotalProb = 0.f;
	for (const auto& Entry : Table)
	{
		TotalProb += Entry.Value;
	}

	// 가중치 랜덤 선택
	float Roll = FMath::FRand() * TotalProb;
	float Accumulated = 0.f;

	for (const auto& Entry : Table)
	{
		Accumulated += Entry.Value;
		if (Roll < Accumulated)
		{
			return Entry.Key;
		}
	}

	// 폴백 (첫 번째 엔트리)
	return Table.Num() > 0 ? Table[0].Key : ELootBoxRarity::Common;
}

void URecruitmentManagerSubsystem::SeedForcedPrimaryDisciplines(const TArray<EProductionDiscipline>& InDisciplines)
{
	ForcedPrimaryQueue = InDisciplines;
}

FEmployeeInstance URecruitmentManagerSubsystem::GenerateGachaEmployee(ELootBoxRarity PotentialRarity, int32 BuildingIndex,
	EProductionDiscipline PrimaryDiscipline)
{
	FEmployeeInstance NewEmployee;

	UEmployeeManager* EmpManager = GetEmployeeManager();
	const int32 DisciplineCount = EmpManager
		? GetDisciplineCountForCompany(EmpManager->GetCompanyTypeForBuilding(BuildingIndex))
		: static_cast<int32>(EProductionDiscipline::Count);

	// 고유 ID 생성
	NewEmployee.EmployeeID = EmpManager ? EmpManager->AllocateNextInstanceID() : FMath::RandRange(10000, 99999);

	// 생산 직능 프로필 롤 — 산업별 3/6 활성 분야 안에서 선택한다.
	EProductionDiscipline PrimaryDisc = PrimaryDiscipline;
	if (PrimaryDisc == EProductionDiscipline::Count && ForcedPrimaryQueue.Num() > 0)
	{
		PrimaryDisc = ForcedPrimaryQueue[0];
		ForcedPrimaryQueue.RemoveAt(0);
	}
	else if (PrimaryDisc == EProductionDiscipline::Count)
	{
		PrimaryDisc = static_cast<EProductionDiscipline>(
			FMath::RandRange(0, DisciplineCount - 1));
	}
	else if (static_cast<int32>(PrimaryDisc) >= DisciplineCount)
	{
		PrimaryDisc = static_cast<EProductionDiscipline>(static_cast<int32>(PrimaryDisc) % DisciplineCount);
	}
	NewEmployee.DisciplinePoints = MakeDisciplineProfile(PrimaryDisc, PotentialRarity, DisciplineCount);
	SyncDerivedDepartment(NewEmployee);

	// 성별 랜덤
	NewEmployee.Gender = FMath::RandBool() ? EEmployeeGender::Male : EEmployeeGender::Female;

	// +0 인턴 (가챠 직원은 항상 +0에서 시작)
	NewEmployee.EnhancementLevel = 0;
	NewEmployee.Level = 1;

	// 고용일 설정
	NewEmployee.HiredDate = FDateTime::Now();

	// 잠재능력 설정 (CurrentRarity = 현재 등급, MaxAchievedRarity = 최고 달성 등급)
	NewEmployee.PotentialAbility.CurrentRarity = PotentialRarity;
	NewEmployee.PotentialAbility.MaxAchievedRarity = PotentialRarity;

	// 입사 시점 레어도 고정 (이름 핸들 tier 의 영구 앵커 — 큐브로 변하는 PotentialAbility 와 분리)
	NewEmployee.SpawnRarity = PotentialRarity;

	// 시작 큐브 줄 — 가챠 등급 그대로 생성 (리롤 티어 굴림 없이, 등급=가챠 로망의 실체)
	{
		const int32 Slots = UEmployeePotentialHelper::GetPotentialSlotsForRank(
			UEmployeeTypeHelper::GetRankFromEnhancementLevel(NewEmployee.EnhancementLevel));
		NewEmployee.PotentialAbility.Options.Empty();
		for (int32 i = 0; i < Slots; ++i)
		{
			NewEmployee.PotentialAbility.Options.Add(
				UEmployeePotentialHelper::GenerateRandomPotentialOption(PotentialRarity));
		}
	}

	// 이름 생성
	if (EmpManager)
	{
		NewEmployee.EmployeeName = EmpManager->GenerateRandomName(NewEmployee.SpawnRarity);
	}
	else
	{
		NewEmployee.EmployeeName = FString::Printf(TEXT("Employee_%d"), NewEmployee.EmployeeID);
	}

	// 초기 스탯 — 총량 30 랜덤 분배(최소 3). 등급은 관여하지 않는다(뾰족함은 직능 총량 전담).
	NewEmployee.Stats = FEmployeeStats();
	ApplyStatProfile(NewEmployee.Stats, MakeStatProfile());

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Generated gacha employee - ID=%d, Name=%s, Dept=%d, Potential=%d"),
		NewEmployee.EmployeeID, *NewEmployee.EmployeeName,
		static_cast<int32>(NewEmployee.Department), static_cast<int32>(PotentialRarity));

	return NewEmployee;
}

// ========================================================================
// 오피스 채용 데이터 초기화
// ========================================================================

void URecruitmentManagerSubsystem::InitializeOfficeRecruitment(int32 BuildingIndex)
{
	if (OfficeRecruitmentMap.Contains(BuildingIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] Office already initialized: %d"), BuildingIndex);
		return;
	}

	FOfficeRecruitmentData NewData(BuildingIndex);
	OfficeRecruitmentMap.Add(BuildingIndex, NewData);

	UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Initialized office recruitment: %d"), BuildingIndex);
	SaveGameData();
}

void URecruitmentManagerSubsystem::RemoveOfficeRecruitment(int32 BuildingIndex)
{
	if (OfficeRecruitmentMap.Remove(BuildingIndex) > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[RecruitmentManager] Removed office recruitment: %d"), BuildingIndex);
		SaveGameData();
	}
}

FOfficeRecruitmentData* URecruitmentManagerSubsystem::GetOrCreateOfficeData(int32 BuildingIndex)
{
	if (FOfficeRecruitmentData* ExistingData = OfficeRecruitmentMap.Find(BuildingIndex))
	{
		return ExistingData;
	}

	UE_LOG(LogTemp, Warning, TEXT("[RecruitmentManager] GetOrCreateOfficeData - Creating NEW data for Index %d"), BuildingIndex);

	FOfficeRecruitmentData NewData(BuildingIndex);
	OfficeRecruitmentMap.Add(BuildingIndex, NewData);

	return OfficeRecruitmentMap.Find(BuildingIndex);
}
