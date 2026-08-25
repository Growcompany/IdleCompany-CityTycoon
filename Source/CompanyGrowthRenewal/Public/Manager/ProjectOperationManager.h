#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Data/StageProgressData.h"
#include "Data/OperationData.h"
#include "Data/BuildingSaveData.h"
#include "Data/ProjectReportData.h"
#include "Enum/QualityGrade.h"
#include "Enum/OperationState.h"
#include "ProjectOperationManager.generated.h"

class UResourceItemManager;
class UTableManagerSubsystem;
class UEmployeeManager;

// ===== Delegate Declarations =====

// 운영 시작 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOperationStarted, int32, BuildingID);

// 운영 완료 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOperationCompleted, int32, BuildingID, const FOperationData&, Data);

// 창고 업데이트 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWarehouseUpdated, int32, BuildingID, float, Amount, float, Capacity);

// 피로도 변경 시

// 수익 수령 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRevenueCollected, int64, Amount);

// 운영 업데이트 시 (매 틱)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOperationUpdated, int32, BuildingID, const FOperationData&, Data);

// 직원 수익 코인 연출 요청 (직원 월드 위치 → 돈 아이콘, OfficeLayer 가 구독)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnIncomeCoinRequested, FVector, WorldPos, int64, Amount, int32, EmployeeID);

/**
 * 프로젝트 운영 매니저
 *
 * 출시된 프로젝트의 운영을 관리하는 GameInstanceSubsystem
 * - 프로젝트 수명 동안 수익 창출
 * - 플레이어 접속 여부에 따른 실시간 획득 / 창고 누적
 * - 피로도 시스템으로 수익 조절
 * - 창고 용량 관리 및 수령
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProjectOperationManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UProjectOperationManager();

	// 직원 수익 코인 연출 요청 브로드캐스트 (연출 전용 — 재화 적립과 무관)
	FOnIncomeCoinRequested OnIncomeCoinRequested;
	// EmployeeID 는 연출 스로틀을 직원별로 나누기 위한 키 — 전역 게이트면 먼저 틱한 직원이 코인을 독식한다.
	void RequestIncomeCoin(const FVector& WorldPos, int64 Amount, int32 EmployeeID)
	{
		if (Amount > 0) { OnIncomeCoinRequested.Broadcast(WorldPos, Amount, EmployeeID); }
	}

	// ===== GameInstanceSubsystem Lifecycle =====
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ===== Operation Start/End =====

	/**
	 * 운영 시작
	 * @param StageData 스테이지 진행 데이터 (품질 정보 포함)
	 * @param BuildingID 건물 ID
	 * @return 생성된 운영 인덱스 (-1: 실패)
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation")
	int32 StartOperation(const FStageProgressData& StageData, int32 BuildingID);

	/**
	 * 운영 종료 (수동 종료 또는 수명 만료)
	 * @param OperationIndex 운영 인덱스
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation")
	void EndOperation(int32 OperationIndex);

	/**
	 * 특정 건물의 모든 운영 종료
	 * @param BuildingID 건물 ID
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation")
	void EndAllOperationsByBuilding(int32 BuildingID);

	// ===== Update =====

	/**
	 * 운영 업데이트 (타이머에서 호출)
	 * @param DeltaTime 경과 시간
	 */
	void UpdateOperations(float DeltaTime);

	// ===== Warehouse =====

	/**
	 * 창고 수익 수령
	 * @param OperationIndex 운영 인덱스
	 * @return 수령한 금액
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Warehouse")
	int64 CollectWarehouseRevenue(int32 OperationIndex);

	/**
	 * 특정 건물의 모든 창고 수익 수령
	 * @param BuildingID 건물 ID
	 * @return 수령한 총 금액
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Warehouse")
	int64 CollectAllWarehouseByBuilding(int32 BuildingID);

	/**
	 * 모든 빌딩의 저장된 수익을 한 번에 수령 (MainMap 하단바 "전체 수거" 버튼용)
	 * @return 총 수령 금액 (빌딩별 합산)
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Warehouse")
	int64 CollectAllStoredRevenue();

	/**
	 * 모든 빌딩의 저장 수익을 건물당 AmountPerBuilding(원, 기본 500만) 절대액으로 즉시 강제 충전 (트레일러 "방치 수익" 컷).
	 * 금고 용량 무시 — 수거(CollectStoredRevenue)는 용량 재클램프 없이 그대로 회수하므로 대량 코인 연출이 나온다.
	 * CollectAllStoredRevenue 의 역 — 전 빌딩 순회하며 채우고 OnWarehouseUpdated 방송(버블 표시). 수거는 안 함.
	 * @return 충전한 빌딩 수
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Warehouse")
	int32 FillAllStoredRevenue(int64 AmountPerBuilding = 5000000);

	/**
	 * 수거 가능한 저장 수익이 하나라도 있는지 (비파괴 — AlertMark 판정용)
	 * CollectAllStoredRevenue 와 동일하게 OfficeDataMap 전 빌딩을 순회하되 수거하지 않고 조기 종료
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Warehouse")
	bool HasAnyStoredRevenue() const;

	/**
	 * 전 빌딩의 저장 수익 합계 (비파괴 — 수거 버튼 금액 표시용).
	 * HasAnyStoredRevenue 의 정량판. CollectAllStoredRevenue 와 같은 순회를 쓰되 회수하지 않는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Warehouse")
	double GetTotalStoredRevenue() const;

	// ===== 기대 수익 (직원 잠재 큐브 수익줄 기반) =====

	// 빌딩 소속 직원 잠재 수익줄 평균 기반 수익 배율 — 결과 캐싱
	UFUNCTION(BlueprintPure, Category = "Operation|Expected")
	float CalculateEmployeeStatBonus(int32 BuildingID) const;

	// 해당 빌딩의 스탯 보너스 캐시 무효화 — 직원 배치/강화/빌딩 강화 시점에 호출
	UFUNCTION(BlueprintCallable, Category = "Operation|Expected")
	void InvalidateStatBonusCache(int32 BuildingID);

	// 해당 빌딩의 ActualRevenuePerSecond 즉시 재계산 + OnExpectedRevenueChanged 브로드캐스트
	UFUNCTION(BlueprintCallable, Category = "Operation|Expected")
	void RefreshExpectedRevenue(int32 BuildingID);

	// 기대 수익 변경 델리게이트 (BuildingID, NewRate) — UI 위젯이 구독
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExpectedRevenueChanged, int32, float);
	FOnExpectedRevenueChanged OnExpectedRevenueChanged;

	// ===== 표시용 순수익 정의 (SOT) — "감쇠 포함, 진동 제외" =====
	// 지급 로직(틱)과 분리된 표시 전용 값. 세 표시 지점(HQ 목록/스트립/관리패널)이 전부 이 함수 경유.
	// 25초 Volatility 진동은 상시 지표에선 노이즈라 제외 (평균 보존 연출은 세일즈 커브 소관).
	float GetBuildingDisplayNetPerSec(int32 BuildingID) const;

	// 회사 전체 = Σ빌딩 표시순수익 + Σ모뉴먼트 패시브. Money 툴팁 드릴다운이 클릭 시 조회.
	double GetCompanyNetPerSec() const;

	// ===== Query =====

	/**
	 * 활성 운영 목록 조회
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	const TArray<FOperationData>& GetActiveOperations() const { return ActiveOperations; }

	/**
	 * 특정 건물의 운영 데이터 조회
	 * @param BuildingID 건물 ID
	 * @return 운영 데이터 포인터 (없으면 nullptr)
	 */
	FOperationData* GetOperationByBuildingID(int32 BuildingID);

	/**
	 * 인덱스로 운영 데이터 조회
	 * @param OperationIndex 운영 인덱스
	 * @return 운영 데이터 포인터 (없으면 nullptr)
	 */
	FOperationData* GetOperationByIndex(int32 OperationIndex);

	/**
	 * 특정 건물의 운영 데이터 조회 (블루프린트용, 복사본 반환)
	 * @param BuildingID 건물 ID
	 * @param OutData 출력 운영 데이터
	 * @return 운영 존재 여부
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	bool GetOperationByBuildingID_BP(int32 BuildingID, FOperationData& OutData);

	/**
	 * 인덱스로 운영 데이터 조회 (블루프린트용, 복사본 반환)
	 * @param OperationIndex 운영 인덱스
	 * @param OutData 출력 운영 데이터
	 * @return 운영 존재 여부
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	bool GetOperationByIndex_BP(int32 OperationIndex, FOperationData& OutData);

	/**
	 * 특정 건물에 활성 운영이 있는지 확인
	 * @param BuildingID 건물 ID
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	bool HasActiveOperation(int32 BuildingID) const;

	/**
	 * 플레이어가 해당 오피스에 있는지 확인
	 * @param BuildingID 건물 ID
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	bool IsPlayerInOffice(int32 BuildingID) const;

	/**
	 * 운영 개수 조회
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	int32 GetActiveOperationCount() const { return ActiveOperations.Num(); }

	/**
	 * 특정 건물의 저장된 수익(창고 누적) 조회
	 * @param BuildingID 건물 ID
	 * @return 저장된 수익 금액 (데이터 없으면 0)
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Query")
	float GetStoredRevenue(int32 BuildingID) const;

	// ===== 백로그 제품 등록 =====

	// 클리어 완료 제품을 백로그에 추가 — 자동 순익 엔트리 생성 및 세이브
	void AddBacklogProduct(const FStageProgressData& StageData, int32 BuildingID);

	// ===== 백로그 순익 계산 (Phase 1, double) =====
	double ComputeProductNetPerSec(const FBacklogProductEntry& E, const FDateTime& NowUtc) const;
	double ComputeBuildingNetPerSec(int32 BuildingID, const FDateTime& NowUtc) const;
	double ComputeBuildingAccrual(int32 BuildingID, const FDateTime& FromUtc, const FDateTime& ToUtc) const;

	// ===== Calculations =====

	// 운영시간 하한(분). 티어1 1번 프로젝트는 등급×번호 곡선상 1.5분이라 "운영 중 수집"을 배울 창이 안 나온다.
	// 하한 위 구간은 곡선 그대로여서 후반 방치 곡선에는 영향이 없다.
	static constexpr float MinOperationMinutes = 3.0f;

	// 하한이 걸린 곡선값(초). 등급×번호 축은 여기 한 곳만 안다.
	static float ComputeOperationCurveSeconds(EQualityGrade Grade, int32 ProjectNumber);

	/** 착수가 실제로 확정하는 운영시간(초) — 곡선 × 수명강화 × IP특성.
	 *  출시확인/픽칭 카드가 이 함수를 거치지 않으면 표기와 실제가 갈린다(표기 9분 / 실제 3분 사고). */
	float ComputeEffectiveOperationTime(EQualityGrade Grade, int32 ProjectNumber, int32 BuildingID) const;

	/**
	 * 기본 수익 계산
	 * @param Data 스테이지 데이터
	 * @return 초당 기본 수익
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Calculation")
	float CalculateBaseRevenue(const FStageProgressData& Data) const;

	// 규모 스칼라 진입점 — 스테이지 없이 프로젝트 번호만으로 기본 수익을 뽑는다(금고 용량 기준축).
	float CalculateBaseRevenueForProjectNumber(int32 ProjectNumber) const;

	// 산업 수익 인격 노브(DT_IndustryProfile) — 피크/감쇠반감기/변동성. 미정의 산업이면 (1, 0.5, 0).
	void GetIndustryCurveKnobs(ECompanyType Industry, float& OutPeakMult, float& OutHalfLifeFrac, float& OutVolatility) const;

	/** 품질 등급 -> 수익 배율. ReviewLeverage 는 산업별 등급 격차 강도(게임 1.5 증폭 / IT 0.6 압축).
	 *  StabilityPct(0~1) = 수익 안정성 특성 — 낮은 등급을 그 산업의 S 등급 쪽으로 당긴다.
	 *  S 는 이미 상단이라 무효과이고, 어떤 등급에서도 손해가 나지 않는다(단조 증가). */
	static float ComputeQualityRevenueMult(EQualityGrade Grade, float ReviewLeverage, float StabilityPct = 0.0f)
	{
		auto GradeMult = [ReviewLeverage](EQualityGrade G)
		{
			return 1.0f + ReviewLeverage * (GetQualityGradeRevenueMultiplier(G) - 1.0f);
		};
		return FMath::Lerp(GradeMult(Grade), GradeMult(EQualityGrade::S), FMath::Clamp(StabilityPct, 0.0f, 1.0f));
	}

	/** 운영 전 구간 평균 배율 = ∫0..T 0.5^(t/(T·HLF)) dt / T = HLF/ln2 · (1-0.5^(1/HLF)).
	 *  총 누적 수익 = 피크 rps × T × 이 값. 카드 추정·결과 화면이 같은 자로 재려면 둘 다 여기를 불러야 한다.
	 *  HLF 0 이하(DT 결손)는 0.5 로 폴백 — 0 나눗셈 방지. */
	static float ComputeDecayIntegralPerSec(float HalfLifeFrac)
	{
		const float HLF = (HalfLifeFrac > 0.0f) ? HalfLifeFrac : 0.5f;
		return HLF / 0.6931472f * (1.0f - FMath::Pow(0.5f, 1.0f / HLF));
	}

	/** 출시 직후(피크) 초당 수익 = 규모 기본 × 산업 피크 × 트렌드 × 품질.
	 *  ⚠ 수익 항 목록의 단일 출처 — 항을 더하거나 빼면 여기만 고친다.
	 *  피치 카드 추정과 출시 결과 화면이 둘 다 이걸 부르므로 항이 갈릴 수 없다. */
	static float ComputePeakRevenuePerSec(float BaseRevenuePerSec, float PeakMult, float TrendMult, float QualityMult)
	{
		return BaseRevenuePerSec * PeakMult * TrendMult * QualityMult;
	}

	/** 운영 전 구간 총수익 = 피크 초당 × 운영시간 × 감쇠 적분. */
	static float ComputeLaunchRevenue(float BaseRevenuePerSec, float PeakMult, float TrendMult,
		float QualityMult, float OpSeconds, float HalfLifeFrac)
	{
		return ComputePeakRevenuePerSec(BaseRevenuePerSec, PeakMult, TrendMult, QualityMult)
			* OpSeconds * ComputeDecayIntegralPerSec(HalfLifeFrac);
	}

	// 수익 안정성 특성 합계 -> 0~1. ComputeQualityRevenueMult 의 StabilityPct 인자용.
	// 착수/백로그/결산/피치추정이 **같은 건물**로 이걸 불러야 카드 표기와 실제가 갈리지 않는다.
	float GetRevenueStabilityFrac(int32 BuildingID) const;

	/**
	 * 창고 용량 계산 (건물 레벨 기반)
	 * @param BuildingID 건물 ID
	 * @return 창고 용량
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Calculation")
	float CalculateWarehouseCapacity(int32 BuildingID) const;

	// 임의 강화 레벨 기준 금고 용량(원) — 강화 슬롯의 "현재 → 다음" 미리보기용.
	// 현재 레벨판(CalculateWarehouseCapacity)이 여기로 위임하므로 특성/하한 적용이 갈라지지 않는다.
	float CalculateWarehouseCapacityAtLevel(int32 BuildingID, int32 VaultLevel) const;

	// 금고 용량의 기준 레이트(원/초). 빌딩 티어 밴드 첫 프로젝트(BandStart) 기준 — 운영 유무·진행 중 프로젝트와 무관하게 항상 같은 값.
	// 같은 티어 후속 프로젝트는 기준보다 수익이 높아 금고를 더 빨리 채우는 것이 의도다.
	// ⚠단조성: 감쇠/품질 굴림/진동 배제. 금고 용량(원)의 단일 소스.
	float GetVaultReferenceRatePerSecond(int32 BuildingID) const;

	// ===== Delegates =====

	UPROPERTY(BlueprintAssignable, Category = "Operation|Events")
	FOnOperationStarted OnOperationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Operation|Events")
	FOnOperationCompleted OnOperationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Operation|Events")
	FOnWarehouseUpdated OnWarehouseUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Operation|Events")
	FOnRevenueCollected OnRevenueCollected;

	UPROPERTY(BlueprintAssignable, Category = "Operation|Events")
	FOnOperationUpdated OnOperationUpdated;

	// ===== Save/Load =====

	/**
	 * 운영 데이터 설정 (로드용)
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|SaveLoad")
	void SetActiveOperations(const TArray<FOperationData>& InOperations);

	/**
	 * 다음 운영 ID 설정 (로드용)
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|SaveLoad")
	void SetNextOperationID(int32 InNextID) { NextOperationID = InNextID; }

	/**
	 * 다음 운영 ID 조회 (저장용)
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|SaveLoad")
	int32 GetNextOperationID() const { return NextOperationID; }

	/**
	 * 저장된 운영 데이터 복원
	 * @param InOperation 복원할 운영 데이터
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|SaveLoad")
	void RestoreOperation(const FOperationData& InOperation);

	/**
	 * SaveData에서 모든 활성 Operation 로드 (게임 시작 시 호출)
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|SaveLoad")
	void LoadAllOperationsFromSave();

	// ===== Report =====

	/**
	 * 직원이 벌어들인 수익을 운영 데이터에 추가 (Office 직접 관리 시)
	 * @param BuildingID 건물 ID
	 * @param Amount 수익 금액
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Revenue")
	void AddEmployeeRevenue(int32 BuildingID, float Amount);

	/**
	 * 해당 건물에 미확인 결산서가 있는지 확인
	 */
	UFUNCTION(BlueprintPure, Category = "Operation|Report")
	bool HasPendingReport(int32 BuildingID) const;

	/**
	 * 해당 건물의 미확인 결산서 (없으면 nullptr)
	 */
	const FProjectReportData* GetPendingReport(int32 BuildingID) const;

	/**
	 * 해당 건물의 미확인 결산서 소비 (읽은 후 삭제). 없으면 빈 구조체 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "Operation|Report")
	FProjectReportData ConsumeReport(int32 BuildingID);

protected:
	/**
	 * 개별 운영 업데이트
	 * @param Operation 운영 데이터
	 * @param DeltaTime 경과 시간
	 */
	void UpdateSingleOperation(FOperationData& Operation, float DeltaTime);

	/**
	 * 수익 처리 (실시간 획득 또는 창고 누적)
	 * @param Operation 운영 데이터
	 * @param RevenueAmount 수익 금액
	 */
	void ProcessRevenue(FOperationData& Operation, float RevenueAmount);

	/**
	 * 운영 완료 처리
	 * @param OperationIndex 운영 인덱스
	 */
	void OnOperationComplete(int32 OperationIndex);

	/**
	 * 운영 완료 시 경험치 분배
	 * @param Operation 완료된 운영 데이터
	 */
	void DistributeOperationExperience(const FOperationData& Operation);

private:
	// 활성 운영 목록
	UPROPERTY()
	TArray<FOperationData> ActiveOperations;

	// 다음 운영 ID
	UPROPERTY()
	int32 NextOperationID = 1;

	// FTSTicker 델리게이트 핸들
	FTSTicker::FDelegateHandle TickDelegateHandle;

	// Ticker 콜백 함수
	bool TickCallback(float DeltaTime);

	// 타이머 틱 간격 (초)
	static constexpr float TimerTickInterval = 1.0f;

	// 캐시된 매니저 참조
	UPROPERTY()
	mutable UResourceItemManager* CachedResourceItemManager = nullptr;

	UPROPERTY()
	mutable UTableManagerSubsystem* CachedTableManager = nullptr;

	// 매니저 캐시 초기화
	UResourceItemManager* GetResourceItemManager() const;
	UTableManagerSubsystem* GetTableManager() const;

	// ===== Building 데이터 접근 (수익 저장/피로도) =====

	// BuildingID로 OfficeSaveData 조회
	FOfficeSaveData* GetOfficeSaveDataByBuildingID(int32 BuildingID) const;

	// BuildingID로 저장된 수익 수령
	int64 CollectStoredRevenueByBuilding(int32 BuildingID);

	// 빌딩 강화 MarketingPower + 성급 보너스 최종 배율 (SaveData 기반 — MainMap/OfficeMap 양쪽 안전)
	float GetBuildingIncomeMultiplier(int32 BuildingID) const;

	// 일반 강화 슬롯의 순수 배율 조회 (성급 보너스 없음). ProjectLifespan, EventResistance 등에서 재사용.
	float GetBuildingEnhancementMultiplier(int32 BuildingID, EBuildingEnhancementType Type) const;

	// 빌딩별 스탯 보너스 캐시(체력+수익보너스) — 이벤트 기반 무효화. 매 tick 순회 비용 회피.
	mutable TMap<int32, float> CachedStatBonus;

	// 백로그 소수점 수익 누적 버퍼 (매 틱 분수 누락 방지 — 1.0 초과 시 정산)
	TMap<int32, double> EarningsBuffer;

	// 기본 상수
	// BaseVaultCapacity는 ABuildingBaseActor::BaseVaultCapacity 참조

	// ===== Report =====
	// 결산서는 FOfficeSaveData(건물별)에만 산다 — 매니저는 캐시를 두지 않는다.

	// 운영 완료 시 결산서 생성
	void GenerateReport(const FOperationData& CompletedOperation);
};
