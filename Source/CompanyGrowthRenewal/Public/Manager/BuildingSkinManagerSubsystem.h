// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingSkinManagerSubsystem.generated.h"

class UTableManagerSubsystem;
class UItemInventoryManager;
class USaveLoadManager;

/**
 * 건물 스킨 가챠 매니저 (BUILDING_SKIN_GACHA v1)
 * - 일반/고급 티켓 2종, 독립 천장(소프트40/하드60 Epic/대천장150 Legendary), 합산 마일리지 — 특성 가챠와 동일 모델.
 * - 소유 스킨은 FGameSaveData.OwnedBuildingSkins(전용 저장소)에 grant. 천장/마일리지만 FBuildingSkinGachaInventory.
 * - 중복(이미 보유) 처리: 같은 등급 미보유 스킨으로 추첨, 등급 전부 보유 시 마일리지 환산.
 * - Mythic 은 일반 풀 제외(특성과 동일). 기본 스킨(SkinID 100)도 풀 제외.
 * 의존성: ItemInventoryManager(티켓), TableManagerSubsystem(DT lookup), SaveLoadManager(소유/천장 영속)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingSkinManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========== 가챠 ==========

	UFUNCTION(BlueprintCallable, Category = "Skin|Gacha")
	bool ExecuteGachaPull(bool bAdvanced, FBuildingSkinGachaResult& OutResult);

	// N개 연속 뽑기 (티켓 N장 소비). 보유 부족 시 가능한 만큼만 — OutResults.Num() = 실제 뽑힌 수.
	UFUNCTION(BlueprintCallable, Category = "Skin|Gacha")
	bool ExecuteGachaPullMulti(bool bAdvanced, int32 Count, TArray<FBuildingSkinGachaResult>& OutResults);

	UFUNCTION(BlueprintCallable, Category = "Skin|Gacha")
	bool CanExecuteGachaPull(bool bAdvanced) const;

	UFUNCTION(BlueprintPure, Category = "Skin|Gacha")
	int32 GetPullsSinceEpic(bool bAdvanced) const;

	UFUNCTION(BlueprintPure, Category = "Skin|Gacha")
	int32 GetPullsSinceLegendary(bool bAdvanced) const;

	UFUNCTION(BlueprintPure, Category = "Skin|Gacha")
	int32 GetMileagePoints() const { return Inventory.GachaData.MileagePoints; }

	/** UI 표시용 등급별 확률표 (RollGachaRarity 기본표 미러, 천장 보정 미반영). 특성과 동일 split. */
	UFUNCTION(BlueprintCallable, Category = "Skin|Gacha")
	TArray<FGachaRarityChance> GetSkinProbabilityTableForUI(bool bAdvanced) const;

	// 마일리지 상자 교환 (Epic 또는 Legendary 등급의 미보유 스킨 선택). v1 패널 버튼은 스텁.
	UFUNCTION(BlueprintCallable, Category = "Skin|Gacha")
	bool ExchangeMileageBox(bool bLegendaryBox, int32 SelectedSkinID, FBuildingSkinGachaResult& OutResult);

	// ========== 소유 조회 ==========

	UFUNCTION(BlueprintPure, Category = "Skin|Inventory")
	bool IsSkinOwned(int32 SkinID) const;

	// 기본 스킨(새 세이브 지급분) 말고 뽑아서 얻은 스킨을 하나라도 들고 있는가 — 미션판 G8 안내 단계 판정용
	bool HasAnyGachaSkin() const;

	// ========== 세이브 / 로드 ==========

	const FBuildingSkinGachaInventory& GetInventory() const { return Inventory; }
	void SetInventory(const FBuildingSkinGachaInventory& InInventory) { Inventory = InInventory; }

	// ========== 델리게이트 ==========

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSkinGachaCompleted, const FBuildingSkinGachaResult& /*Result*/);
	FOnSkinGachaCompleted OnSkinGachaCompleted;

private:
	// 가챠 등급 롤 (천장/소프트 보정 포함) — 특성 RollGachaRarity 와 동일 로직
	ELootBoxRarity RollGachaRarity(bool bAdvanced, bool& bOutPityGuaranteed);

	// 등급 내 '미보유' 스킨 ID 랜덤 선택 (기본/Mythic 제외). 전부 보유면 INDEX_NONE.
	int32 PickUnownedSkinOfRarity(ELootBoxRarity Rarity) const;

	// 등급 내 아무 스킨 ID 랜덤 (중복 표시용 — 풀 비었으면 INDEX_NONE)
	int32 PickAnySkinOfRarity(ELootBoxRarity Rarity) const;

	// 소유 목록에 추가 (이미 보유면 no-op). FGameSaveData.OwnedBuildingSkins 에 기록.
	void GrantSkin(int32 SkinID);

	// 중복(등급 만보유) 시 마일리지 보상량 (등급 비례)
	static int32 GetDuplicateMileage(ELootBoxRarity Rarity);

	void AddMileagePoints(int32 Amount);
	void SaveGameData();

	// 캐시
	UPROPERTY()
	mutable UTableManagerSubsystem* TableManager = nullptr;

	UPROPERTY()
	mutable UItemInventoryManager* ItemInventoryManager = nullptr;

	UPROPERTY()
	mutable USaveLoadManager* SaveLoadManager = nullptr;

	UTableManagerSubsystem* GetTableManager() const;
	UItemInventoryManager* GetItemInventoryManager() const;
	USaveLoadManager* GetSaveLoadManager() const;

	// ========== 데이터 ==========

	UPROPERTY()
	FBuildingSkinGachaInventory Inventory;
};
