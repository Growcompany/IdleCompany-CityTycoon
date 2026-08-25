#include "Manager/LaunchLootManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Data/BuildingSaveData.h"
#include "Data/ProjectBoardData.h"
#include "Table/LaunchLootTable.h"

FName ULaunchLootManagerSubsystem::ReviewScoreToTableKey(int32 ReviewScore)
{
	if (ReviewScore >= HighScoreThreshold) return FName(TEXT("High"));
	if (ReviewScore >= MidScoreThreshold) return FName(TEXT("Mid"));
	return FName(TEXT("Low"));
}

FString ULaunchLootManagerSubsystem::GetScoreBandLabel(int32 BandIndex)
{
	if (BandIndex <= 0) return FString::Printf(TEXT("%d점 이하"), MidScoreThreshold - 1);
	if (BandIndex == 1) return FString::Printf(TEXT("%d~%d점"), MidScoreThreshold, HighScoreThreshold - 1);
	return FString::Printf(TEXT("%d점 이상"), HighScoreThreshold);
}

int32 ULaunchLootManagerSubsystem::GetTierRollBonus(int32 Tier)
{
	if (Tier >= 7) return 2;
	if (Tier >= 4) return 1;
	return 0;
}

int32 ULaunchLootManagerSubsystem::GetBaseRolls(FName TableKey)
{
	return (TableKey == FName(TEXT("High"))) ? 3 : 2;
}

int32 ULaunchLootManagerSubsystem::GetProgressionTierContext() const
{
	// OfficeStageProgressManager 는 OfficeMap 전용 WorldSubsystem — 그 외 맵(인수/HQ 훅)은 영속 세이브에서 최고 티어 폴백
	if (const UWorld* CurWorld = GetWorld())
	{
		if (const UOfficeStageProgressManager* StageMgr = CurWorld->GetSubsystem<UOfficeStageProgressManager>())
		{
			return FMath::Clamp(StageMgr->GetTierProgress().CurrentTier, 1, TierConstants::MAX_TIER);
		}
	}

	int32 MaxTier = 1;
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (const USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			for (const TPair<int32, FOfficeSaveData>& OfficePair : SaveData->GameData.OfficeDataMap)
			{
				MaxTier = FMath::Max(MaxTier, OfficePair.Value.TierProgress.CurrentTier);
			}
		}
	}
	return FMath::Clamp(MaxTier, 1, TierConstants::MAX_TIER);
}

TArray<FLaunchLootPreviewEntry> ULaunchLootManagerSubsystem::GetLootPreview(int32 Tier) const
{
	TArray<FLaunchLootPreviewEntry> Out;
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return Out;
	}

	static const FName TableKeys[3] = { FName(TEXT("Low")), FName(TEXT("Mid")), FName(TEXT("High")) };
	for (int32 TableIdx = 0; TableIdx < 3; ++TableIdx)
	{
		const TArray<FLaunchLootTable>* Entries = TableMgr->GetLaunchLootEntries(TableKeys[TableIdx]);
		if (!Entries)
		{
			continue;
		}

		// RollAndGrant 와 같은 티어 게이트 필터 (미리보기라 스케일 기준은 항상 유효로 본다)
		TArray<const FLaunchLootTable*> Candidates;
		int32 TotalWeight = 0;
		for (const FLaunchLootTable& E : *Entries)
		{
			if (E.MinTier > 0 && Tier < E.MinTier) continue;
			if (E.MaxTier > 0 && Tier > E.MaxTier) continue;
			if (E.Weight <= 0) continue;
			Candidates.Add(&E);
			TotalWeight += E.Weight;
		}
		if (TotalWeight <= 0)
		{
			continue;
		}

		for (const FLaunchLootTable* C : Candidates)
		{
			FLaunchLootPreviewEntry* Found = Out.FindByPredicate([&](const FLaunchLootPreviewEntry& P)
			{
				return P.ResourceType == C->ResourceType && P.ItemType == C->ItemType;
			});
			if (!Found)
			{
				FLaunchLootPreviewEntry NewEntry;
				NewEntry.ResourceType = C->ResourceType;
				NewEntry.ItemType = C->ItemType;
				NewEntry.MinScore = (TableIdx == 0) ? 0 : (TableIdx == 1 ? MidScoreThreshold : HighScoreThreshold);
				NewEntry.MaxTier = C->MaxTier;
				Found = &Out[Out.Add(NewEntry)];
			}
			// 같은 보상의 강화 엔트리(x2 등)는 확률 합산으로 병합, 수량은 범위 합집합
			Found->ChancePercent[TableIdx] += FMath::RoundToInt(100.0f * C->Weight / TotalWeight);
			const int32 AmtMin = static_cast<int32>(C->AmountMin);
			const int32 AmtMax = static_cast<int32>(C->AmountMax);
			Found->BandAmountMin[TableIdx] = (Found->BandAmountMin[TableIdx] == 0)
				? AmtMin : FMath::Min(Found->BandAmountMin[TableIdx], AmtMin);
			Found->BandAmountMax[TableIdx] = FMath::Max(Found->BandAmountMax[TableIdx], AmtMax);
		}
	}

	// 메인 보상 우선 표시 (사용자 확정 2026-08-12) — 채용권/특성권이 앞, 스킨/벽돌 필러는 뒤로.
	// 미리보기 스트립이 상한(5장)까지만 펼치므로 이 순서가 곧 "무엇이 접히는가"를 정한다
	auto Priority = [](const FLaunchLootPreviewEntry& P) -> int32
	{
		switch (P.ItemType)
		{
		case EItemType::RecruitTicketNormal: return 1;
		case EItemType::RecruitTicketAdvanced: return 2;
		case EItemType::BuildingTraitTicketNormal: return 3;
		case EItemType::RecruitTicketPremium: return 4;
		case EItemType::BuildingTraitTicketAdvanced: return 5;
		case EItemType::SkinTicketNormal: return 6;
		case EItemType::SkinTicketAdvanced: return 7;
		default: break;
		}
		return (P.ResourceType != EResourceType::None) ? 8 : 9;
	};
	Out.StableSort([&Priority](const FLaunchLootPreviewEntry& A, const FLaunchLootPreviewEntry& B)
	{
		return Priority(A) < Priority(B);
	});
	return Out;
}

TArray<FMissionReward> ULaunchLootManagerSubsystem::RollAndGrant(FName TableKey, int32 Tier, int64 ScaleBasis, int32 ExtraRolls, bool bGrant, bool bSaveAfterGrant)
{
	TArray<FMissionReward> Result;

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	const TArray<FLaunchLootTable>* Entries = TableMgr ? TableMgr->GetLaunchLootEntries(TableKey) : nullptr;
	if (!Entries || Entries->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchLoot] 테이블 없음 또는 빈 테이블: %s"), *TableKey.ToString());
		return Result;
	}

	// 티어 게이트 + 스케일 기준 없는 비례 엔트리 제외
	TArray<const FLaunchLootTable*> Candidates;
	int32 TotalWeight = 0;
	for (const FLaunchLootTable& E : *Entries)
	{
		if (E.MinTier > 0 && Tier < E.MinTier) continue;
		if (E.MaxTier > 0 && Tier > E.MaxTier) continue;
		if (E.bScaleWithProject && ScaleBasis <= 0) continue;
		if (E.Weight <= 0) continue;
		Candidates.Add(&E);
		TotalWeight += E.Weight;
	}
	if (Candidates.Num() == 0 || TotalWeight <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchLoot] 후보 없음: %s (Tier=%d)"), *TableKey.ToString(), Tier);
		return Result;
	}

	// Low 는 티어 롤 보너스 제외 — 재화 엔트리 제거(2026-08-12) 후 롤 수 = 아이템 개수가 되어,
	// 보너스가 붙으면 평범 출시가 고티어에서 티켓 3~4장 확정이 되어 상위 테이블 가치를 역전한다
	const int32 TierBonus = (TableKey == FName(TEXT("Low"))) ? 0 : GetTierRollBonus(Tier);
	const int32 NumRolls = GetBaseRolls(TableKey) + TierBonus + ExtraRolls;
	for (int32 RollIdx = 0; RollIdx < NumRolls; ++RollIdx)
	{
		// 정수 가중 누적 롤 (FLootBoxRarityUtility::RollRandomRarity 패턴)
		const int32 RandomValue = FMath::RandRange(0, TotalWeight - 1);
		int32 Accumulated = 0;
		const FLaunchLootTable* Picked = Candidates.Last();
		for (const FLaunchLootTable* C : Candidates)
		{
			Accumulated += C->Weight;
			if (RandomValue < Accumulated) { Picked = C; break; }
		}

		int64 Amount = FMath::RandRange((int32)Picked->AmountMin, (int32)Picked->AmountMax);
		if (Picked->bScaleWithProject)
		{
			Amount = FMath::Max<int64>(1, ScaleBasis * Amount / 10000);
		}

		// 같은 종류는 병합 (UI 카드 1장으로)
		FMissionReward* Existing = Result.FindByPredicate([&](const FMissionReward& R)
		{
			return R.ResourceType == Picked->ResourceType && R.ItemType == Picked->ItemType;
		});
		if (Existing)
		{
			if (Picked->ResourceType != EResourceType::None) { Existing->Amount += Amount; }
			else { Existing->ItemAmount += (int32)Amount; }
		}
		else
		{
			FMissionReward R;
			R.ResourceType = Picked->ResourceType;
			if (Picked->ResourceType != EResourceType::None) { R.Amount = Amount; }
			else { R.ItemType = Picked->ItemType; R.ItemAmount = (int32)Amount; }
			Result.Add(R);
		}

		UE_LOG(LogTemp, Log, TEXT("[LaunchLoot] Roll %d/%d %s Tier=%d -> Res=%d Item=%d x%lld"),
			RollIdx + 1, NumRolls, *TableKey.ToString(), Tier,
			(int32)Picked->ResourceType, (int32)Picked->ItemType, Amount);
	}

	if (bGrant)
	{
		GrantRewards(Result, bSaveAfterGrant);
	}
	return Result;
}

void ULaunchLootManagerSubsystem::GrantRewards(const TArray<FMissionReward>& Rewards, bool bSave)
{
	UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>();

	// bShouldSave=false — 기본값이면 보상 1건당 전체 세이브(출시 1회 최대 5회)라 말미 1회로 통합 (GoalBoardSubsystem::ClaimGoal 패턴)
	int32 GrantedCount = 0;
	for (const FMissionReward& R : Rewards)
	{
		if (R.ResourceType != EResourceType::None && R.Amount > 0 && ResMgr)
		{
			ResMgr->StoreResource(R.ResourceType, R.Amount, /*bShouldSave=*/false);
			++GrantedCount;
		}
		if (R.ItemType != EItemType::None && R.ItemAmount > 0 && ItemMgr)
		{
			ItemMgr->AddItem(R.ItemType, R.ItemAmount, /*bShouldSave=*/false);
			++GrantedCount;
		}
	}

	if (bSave && GrantedCount > 0)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
}

void ULaunchLootManagerSubsystem::RollLaunchLoot(int32 ReviewScore, int32 ProjectTier)
{
	// 유일한 호출 경로인 StartLaunch() 가 RequestLaunchConfirm() 직후 SaveGameData() 를 부른다 — 여기서 또 저장하면 출시 1회에 2회
	// ScaleBasis 0 = 비례(bScaleWithProject) 엔트리 제외. 출시 전리품은 티켓 고정수량 테이블이라 비례 행이 없다.
	LastLaunchLoot = RollAndGrant(ReviewScoreToTableKey(ReviewScore), ProjectTier, /*ScaleBasis=*/0,
		/*ExtraRolls=*/0, /*bGrant=*/true, /*bSaveAfterGrant=*/false);
}
