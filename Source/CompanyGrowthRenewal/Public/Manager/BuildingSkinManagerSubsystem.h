// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/ItemType.h"
#include "BuildingSkinManagerSubsystem.generated.h"

class UTableManagerSubsystem;
class UItemInventoryManager;
class USaveLoadManager;
class UCloudEconomySubsystem;

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

	// ========== 서버 권위 뽑기 ==========

	/** 뽑기 1건 완료. bOk=true 라도 Results 가 빌 수 있다(리플레이·서버 인정 0) — 호출부는 Num() 을 보고 연출을 띄운다 */
	DECLARE_DELEGATE_TwoParams(FOnSkinPullDone, bool /*bOk*/, const TArray<FBuildingSkinGachaResult>& /*Results*/);

	/**
	 * `/item/consume`(skin.pull) 1 op -> 성공 응답 뒤에만 로컬 RNG N회.
	 * 스킨은 티켓 전용이라 다이아 폴백이 없다 — 실패하면 로컬 효과를 적용하지 않는다.
	 * 수량은 1·10·30 만 받는다 ― 그 밖의 수는 줄이지 않고 Warning 후 Done 에 bOk=false. 같은 배너의 중복 클릭은 pending 플래그가 막는다.
	 */
	void RequestGachaPull(bool bAdvanced, int32 RequestedCount, FOnSkinPullDone Done);

	/** consumeRules.json `skin.pull` 의 maxPerOp = 허용 수(GachaBatchMath::IsAllowedPullCount)의 최댓값. 더 보내면 서버가 400 BAD_REQUEST 로 거절한다 */
	static constexpr int32 MaxPullsPerOp = 30;

	/** 로컬 RNG 횟수 = 서버가 인정한 수량과 요청 수량 중 작은 쪽. 응답을 못 읽었으면 0 */
	static int32 ResolveRollCount(int32 Requested, int32 ServerAmount);

	/** true = 서버가 영수증을 재생했다. 다시 굴리면 플레이어가 본 스킨이 바뀐다 */
	static bool ShouldSkipLocalEffect(bool bReplayed);

	/** `/item/consume` reason — consumeRules.json 과 바이트 단위로 같아야 한다(다르면 400) */
	static FName GetPullConsumeReason();

	/** 티켓 아이템 — 서버 `items` 허용목록과 같은 식별자로 직렬화된다 */
	static EItemType GetTicketType(bool bAdvanced);

	/** 중복 클릭 차단 키 — 배너별로 따로 잠그고 특성·채용 키와도 겹치지 않는다 */
	static FName GachaActionKey(bool bAdvanced);

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

	// 뽑기 1회의 로컬 효과(등급 롤·지급·마일리지·천장). 등급 풀이 비면 천장을 원복하고 false.
	// bRefundTicketOnPoolEmpty 는 로컬 차감 경로만 true — 서버가 소모한 티켓을 클라가 되돌리면 유령 수량이 된다.
	bool ApplyLocalPullEffect(bool bAdvanced, bool bRefundTicketOnPoolEmpty, FBuildingSkinGachaResult& OutResult);

	void SendConsumeThenRoll(bool bAdvanced, int32 RequestedCount, FName ActionKey, FOnSkinPullDone Done);

	// 성공·실패 모든 경로의 유일한 종료 지점 — pending 해제와 Done 호출이 여기 하나로 모인다.
	void FinishServerPull(bool bAdvanced, bool bOk, int32 Rolls, bool bReplayed, FName ActionKey, FOnSkinPullDone Done);

	UCloudEconomySubsystem* GetCloudEconomy() const;

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
