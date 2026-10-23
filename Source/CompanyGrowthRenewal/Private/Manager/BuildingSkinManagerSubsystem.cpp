// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Table/BuildingSkinData.h"
#include "Enum/ItemType.h"
#include "Engine/GameInstance.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Data/CloudEconomyData.h"
#include "Dom/JsonObject.h"
#include "Utils/GachaBatchMath.h"

namespace
{
	// 기본(시작) 스킨 — 가챠 풀에서 제외
	constexpr int32 BaseSkinID = 100;

	// 서버 카탈로그 reason — consumeRules.json 과 바이트 단위로 같아야 한다(다르면 400)
	const FName ConsumeReason_SkinPull(TEXT("skin.pull"));

	// 중복 클릭 차단 키 — 특성(gacha.trait.*)·채용(gacha.recruit.*)과 겹치지 않는 도메인 접두사
	const FName ActionKey_SkinNormal(TEXT("gacha.skin.normal"));
	const FName ActionKey_SkinAdvanced(TEXT("gacha.skin.advanced"));
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
	return IIM->HasItem(GetTicketType(bAdvanced), 1);
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
	// 티켓 리터럴과 SpendItem 은 이 본문에 남긴다 — export_catalog.py 가 consumeRules.json 의 items/maxPerOp 를 여기서 파싱한다
	const EItemType TicketType = bAdvanced ? EItemType::SkinTicketAdvanced : EItemType::SkinTicketNormal;
	if (!IIM->SpendItem(TicketType, 1, false)) { return false; }

	// 로컬 차감 경로라 등급 풀이 비면 티켓을 되돌릴 수 있다(서버 경로는 되돌리지 않는다)
	if (!ApplyLocalPullEffect(bAdvanced, /*bRefundTicketOnPoolEmpty=*/true, OutResult)) { return false; }

	SaveGameData();
	return true;
}

bool UBuildingSkinManagerSubsystem::ApplyLocalPullEffect(bool bAdvanced, bool bRefundTicketOnPoolEmpty,
	FBuildingSkinGachaResult& OutResult)
{
	// 천장 스냅샷 — 등급 롤이 카운터를 먼저 올리므로 Pick 실패 시 원복해야 한다
	FBuildingSkinPityData& PityRef = bAdvanced ? Inventory.GachaData.AdvancedPity : Inventory.GachaData.NormalPity;
	const FBuildingSkinPityData PitySnapshot = PityRef;

	bool bPityGuaranteed = false;
	const ELootBoxRarity Rarity = RollGachaRarity(bAdvanced, bPityGuaranteed);

	bool bDuplicate = false;
	int32 MileageGain = 0;
	int32 SkinID = PickUnownedSkinOfRarity(Rarity);

	if (SkinID == INDEX_NONE)
	{
		// 해당 등급 전부 보유 -> 중복 처리(마일리지 환산). 카드는 표시용으로 같은 등급 아무 스킨.
		SkinID = PickAnySkinOfRarity(Rarity);
		if (SkinID == INDEX_NONE)
		{
			// 등급 풀 자체가 비어있음(데이터 오류) — 천장 롤백
			UE_LOG(LogTemp, Error, TEXT("[BuildingSkinManager] 등급 %d 의 스킨 풀이 비어있음 — 롤백"), static_cast<int32>(Rarity));
			PityRef = PitySnapshot;
			// 서버가 소모한 티켓은 클라가 되돌리지 않는다 — 다음 /player/state 가 지우는 유령 수량이고 소모품 자체 지급이다
			if (bRefundTicketOnPoolEmpty)
			{
				if (UItemInventoryManager* IIM = GetItemInventoryManager())
				{
					IIM->AddItem(GetTicketType(bAdvanced), 1, false);
				}
			}
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
	// 풀당 기본 마일리지 1 (특성과 동일). 스킨 마일리지는 로컬 권위 — 서버 wallet.mileage 는 채용 풀이다
	AddMileagePoints(1);

	OutResult.ResultSkinID = SkinID;
	OutResult.Rarity = Rarity;
	OutResult.bWasPityGuaranteed = bPityGuaranteed;
	OutResult.bFromMileageBox = false;
	OutResult.bDuplicate = bDuplicate;
	OutResult.MileageGained = bDuplicate ? MileageGain : 0;

	OnSkinGachaCompleted.Broadcast(OutResult);
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

// ===== 서버 권위 뽑기 =====

UCloudEconomySubsystem* UBuildingSkinManagerSubsystem::GetCloudEconomy() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCloudEconomySubsystem>() : nullptr;
}

EItemType UBuildingSkinManagerSubsystem::GetTicketType(bool bAdvanced)
{
	return bAdvanced ? EItemType::SkinTicketAdvanced : EItemType::SkinTicketNormal;
}

FName UBuildingSkinManagerSubsystem::GetPullConsumeReason()
{
	return ConsumeReason_SkinPull;
}

FName UBuildingSkinManagerSubsystem::GachaActionKey(bool bAdvanced)
{
	return bAdvanced ? ActionKey_SkinAdvanced : ActionKey_SkinNormal;
}

int32 UBuildingSkinManagerSubsystem::ResolveRollCount(int32 Requested, int32 ServerAmount)
{
	return FMath::Clamp(ServerAmount, 0, FMath::Max(0, Requested));
}

bool UBuildingSkinManagerSubsystem::ShouldSkipLocalEffect(bool bReplayed)
{
	return bReplayed;
}

void UBuildingSkinManagerSubsystem::RequestGachaPull(bool bAdvanced, int32 RequestedCount, FOnSkinPullDone Done)
{
	// 허용 밖의 수를 줄여 보내면 고른 수와 다른 수가 소모된다 ― pending 을 잡기 전에 거절한다
	if (!GachaBatchMath::IsAllowedPullCount(RequestedCount))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildingSkinManager] 스킨 뽑기 거절 - 1/10/30 회만 허용. Requested=%d Advanced=%d"),
			RequestedCount, bAdvanced ? 1 : 0);
		Done.ExecuteIfBound(false, TArray<FBuildingSkinGachaResult>());
		return;
	}

	const FName ActionKey = GachaActionKey(bAdvanced);

	UCloudEconomySubsystem* Cloud = GetCloudEconomy();
	if (!Cloud)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildingSkinManager] 스킨 뽑기 실패 - CloudEconomySubsystem 없음. Advanced=%d"),
			bAdvanced ? 1 : 0);
		Done.ExecuteIfBound(false, TArray<FBuildingSkinGachaResult>());
		return;
	}

	// 중복 클릭 — 내가 잡은 pending 이 아니므로 풀지 않는다
	if (!Cloud->BeginPendingAction(ActionKey))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildingSkinManager] 스킨 뽑기 요청 중복 - 진행 중. Advanced=%d"), bAdvanced ? 1 : 0);
		Done.ExecuteIfBound(false, TArray<FBuildingSkinGachaResult>());
		return;
	}

	// 로컬 캐시로 가능 판정만 — 차감은 서버가 한다. 다이아 폴백이 없어 티켓이 없으면 요청 자체를 보내지 않는다
	UItemInventoryManager* IIM = GetItemInventoryManager();
	if (!IIM || !IIM->HasItem(GetTicketType(bAdvanced), RequestedCount))
	{
		UE_LOG(LogTemp, Log, TEXT("[BuildingSkinManager] 스킨 뽑기 미전송 - 티켓 부족(로컬 캐시). Count=%d"), RequestedCount);
		FinishServerPull(bAdvanced, /*bOk=*/false, /*Rolls=*/0, /*bReplayed=*/false, ActionKey, Done);
		return;
	}

	SendConsumeThenRoll(bAdvanced, RequestedCount, ActionKey, Done);
}

void UBuildingSkinManagerSubsystem::SendConsumeThenRoll(bool bAdvanced, int32 RequestedCount, FName ActionKey,
	FOnSkinPullDone Done)
{
	UCloudEconomySubsystem* Cloud = GetCloudEconomy();
	if (!Cloud)
	{
		FinishServerPull(bAdvanced, /*bOk=*/false, /*Rolls=*/0, /*bReplayed=*/false, ActionKey, Done);
		return;
	}

	const FName ItemName = FCloudEconomyData::ItemTypeToName(GetTicketType(bAdvanced));

	// 응답 전에 서브시스템이 내려가면 CreateLambda 는 죽은 this 를 만진다
	Cloud->RequestConsume(GetPullConsumeReason(), ItemName, RequestedCount,
		UCloudEconomySubsystem::FOnCloudOp::CreateWeakLambda(this,
			[this, bAdvanced, RequestedCount, ActionKey, Done](const FCloudOpResult& OpResult)
	{
		if (!OpResult.bOk)
		{
			// 스킨은 티켓 전용이라 폴백 경로가 없다 — 실패는 그대로 실패다
			UE_LOG(LogTemp, Warning, TEXT("[BuildingSkinManager] skin.pull 실패 - Code=%s Http=%d"),
				*OpResult.Code.ToString(), OpResult.HttpStatus);
			FinishServerPull(bAdvanced, /*bOk=*/false, /*Rolls=*/0, OpResult.bReplayed, ActionKey, Done);
			return;
		}

		// 요청값이 아니라 서버가 인정한 수량만큼 돌린다
		int32 ServerAmount = 0;
		if (OpResult.Result.IsValid())
		{
			OpResult.Result->TryGetNumberField(TEXT("amount"), ServerAmount);
		}

		FinishServerPull(bAdvanced, /*bOk=*/true, ResolveRollCount(RequestedCount, ServerAmount),
			OpResult.bReplayed, ActionKey, Done);
	}));
}

void UBuildingSkinManagerSubsystem::FinishServerPull(bool bAdvanced, bool bOk, int32 Rolls, bool bReplayed,
	FName ActionKey, FOnSkinPullDone Done)
{
	TArray<FBuildingSkinGachaResult> Results;

	if (bOk && !ShouldSkipLocalEffect(bReplayed) && Rolls > 0)
	{
		Results.Reserve(Rolls);
		for (int32 i = 0; i < Rolls; ++i)
		{
			FBuildingSkinGachaResult Rolled;
			if (!ApplyLocalPullEffect(bAdvanced, /*bRefundTicketOnPoolEmpty=*/false, Rolled))
			{
				// 등급 풀 공백(DT 오류)은 되돌릴 티켓이 없다 — 남은 굴림을 포기하고 크게 남긴다
				UE_LOG(LogTemp, Error, TEXT("[BuildingSkinManager] 서버 뽑기 %d/%d 에서 로컬 등급 풀이 비어 중단"), i + 1, Rolls);
				break;
			}
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
