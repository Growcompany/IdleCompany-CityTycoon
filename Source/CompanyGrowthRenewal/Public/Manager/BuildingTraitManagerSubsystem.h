// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/GachaRecruitmentData.h"
#include "Table/BuildingTraitTable.h"
#include "Table/BuildingData.h"
#include "Enum/BuildingTraitCategory.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/BuildingTraitTarget.h"
#include "BuildingTraitManagerSubsystem.generated.h"

class UTableManagerSubsystem;
class UItemInventoryManager;
class USaveLoadManager;
class UResourceItemManager;
class USaveGame_GameData;

// 빌딩의 활성 세트 보너스 (UI 표시용)
USTRUCT(BlueprintType)
struct FActiveBuildingTraitSetBonus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EBuildingTraitCategory Category = EBuildingTraitCategory::Revenue;

	UPROPERTY(BlueprintReadOnly)
	int32 CountInBuilding = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RequiredCount = 2;

	UPROPERTY(BlueprintReadOnly)
	bool bActive = false;

	// CalculateSetBonuses 가 (대상, 수치) 로 생성한다 — DT 자유 텍스트가 아니라 표시와 실효과가 어긋날 수 없다.
	UPROPERTY(BlueprintReadOnly)
	FText BonusDescription;

	UPROPERTY(BlueprintReadOnly)
	float BonusValue = 0.0f;
};

/**
 * 건물 특성 매니저 (Phase 2 신규)
 * - 글로벌 인벤토리 (FName TraitID → 보유 수량)
 * - 건물별 슬롯 장착/교체/해제 (FOfficeSaveData.TraitSlots 에 영속)
 * - 분해 (등급별 dust 환원) + 분해 상점 교환
 * - 합성 (3개 → 상위 1개)
 * - 가챠 (일반/고급 티켓 2종, 독립 천장, 합산 마일리지)
 * - 세트 보너스 계산
 *
 * 의존성: ItemInventoryManager (티켓), TableManagerSubsystem (DT lookup), SaveLoadManager (슬롯 영속)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingTraitManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========== 인벤토리 ==========

	UFUNCTION(BlueprintCallable, Category = "Trait|Inventory")
	int32 GetTraitCount(FName TraitID) const;

	UFUNCTION(BlueprintCallable, Category = "Trait|Inventory")
	bool HasTrait(FName TraitID, int32 Amount = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Trait|Inventory")
	void GrantTrait(FName TraitID, int32 Amount = 1, bool bShouldSave = true);

	UFUNCTION(BlueprintCallable, Category = "Trait|Inventory")
	bool RemoveTrait(FName TraitID, int32 Amount = 1, bool bShouldSave = true);

	const TMap<FName, int32>& GetAllOwnedTraits() const { return Inventory.OwnedTraits; }

	// ========== 도감 ==========

	UFUNCTION(BlueprintPure, Category = "Trait|Encyclopedia")
	bool IsTraitInEncyclopedia(FName TraitID) const;

	UFUNCTION(BlueprintPure, Category = "Trait|Encyclopedia")
	int32 GetEncyclopediaCount() const;

	// ========== 장착 / 교체 / 해제 ==========

	UFUNCTION(BlueprintCallable, Category = "Trait|Slot")
	bool EquipTrait(int32 BuildingIndex, int32 SlotIndex, FName TraitID);

	UFUNCTION(BlueprintCallable, Category = "Trait|Slot")
	bool UnequipTrait(int32 BuildingIndex, int32 SlotIndex);

	// 교체: 슬롯의 기존 특성을 인벤토리로 복귀시킨 뒤 새 특성을 장착
	UFUNCTION(BlueprintCallable, Category = "Trait|Slot")
	bool ReplaceTrait(int32 BuildingIndex, int32 SlotIndex, FName NewTraitID);

	UFUNCTION(BlueprintCallable, Category = "Trait|Slot")
	TArray<FName> GetEquippedTraits(int32 BuildingIndex) const;

	// 빌딩 타입에 따라 장착 가능 여부 (제조/프로젝트 전용 특성 필터링)
	UFUNCTION(BlueprintCallable, Category = "Trait|Slot")
	bool CanEquipTraitToBuilding(int32 BuildingIndex, FName TraitID) const;

	// ========== 슬롯 해금 (빌딩 Rarity 무료 + 다이아 구매) ==========

	// 슬롯 용량 (모든 빌딩 공통, 5칸)
	UFUNCTION(BlueprintPure, Category = "Trait|Slot")
	static int32 GetMaxSlotCount() { return FBuildingTraitSlotData::MaxBuildingTraitSlots; }

	// footprint 기반 무료 해금 슬롯 수 = GetFootprintCells() 사용
	UFUNCTION(BlueprintPure, Category = "Trait|Slot")
	int32 GetFreeSlotCount(int32 BuildingIndex) const;

	// 슬롯 해금 여부 (무료 슬롯 OR 다이아 구매 비트마스크)
	UFUNCTION(BlueprintPure, Category = "Trait|Slot")
	bool IsSlotUnlocked(int32 BuildingIndex, int32 SlotIndex) const;

	// 총 사용 가능 슬롯 수 (무료 + 다이아 구매분)
	UFUNCTION(BlueprintPure, Category = "Trait|Slot")
	int32 GetUnlockedSlotCount(int32 BuildingIndex) const;

	// 슬롯 인덱스별 다이아 해금 비용 (점증: 30/60/120/240/480)
	UFUNCTION(BlueprintPure, Category = "Trait|Slot")
	static int32 GetDiamondCostForSlot(int32 SlotIndex);

	// 잠긴 슬롯을 다이아로 영구 해금 (비용 충족 + 차감 + 마스크 set + 저장)
	UFUNCTION(BlueprintCallable, Category = "Trait|Slot")
	bool UnlockSlotWithDiamond(int32 BuildingIndex, int32 SlotIndex);

	// ========== 분해 ==========

	UFUNCTION(BlueprintCallable, Category = "Trait|Dismantle")
	bool DismantleTrait(FName TraitID, int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "Trait|Dismantle")
	int32 GetDust() const { return Inventory.DismantleDust; }

	UFUNCTION(BlueprintPure, Category = "Trait|Dismantle")
	static int32 GetDismantleDustValue(ELootBoxRarity Rarity);

	// 분해 상점 교환 (지정 등급 선택 시 SelectedTraitID 사용)
	UFUNCTION(BlueprintCallable, Category = "Trait|Dismantle")
	bool ExchangeDust(EBuildingTraitDustExchange Exchange, FName SelectedTraitID = NAME_None);

	// ========== 합성 ==========

	// 같은 등급 3개 (또는 5개) → 1개 상위 등급
	UFUNCTION(BlueprintCallable, Category = "Trait|Synthesis")
	bool SynthesizeTraits(const TArray<FName>& MaterialTraitIDs, FBuildingTraitGachaResult& OutResult);

	// ========== 가챠 ==========

	UFUNCTION(BlueprintCallable, Category = "Trait|Gacha")
	bool ExecuteGachaPull(bool bAdvanced, FBuildingTraitGachaResult& OutResult);

	// N개 연속 뽑기 (티켓 N장 소비). 보유 부족 시 가능한 만큼만 — OutResults.Num() = 실제 뽑힌 수.
	UFUNCTION(BlueprintCallable, Category = "Trait|Gacha")
	bool ExecuteGachaPullMulti(bool bAdvanced, int32 Count, TArray<FBuildingTraitGachaResult>& OutResults);

	UFUNCTION(BlueprintCallable, Category = "Trait|Gacha")
	bool CanExecuteGachaPull(bool bAdvanced) const;

	UFUNCTION(BlueprintPure, Category = "Trait|Gacha")
	int32 GetPullsSinceEpic(bool bAdvanced) const;

	UFUNCTION(BlueprintPure, Category = "Trait|Gacha")
	int32 GetPullsSinceLegendary(bool bAdvanced) const;

	UFUNCTION(BlueprintPure, Category = "Trait|Gacha")
	int32 GetMileagePoints() const { return Inventory.GachaData.MileagePoints; }

	/** UI 표시용 등급별 확률표 (RollGachaRarity 기본표 미러, 천장 보정 미반영). bAdvanced=고급 티어. */
	UFUNCTION(BlueprintCallable, Category = "Trait|Gacha")
	TArray<FGachaRarityChance> GetTraitProbabilityTableForUI(bool bAdvanced) const;

	// 마일리지 상자 교환 (Epic 또는 Legendary 등급에서 선택)
	UFUNCTION(BlueprintCallable, Category = "Trait|Gacha")
	bool ExchangeMileageBox(bool bLegendaryBox, FName SelectedTraitID, FBuildingTraitGachaResult& OutResult);

	// ========== 효과 집계 (게임플레이 적용 레이어) ==========

	// 장착 특성(주/2차 대상) + 활성 세트보너스 + 키스톤 오라 + 마인드셋 메타 증폭까지 합산한 퍼센트
	UFUNCTION(BlueprintCallable, Category = "Trait|Effect")
	float GetAggregatedTraitPercent(int32 BuildingIndex, EBuildingTraitTarget Target) const;

	// factor = 1 + GetAggregatedTraitPercent/100 (None/빈슬롯이면 1.0)
	// ⚠ FlatCount 대상(HRPower/ProductionCount)에는 쓰지 말 것 — 배율이 아니라 개수 가산이다.
	UFUNCTION(BlueprintCallable, Category = "Trait|Effect")
	float GetTraitFactor(int32 BuildingIndex, EBuildingTraitTarget Target) const;

	// 장착 특성 + 세트 보너스만 합산 (오라/메타 증폭 제외).
	// AuraPower/MetaAmplify 자체를 구할 때 쓴다 — GetAggregatedTraitPercent 로 구하면 무한 재귀가 된다.
	float GetRawTraitPercent(int32 BuildingIndex, EBuildingTraitTarget Target) const;

	// 전사(company-wide) 적용 — 건물별 값 중 최댓값. TradeValue 전용.
	// 합산이 아닌 이유: 건물 도배가 지배 전략이 되는 걸 막고 상한을 한 분야 몰빵값으로 자연히 닫기 위함.
	float GetCompanyWideTraitPercent(EBuildingTraitTarget Target) const;

	// ========== 세트 보너스 ==========

	UFUNCTION(BlueprintCallable, Category = "Trait|SetBonus")
	TArray<FActiveBuildingTraitSetBonus> CalculateSetBonuses(int32 BuildingIndex) const;

	// ========== 세이브 / 로드 ==========

	const FBuildingTraitInventory& GetInventory() const { return Inventory; }
	void SetInventory(const FBuildingTraitInventory& InInventory) { Inventory = InInventory; }

	// ========== 델리게이트 ==========

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTraitInventoryChanged, FName /*TraitID*/, int32 /*NewCount*/);
	FOnTraitInventoryChanged OnTraitInventoryChanged;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTraitSlotChanged, int32 /*BuildingIndex*/, int32 /*SlotIndex*/, FName /*NewTraitID*/);
	FOnTraitSlotChanged OnTraitSlotChanged;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTraitSlotUnlocked, int32 /*BuildingIndex*/, int32 /*SlotIndex*/);
	FOnTraitSlotUnlocked OnTraitSlotUnlocked;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTraitGachaCompleted, const FBuildingTraitGachaResult& /*Result*/);
	FOnTraitGachaCompleted OnTraitGachaCompleted;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDustChanged, int32 /*NewDust*/);
	FOnDustChanged OnDustChanged;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEncyclopediaProgressed, FName /*NewTraitID*/);
	FOnEncyclopediaProgressed OnEncyclopediaProgressed;

	// 슬롯 구성이 바뀔 때 호출 — raw 집계 캐시를 버린다.
	void InvalidateTraitCache();

private:
	// ========== 내부 로직 ==========

	// 슬롯 구성에만 의존하는 raw 집계 캐시. 피로/방치 틱이 매 프레임 부르므로 DT 조회 + 세트 재계산을
	// 그대로 두면 모바일에서 비싸다. 오라/메타는 외부 요인(건물 배치/레벨)으로 변하므로 캐시하지 않는다.
	mutable TMap<int32, TMap<EBuildingTraitTarget, float>> RawPercentCache;

	// 캐시를 만든 세이브 객체. 레벨 전환/재로드로 세이브가 갈리면 슬롯도 갈리므로 자동으로 버린다.
	mutable TWeakObjectPtr<USaveGame_GameData> RawPercentCacheOwner;

	// 가챠 잠재능력 등급 롤 (천장/소프트 보정 포함)
	ELootBoxRarity RollGachaRarity(bool bAdvanced, bool& bOutPityGuaranteed);

	// 등급별 랜덤 TraitID 선택 (Mythic 제외)
	FName PickRandomTraitOfRarity(ELootBoxRarity Rarity, EBuildingTraitCategory CategoryFilter = EBuildingTraitCategory::Revenue, bool bUseFilter = false) const;

	// 슬롯 인덱스 변경 후 도감 등록 + 알림
	void TryRegisterEncyclopedia(FName TraitID);

	// 슬롯 직접 접근 — 쓰기 경로 전용 (없으면 생성 + 5칸 정규화). const 아님(저장 변경).
	FBuildingTraitSlotData* GetOrCreateSlotData(int32 BuildingIndex);

	// 슬롯 읽기 전용 — 없으면 nullptr (pure getter 가 phantom 엔트리를 저장에 만들지 않도록)
	const FBuildingTraitSlotData* FindSlotData(int32 BuildingIndex) const;

	// BuildingIndex → FBuildingData 공통 해석 (GetFootprintCells 등 공유)
	bool ResolveBuildingData(int32 BuildingIndex, FBuildingData& OutData) const;

	// footprint 칸수 = Width * Depth (최소 1). GetFreeSlotCount 용.
	int32 GetFootprintCells(int32 BuildingIndex) const;

	// 마일리지 누적 + 자동 임계 검사 (안내만, 자동 교환은 안 함 — 사용자 선택)
	void AddMileagePoints(int32 Amount);

	// 저장 트리거
	void SaveGameData();

	// 캐시
	UPROPERTY()
	mutable UTableManagerSubsystem* TableManager = nullptr;

	UPROPERTY()
	mutable UItemInventoryManager* ItemInventoryManager = nullptr;

	UPROPERTY()
	mutable USaveLoadManager* SaveLoadManager = nullptr;

	UPROPERTY()
	mutable UResourceItemManager* ResourceManager = nullptr;

	UTableManagerSubsystem* GetTableManager() const;
	UItemInventoryManager* GetItemInventoryManager() const;
	USaveLoadManager* GetSaveLoadManager() const;
	UResourceItemManager* GetResourceManager() const;

	// ========== 데이터 ==========

	UPROPERTY()
	FBuildingTraitInventory Inventory;
};
