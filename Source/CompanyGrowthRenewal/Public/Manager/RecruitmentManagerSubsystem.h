// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/RecruitmentData.h"
#include "Data/EmployeeTypes.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/GachaTier.h"
#include "Enum/ItemType.h"
#include "Enum/ProductionDiscipline.h"
#include "RecruitmentManagerSubsystem.generated.h"

class UEmployeeManager;
class UTableManagerSubsystem;
class UItemInventoryManager;
class UResourceItemManager;
class AWorkstationActorBase;

/**
 * 채용 시스템 매니저 (가챠 기반)
 * - 채용권 3종 가챠 뽑기 (일반/고급/프리미엄)
 * - 천장(Pity) 시스템 (고급/프리미엄)
 * - 마일리지 시스템 (프리미엄 전용)
 * - HR 파워 → 일반 뽑기 확률 보정
 * - 오피스별 업무공간 자리 관리
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URecruitmentManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========== 가챠 뽑기 ==========

	/** 뽑기 실행 (재화 차감 + 결과 생성). bShouldSave=false면 호출자가 세이브 책임 */
	UFUNCTION(BlueprintCallable, Category = "Gacha")
	bool ExecuteGachaPull(EGachaTier Tier, int32 BuildingIndex, FGachaResultData& OutResult,
		bool bShouldSave = true, bool bAllowDiamondFallback = true);

	/** 멀티 뽑기 (전량 ×N) — 실행 시점 재클램프(티켓·정원·5), 티켓 전용, 세이브는 끝 1회 */
	UFUNCTION(BlueprintCallable, Category = "Gacha")
	bool ExecuteGachaPullBatch(EGachaTier Tier, int32 RequestedCount, int32 BuildingIndex,
		TArray<FGachaResultData>& OutResults);

	/** 뽑기 가능 여부 (재화 충분?) */
	UFUNCTION(BlueprintPure, Category = "Gacha")
	bool CanExecuteGachaPull(EGachaTier Tier) const;

	/** 티어별 다이아 폴백 비용 (UI 라벨용 — 하드코딩 중복 방지). 폴백 없으면 0(일반). */
	UFUNCTION(BlueprintPure, Category = "Gacha")
	int32 GetDiamondCost(EGachaTier Tier) const;

	/** 티어별 전용 채용권 타입 (UI 라벨용 — 하드코딩 중복 방지). 라벨과 배치 API가 같은 티켓을 봐야 한다. */
	UFUNCTION(BlueprintPure, Category = "Gacha")
	static EItemType GetTicketTypeForTier(EGachaTier Tier);

	/** 뽑기 결과 고용 확정 + 자동 착석 (만석이면 벤치 유지). PreferredWorkstation = 채용 진입 책상(1순위) */
	UFUNCTION(BlueprintCallable, Category = "Gacha")
	bool ConfirmGachaHire(const FGachaResultData& ResultData, AWorkstationActorBase* PreferredWorkstation = nullptr,
		bool bShouldSave = true);

	/** 일괄 채용 — 뽑기 순서 유지(사번 정합), 첫 명만 preferred 책상, 세이브 끝 1회 */
	UFUNCTION(BlueprintCallable, Category = "Gacha")
	bool ConfirmGachaHireBatch(const TArray<FGachaResultData>& Results,
		AWorkstationActorBase* PreferredWorkstation, int32& OutSeatedCount, int32& OutBenchedCount);

	/** 남은 정원 (멀티 수량 클램프 분모) — 패널 라벨과 배치 API가 같은 값을 봐야 한다. */
	UFUNCTION(BlueprintPure, Category = "Gacha")
	int32 GetFreeCapacity(int32 BuildingIndex) const;

	/** 뽑기 결과 폐기 */
	UFUNCTION(BlueprintCallable, Category = "Gacha")
	void DiscardGachaResult();

	// ========== 천장 / 마일리지 ==========

	/** 현재 Pity 카운트 반환 */
	UFUNCTION(BlueprintPure, Category = "Gacha|Pity")
	int32 GetPityCount(EGachaTier Tier) const;

	/** 현재 마일리지 포인트 반환 */
	UFUNCTION(BlueprintPure, Category = "Gacha|Mileage")
	int32 GetMileagePoints() const;

	/** UI 표시용 등급별 확률표 (private 하드코딩표 미러, 일반 탭은 HR 파워 반영) */
	UFUNCTION(BlueprintCallable, Category = "Gacha|Probability")
	TArray<FGachaRarityChance> GetProbabilityTableForUI(EGachaTier Tier, int32 HRPower) const;

	/** 마일리지 교환 (200pt → Legendary 직원) */
	UFUNCTION(BlueprintCallable, Category = "Gacha|Mileage")
	bool ExchangeMileage(int32 BuildingIndex, FGachaResultData& OutResult);

	/** 마일리지 차감 (상점 등 외부 결제용, 부족 시 false). bShouldSave=false면 호출자가 세이브 책임 */
	UFUNCTION(BlueprintCallable, Category = "Gacha|Mileage")
	bool SpendMileage(int32 Points, bool bShouldSave = true);

	// ========== HR 파워 ==========

	/** 건물의 HR 파워 계산 (HR 직원 강화레벨 합) */
	UFUNCTION(BlueprintPure, Category = "Gacha|HR")
	int32 GetHRPower(int32 BuildingIndex) const;

	// ========== 가챠 델리게이트 ==========

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGachaPullCompleted, const FGachaResultData& /*Result*/);
	FOnGachaPullCompleted OnGachaPullCompleted;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMileageChanged, int32 /*NewPoints*/);
	FOnMileageChanged OnMileageChanged;

	// ========== 가챠 세이브/로드 ==========

	const FGachaRecruitmentData& GetGachaData() const { return GachaData; }
	void SetGachaData(const FGachaRecruitmentData& InData) { GachaData = InData; }

	// ========== 오피스 채용 데이터 초기화 ==========

	UFUNCTION(BlueprintCallable, Category = "OfficeRecruitment|Setup")
	void InitializeOfficeRecruitment(int32 BuildingIndex);

	UFUNCTION(BlueprintCallable, Category = "OfficeRecruitment|Setup")
	void RemoveOfficeRecruitment(int32 BuildingIndex);

	/** 현재 월드의 완공된 회사 건물이 제공하는 영구 직원 정원 합계. 계산 실패 시 bOutComplete=false. */
	int32 GetTotalPermanentEmployeeCapacity(bool& bOutComplete) const;

	/** 계정 정원 최고치의 증가분만 일반 채용권으로 지급한다. 반환값은 이번 지급량. */
	int32 ReconcilePermanentCapacityTickets(bool bShouldSave = true);

	// ========== 오피스 데이터 접근 ==========

	FOfficeRecruitmentData* GetOrCreateOfficeData(int32 BuildingIndex);

	const TMap<int32, FOfficeRecruitmentData>& GetOfficeRecruitmentMap() const { return OfficeRecruitmentMap; }
	void SetOfficeRecruitmentMap(const TMap<int32, FOfficeRecruitmentData>& InMap) { OfficeRecruitmentMap = InMap; }

	// 이 건물 누적 채용 수 (사원증 사번용)
	int32 GetTotalHiredCount(int32 BuildingIndex) const;

	// ========== 튜토리얼 직능 보정 ==========

	// 다음 N회 뽑기의 주 직능을 지정한다. 세이브하지 않는다 — 미션 활성화 때마다 재시드된다.
	void SeedForcedPrimaryDisciplines(const TArray<EProductionDiscipline>& InDisciplines);

private:
	// ========== 가챠 내부 로직 ==========

	// 티어별 확률 테이블 반환 (HR 파워 적용)
	TArray<TPair<ELootBoxRarity, float>> GetProbabilityTable(EGachaTier Tier, int32 HRPower) const;

	// Pity 보정 적용한 잠재능력 등급 롤
	ELootBoxRarity RollPotentialRarity(EGachaTier Tier, int32 HRPower);

	// 재화 차감 (채용권 우선 → Diamond 폴백)
	bool DeductGachaCost(EGachaTier Tier, bool bAllowDiamondFallback);

	// 가챠 직원 인스턴스 생성 (+0 인턴, 랜덤 부서/외모)
	FEmployeeInstance GenerateGachaEmployee(ELootBoxRarity PotentialRarity, int32 BuildingIndex);

	// ========== 가챠 비용 상수 ==========

	static constexpr int32 AdvancedDiamondCost = 30;
	static constexpr int32 PremiumDiamondCost = 50;

	// ========== 데이터 ==========

	FGachaRecruitmentData GachaData;

	// 튜토리얼 강제 주 직능 — 앞에서부터 소비하고, 비면 기존 랜덤 롤로 복귀한다
	TArray<EProductionDiscipline> ForcedPrimaryQueue;

	UPROPERTY()
	TMap<int32, FOfficeRecruitmentData> OfficeRecruitmentMap;

	// ========== 캐시된 서브시스템 참조 ==========

	UPROPERTY()
	mutable UEmployeeManager* EmployeeManager = nullptr;

	UPROPERTY()
	mutable UTableManagerSubsystem* TableManager = nullptr;

	UEmployeeManager* GetEmployeeManager() const;
	UTableManagerSubsystem* GetTableManager() const;

	void SaveGameData();
};
