// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Data/GameSaveData.h"
#include "Data/BuildingSaveData.h"
#include "Data/EntitySaveData.h"
#include "Table/BuildingData.h"
#include "Enum/ItemType.h"
#include "Enum/ResourceType.h"
#include "Enum/BuildingTraitRequirement.h"
#include "Enum/CompanyType.h"
#include "Enum/BuildingTraitTarget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Manager/KeystoneAuraSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UBuildingTraitManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[BuildingTraitManager] Initialized"));
}

void UBuildingTraitManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ========================================================================
// 캐시된 서브시스템 접근자
// ========================================================================

UTableManagerSubsystem* UBuildingTraitManagerSubsystem::GetTableManager() const
{
	if (!TableManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			TableManager = GI->GetSubsystem<UTableManagerSubsystem>();
		}
	}
	return TableManager;
}

UItemInventoryManager* UBuildingTraitManagerSubsystem::GetItemInventoryManager() const
{
	if (!ItemInventoryManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			ItemInventoryManager = GI->GetSubsystem<UItemInventoryManager>();
		}
	}
	return ItemInventoryManager;
}

USaveLoadManager* UBuildingTraitManagerSubsystem::GetSaveLoadManager() const
{
	if (!SaveLoadManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			SaveLoadManager = GI->GetSubsystem<USaveLoadManager>();
		}
	}
	return SaveLoadManager;
}

UResourceItemManager* UBuildingTraitManagerSubsystem::GetResourceManager() const
{
	if (!ResourceManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			ResourceManager = GI->GetSubsystem<UResourceItemManager>();
		}
	}
	return ResourceManager;
}

// ========================================================================
// 인벤토리
// ========================================================================

int32 UBuildingTraitManagerSubsystem::GetTraitCount(FName TraitID) const
{
	if (TraitID.IsNone()) return 0;
	const int32* Found = Inventory.OwnedTraits.Find(TraitID);
	return Found ? *Found : 0;
}

bool UBuildingTraitManagerSubsystem::HasTrait(FName TraitID, int32 Amount) const
{
	return GetTraitCount(TraitID) >= Amount;
}

void UBuildingTraitManagerSubsystem::GrantTrait(FName TraitID, int32 Amount, bool bShouldSave)
{
	if (TraitID.IsNone() || Amount <= 0) return;

	int32& Count = Inventory.OwnedTraits.FindOrAdd(TraitID, 0);
	Count += Amount;

	TryRegisterEncyclopedia(TraitID);
	OnTraitInventoryChanged.Broadcast(TraitID, Count);

	if (bShouldSave) SaveGameData();
}

bool UBuildingTraitManagerSubsystem::RemoveTrait(FName TraitID, int32 Amount, bool bShouldSave)
{
	if (TraitID.IsNone() || Amount <= 0) return false;
	if (!HasTrait(TraitID, Amount)) return false;

	// Remove() 후 Count 참조가 무효화되므로 새 카운트를 로컬에 먼저 확정한다.
	const int32 NewCount = Inventory.OwnedTraits.FindChecked(TraitID) - Amount;
	if (NewCount <= 0)
	{
		Inventory.OwnedTraits.Remove(TraitID);
	}
	else
	{
		Inventory.OwnedTraits[TraitID] = NewCount;
	}

	OnTraitInventoryChanged.Broadcast(TraitID, FMath::Max(0, NewCount));
	if (bShouldSave) SaveGameData();
	return true;
}

// ========================================================================
// 도감
// ========================================================================

bool UBuildingTraitManagerSubsystem::IsTraitInEncyclopedia(FName TraitID) const
{
	return Inventory.EncyclopediaUnlocked.Contains(TraitID);
}

int32 UBuildingTraitManagerSubsystem::GetEncyclopediaCount() const
{
	return Inventory.EncyclopediaUnlocked.Num();
}

void UBuildingTraitManagerSubsystem::TryRegisterEncyclopedia(FName TraitID)
{
	if (TraitID.IsNone()) return;
	if (!Inventory.EncyclopediaUnlocked.Contains(TraitID))
	{
		Inventory.EncyclopediaUnlocked.Add(TraitID);
		OnEncyclopediaProgressed.Broadcast(TraitID);
	}
}

// ========================================================================
// 슬롯 (장착 / 교체 / 해제)
// ========================================================================

FBuildingTraitSlotData* UBuildingTraitManagerSubsystem::GetOrCreateSlotData(int32 BuildingIndex)
{
	USaveLoadManager* SLMgr = GetSaveLoadManager();
	if (!SLMgr) return nullptr;

	USaveGame_GameData* SaveData = SLMgr->GetCurrentSaveData();
	if (!SaveData) return nullptr;

	FOfficeSaveData& OfficeData = SaveData->GameData.OfficeDataMap.FindOrAdd(BuildingIndex);
	// 슬롯 길이를 용량으로 정규화. 축소 방향이면 초과분 장착은 인벤토리 복귀 없이 소멸한다(프로토타입 와이프 전제).
	if (OfficeData.TraitSlots.EquippedTraits.Num() != FBuildingTraitSlotData::MaxBuildingTraitSlots)
	{
		OfficeData.TraitSlots.EquippedTraits.SetNum(FBuildingTraitSlotData::MaxBuildingTraitSlots);
	}
	return &OfficeData.TraitSlots;
}

const FBuildingTraitSlotData* UBuildingTraitManagerSubsystem::FindSlotData(int32 BuildingIndex) const
{
	USaveLoadManager* SLMgr = GetSaveLoadManager();
	if (!SLMgr) return nullptr;

	USaveGame_GameData* SaveData = SLMgr->GetCurrentSaveData();
	if (!SaveData) return nullptr;

	// 읽기 전용: 없으면 nullptr (FindOrAdd 미사용 → phantom 엔트리 생성 안 함)
	if (FOfficeSaveData* OfficeData = SaveData->GameData.OfficeDataMap.Find(BuildingIndex))
	{
		return &OfficeData->TraitSlots;
	}
	return nullptr;
}

TArray<FName> UBuildingTraitManagerSubsystem::GetEquippedTraits(int32 BuildingIndex) const
{
	if (const FBuildingTraitSlotData* SlotData = FindSlotData(BuildingIndex))
	{
		// 반환 사본만 5칸으로 패딩(구 엔트리 대비) — 저장 원본은 불변
		TArray<FName> Result = SlotData->EquippedTraits;
		if (Result.Num() < FBuildingTraitSlotData::MaxBuildingTraitSlots)
		{
			Result.SetNum(FBuildingTraitSlotData::MaxBuildingTraitSlots);
		}
		return Result;
	}
	TArray<FName> Empty;
	Empty.Init(NAME_None, FBuildingTraitSlotData::MaxBuildingTraitSlots);
	return Empty;
}

bool UBuildingTraitManagerSubsystem::CanEquipTraitToBuilding(int32 BuildingIndex, FName TraitID) const
{
	if (TraitID.IsNone()) return false;

	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return false;

	FBuildingTraitTableRow TraitRow;
	if (!TMgr->GetBuildingTraitData(TraitID, TraitRow)) return false;

	// 타입 전용 특성은 빌딩 산업 분류와 매칭
	if (TraitRow.RequiredType == EBuildingTraitRequirement::None) return true;

	USaveLoadManager* SLMgr = GetSaveLoadManager();
	if (!SLMgr) return false;

	USaveGame_GameData* SaveData = SLMgr->GetCurrentSaveData();
	if (!SaveData) return false;

	// BuildingIndex → CompanyType → RequiredType 매칭 (제조 전용은 제조 업종, 프로젝트 전용은 프로젝트 업종에만)
	ECompanyType CompanyType = ECompanyType::None;
	for (const FBuildingEntitySaveData& BS : SaveData->GameData.Buildings)
	{
		if (BS.BuildingIndex == BuildingIndex)
		{
			CompanyType = BS.BuildingData.CompanyType;
			break;
		}
	}

	switch (TraitRow.RequiredType)
	{
	case EBuildingTraitRequirement::Manufacturing:
		return IsManufacturingType(CompanyType);
	case EBuildingTraitRequirement::Project:
		return IsProjectType(CompanyType);
	default:
		return true;
	}
}

// ========================================================================
// 슬롯 해금 (빌딩 Rarity 무료 + 다이아 구매)
// ========================================================================

bool UBuildingTraitManagerSubsystem::ResolveBuildingData(int32 BuildingIndex, FBuildingData& OutData) const
{
	USaveLoadManager* SLMgr = GetSaveLoadManager();
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!SLMgr || !TMgr) return false;

	USaveGame_GameData* SaveData = SLMgr->GetCurrentSaveData();
	if (!SaveData) return false;

	// BuildingIndex → InteractableName(=BuildingID) → BuildingDataTable 행
	for (const FBuildingEntitySaveData& BS : SaveData->GameData.Buildings)
	{
		if (BS.BuildingIndex == BuildingIndex)
		{
			bool bOk = false;
			OutData = TMgr->GetBuildingData(BS.InteractableName, bOk);
			return bOk;
		}
	}
	return false;
}

int32 UBuildingTraitManagerSubsystem::GetFootprintCells(int32 BuildingIndex) const
{
	FBuildingData Row;
	if (!ResolveBuildingData(BuildingIndex, Row)) return 1;
	return FMath::Max(1, Row.FootprintWidthCells * Row.FootprintDepthCells);
}

int32 UBuildingTraitManagerSubsystem::GetFreeSlotCount(int32 BuildingIndex) const
{
	// 큰 땅의 대가는 DECISION_RECORDS §3.12 로 인원 상한(2 x 칸수)이 담당하므로, 여기선 2칸에서 끊는다.
	const int32 Cells = GetFootprintCells(BuildingIndex);
	return FMath::Clamp(Cells, 0, FBuildingTraitSlotData::MaxFreeTraitSlots);
}

bool UBuildingTraitManagerSubsystem::IsSlotUnlocked(int32 BuildingIndex, int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= FBuildingTraitSlotData::MaxBuildingTraitSlots) return false;

	// 1) 빌딩 Rarity 무료 해금분
	if (SlotIndex < GetFreeSlotCount(BuildingIndex)) return true;

	// 2) 다이아 구매 비트마스크 (읽기 전용 — 슬롯 데이터 없으면 미구매)
	if (const FBuildingTraitSlotData* SlotData = FindSlotData(BuildingIndex))
	{
		return (SlotData->DiamondUnlockedSlotMask & (1 << SlotIndex)) != 0;
	}
	return false;
}

int32 UBuildingTraitManagerSubsystem::GetUnlockedSlotCount(int32 BuildingIndex) const
{
	int32 Count = 0;
	for (int32 i = 0; i < FBuildingTraitSlotData::MaxBuildingTraitSlots; ++i)
	{
		if (IsSlotUnlocked(BuildingIndex, i)) ++Count;
	}
	return Count;
}

int32 UBuildingTraitManagerSubsystem::GetDiamondCostForSlot(int32 SlotIndex)
{
	// 0번은 도달 불가 — 칸수가 최소 1이라 항상 무료다. 인덱스 정합용으로만 남긴다.
	static constexpr int32 Costs[FBuildingTraitSlotData::MaxBuildingTraitSlots] = { 30, 150, 450 };
	if (SlotIndex < 0 || SlotIndex >= FBuildingTraitSlotData::MaxBuildingTraitSlots) return 0;
	return Costs[SlotIndex];
}

bool UBuildingTraitManagerSubsystem::UnlockSlotWithDiamond(int32 BuildingIndex, int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= FBuildingTraitSlotData::MaxBuildingTraitSlots) return false;

	// 이미 해금된 슬롯(무료 or 기존 구매)이면 무시
	if (IsSlotUnlocked(BuildingIndex, SlotIndex)) return false;

	UResourceItemManager* ResMgr = GetResourceManager();
	if (!ResMgr) return false;

	const int32 Cost = GetDiamondCostForSlot(SlotIndex);
	if (Cost <= 0) return false;
	if (!ResMgr->HasResource(EResourceType::Diamond, Cost)) return false;

	FBuildingTraitSlotData* SlotData = GetOrCreateSlotData(BuildingIndex);
	if (!SlotData) return false;

	ResMgr->SpendResource(EResourceType::Diamond, Cost, false);
	SlotData->DiamondUnlockedSlotMask |= (1 << SlotIndex);

	OnTraitSlotUnlocked.Broadcast(BuildingIndex, SlotIndex);
	SaveGameData();
	return true;
}

bool UBuildingTraitManagerSubsystem::EquipTrait(int32 BuildingIndex, int32 SlotIndex, FName TraitID)
{
	if (SlotIndex < 0 || SlotIndex >= FBuildingTraitSlotData::MaxBuildingTraitSlots) return false;
	if (!IsSlotUnlocked(BuildingIndex, SlotIndex)) return false;
	if (!HasTrait(TraitID, 1)) return false;
	if (!CanEquipTraitToBuilding(BuildingIndex, TraitID)) return false;

	FBuildingTraitSlotData* SlotData = GetOrCreateSlotData(BuildingIndex);
	if (!SlotData) return false;

	// 같은 TraitID 중복 장착 금지
	if (SlotData->EquippedTraits.Contains(TraitID)) return false;

	// 기존 슬롯이 차 있으면 해제 먼저
	if (!SlotData->EquippedTraits[SlotIndex].IsNone())
	{
		const FName OldTraitID = SlotData->EquippedTraits[SlotIndex];
		GrantTrait(OldTraitID, 1, false);
	}

	// 인벤토리 차감 + 슬롯 박기
	RemoveTrait(TraitID, 1, false);
	SlotData->EquippedTraits[SlotIndex] = TraitID;

	InvalidateTraitCache();
	OnTraitSlotChanged.Broadcast(BuildingIndex, SlotIndex, TraitID);
	SaveGameData();
	return true;
}

bool UBuildingTraitManagerSubsystem::UnequipTrait(int32 BuildingIndex, int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= FBuildingTraitSlotData::MaxBuildingTraitSlots) return false;

	FBuildingTraitSlotData* SlotData = GetOrCreateSlotData(BuildingIndex);
	if (!SlotData) return false;

	const FName OldTraitID = SlotData->EquippedTraits[SlotIndex];
	if (OldTraitID.IsNone()) return false;

	// 슬롯 비우고 인벤토리로 복귀
	SlotData->EquippedTraits[SlotIndex] = NAME_None;
	GrantTrait(OldTraitID, 1, false);

	InvalidateTraitCache();
	OnTraitSlotChanged.Broadcast(BuildingIndex, SlotIndex, NAME_None);
	SaveGameData();
	return true;
}

bool UBuildingTraitManagerSubsystem::ReplaceTrait(int32 BuildingIndex, int32 SlotIndex, FName NewTraitID)
{
	if (SlotIndex < 0 || SlotIndex >= FBuildingTraitSlotData::MaxBuildingTraitSlots) return false;
	if (!IsSlotUnlocked(BuildingIndex, SlotIndex)) return false;
	if (NewTraitID.IsNone()) return false;
	if (!HasTrait(NewTraitID, 1)) return false;
	if (!CanEquipTraitToBuilding(BuildingIndex, NewTraitID)) return false;

	FBuildingTraitSlotData* SlotData = GetOrCreateSlotData(BuildingIndex);
	if (!SlotData) return false;

	// 다른 슬롯에 같은 TraitID 가 있으면 거부
	for (int32 i = 0; i < SlotData->EquippedTraits.Num(); ++i)
	{
		if (i != SlotIndex && SlotData->EquippedTraits[i] == NewTraitID) return false;
	}

	// 기존 슬롯 특성 -> 인벤토리 복귀
	const FName OldTraitID = SlotData->EquippedTraits[SlotIndex];
	if (!OldTraitID.IsNone())
	{
		GrantTrait(OldTraitID, 1, false);
	}

	// 새 특성 -> 슬롯
	RemoveTrait(NewTraitID, 1, false);
	SlotData->EquippedTraits[SlotIndex] = NewTraitID;

	InvalidateTraitCache();
	OnTraitSlotChanged.Broadcast(BuildingIndex, SlotIndex, NewTraitID);
	SaveGameData();
	return true;
}

// ========================================================================
// 분해 (Dismantle)
// ========================================================================

int32 UBuildingTraitManagerSubsystem::GetDismantleDustValue(ELootBoxRarity Rarity)
{
	switch (Rarity)
	{
		case ELootBoxRarity::Common:    return 1;
		case ELootBoxRarity::Unusual:   return 3;
		case ELootBoxRarity::Rare:      return 8;
		case ELootBoxRarity::Epic:      return 25;
		case ELootBoxRarity::Legendary: return 80;
		case ELootBoxRarity::Mythic:    return 0; // 분해 불가
		default:                        return 0;
	}
}

bool UBuildingTraitManagerSubsystem::DismantleTrait(FName TraitID, int32 Amount)
{
	if (Amount <= 0 || !HasTrait(TraitID, Amount)) return false;

	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return false;

	FBuildingTraitTableRow Row;
	if (!TMgr->GetBuildingTraitData(TraitID, Row)) return false;

	// Mythic 분해 불가
	if (Row.Rarity == ELootBoxRarity::Mythic) return false;

	const int32 DustPerOne = GetDismantleDustValue(Row.Rarity);
	if (DustPerOne <= 0) return false;

	RemoveTrait(TraitID, Amount, false);
	Inventory.DismantleDust += DustPerOne * Amount;

	OnDustChanged.Broadcast(Inventory.DismantleDust);
	SaveGameData();
	return true;
}

bool UBuildingTraitManagerSubsystem::ExchangeDust(EBuildingTraitDustExchange Exchange, FName SelectedTraitID)
{
	UItemInventoryManager* IIM = GetItemInventoryManager();
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!IIM || !TMgr) return false;

	int32 Cost = 0;
	switch (Exchange)
	{
		case EBuildingTraitDustExchange::NormalTicket:   Cost = 30;   break;
		case EBuildingTraitDustExchange::AdvancedTicket: Cost = 150;  break;
		case EBuildingTraitDustExchange::CommonChoice:   Cost = 10;   break;
		case EBuildingTraitDustExchange::UnusualChoice:  Cost = 40;   break;
		case EBuildingTraitDustExchange::RareChoice:     Cost = 200;  break;
		case EBuildingTraitDustExchange::EpicBox:        Cost = 1500; break;
		case EBuildingTraitDustExchange::LegendaryBox:   Cost = 8000; break;
		default: return false;
	}

	if (Inventory.DismantleDust < Cost) return false;

	// 지정 등급 교환은 SelectedTraitID + 등급 일치 검증
	auto ValidateRarity = [&](ELootBoxRarity ExpectedRarity) -> bool
	{
		FBuildingTraitTableRow Row;
		if (!TMgr->GetBuildingTraitData(SelectedTraitID, Row)) return false;
		return Row.Rarity == ExpectedRarity;
	};

	switch (Exchange)
	{
		case EBuildingTraitDustExchange::NormalTicket:
			IIM->AddItem(EItemType::BuildingTraitTicketNormal, 1, false);
			break;
		case EBuildingTraitDustExchange::AdvancedTicket:
			IIM->AddItem(EItemType::BuildingTraitTicketAdvanced, 1, false);
			break;
		case EBuildingTraitDustExchange::CommonChoice:
			if (SelectedTraitID.IsNone() || !ValidateRarity(ELootBoxRarity::Common)) return false;
			GrantTrait(SelectedTraitID, 1, false);
			break;
		case EBuildingTraitDustExchange::UnusualChoice:
			if (SelectedTraitID.IsNone() || !ValidateRarity(ELootBoxRarity::Unusual)) return false;
			GrantTrait(SelectedTraitID, 1, false);
			break;
		case EBuildingTraitDustExchange::RareChoice:
			if (SelectedTraitID.IsNone() || !ValidateRarity(ELootBoxRarity::Rare)) return false;
			GrantTrait(SelectedTraitID, 1, false);
			break;
		case EBuildingTraitDustExchange::EpicBox:
			if (SelectedTraitID.IsNone() || !ValidateRarity(ELootBoxRarity::Epic)) return false;
			GrantTrait(SelectedTraitID, 1, false);
			break;
		case EBuildingTraitDustExchange::LegendaryBox:
			if (SelectedTraitID.IsNone() || !ValidateRarity(ELootBoxRarity::Legendary)) return false;
			GrantTrait(SelectedTraitID, 1, false);
			break;
		default: return false;
	}

	Inventory.DismantleDust -= Cost;
	OnDustChanged.Broadcast(Inventory.DismantleDust);
	SaveGameData();
	return true;
}

// ========================================================================
// 합성 (3개 → 상위 1개)
// ========================================================================

bool UBuildingTraitManagerSubsystem::SynthesizeTraits(const TArray<FName>& MaterialTraitIDs, FBuildingTraitGachaResult& OutResult)
{
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return false;

	if (MaterialTraitIDs.Num() != 3 && MaterialTraitIDs.Num() != 5) return false;

	// 모든 재료 보유 확인 + 같은 등급/분야 검증
	TMap<FName, int32> RequiredCounts;
	ELootBoxRarity SharedRarity = ELootBoxRarity::Common;
	EBuildingTraitCategory SharedCategory = EBuildingTraitCategory::Revenue;
	bool bFirst = true;
	bool bAllSameCategory = true;

	for (const FName& MatID : MaterialTraitIDs)
	{
		RequiredCounts.FindOrAdd(MatID, 0) += 1;
		FBuildingTraitTableRow Row;
		if (!TMgr->GetBuildingTraitData(MatID, Row)) return false;

		if (bFirst)
		{
			SharedRarity = Row.Rarity;
			SharedCategory = Row.Category;
			bFirst = false;
		}
		else
		{
			if (Row.Rarity != SharedRarity) return false;
			if (Row.Category != SharedCategory) bAllSameCategory = false;
		}
	}

	for (const TPair<FName, int32>& Pair : RequiredCounts)
	{
		if (!HasTrait(Pair.Key, Pair.Value)) return false;
	}

	// 결과 등급 결정
	ELootBoxRarity ResultRarity = SharedRarity;
	if (MaterialTraitIDs.Num() == 3 && bAllSameCategory)
	{
		// 같은 분야 3개 → 분야 내 상위 등급 랜덤
		switch (SharedRarity)
		{
			case ELootBoxRarity::Common:    ResultRarity = ELootBoxRarity::Unusual;   break;
			case ELootBoxRarity::Unusual:   ResultRarity = ELootBoxRarity::Rare;      break;
			case ELootBoxRarity::Rare:      ResultRarity = ELootBoxRarity::Epic;      break;
			default: return false; // Epic 동일분야 3 → Lgd 는 불가 (5개 필요)
		}
	}
	else if (MaterialTraitIDs.Num() == 5 && SharedRarity == ELootBoxRarity::Epic)
	{
		// Epic 5개 (분야 무관) → Legendary 랜덤
		ResultRarity = ELootBoxRarity::Legendary;
	}
	else
	{
		// 동일 등급 5개 범용 합성 → 한 단계 상위 등급
		switch (SharedRarity)
		{
			case ELootBoxRarity::Common:  ResultRarity = ELootBoxRarity::Unusual; break;
			case ELootBoxRarity::Unusual: ResultRarity = ELootBoxRarity::Rare;    break;
			case ELootBoxRarity::Rare:    ResultRarity = ELootBoxRarity::Epic;    break;
			default: return false;
		}
	}

	// 결과 픽을 재료 차감 전에 수행 — 실패 시 재료 보존
	const bool bUseCategoryFilter = (MaterialTraitIDs.Num() == 3 && bAllSameCategory);
	FName ResultID = PickRandomTraitOfRarity(ResultRarity, SharedCategory, bUseCategoryFilter);
	if (ResultID.IsNone()) return false;

	for (const TPair<FName, int32>& Pair : RequiredCounts)
	{
		RemoveTrait(Pair.Key, Pair.Value, false);
	}

	GrantTrait(ResultID, 1, false);

	OutResult.ResultTraitID = ResultID;
	OutResult.Rarity = ResultRarity;
	OutResult.bWasPityGuaranteed = false;
	OutResult.bFromMileageBox = false;

	SaveGameData();
	return true;
}

// ========================================================================
// 가챠 (확률 + 천장 + 마일리지)
// ========================================================================

bool UBuildingTraitManagerSubsystem::CanExecuteGachaPull(bool bAdvanced) const
{
	UItemInventoryManager* IIM = GetItemInventoryManager();
	if (!IIM) return false;
	const EItemType TicketType = bAdvanced
		? EItemType::BuildingTraitTicketAdvanced
		: EItemType::BuildingTraitTicketNormal;
	return IIM->HasItem(TicketType, 1);
}

int32 UBuildingTraitManagerSubsystem::GetPullsSinceEpic(bool bAdvanced) const
{
	return bAdvanced
		? Inventory.GachaData.AdvancedPity.PullsSinceLastEpic
		: Inventory.GachaData.NormalPity.PullsSinceLastEpic;
}

int32 UBuildingTraitManagerSubsystem::GetPullsSinceLegendary(bool bAdvanced) const
{
	return bAdvanced
		? Inventory.GachaData.AdvancedPity.PullsSinceLastLegendary
		: Inventory.GachaData.NormalPity.PullsSinceLastLegendary;
}

ELootBoxRarity UBuildingTraitManagerSubsystem::RollGachaRarity(bool bAdvanced, bool& bOutPityGuaranteed)
{
	FBuildingTraitPityData& Pity = bAdvanced ? Inventory.GachaData.AdvancedPity : Inventory.GachaData.NormalPity;
	Pity.IncrementPull();
	bOutPityGuaranteed = false;

	// 1) 대천장 — Legendary 확정
	if (Pity.PullsSinceLastLegendary >= FBuildingTraitPityData::GrandPity)
	{
		bOutPityGuaranteed = true;
		Pity.OnLegendaryObtained();
		return ELootBoxRarity::Legendary;
	}

	// 2) 하드 천장 — Epic 이상 확정 (Legendary 롤 후 안 떨어지면 Epic)
	if (Pity.PullsSinceLastEpic >= FBuildingTraitPityData::HardPity)
	{
		bOutPityGuaranteed = true;
		// 하드 천장 발동 시 Lgd 확률만 그대로, 나머지는 Epic 으로
		const float Roll = FMath::FRand();
		const float LegendaryProb = bAdvanced ? 0.03f : 0.01f;
		if (Roll < LegendaryProb)
		{
			Pity.OnLegendaryObtained();
			return ELootBoxRarity::Legendary;
		}
		Pity.OnEpicObtained();
		return ELootBoxRarity::Epic;
	}

	// 3) 기본 확률표 + 소프트 천장 Epic 가산
	// 일반 티켓: C 45 / U 30 / R 18 / E 6 / L 1
	// 고급 티켓: U 30 / R 50 / E 17 / L 3
	float CommonProb = bAdvanced ? 0.00f : 0.45f;
	float UnusualProb = 0.30f;
	float RareProb = bAdvanced ? 0.50f : 0.18f;
	float EpicProb = bAdvanced ? 0.17f : 0.06f;
	float LegendaryProb = bAdvanced ? 0.03f : 0.01f;

	// 소프트 천장 (40회 이상): 매 회 Epic 확률 +2%
	if (Pity.PullsSinceLastEpic >= FBuildingTraitPityData::SoftPityStart)
	{
		const int32 OverPulls = Pity.PullsSinceLastEpic - FBuildingTraitPityData::SoftPityStart;
		const float Bonus = (OverPulls + 1) * FBuildingTraitPityData::SoftPityBonusPerPull;
		EpicProb += Bonus;
		// Bonus 가 한 풀을 초과해도 합이 1.0 을 넘지 않도록 Common → Unusual → Rare 순으로 차감.
		// 단일 풀에서만 빼면 Advanced 티켓처럼 CommonProb=0 인 경우 Legendary 가 사실상 막힘.
		float Remaining = Bonus;
		const float CommonCut = FMath::Min(Remaining, CommonProb);
		CommonProb -= CommonCut;
		Remaining -= CommonCut;
		if (Remaining > 0.0f)
		{
			const float UnusualCut = FMath::Min(Remaining, UnusualProb);
			UnusualProb -= UnusualCut;
			Remaining -= UnusualCut;
		}
		if (Remaining > 0.0f)
		{
			const float RareCut = FMath::Min(Remaining, RareProb);
			RareProb -= RareCut;
		}
	}

	const float Roll = FMath::FRand();
	float Cumulative = 0.0f;

	Cumulative += CommonProb;
	if (Roll < Cumulative) return ELootBoxRarity::Common;

	Cumulative += UnusualProb;
	if (Roll < Cumulative) return ELootBoxRarity::Unusual;

	Cumulative += RareProb;
	if (Roll < Cumulative) return ELootBoxRarity::Rare;

	Cumulative += EpicProb;
	if (Roll < Cumulative)
	{
		Pity.OnEpicObtained();
		return ELootBoxRarity::Epic;
	}

	Pity.OnLegendaryObtained();
	return ELootBoxRarity::Legendary;
}

TArray<FGachaRarityChance> UBuildingTraitManagerSubsystem::GetTraitProbabilityTableForUI(bool bAdvanced) const
{
	auto Make = [](ELootBoxRarity R, float P) { FGachaRarityChance C; C.Rarity = R; C.Percent = P; return C; };
	TArray<FGachaRarityChance> Rows;
	if (bAdvanced)
	{
		// 고급 티켓 기본 확률표 — RollGachaRarity 표와 정확히 일치 (Common 없음)
		Rows.Add(Make(ELootBoxRarity::Unusual,   30.f));
		Rows.Add(Make(ELootBoxRarity::Rare,      50.f));
		Rows.Add(Make(ELootBoxRarity::Epic,      17.f));
		Rows.Add(Make(ELootBoxRarity::Legendary,  3.f));
	}
	else
	{
		// 일반 티켓 기본 확률표 — RollGachaRarity 표와 정확히 일치
		Rows.Add(Make(ELootBoxRarity::Common,    45.f));
		Rows.Add(Make(ELootBoxRarity::Unusual,   30.f));
		Rows.Add(Make(ELootBoxRarity::Rare,      18.f));
		Rows.Add(Make(ELootBoxRarity::Epic,       6.f));
		Rows.Add(Make(ELootBoxRarity::Legendary,  1.f));
	}
	return Rows;
}

FName UBuildingTraitManagerSubsystem::PickRandomTraitOfRarity(ELootBoxRarity Rarity, EBuildingTraitCategory CategoryFilter, bool bUseFilter) const
{
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return NAME_None;

	TArray<FBuildingTraitTableRow> Candidates = TMgr->GetBuildingTraitsByRarity(Rarity);

	// Mythic 제외 (Rarity 가 Mythic 이 아니어도 안전망)
	Candidates.RemoveAll([](const FBuildingTraitTableRow& R) { return R.Rarity == ELootBoxRarity::Mythic; });

	if (bUseFilter)
	{
		Candidates.RemoveAll([CategoryFilter](const FBuildingTraitTableRow& R) { return R.Category != CategoryFilter; });
	}
	else
	{
		// 타입 전용 (Manufacturing/Project) 도 가챠 풀에서 제외 (별도 획득 경로)
		Candidates.RemoveAll([](const FBuildingTraitTableRow& R) { return R.RequiredType != EBuildingTraitRequirement::None; });
	}

	if (Candidates.Num() == 0) return NAME_None;
	const int32 Index = FMath::RandRange(0, Candidates.Num() - 1);
	return Candidates[Index].TraitID;
}

bool UBuildingTraitManagerSubsystem::ExecuteGachaPull(bool bAdvanced, FBuildingTraitGachaResult& OutResult)
{
	if (!CanExecuteGachaPull(bAdvanced)) return false;

	UItemInventoryManager* IIM = GetItemInventoryManager();
	const EItemType TicketType = bAdvanced
		? EItemType::BuildingTraitTicketAdvanced
		: EItemType::BuildingTraitTicketNormal;
	if (!IIM->SpendItem(TicketType, 1, false)) return false;

	// 천장 스냅샷 — PickRandom 실패 시 롤백용
	FBuildingTraitPityData& PityRef = bAdvanced ? Inventory.GachaData.AdvancedPity : Inventory.GachaData.NormalPity;
	const FBuildingTraitPityData PitySnapshot = PityRef;

	bool bPityGuaranteed = false;
	const ELootBoxRarity Rarity = RollGachaRarity(bAdvanced, bPityGuaranteed);

	const FName ResultID = PickRandomTraitOfRarity(Rarity);
	if (ResultID.IsNone())
	{
		PityRef = PitySnapshot;
		IIM->AddItem(TicketType, 1, false);
		return false;
	}

	GrantTrait(ResultID, 1, false);
	Inventory.GachaData.TotalPullsAllTime++;
	AddMileagePoints(1);

	OutResult.ResultTraitID = ResultID;
	OutResult.Rarity = Rarity;
	OutResult.bWasPityGuaranteed = bPityGuaranteed;
	OutResult.bFromMileageBox = false;

	OnTraitGachaCompleted.Broadcast(OutResult);
	SaveGameData();
	return true;
}

bool UBuildingTraitManagerSubsystem::ExecuteGachaPullMulti(bool bAdvanced, int32 Count, TArray<FBuildingTraitGachaResult>& OutResults)
{
	OutResults.Reset();
	const int32 N = FMath::Clamp(Count, 1, 100);
	for (int32 i = 0; i < N; ++i)
	{
		if (!CanExecuteGachaPull(bAdvanced)) break;   // 티켓 소진 시 가능한 만큼만
		FBuildingTraitGachaResult R;
		if (!ExecuteGachaPull(bAdvanced, R)) break;
		OutResults.Add(R);
	}
	return OutResults.Num() > 0;
}

void UBuildingTraitManagerSubsystem::AddMileagePoints(int32 Amount)
{
	if (Amount <= 0) return;
	Inventory.GachaData.MileagePoints += Amount;
	// 마일리지 임계점 도달은 UI 가 표시 (자동 교환 안 함)
}

bool UBuildingTraitManagerSubsystem::ExchangeMileageBox(bool bLegendaryBox, FName SelectedTraitID, FBuildingTraitGachaResult& OutResult)
{
	const int32 RequiredCost = bLegendaryBox
		? FGachaTraitData::MileageLegendaryBox
		: FGachaTraitData::MileageEpicBox;

	if (Inventory.GachaData.MileagePoints < RequiredCost) return false;
	if (SelectedTraitID.IsNone()) return false;

	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return false;

	FBuildingTraitTableRow Row;
	if (!TMgr->GetBuildingTraitData(SelectedTraitID, Row)) return false;

	const ELootBoxRarity Expected = bLegendaryBox ? ELootBoxRarity::Legendary : ELootBoxRarity::Epic;
	if (Row.Rarity != Expected) return false;

	Inventory.GachaData.MileagePoints -= RequiredCost;
	GrantTrait(SelectedTraitID, 1, false);

	OutResult.ResultTraitID = SelectedTraitID;
	OutResult.Rarity = Expected;
	OutResult.bWasPityGuaranteed = false;
	OutResult.bFromMileageBox = true;

	OnTraitGachaCompleted.Broadcast(OutResult);
	SaveGameData();
	return true;
}

// ========================================================================
// 세트 보너스
// ========================================================================

TArray<FActiveBuildingTraitSetBonus> UBuildingTraitManagerSubsystem::CalculateSetBonuses(int32 BuildingIndex) const
{
	TArray<FActiveBuildingTraitSetBonus> Result;
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return Result;

	const FBuildingTraitSlotData* SlotData = FindSlotData(BuildingIndex);
	if (!SlotData) return Result;

	// 슬롯의 분야 카운트 집계
	TMap<EBuildingTraitCategory, int32> CategoryCount;
	for (const FName& Equipped : SlotData->EquippedTraits)
	{
		if (Equipped.IsNone()) continue;
		FBuildingTraitTableRow Row;
		if (!TMgr->GetBuildingTraitData(Equipped, Row)) continue;
		CategoryCount.FindOrAdd(Row.Category, 0) += 1;
	}

	// 각 분야의 세트 보너스 (2세트 / 3세트) 활성 여부 평가
	for (const TPair<EBuildingTraitCategory, int32>& Pair : CategoryCount)
	{
		const EBuildingTraitCategory Cat = Pair.Key;
		const int32 Count = Pair.Value;

		for (int32 RequiredCount = 2; RequiredCount <= 3; ++RequiredCount)
		{
			FBuildingTraitSetBonus BonusRow;
			if (!TMgr->GetBuildingTraitSetBonus(Cat, RequiredCount, BonusRow)) continue;

			FActiveBuildingTraitSetBonus Active;
			Active.Category = Cat;
			Active.CountInBuilding = Count;
			Active.RequiredCount = RequiredCount;
			Active.bActive = (Count >= RequiredCount);
			Active.BonusDescription = FormatTraitEffectText(GetTargetForCategory(Cat), BonusRow.BonusValue);
			Active.BonusValue = BonusRow.BonusValue;
			Result.Add(Active);
		}
	}

	return Result;
}

// ========================================================================
// 효과 집계 (게임플레이 적용 레이어)
// ========================================================================

void UBuildingTraitManagerSubsystem::InvalidateTraitCache()
{
	RawPercentCache.Empty();
}

float UBuildingTraitManagerSubsystem::GetRawTraitPercent(int32 BuildingIndex, EBuildingTraitTarget Target) const
{
	if (Target == EBuildingTraitTarget::None) return 0.0f;

	// 세이브가 갈리면(레벨 전환/재로드) 슬롯 구성도 갈린다 — 소유자가 다르면 캐시를 버린다.
	{
		USaveLoadManager* SLMgr = GetSaveLoadManager();
		USaveGame_GameData* SaveNow = SLMgr ? SLMgr->GetCurrentSaveData() : nullptr;
		if (RawPercentCacheOwner.Get() != SaveNow)
		{
			RawPercentCache.Empty();
			RawPercentCacheOwner = SaveNow;
		}
	}

	if (const TMap<EBuildingTraitTarget, float>* Inner = RawPercentCache.Find(BuildingIndex))
	{
		if (const float* Hit = Inner->Find(Target)) { return *Hit; }
	}

	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) return 0.0f;

	float Sum = 0.0f;

	// 1) 장착 특성 — 주대상(Category 파생)
	const TArray<FName> Equipped = GetEquippedTraits(BuildingIndex);
	for (const FName& TraitID : Equipped)
	{
		if (TraitID.IsNone()) continue;
		FBuildingTraitTableRow Row;
		if (!TMgr->GetBuildingTraitData(TraitID, Row)) continue;

		if (GetTargetForCategory(Row.Category) == Target)
		{
			Sum += Row.BaseEffect;
		}
	}

	// 2) 활성 세트보너스 (2세트/3세트 둘 다 — 3개 장착 시 합산)
	const TArray<FActiveBuildingTraitSetBonus> SetBonuses = CalculateSetBonuses(BuildingIndex);
	for (const FActiveBuildingTraitSetBonus& Bonus : SetBonuses)
	{
		if (Bonus.bActive && GetTargetForCategory(Bonus.Category) == Target)
		{
			Sum += Bonus.BonusValue;
		}
	}

	RawPercentCache.FindOrAdd(BuildingIndex).Add(Target, Sum);
	return Sum;
}

float UBuildingTraitManagerSubsystem::GetAggregatedTraitPercent(int32 BuildingIndex, EBuildingTraitTarget Target) const
{
	if (Target == EBuildingTraitTarget::None) return 0.0f;

	float Sum = GetRawTraitPercent(BuildingIndex, Target);

	// 두 메타축(오라/증폭) 자신을 구할 때는 여기서 끝낸다 — 서로를 곱하면 무한 재귀 + 밸런스 폭주.
	if (Target == EBuildingTraitTarget::AuraPower || Target == EBuildingTraitTarget::MetaAmplify)
	{
		return Sum;
	}

	// 3) 키스톤 오라 기여 (영향권 내 이웃 빌딩에 가산). 파트너십(AuraPower)이 이 기여분만 증폭한다.
	if (UWorld* AuraWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		if (UKeystoneAuraSubsystem* AuraSys = AuraWorld->GetSubsystem<UKeystoneAuraSubsystem>())
		{
			const float AuraPct = AuraSys->GetAuraPercent(BuildingIndex, Target);
			if (!FMath::IsNearlyZero(AuraPct))
			{
				Sum += AuraPct * (1.0f + GetRawTraitPercent(BuildingIndex, EBuildingTraitTarget::AuraPower) / 100.0f);
			}
		}
	}

	// 4) 마인드셋 메타 증폭 — 합산 후 곱(개별 효과에 각각 곱하면 마인드셋 몰빵이 압도적으로 우월해진다)
	const float Meta = GetRawTraitPercent(BuildingIndex, EBuildingTraitTarget::MetaAmplify);
	if (!FMath::IsNearlyZero(Meta))
	{
		Sum *= (1.0f + Meta / 100.0f);
	}

	return Sum;
}

float UBuildingTraitManagerSubsystem::GetTraitFactor(int32 BuildingIndex, EBuildingTraitTarget Target) const
{
	return 1.0f + GetAggregatedTraitPercent(BuildingIndex, Target) / 100.0f;
}

float UBuildingTraitManagerSubsystem::GetCompanyWideTraitPercent(EBuildingTraitTarget Target) const
{
	if (Target == EBuildingTraitTarget::None) return 0.0f;

	USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
	if (!SaveMgr) return 0.0f;
	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return 0.0f;

	float Best = 0.0f;
	for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
	{
		Best = FMath::Max(Best, GetAggregatedTraitPercent(B.BuildingIndex, Target));
	}
	return Best;
}

// ========================================================================
// 저장
// ========================================================================

void UBuildingTraitManagerSubsystem::SaveGameData()
{
	if (USaveLoadManager* SLMgr = GetSaveLoadManager())
	{
		SLMgr->SaveGameData();
	}
}
