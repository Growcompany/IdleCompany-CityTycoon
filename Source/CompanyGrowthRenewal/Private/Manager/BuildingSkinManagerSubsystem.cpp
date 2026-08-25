// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Table/BuildingSkinData.h"
#include "Enum/ItemType.h"
#include "Engine/GameInstance.h"

namespace
{
	// 기본(시작) 스킨 — 가챠 풀에서 제외
	constexpr int32 BaseSkinID = 100;
}

void UBuildingSkinManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[BuildingSkinManager] Initialized"));
}

void UBuildingSkinManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ========================================================================
// 캐시된 서브시스템 접근자
// ========================================================================

UTableManagerSubsystem* UBuildingSkinManagerSubsystem::GetTableManager() const
{
	if (!TableManager)
	{
		if (UGameInstance* GI = GetGameInstance()) { TableManager = GI->GetSubsystem<UTableManagerSubsystem>(); }
	}
	return TableManager;
}

UItemInventoryManager* UBuildingSkinManagerSubsystem::GetItemInventoryManager() const
{
	if (!ItemInventoryManager)
	{
		if (UGameInstance* GI = GetGameInstance()) { ItemInventoryManager = GI->GetSubsystem<UItemInventoryManager>(); }
	}
	return ItemInventoryManager;
}

USaveLoadManager* UBuildingSkinManagerSubsystem::GetSaveLoadManager() const
{
	if (!SaveLoadManager)
	{
		if (UGameInstance* GI = GetGameInstance()) { SaveLoadManager = GI->GetSubsystem<USaveLoadManager>(); }
	}
	return SaveLoadManager;
}

// ========================================================================
// 소유 조회 / 지급
// ========================================================================

bool UBuildingSkinManagerSubsystem::IsSkinOwned(int32 SkinID) const
{
	USaveLoadManager* SL = GetSaveLoadManager();
	if (!SL) { return false; }
	USaveGame_GameData* SG = SL->GetCurrentSaveData();
	if (!SG) { return false; }
	return SG->GameData.OwnedBuildingSkins.ContainsByPredicate(
		[SkinID](const FBuildingSkinInstance& S) { return S.SkinID == SkinID; });
}

bool UBuildingSkinManagerSubsystem::HasAnyGachaSkin() const
{
	USaveLoadManager* SL = GetSaveLoadManager();
	if (!SL) { return false; }
	USaveGame_GameData* SG = SL->GetCurrentSaveData();
	if (!SG) { return false; }
	// 기본 스킨은 새 세이브가 무조건 들고 시작한다 — 세면 "이미 뽑았다"가 항상 참이 된다
	return SG->GameData.OwnedBuildingSkins.ContainsByPredicate(
		[](const FBuildingSkinInstance& S) { return S.SkinID != BaseSkinID; });
}

void UBuildingSkinManagerSubsystem::GrantSkin(int32 SkinID)
{
	if (SkinID <= 0) { return; }
	USaveLoadManager* SL = GetSaveLoadManager();
	if (!SL) { return; }
	USaveGame_GameData* SG = SL->GetCurrentSaveData();
	if (!SG) { return; }

	const bool bAlready = SG->GameData.OwnedBuildingSkins.ContainsByPredicate(
		[SkinID](const FBuildingSkinInstance& S) { return S.SkinID == SkinID; });
	if (!bAlready)
	{
		SG->GameData.OwnedBuildingSkins.Add(FBuildingSkinInstance(SkinID));
	}
}

// ========================================================================
// 가챠
// ========================================================================

bool UBuildingSkinManagerSubsystem::CanExecuteGachaPull(bool bAdvanced) const
{
	UItemInventoryManager* IIM = GetItemInventoryManager();
	if (!IIM) { return false; }
	const EItemType TicketType = bAdvanced ? EItemType::SkinTicketAdvanced : EItemType::SkinTicketNormal;
	return IIM->HasItem(TicketType, 1);
}

int32 UBuildingSkinManagerSubsystem::GetPullsSinceEpic(bool bAdvanced) const
{
	return bAdvanced
		? Inventory.GachaData.AdvancedPity.PullsSinceLastEpic
		: Inventory.GachaData.NormalPity.PullsSinceLastEpic;
}

int32 UBuildingSkinManagerSubsystem::GetPullsSinceLegendary(bool bAdvanced) const
{
	return bAdvanced
		? Inventory.GachaData.AdvancedPity.PullsSinceLastLegendary
		: Inventory.GachaData.NormalPity.PullsSinceLastLegendary;
}

ELootBoxRarity UBuildingSkinManagerSubsystem::RollGachaRarity(bool bAdvanced, bool& bOutPityGuaranteed)
{
	FBuildingSkinPityData& Pity = bAdvanced ? Inventory.GachaData.AdvancedPity : Inventory.GachaData.NormalPity;
	Pity.IncrementPull();
	bOutPityGuaranteed = false;

	// 1) 대천장 — Legendary 확정
	if (Pity.PullsSinceLastLegendary >= FBuildingSkinPityData::GrandPity)
	{
		bOutPityGuaranteed = true;
		Pity.OnLegendaryObtained();
		return ELootBoxRarity::Legendary;
	}

	// 2) 하드 천장 — Epic 이상 확정
	if (Pity.PullsSinceLastEpic >= FBuildingSkinPityData::HardPity)
	{
		bOutPityGuaranteed = true;
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

	// 3) 기본 확률표 + 소프트 천장 Epic 가산 (특성과 동일 split)
	// 일반: C45/U30/R18/E6/L1, 고급: U30/R50/E17/L3
	float CommonProb = bAdvanced ? 0.00f : 0.45f;
	float UnusualProb = 0.30f;
	float RareProb = bAdvanced ? 0.50f : 0.18f;
	float EpicProb = bAdvanced ? 0.17f : 0.06f;
	float LegendaryProb = bAdvanced ? 0.03f : 0.01f;

	if (Pity.PullsSinceLastEpic >= FBuildingSkinPityData::SoftPityStart)
	{
		const int32 OverPulls = Pity.PullsSinceLastEpic - FBuildingSkinPityData::SoftPityStart;
		const float Bonus = (OverPulls + 1) * FBuildingSkinPityData::SoftPityBonusPerPull;
		EpicProb += Bonus;
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
	if (Roll < Cumulative) { return ELootBoxRarity::Common; }

	Cumulative += UnusualProb;
	if (Roll < Cumulative) { return ELootBoxRarity::Unusual; }

	Cumulative += RareProb;
	if (Roll < Cumulative) { return ELootBoxRarity::Rare; }

	Cumulative += EpicProb;
	if (Roll < Cumulative)
	{
		Pity.OnEpicObtained();
		return ELootBoxRarity::Epic;
	}

	Pity.OnLegendaryObtained();
	return ELootBoxRarity::Legendary;
}

TArray<FGachaRarityChance> UBuildingSkinManagerSubsystem::GetSkinProbabilityTableForUI(bool bAdvanced) const
{
	auto Make = [](ELootBoxRarity R, float P) { FGachaRarityChance C; C.Rarity = R; C.Percent = P; return C; };
	TArray<FGachaRarityChance> Rows;
	if (bAdvanced)
	{
		Rows.Add(Make(ELootBoxRarity::Unusual,   30.f));
		Rows.Add(Make(ELootBoxRarity::Rare,      50.f));
		Rows.Add(Make(ELootBoxRarity::Epic,      17.f));
		Rows.Add(Make(ELootBoxRarity::Legendary,  3.f));
	}
	else
	{
		Rows.Add(Make(ELootBoxRarity::Common,    45.f));
		Rows.Add(Make(ELootBoxRarity::Unusual,   30.f));
		Rows.Add(Make(ELootBoxRarity::Rare,      18.f));
		Rows.Add(Make(ELootBoxRarity::Epic,       6.f));
		Rows.Add(Make(ELootBoxRarity::Legendary,  1.f));
	}
	return Rows;
}

int32 UBuildingSkinManagerSubsystem::PickUnownedSkinOfRarity(ELootBoxRarity Rarity) const
{
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) { return INDEX_NONE; }

	TArray<FBuildingSkinData> Candidates = TMgr->GetBuildingSkinsByRarity(Rarity);
	TArray<int32> Unowned;
	for (const FBuildingSkinData& S : Candidates)
	{
		if (S.Rarity == ELootBoxRarity::Mythic) { continue; } // Mythic 제외(특성과 동일)
		if (S.SkinID == BaseSkinID) { continue; }             // 기본 스킨 제외
		if (!IsSkinOwned(S.SkinID)) { Unowned.Add(S.SkinID); }
	}

	if (Unowned.Num() == 0) { return INDEX_NONE; } // 등급 전부 보유
	return Unowned[FMath::RandRange(0, Unowned.Num() - 1)];
}

int32 UBuildingSkinManagerSubsystem::PickAnySkinOfRarity(ELootBoxRarity Rarity) const
{
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) { return INDEX_NONE; }

	TArray<FBuildingSkinData> Candidates = TMgr->GetBuildingSkinsByRarity(Rarity);
	Candidates.RemoveAll([](const FBuildingSkinData& S)
	{
		return S.Rarity == ELootBoxRarity::Mythic || S.SkinID == BaseSkinID;
	});

	if (Candidates.Num() == 0) { return INDEX_NONE; }
	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)].SkinID;
}

int32 UBuildingSkinManagerSubsystem::GetDuplicateMileage(ELootBoxRarity Rarity)
{
	switch (Rarity)
	{
	case ELootBoxRarity::Common:    return 1;
	case ELootBoxRarity::Unusual:   return 2;
	case ELootBoxRarity::Rare:      return 4;
	case ELootBoxRarity::Epic:      return 10;
	case ELootBoxRarity::Legendary: return 25;
	default:                        return 1;
	}
}

bool UBuildingSkinManagerSubsystem::ExecuteGachaPull(bool bAdvanced, FBuildingSkinGachaResult& OutResult)
{
	if (!CanExecuteGachaPull(bAdvanced)) { return false; }

	UItemInventoryManager* IIM = GetItemInventoryManager();
	const EItemType TicketType = bAdvanced ? EItemType::SkinTicketAdvanced : EItemType::SkinTicketNormal;
	if (!IIM->SpendItem(TicketType, 1, false)) { return false; }

	// 천장 스냅샷 — Pick 실패 시 롤백용
	FBuildingSkinPityData& PityRef = bAdvanced ? Inventory.GachaData.AdvancedPity : Inventory.GachaData.NormalPity;
	const FBuildingSkinPityData PitySnapshot = PityRef;

	bool bPityGuaranteed = false;
	const ELootBoxRarity Rarity = RollGachaRarity(bAdvanced, bPityGuaranteed);

	bool bDuplicate = false;
	int32 MileageGain = 0;
	int32 SkinID = PickUnownedSkinOfRarity(Rarity);

	if (SkinID == INDEX_NONE)
	{
		// 해당 등급 전부 보유 → 중복 처리(마일리지 환산). 카드는 표시용으로 같은 등급 아무 스킨.
		SkinID = PickAnySkinOfRarity(Rarity);
		if (SkinID == INDEX_NONE)
		{
			// 등급 풀 자체가 비어있음(데이터 오류) — 롤백 + 환불
			UE_LOG(LogTemp, Warning, TEXT("[BuildingSkinManager] 등급 %d 의 스킨 풀이 비어있음 — 롤백/환불"), static_cast<int32>(Rarity));
			PityRef = PitySnapshot;
			IIM->AddItem(TicketType, 1, false);
			return false;
		}
		bDuplicate = true;
		MileageGain = GetDuplicateMileage(Rarity);
		AddMileagePoints(MileageGain);
	}
	else
	{
		GrantSkin(SkinID);
	}

	Inventory.GachaData.TotalPullsAllTime++;
	AddMileagePoints(1); // 풀당 기본 마일리지 1 (특성과 동일)

	OutResult.ResultSkinID = SkinID;
	OutResult.Rarity = Rarity;
	OutResult.bWasPityGuaranteed = bPityGuaranteed;
	OutResult.bFromMileageBox = false;
	OutResult.bDuplicate = bDuplicate;
	OutResult.MileageGained = bDuplicate ? MileageGain : 0;

	OnSkinGachaCompleted.Broadcast(OutResult);
	SaveGameData();
	return true;
}

bool UBuildingSkinManagerSubsystem::ExecuteGachaPullMulti(bool bAdvanced, int32 Count, TArray<FBuildingSkinGachaResult>& OutResults)
{
	OutResults.Reset();
	const int32 N = FMath::Clamp(Count, 1, 100);
	for (int32 i = 0; i < N; ++i)
	{
		if (!CanExecuteGachaPull(bAdvanced)) { break; }
		FBuildingSkinGachaResult R;
		if (!ExecuteGachaPull(bAdvanced, R)) { break; }
		OutResults.Add(R);
	}
	return OutResults.Num() > 0;
}

void UBuildingSkinManagerSubsystem::AddMileagePoints(int32 Amount)
{
	if (Amount <= 0) { return; }
	Inventory.GachaData.MileagePoints += Amount;
}

bool UBuildingSkinManagerSubsystem::ExchangeMileageBox(bool bLegendaryBox, int32 SelectedSkinID, FBuildingSkinGachaResult& OutResult)
{
	const int32 RequiredCost = bLegendaryBox
		? FGachaSkinData::MileageLegendaryBox
		: FGachaSkinData::MileageEpicBox;

	if (Inventory.GachaData.MileagePoints < RequiredCost) { return false; }
	if (SelectedSkinID <= 0) { return false; }

	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!TMgr) { return false; }

	bool bOk = false;
	const FBuildingSkinData Row = TMgr->GetBuildingSkinData(SelectedSkinID, bOk);
	if (!bOk) { return false; }

	const ELootBoxRarity Expected = bLegendaryBox ? ELootBoxRarity::Legendary : ELootBoxRarity::Epic;
	if (Row.Rarity != Expected) { return false; }

	// 이미 보유한 스킨이면 거부 — 스킨은 중복 불가(GrantSkin no-op)라 마일리지만 날아감
	if (IsSkinOwned(SelectedSkinID)) { return false; }

	Inventory.GachaData.MileagePoints -= RequiredCost;
	GrantSkin(SelectedSkinID);

	OutResult.ResultSkinID = SelectedSkinID;
	OutResult.Rarity = Expected;
	OutResult.bWasPityGuaranteed = false;
	OutResult.bFromMileageBox = true;
	OutResult.bDuplicate = false;
	OutResult.MileageGained = 0;

	OnSkinGachaCompleted.Broadcast(OutResult);
	SaveGameData();
	return true;
}

void UBuildingSkinManagerSubsystem::SaveGameData()
{
	if (USaveLoadManager* SLMgr = GetSaveLoadManager())
	{
		SLMgr->SaveGameData();
	}
}
