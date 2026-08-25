#include "Manager/ProjectOperationManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/EntityManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/TrendManagerSubsystem.h"
#include "TableManagerSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Engine/World.h"
#include "Data/BuildingEnhancementData.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Enum/BuildingTraitTarget.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeePotentialData.h"
#include "Data/ProjectBoardData.h"
#include "Data/TierLayout.h"

// 이 파일에서 특성 배율을 네 군데(감쇠/수명/스프레드/금고)에 붙이므로 획득을 한 곳으로 모은다.
// R&D 특성 — 출시 품질 배율. 품질은 등급/수익밴드/리뷰를 움직이되 출시 통과 게이트는 건드리지 않는다.
static float TraitQualityFactor(int32 BuildingID);

static UBuildingTraitManagerSubsystem* GetTraitMgrForOps()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	return GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
}

static float TraitQualityFactor(int32 BuildingID)
{
	UBuildingTraitManagerSubsystem* M = GetTraitMgrForOps();
	return M ? M->GetTraitFactor(BuildingID, EBuildingTraitTarget::Quality) : 1.0f;
}

UProjectOperationManager::UProjectOperationManager()
{
	NextOperationID = 1;
}

void UProjectOperationManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveOperations.Empty();

	// FTSTicker 등록
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UProjectOperationManager::TickCallback),
		TimerTickInterval
	);

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Initialized with FTSTicker"));
}

void UProjectOperationManager::Deinitialize()
{
	// FTSTicker 해제
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Deinitialized"));
}

bool UProjectOperationManager::TickCallback(float DeltaTime)
{
	UpdateOperations(TimerTickInterval);
	return true;
}

// ===== Building 데이터 접근 =====

FOfficeSaveData* UProjectOperationManager::GetOfficeSaveDataByBuildingID(int32 BuildingID) const
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[ProjectOperationManager] GetOfficeSaveData - GI is null!"));
		return nullptr;
	}

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ProjectOperationManager] GetOfficeSaveData - SaveLoadMgr is null!"));
		return nullptr;
	}

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData)
	{
		UE_LOG(LogTemp, Error, TEXT("[ProjectOperationManager] GetOfficeSaveData - SaveData is null!"));
		return nullptr;
	}

	// BuildingID를 int32 키로 직접 사용
	return &SaveData->GameData.OfficeDataMap.FindOrAdd(BuildingID);
}

// ===== Operation Start/End =====

namespace
{
	// StageData 활성 직능 스텝(출시=INDEX_NONE 제외)을 결산 배열로 복사. StartOperation 전용.
	void FillDisciplineReport(const FStageProgressData& StageData, FOperationData& Op)
	{
		Op.DisciplineScores.Reset();
		Op.DisciplineTargets.Reset();
		Op.DisciplineSlots.Reset();
		for (const FStepRoundData& S : StageData.Steps)
		{
			if (S.DisciplineSlot == INDEX_NONE) { continue; }
			Op.DisciplineScores.Add(S.AcquiredScore);
			Op.DisciplineTargets.Add(S.TargetScore);
			Op.DisciplineSlots.Add(S.DisciplineSlot);
		}
	}
}

int32 UProjectOperationManager::StartOperation(const FStageProgressData& StageData, int32 BuildingID)
{
	FOperationData NewOperation;
	NewOperation.ProjectID = StageData.ProjectID;
	NewOperation.ProjectNumber = StageData.ProjectNumber;
	NewOperation.StageNumber = StageData.StageNumber;
	NewOperation.ProjectName = StageData.ProjectName;
	NewOperation.BuildingID = BuildingID;
	// 직능별 결산 배열 — 활성 직능 스텝만 순서대로 (출시 제외). 결산서 N행.
	FillDisciplineReport(StageData, NewOperation);
	NewOperation.QualityScore = StageData.CalculateQualityScore();
	// ProjectGrade 배율 적용 (Phase 3) — 같은 플레이로 더 높은 등급 도달
	const float QualityMult = GetBuildingEnhancementMultiplier(BuildingID, EBuildingEnhancementType::ProjectGrade);
	NewOperation.QualityScore = FMath::Clamp(NewOperation.QualityScore * QualityMult * TraitQualityFactor(BuildingID), 0.5f, 2.0f);
	NewOperation.QualityGrade = QualityScoreToGrade(NewOperation.QualityScore);
	// 산업 수익 인격 노브 (같은 감쇠 엔진, 다른 곡선 모양) — 피크/반감기/변동성 캐시
	float PeakMult = 1.0f, HalfLifeFrac = 0.5f, Volatility = 0.0f;
	GetIndustryCurveKnobs(StageData.CompanyType, PeakMult, HalfLifeFrac, Volatility);
	// ESG 특성 — 반감기 연장 = 감쇠 완화. 상한 0.95 로 "안 식는 수익" 을 막는다.
	if (UBuildingTraitManagerSubsystem* DecayTraitMgr = GetTraitMgrForOps())
	{
		HalfLifeFrac = FMath::Min(0.95f, HalfLifeFrac * DecayTraitMgr->GetTraitFactor(BuildingID, EBuildingTraitTarget::RevenueDecay));
	}
	NewOperation.HalfLifeFrac = HalfLifeFrac;
	NewOperation.Volatility = Volatility;

	// 이벤트 보상 배율(예: 규제 변경 "무시" → 0.8) 반영 — 자체개발 운영수익에 적용 (트레이트 배율과 분리)
	// 산업 PeakMult 곱: 게임 2.2(뾰족)/IT 0.55(낮음). 트렌드 매칭 = 추가 x1.5
	const float TrendMult = StageData.bTrendMatched ? UTrendManagerSubsystem::TrendPeakMult : 1.0f;
	// 품질 등급이 수익 배율을 좌우 (ReviewLeverage=산업별 등급 격차 강도). 리뷰/40은 표시 전용.
	float Leverage = 1.0f;
	if (UCGGameInstance* RevGI = UCGGameInstance::GetInstance())
	{
		if (UTableManagerSubsystem* RevTableMgr = RevGI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bProfOK = false;
			const FIndustryProfileRow Profile = RevTableMgr->GetIndustryProfile(StageData.CompanyType, bProfOK);
			if (bProfOK) { Leverage = Profile.ReviewLeverage; }
		}
	}
	const float SpreadMult = ComputeQualityRevenueMult(NewOperation.QualityGrade, Leverage, GetRevenueStabilityFrac(BuildingID));
	NewOperation.BaseRevenuePerSecond = CalculateBaseRevenue(StageData) * StageData.EventRewardMultiplier * PeakMult * TrendMult * SpreadMult;

	// 표시 경로(출시확인/픽칭 카드)와 같은 함수 — 여기서 배율을 따로 곱하면 표기와 실제가 갈린다
	NewOperation.TotalOperationTime = ComputeEffectiveOperationTime(
		StageData.CalculateQualityGrade(), StageData.ProjectNumber, BuildingID);
	NewOperation.RemainingTime = NewOperation.TotalOperationTime;
	NewOperation.ElapsedTime = 0.0f;

	// 실제 수익 계산 — 빌딩 강화 MarketingPower + 성급 + 직원 수익보너스
	const float IncomeMult = GetBuildingIncomeMultiplier(BuildingID);
	const float StatBonus = CalculateEmployeeStatBonus(BuildingID);
	NewOperation.ActualRevenuePerSecond = NewOperation.BaseRevenuePerSecond * NewOperation.GetTotalMultiplier() * IncomeMult * StatBonus;
	NewOperation.State = EOperationState::Operating;

	int32 OperationIndex = ActiveOperations.Add(NewOperation);

	OnOperationStarted.Broadcast(BuildingID);

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Started Operation: Project=%s, Building=%d, Quality=%s (%.2f), Duration=%.0fs"),
		*NewOperation.ProjectName,
		BuildingID,
		*QualityGradeToAlphabetString(NewOperation.QualityGrade),
		NewOperation.QualityScore,
		NewOperation.TotalOperationTime);

	return OperationIndex;
}

void UProjectOperationManager::EndOperation(int32 OperationIndex)
{
	if (!ActiveOperations.IsValidIndex(OperationIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProjectOperationManager] Invalid OperationIndex: %d"), OperationIndex);
		return;
	}

	FOperationData& Operation = ActiveOperations[OperationIndex];
	Operation.State = EOperationState::Completed;

	// 남은 저장 수익 자동 수령
	CollectStoredRevenueByBuilding(Operation.BuildingID);

	int32 BuildingID = Operation.BuildingID;
	FOperationData CompletedData = Operation;

	ActiveOperations.RemoveAt(OperationIndex);

	OnOperationCompleted.Broadcast(BuildingID, CompletedData);

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Ended Operation: BuildingID=%d"), BuildingID);
}

void UProjectOperationManager::EndAllOperationsByBuilding(int32 BuildingID)
{
	for (int32 i = ActiveOperations.Num() - 1; i >= 0; i--)
	{
		if (ActiveOperations[i].BuildingID == BuildingID)
		{
			EndOperation(i);
		}
	}
}

// ===== Update =====

void UProjectOperationManager::UpdateOperations(float DeltaTime)
{
	TArray<int32> CompletedIndices;

	for (int32 i = 0; i < ActiveOperations.Num(); i++)
	{
		FOperationData& Operation = ActiveOperations[i];

		if (Operation.State != EOperationState::Operating && Operation.State != EOperationState::Paused)
		{
			continue;
		}

		UpdateSingleOperation(Operation, DeltaTime);

		if (Operation.ElapsedTime >= Operation.TotalOperationTime)
		{
			CompletedIndices.Add(i);
		}
	}

	for (int32 i = CompletedIndices.Num() - 1; i >= 0; i--)
	{
		OnOperationComplete(CompletedIndices[i]);
	}

	// 모뉴먼트 특수빌딩 자체 패시브 산출 (직원 무관, 매 틱 가산)
	if (UWorld* W = GetWorld())
	{
		if (UEntityManager* EM = W->GetSubsystem<UEntityManager>())
		{
			int64 MonumentIncome = 0;
			for (ABuildingBaseActor* B : EM->GetBuildings())
			{
				if (B && B->IsKeystoneMonument())
				{
					MonumentIncome += B->GetMonumentPassiveOutput();
				}
			}
			if (MonumentIncome > 0)
			{
				MonumentIncome = (int64)(MonumentIncome * DeltaTime); // /sec 정규화
				if (MonumentIncome > 0)
				{
					if (UResourceItemManager* ResMgr = GetResourceItemManager())
					{
						// 매 틱 디스크 저장 방지(bShouldSave=false) — 주기적 자동저장이 커버
						ResMgr->StoreResource(EResourceType::Money, MonumentIncome, false);
					}
				}
			}
		}
	}

}

float UProjectOperationManager::GetBuildingDisplayNetPerSec(int32 BuildingID) const
{
	// 표시 정의 = Base × TotalMult × IncomeMult × StatBonus × Decay (진동 제외).
	// UpdateSingleOperation 의 지급 계산과 동일하되 25초 Volatility 항만 뺀다.
	for (const FOperationData& Op : ActiveOperations)
	{
		if (Op.BuildingID != BuildingID)
		{
			continue;
		}

		const float IncomeMult = GetBuildingIncomeMultiplier(BuildingID);
		const float StatBonus = CalculateEmployeeStatBonus(BuildingID);
		float Rate = Op.BaseRevenuePerSecond * Op.GetTotalMultiplier() * IncomeMult * StatBonus;

		const float HalfLifeFrac = (Op.HalfLifeFrac > 0.0f) ? Op.HalfLifeFrac : 0.5f;
		const float DecayHalfLife = FMath::Max(1.0f, Op.TotalOperationTime * HalfLifeFrac);
		Rate *= FMath::Pow(0.5f, Op.ElapsedTime / DecayHalfLife);
		return Rate;
	}
	return 0.0f;
}

double UProjectOperationManager::GetCompanyNetPerSec() const
{
	double Sum = 0.0;
	for (const FOperationData& Op : ActiveOperations)
	{
		Sum += GetBuildingDisplayNetPerSec(Op.BuildingID);
	}

	// 모뉴먼트 패시브 합산 (어떤 수익 UI에도 안 잡히던 값 — 회사 전체엔 포함)
	if (UWorld* W = GetWorld())
	{
		if (UEntityManager* EM = W->GetSubsystem<UEntityManager>())
		{
			for (ABuildingBaseActor* B : EM->GetBuildings())
			{
				if (B && B->IsKeystoneMonument())
				{
					Sum += static_cast<double>(B->GetMonumentPassiveOutput());
				}
			}
		}
	}
	return Sum;
}

void UProjectOperationManager::UpdateSingleOperation(FOperationData& Operation, float DeltaTime)
{
	// 시간 경과 (항상 처리)
	Operation.ElapsedTime += DeltaTime;
	Operation.RemainingTime = FMath::Max(Operation.TotalOperationTime - Operation.ElapsedTime, 0.0f);

	// 실제 수익 계산 — 기본 × 배율 × MarketingPower/성급 × 직원 수익보너스
	// StatBonus 는 캐시 기반이라 tick 당 비용 O(1)
	const float IncomeMult = GetBuildingIncomeMultiplier(Operation.BuildingID);
	const float StatBonus = CalculateEmployeeStatBonus(Operation.BuildingID);
	Operation.ActualRevenuePerSecond = Operation.BaseRevenuePerSecond * Operation.GetTotalMultiplier() * IncomeMult * StatBonus;

	// 운영 수익 시간 감쇠 — 산업별 반감기(HalfLifeFrac). 게임 빨리 식음(0.28)/IT 긴 꼬리(0.85).
	const float HalfLifeFrac = (Operation.HalfLifeFrac > 0.0f) ? Operation.HalfLifeFrac : 0.5f;
	const float DecayHalfLife = FMath::Max(1.0f, Operation.TotalOperationTime * HalfLifeFrac);
	const float DecayFactor = FMath::Pow(0.5f, Operation.ElapsedTime / DecayHalfLife);
	Operation.ActualRevenuePerSecond *= DecayFactor;

	// 산업 변동성 — 운영 중 시장 출렁임(금융 큼/IT 잔잔). 감쇠 곡선 위 평균보존 진동.
	if (Operation.Volatility > 0.0f)
	{
		constexpr float VolatilityPeriodSec = 25.0f;
		constexpr float TwoPi = 6.2831853f;
		const float Osc = FMath::Sin(Operation.ElapsedTime * (TwoPi / VolatilityPeriodSec));
		Operation.ActualRevenuePerSecond *= FMath::Max(0.0f, 1.0f + Operation.Volatility * Osc);
	}

	// 수익 누적 (오피스 안/밖 모두 TotalRevenueEarned에는 기록)
	// 오피스 안에선 StoredRevenue 지급은 건너뛰고 누적 표시만 갱신 (직원 수익과 중복 방지)
	const float Revenue = Operation.ActualRevenuePerSecond * DeltaTime;
	if (IsPlayerInOffice(Operation.BuildingID))
	{
		Operation.TotalRevenueEarned += Revenue;
	}
	else
	{
		ProcessRevenue(Operation, Revenue);
	}

	// 업데이트 델리게이트
	OnOperationUpdated.Broadcast(Operation.BuildingID, Operation);
}

void UProjectOperationManager::ProcessRevenue(FOperationData& Operation, float RevenueAmount)
{
	if (RevenueAmount <= 0.0f)
	{
		return;
	}

	// 결산용 누적 수익 기록
	Operation.TotalRevenueEarned += RevenueAmount;

	// 프로젝트 수익은 항상 StoredRevenue에 누적 (OfficeMap 직원 수익과 분리)
	FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(Operation.BuildingID);
	if (OfficeData)
	{
		float Capacity = CalculateWarehouseCapacity(Operation.BuildingID);

		// 이미 번 돈은 깎지 않고 신규 적립만 막는다 (용량 축소 시 "쌓은 돈 증발" 회귀 방지). 초과분은 수거로 자연해소.
		float AvailableSpace = FMath::Max(0.0f, Capacity - OfficeData->StoredRevenue);
		float AmountToStore = FMath::Min(RevenueAmount, AvailableSpace);

		if (AmountToStore > 0.0f)
		{
			OfficeData->StoredRevenue += AmountToStore;
		}

		// 적립 여부와 무관하게 항상 브로드캐스트 (버블 갱신용)
		OnWarehouseUpdated.Broadcast(Operation.BuildingID, OfficeData->StoredRevenue, Capacity);

		if (OfficeData->StoredRevenue >= Capacity)
		{
			UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Stored Revenue Full: BuildingID=%d"), Operation.BuildingID);
		}
	}
}

void UProjectOperationManager::OnOperationComplete(int32 OperationIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Operation Complete: Index=%d"), OperationIndex);

	int32 CompletedBuildingID = -1;

	// 운영 완료 시 경험치 분배 및 결산서 생성
	if (ActiveOperations.IsValidIndex(OperationIndex))
	{
		const FOperationData& Operation = ActiveOperations[OperationIndex];
		CompletedBuildingID = Operation.BuildingID;
		DistributeOperationExperience(Operation);
		GenerateReport(Operation);

	}

	EndOperation(OperationIndex);

	// 운영 완료 상태를 CachedSaveData에 즉시 반영 → 디스크 저장
	if (CompletedBuildingID >= 0)
	{
		FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(CompletedBuildingID);
		if (OfficeData)
		{
			OfficeData->bHasActiveOperation = false;
			OfficeData->CurrentOperation = FOperationData();
		}

		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
}

// ===== Stored Revenue =====

int64 UProjectOperationManager::CollectWarehouseRevenue(int32 OperationIndex)
{
	if (!ActiveOperations.IsValidIndex(OperationIndex))
	{
		return 0;
	}

	return CollectStoredRevenueByBuilding(ActiveOperations[OperationIndex].BuildingID);
}

int64 UProjectOperationManager::CollectAllWarehouseByBuilding(int32 BuildingID)
{
	return CollectStoredRevenueByBuilding(BuildingID);
}

int64 UProjectOperationManager::CollectStoredRevenueByBuilding(int32 BuildingID)
{
	FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
	if (!OfficeData)
	{
		return 0;
	}

	int64 CollectedAmount = static_cast<int64>(OfficeData->StoredRevenue);
	if (CollectedAmount <= 0)
	{
		return 0;
	}

	// bShouldSave=false — 아래 명시적 저장 1회가 책임 (기본값이면 수령 1탭당 전체 세이브 2회)
	if (UResourceItemManager* ResourceMgr = GetResourceItemManager())
	{
		ResourceMgr->StoreResource(EResourceType::Money, CollectedAmount, /*bShouldSave=*/false);
	}

	OfficeData->StoredRevenue = 0.0f;

	OnRevenueCollected.Broadcast(CollectedAmount);
	OnWarehouseUpdated.Broadcast(BuildingID, 0.0f, CalculateWarehouseCapacity(BuildingID));

	// 디스크 저장 — 레벨 전환 시 LoadGameData 가 디스크 옛값으로 메모리를 덮어쓰는 부활 버그 방지
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SLMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SLMgr->SaveGameData();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Collected Stored Revenue: %lld, BuildingID=%d"),
		CollectedAmount, BuildingID);

	return CollectedAmount;
}

float UProjectOperationManager::GetStoredRevenue(int32 BuildingID) const
{
	FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
	return OfficeData ? OfficeData->StoredRevenue : 0.0f;
}

int64 UProjectOperationManager::CollectAllStoredRevenue()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return 0;

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return 0;

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return 0;

	// TMap 키를 미리 복사 — 개별 수거 중 FindOrAdd 사이드이펙트 안전성 확보
	TArray<int32> BuildingIds;
	SaveData->GameData.OfficeDataMap.GetKeys(BuildingIds);

	int64 TotalCollected = 0;
	for (int32 BuildingID : BuildingIds)
	{
		TotalCollected += CollectStoredRevenueByBuilding(BuildingID);
	}

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] CollectAllStoredRevenue: %d개 빌딩, 총 %lld원"),
		BuildingIds.Num(), TotalCollected);

	return TotalCollected;
}

int32 UProjectOperationManager::FillAllStoredRevenue(int64 AmountPerBuilding)
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return 0;

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return 0;

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return 0;

	const float Amount = static_cast<float>(FMath::Max<int64>(AmountPerBuilding, 0));

	// 키 복사 — 순회 중 FindOrAdd 사이드이펙트 안전성 (CollectAllStoredRevenue 와 동일 패턴)
	TArray<int32> BuildingIds;
	SaveData->GameData.OfficeDataMap.GetKeys(BuildingIds);

	int32 FilledCount = 0;
	for (int32 BuildingID : BuildingIds)
	{
		FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
		if (!OfficeData) continue;
		const float Capacity = CalculateWarehouseCapacity(BuildingID);
		if (Capacity <= 0.0f) continue; // 금고 없는(비프로젝트) 건물은 스킵
		OfficeData->StoredRevenue = Amount; // 용량 무시 절대액 강제 — 수거 시 그대로 회수(대량 코인)
		// 버블 재평가 트리거 — StoredRevenue >= Capacity 라 VaultFull 버블. InGameLayer 가 OnWarehouseUpdated 구독.
		OnWarehouseUpdated.Broadcast(BuildingID, OfficeData->StoredRevenue, Capacity);
		++FilledCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] FillAllStoredRevenue: %d개 빌딩 금고 각 %.0f원 강제 충전"),
		FilledCount, Amount);

	return FilledCount;
}

bool UProjectOperationManager::HasAnyStoredRevenue() const
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return false;

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return false;

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return false;

	// CollectAllStoredRevenue 와 동일한 빌딩 집합을 순회하되, 하나라도 발견 시 즉시 종료
	for (const TPair<int32, FOfficeSaveData>& Pair : SaveData->GameData.OfficeDataMap)
	{
		// 임계가 0 이 아니라 1.0 인 이유: 수거는 건물별로 절단하므로(CollectStoredRevenueByBuilding
		// 의 static_cast<int64> + `<= 0 이면 return 0`) 1 미만은 수거해도 0 원이고 잔액도 안 비워진다.
		// 가용 신호(초록 점)가 수거 불가한 돈을 있다고 말하면 안 된다 — 점/숫자/수거가 같은 자를 쓴다.
		if (GetStoredRevenue(Pair.Key) >= 1.0f)
		{
			return true;
		}
	}
	return false;
}

double UProjectOperationManager::GetTotalStoredRevenue() const
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return 0.0;

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return 0.0;

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return 0.0;

	// HasAnyStoredRevenue 와 같은 순회 + 같은 접근자(GetStoredRevenue) — 조기 종료만 빼고 전부 합산한다.
	// 대상 집합이나 접근 경로가 갈리면 "도트는 떴는데 금액이 0" 인 모순이 나온다.
	// ⚠ 순회 중 GetStoredRevenue 가 이 맵에 FindOrAdd 를 건다 — 모든 키가 이미 존재한다는 전제로만
	//    안전하다(엔트리를 도는 중이니 성립). 엔트리 해석 방식을 바꿀 땐 삽입=이터레이터 무효화임을 유의.
	// 남은 전제 2개: (a) 수거측과 키 집합이 같다(양쪽 다 OfficeDataMap 전 키, 필터 없음),
	//    (b) 총액이 2^53 미만이라 double 누산에 반올림이 없다. 둘 중 하나가 깨지면 항별 일치가 깨진다.
	double Sum = 0.0;
	for (const TPair<int32, FOfficeSaveData>& Pair : SaveData->GameData.OfficeDataMap)
	{
		// ⚠ 건물별로 max(0, trunc) 를 걸어 수거와 같은 자를 쓴다 — CollectStoredRevenueByBuilding 의
		//    항별 연산이 절단만이 아니라 `CollectedAmount <= 0 이면 return 0` 하한까지 포함이라
		//    (절단, 하한) 둘이 한 쌍의 미러다. 절단을 빼면 각각 수거 불가인 0<x<1 잔액 N 개가
		//    합계에서 1 을 넘겨 "버튼엔 숫자, 수거하면 0" 이 되고(200건물×0.5=100),
		//    하한을 빼면 음수 잔액이 남의 수거 가능액을 깎는다(수거는 그 건물을 0 으로 볼 뿐이다).
		//    둘 다 걸어야 이 값이 수거액과 항별로 일치한다.
		Sum += static_cast<double>(FMath::Max<int64>(0, static_cast<int64>(GetStoredRevenue(Pair.Key))));
	}
	return Sum;
}

// ===== 기대 수익 (직원 잠재 큐브 수익줄 기반) =====

float UProjectOperationManager::CalculateEmployeeStatBonus(int32 BuildingID) const
{
	if (const float* Cached = CachedStatBonus.Find(BuildingID))
	{
		return *Cached;
	}

	FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
	if (!OfficeData || OfficeData->EmployeeList.Num() == 0)
	{
		CachedStatBonus.Add(BuildingID, 1.0f);
		return 1.0f;
	}

	float TotalPotentialMult = 0.0f;
	for (const FEmployeeInstance& Emp : OfficeData->EmployeeList)
	{
		TotalPotentialMult += UEmployeePotentialHelper::AggregateModifiers(Emp.PotentialAbility).IncomeMult;
	}
	const float Count = static_cast<float>(OfficeData->EmployeeList.Num());

	// 잠재 수익줄도 여기로 합류(2026-08-13) — 유일한 소비처였던 방치 수익이 사라져 무효 옵션이 될 뻔했다.
	// "사무실 평균" 단위여야 인원 증감이 요율을 흔들지 않는다.
	//
	// 직원 **스탯**의 수익 기여는 2026-08-13 폐지(구 IncomeBonus → Focus 로 교체). 6스탯 중 유일하게
	// 사무실 평균으로 작동해 개인 카드에 개인값을 적으면 거짓말이 되는 축이었다. 남은 건 잠재 축뿐이지만
	// 함수·캐시·무효화 배선은 그대로 둔다 — 잠재 옵션이 바뀌면 여전히 재계산이 필요하다.
	const float Bonus = TotalPotentialMult / Count;

	CachedStatBonus.Add(BuildingID, Bonus);
	return Bonus;
}

void UProjectOperationManager::InvalidateStatBonusCache(int32 BuildingID)
{
	CachedStatBonus.Remove(BuildingID);
	// 캐시만 비우고 즉시 재계산까지 가는 건 RefreshExpectedRevenue 에 위임
	RefreshExpectedRevenue(BuildingID);
}

void UProjectOperationManager::RefreshExpectedRevenue(int32 BuildingID)
{
	// 캐시 무효화 (다음 Calculate 호출이 새 값으로 채움)
	CachedStatBonus.Remove(BuildingID);

	// 해당 빌딩의 표시 순수익 브로드캐스트 — 필드(ActualRevenuePerSecond)는 틱이 소유하므로 덮어쓰지 않음.
	// 무감쇠 값으로 필드를 덮으면 다음 틱까지 HQ 목록이 부풀려진 값을 캡처하던 버그를 여기서 제거.
	for (const FOperationData& Op : ActiveOperations)
	{
		if (Op.BuildingID == BuildingID)
		{
			OnExpectedRevenueChanged.Broadcast(BuildingID, GetBuildingDisplayNetPerSec(BuildingID));
			return;
		}
	}

	// Operation 없음 → 0 브로드캐스트 (위젯이 "운영 없음" 처리)
	OnExpectedRevenueChanged.Broadcast(BuildingID, 0.0f);
}

// ===== Query =====

FOperationData* UProjectOperationManager::GetOperationByBuildingID(int32 BuildingID)
{
	for (FOperationData& Operation : ActiveOperations)
	{
		if (Operation.BuildingID == BuildingID &&
			(Operation.State == EOperationState::Operating || Operation.State == EOperationState::Paused))
		{
			return &Operation;
		}
	}
	return nullptr;
}

FOperationData* UProjectOperationManager::GetOperationByIndex(int32 OperationIndex)
{
	if (ActiveOperations.IsValidIndex(OperationIndex))
	{
		return &ActiveOperations[OperationIndex];
	}
	return nullptr;
}

bool UProjectOperationManager::GetOperationByBuildingID_BP(int32 BuildingID, FOperationData& OutData)
{
	FOperationData* FoundData = GetOperationByBuildingID(BuildingID);
	if (FoundData)
	{
		OutData = *FoundData;
		return true;
	}
	return false;
}

bool UProjectOperationManager::GetOperationByIndex_BP(int32 OperationIndex, FOperationData& OutData)
{
	FOperationData* FoundData = GetOperationByIndex(OperationIndex);
	if (FoundData)
	{
		OutData = *FoundData;
		return true;
	}
	return false;
}

bool UProjectOperationManager::HasActiveOperation(int32 BuildingID) const
{
	for (const FOperationData& Operation : ActiveOperations)
	{
		if (Operation.BuildingID == BuildingID &&
			(Operation.State == EOperationState::Operating || Operation.State == EOperationState::Paused))
		{
			return true;
		}
	}
	return false;
}

bool UProjectOperationManager::IsPlayerInOffice(int32 BuildingID) const
{
	UCGGameInstance* GameInstance = UCGGameInstance::GetInstance();
	if (!GameInstance)
	{
		return false;
	}

	if (!GameInstance->IsInOfficeMap())
	{
		return false;
	}

	int32 CurrentBuildingIndex = GameInstance->GetCurrentManagedBuildingIndex();

	return CurrentBuildingIndex == BuildingID;
}

// ===== Calculations =====

float UProjectOperationManager::ComputeOperationCurveSeconds(EQualityGrade Grade, int32 ProjectNumber)
{
	// 프로젝트 번호가 올라갈수록 운영시간 증가
	const float TotalMinutes = GetQualityGradeBaseLifespanMinutes(Grade) * FMath::Max(ProjectNumber, 1);
	return FMath::Max(TotalMinutes, MinOperationMinutes) * 60.0f;
}

float UProjectOperationManager::ComputeEffectiveOperationTime(EQualityGrade Grade, int32 ProjectNumber, int32 BuildingID) const
{
	float Seconds = ComputeOperationCurveSeconds(Grade, ProjectNumber);

	// ProjectLifespan 강화 + IP 특성 — 착수만 곱하고 표시가 빠뜨리면 강화한 만큼 표기가 어긋난다
	Seconds *= GetBuildingEnhancementMultiplier(BuildingID, EBuildingEnhancementType::ProjectLifespan);
	if (UBuildingTraitManagerSubsystem* LifeTraitMgr = GetTraitMgrForOps())
	{
		Seconds *= LifeTraitMgr->GetTraitFactor(BuildingID, EBuildingTraitTarget::OperationTime);
	}
	return Seconds;
}

float UProjectOperationManager::CalculateBaseRevenue(const FStageProgressData& Data) const
{
	return CalculateBaseRevenueForProjectNumber(Data.ProjectNumber);
}

float UProjectOperationManager::CalculateBaseRevenueForProjectNumber(int32 ProjectNumber) const
{
	// 프로젝트 번호 기반 수익, 원/초 (StageNumber 확장 시: (ProjectNumber-1)*MaxStages + StageNumber)
	// ⚠ OfficeStageProgressManager::EstimatePitchEconomy 의 Base 와 거울이다 — 한쪽만 바꾸면
	//    카드 표기와 실제 결산이 갈라진다(둘이 같은 산식이라는 게 이 시스템의 계약).
	float BaseRevenue = static_cast<float>(FTierLayout::RevenueScaleIndex(ProjectNumber)) * 200.0f;
	return FMath::Max(BaseRevenue, 20.0f);
}

void UProjectOperationManager::GetIndustryCurveKnobs(ECompanyType Industry, float& OutPeakMult, float& OutHalfLifeFrac, float& OutVolatility) const
{
	OutPeakMult = 1.0f;
	OutHalfLifeFrac = 0.5f;
	OutVolatility = 0.0f;

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	bool bOK = false;
	const FIndustryProfileRow Profile = TableMgr->GetIndustryProfile(Industry, bOK);
	if (bOK)
	{
		OutPeakMult = Profile.PeakMult;
		OutHalfLifeFrac = Profile.HalfLifeFrac;
		OutVolatility = Profile.Volatility;
	}
}

float UProjectOperationManager::GetRevenueStabilityFrac(int32 BuildingID) const
{
	if (BuildingID == INDEX_NONE)
	{
		return 0.0f;
	}
	UBuildingTraitManagerSubsystem* StabTraitMgr = GetTraitMgrForOps();
	if (!StabTraitMgr)
	{
		return 0.0f;
	}
	const float Pct = StabTraitMgr->GetAggregatedTraitPercent(BuildingID, EBuildingTraitTarget::RevenueStability);
	return FMath::Clamp(Pct / 100.0f, 0.0f, 1.0f);
}

float UProjectOperationManager::GetBuildingIncomeMultiplier(int32 BuildingID) const
{
	// SaveData 에서 직접 조회해 MainMap/OfficeMap 전환 시 Actor가 unload 되어도 안전하게 동작.
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return 1.0f;
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return 1.0f;
	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return 1.0f;

	for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
	{
		if (B.BuildingIndex != BuildingID) continue;

		// MarketingPower 배율
		const int32 IncomeLv = B.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::MarketingPower);
		const float IncomeMult = UBuildingEnhancementHelper::CalculateEffectMultiplier(
			EBuildingEnhancementType::MarketingPower, IncomeLv);

		// 건물 특성 (Revenue → 운영수익 배율) — 강화 배율과 곱셈 결합
		float RevenueTraitMultiplier = 1.0f;
		if (UBuildingTraitManagerSubsystem* TraitMgr = GI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			RevenueTraitMultiplier = TraitMgr->GetTraitFactor(B.BuildingIndex, EBuildingTraitTarget::OperationRevenue);
		}
		return IncomeMult * RevenueTraitMultiplier;
	}
	return 1.0f;
}

float UProjectOperationManager::GetBuildingEnhancementMultiplier(int32 BuildingID, EBuildingEnhancementType Type) const
{
	// 일반 강화 슬롯의 순수 배율 (성급 보너스 없음). SaveData 기반 — MainMap/OfficeMap 양쪽 안전.
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return 1.0f;
	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return 1.0f;
	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return 1.0f;

	for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
	{
		if (B.BuildingIndex != BuildingID) continue;
		const int32 Lv = B.BuildingData.EnhancementLevels.FindRef(Type);
		return UBuildingEnhancementHelper::CalculateEffectMultiplier(Type, Lv);
	}
	return 1.0f;
}

float UProjectOperationManager::GetVaultReferenceRatePerSecond(int32 BuildingID) const
{
	// 기준을 운영 스냅샷이 아니라 빌딩 티어에 두는 이유: 금고는 "이 빌딩이 얼마까지 보관하나"라
	// 운영 착수/종료로 용량이 튀면 안 되고, 비운영 빌딩이 200원으로 붕괴해서도 안 된다.
	// 티어 밴드 시작 = 그 단계의 첫 프로젝트 규모 → 티어 진행 중 금고 강화 압력을 유지한다.
	// 배율 스택은 적립(ProcessRevenue)과 동일. 감쇠·품질 굴림만 제외 — 용량이 시간에 따라 흔들리면 안 되므로.
	int32 Tier = 1;
	if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			Tier = SaveLoadMgr->GetBuildingTier(BuildingID);
		}
	}

	int32 BandStart = 0, UnusedBandEnd = 0;
	FProjectTierProgress::GetTierProjectRange(FMath::Max(1, Tier), BandStart, UnusedBandEnd);

	const float ReferenceRate = CalculateBaseRevenueForProjectNumber(BandStart)
		* GetBuildingIncomeMultiplier(BuildingID)
		* CalculateEmployeeStatBonus(BuildingID);
	return FMath::Max(0.0f, ReferenceRate);
}

float UProjectOperationManager::CalculateWarehouseCapacity(int32 BuildingID) const
{
	// 단일 정본: Capacity(원) = 티어 기준레이트 × VaultSeconds(Lv). 운영 유무와 무관하게 항상 같은 값.
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return ABuildingBaseActor::BaseVaultCapacity;
	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return ABuildingBaseActor::BaseVaultCapacity;
	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return ABuildingBaseActor::BaseVaultCapacity;

	int32 VaultLevel = 0;
	bool bFoundBuilding = false;
	for (const FBuildingEntitySaveData& Building : SaveData->GameData.Buildings)
	{
		if (Building.BuildingIndex == BuildingID)
		{
			VaultLevel = Building.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::VaultCapacity);
			bFoundBuilding = true;
			break;
		}
	}
	if (!bFoundBuilding)
	{
		return ABuildingBaseActor::BaseVaultCapacity;  // 저장 데이터가 없으면 공용 기본 용량 사용
	}

	return CalculateWarehouseCapacityAtLevel(BuildingID, VaultLevel);
}

float UProjectOperationManager::CalculateWarehouseCapacityAtLevel(int32 BuildingID, int32 VaultLevel) const
{
	// "0 아님" 가드 (VaultFull 버블/치트가 Capacity>0 을 전제). 기준레이트가 티어 기반이라 실제로는 안 걸린다.
	// ⚠ CalculateEffectMultiplier(VaultCapacity) 를 쓰지 말 것 — EffectPerLevel 이 곡선 k 로 재정의돼
	// 배율로 오독되면 하한이 의미 없이 쪼그라든다. 하한은 가드라 배율 불필요.
	const float FloorCapacity = ABuildingBaseActor::BaseVaultCapacity;

	float TimeCapacity = GetVaultReferenceRatePerSecond(BuildingID)
		* UBuildingEnhancementHelper::CalculateVaultSeconds(FMath::Max(0, VaultLevel));
	// CRM 특성 — 금고 용량. 초과분은 조용히 잘려나가므로 용량은 실질 손실 방어다.
	if (UBuildingTraitManagerSubsystem* VaultTraitMgr = GetTraitMgrForOps())
	{
		TimeCapacity *= VaultTraitMgr->GetTraitFactor(BuildingID, EBuildingTraitTarget::VaultCapacity);
	}
	return FMath::Max(TimeCapacity, FloorCapacity);
}

// ===== 백로그 순익 계산 =====

// ===== 백로그 제품 등록 =====

void UProjectOperationManager::AddBacklogProduct(const FStageProgressData& StageData, int32 BuildingID)
{
	FOfficeSaveData* Office = GetOfficeSaveDataByBuildingID(BuildingID);
	if (!Office) return;

	FBacklogProductEntry E;
	E.ProjectID = StageData.ProjectID;
	E.ProjectNumber = StageData.ProjectNumber;
	E.CompanyType = StageData.CompanyType;
	E.LaunchWallClock = FDateTime::UtcNow();

	const float BLQuality = FMath::Clamp(StageData.CalculateQualityScore() * TraitQualityFactor(BuildingID), 0.5f, 2.0f);
	float BLPeakMult = 1.0f, BLHalf = 0.5f, BLVol = 0.0f;
	GetIndustryCurveKnobs(StageData.CompanyType, BLPeakMult, BLHalf, BLVol);
	float BLLeverage = 1.0f;
	{
		bool bProfOK = false;
		if (UTableManagerSubsystem* BLTableMgr = GetTableManager())
		{
			const FIndustryProfileRow Prof = BLTableMgr->GetIndustryProfile(StageData.CompanyType, bProfOK);
			if (bProfOK) { BLLeverage = Prof.ReviewLeverage; }
		}
	}
	// 품질 배율로 통일 (운영/결산과 동일 공식)
	const double SpreadMult = static_cast<double>(ComputeQualityRevenueMult(QualityScoreToGrade(BLQuality), BLLeverage, GetRevenueStabilityFrac(BuildingID)));
	E.BasePeakRevenuePerSec = static_cast<double>(CalculateBaseRevenue(StageData)) * static_cast<double>(StageData.EventRewardMultiplier) * SpreadMult * static_cast<double>(BLPeakMult)
		* (StageData.bTrendMatched ? static_cast<double>(UTrendManagerSubsystem::TrendPeakMult) : 1.0);
	// 임시 튜너블: 운영비 = 출시 peak의 10% (추후 DT/직원 비례로 교체)
	E.OperatingCostPerSec = E.BasePeakRevenuePerSec * 0.1;
	// AssignedEmployeeIDs는 Phase 2(종료·재배치)에서 배선 — 지금은 빈 배열

	Office->BacklogProducts.Add(E);

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}

	UE_LOG(LogTemp, Log, TEXT("[Backlog] +제품 PID=%d peak=%.2f cost=%.2f (총 %d개)"),
		E.ProjectID, E.BasePeakRevenuePerSec, E.OperatingCostPerSec, Office->BacklogProducts.Num());
}

// 임시 튜너블(추후 DT_*Config 승격). 반감기 600초(10분) 가정: Lambda = ln(2)/600.
static constexpr double BacklogDecayLambda = 0.00115524530093; // ln(2)/600

double UProjectOperationManager::ComputeProductNetPerSec(const FBacklogProductEntry& E, const FDateTime& NowUtc) const
{
	const double Dt = FMath::Max(0.0, (NowUtc - E.LaunchWallClock).GetTotalSeconds());
	const double Gross = E.BasePeakRevenuePerSec * FMath::Exp(-BacklogDecayLambda * Dt);
	return FMath::Max(0.0, Gross - E.OperatingCostPerSec); // 순익 0 바닥(마이너스 없음)
}

double UProjectOperationManager::ComputeBuildingNetPerSec(int32 BuildingID, const FDateTime& NowUtc) const
{
	FOfficeSaveData* Office = GetOfficeSaveDataByBuildingID(BuildingID);
	if (!Office) return 0.0;
	double Sum = 0.0;
	for (const FBacklogProductEntry& E : Office->BacklogProducts)
		Sum += ComputeProductNetPerSec(E, NowUtc);
	return Sum * static_cast<double>(GetBuildingIncomeMultiplier(BuildingID));
}

// 오프라인 누적 — 닫힌형 적분 int max(0, Peak e^{-ls} - Cost) ds over [From,To] (감쇠 정지 정책은 후속 태스크)
double UProjectOperationManager::ComputeBuildingAccrual(int32 BuildingID, const FDateTime& FromUtc, const FDateTime& ToUtc) const
{
	FOfficeSaveData* Office = GetOfficeSaveDataByBuildingID(BuildingID);
	if (!Office) return 0.0;
	const double Mult = static_cast<double>(GetBuildingIncomeMultiplier(BuildingID));
	double Total = 0.0;
	for (const FBacklogProductEntry& E : Office->BacklogProducts)
	{
		const double Peak = E.BasePeakRevenuePerSec;
		const double Cost = E.OperatingCostPerSec;
		if (Peak <= Cost) continue; // 출시부터 순익 0
		const double Tstar = FMath::Loge(Peak / Cost) / BacklogDecayLambda; // 손익분기 경과초
		const double A = FMath::Max(0.0, (FromUtc - E.LaunchWallClock).GetTotalSeconds());
		const double B = FMath::Max(0.0, (ToUtc   - E.LaunchWallClock).GetTotalSeconds());
		const double Lo = FMath::Min(A, Tstar);
		const double Hi = FMath::Min(B, Tstar);
		if (Hi <= Lo) continue;
		const double Integral = (Peak / BacklogDecayLambda) * (FMath::Exp(-BacklogDecayLambda * Lo) - FMath::Exp(-BacklogDecayLambda * Hi)) - Cost * (Hi - Lo);
		Total += FMath::Max(0.0, Integral);
	}
	return Total * Mult;
}

// ===== Save/Load =====

void UProjectOperationManager::SetActiveOperations(const TArray<FOperationData>& InOperations)
{
	ActiveOperations = InOperations;
	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Loaded %d operations"), ActiveOperations.Num());
}

void UProjectOperationManager::RestoreOperation(const FOperationData& InOperation)
{
	// 이미 같은 BuildingID의 Operation이 있으면 업데이트
	for (FOperationData& Op : ActiveOperations)
	{
		if (Op.BuildingID == InOperation.BuildingID)
		{
			Op = InOperation;
			UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Updated existing operation for Building %d"),
				InOperation.BuildingID);
			return;
		}
	}

	// 없으면 새로 추가
	ActiveOperations.Add(InOperation);
	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Restored operation for Building %d: %s"),
		InOperation.BuildingID, *InOperation.ProjectName);
}

void UProjectOperationManager::LoadAllOperationsFromSave()
{
	// 이미 Operation이 있으면 스킵 (레벨 전환 시 중복 로드 방지)
	if (ActiveOperations.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] LoadAllOperationsFromSave - Skipped, already have %d operations"),
			ActiveOperations.Num());
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	USaveLoadManager* SaveLoadMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveLoadMgr) return;

	USaveGame_GameData* SaveData = SaveLoadMgr->GetCurrentSaveData();
	if (!SaveData) return;

	// 모든 건물의 Operation 로드
	for (const auto& Pair : SaveData->GameData.OfficeDataMap)
	{
		if (Pair.Value.bHasActiveOperation)
		{
			RestoreOperation(Pair.Value.CurrentOperation);
			UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Loaded Operation for Building %d: %s"),
				Pair.Key, *Pair.Value.CurrentOperation.ProjectName);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] LoadAllOperationsFromSave - Total: %d"),
		ActiveOperations.Num());
}

// ===== Manager Cache =====

UResourceItemManager* UProjectOperationManager::GetResourceItemManager() const
{
	if (!CachedResourceItemManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			CachedResourceItemManager = GI->GetSubsystem<UResourceItemManager>();
		}
	}
	return CachedResourceItemManager;
}

UTableManagerSubsystem* UProjectOperationManager::GetTableManager() const
{
	if (!CachedTableManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			CachedTableManager = GI->GetSubsystem<UTableManagerSubsystem>();
		}
	}
	return CachedTableManager;
}

void UProjectOperationManager::DistributeOperationExperience(const FOperationData& Operation)
{
	// 운영 완료 기본 경험치: 100 EXP
	const float BaseOperationExp = 100.0f;

	// EmployeeManager를 통해 경험치 분배 (품질 등급 배율 적용)
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI)
	{
		return;
	}

	UEmployeeManager* EmployeeMgr = GI->GetSubsystem<UEmployeeManager>();
	if (EmployeeMgr)
	{
		EmployeeMgr->DistributeExperienceToBuilding(Operation.BuildingID, BaseOperationExp, Operation.QualityGrade);

		UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Operation completed - Distributed %.1f base EXP (Grade: %s) to building %d"),
			BaseOperationExp, *QualityGradeToAlphabetString(Operation.QualityGrade), Operation.BuildingID);
	}
}

// ===== Employee Revenue Tracking =====

void UProjectOperationManager::AddEmployeeRevenue(int32 BuildingID, float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	FOperationData* Operation = GetOperationByBuildingID(BuildingID);
	if (Operation)
	{
		Operation->TotalRevenueEarned += Amount;
	}
}

// ===== Report =====

void UProjectOperationManager::GenerateReport(const FOperationData& CompletedOperation)
{
	FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(CompletedOperation.BuildingID);

	FProjectReportData Report;
	Report.ProjectID = CompletedOperation.ProjectID;
	Report.ProjectNumber = CompletedOperation.ProjectNumber;
	Report.StageNumber = CompletedOperation.StageNumber;
	Report.ProjectName = CompletedOperation.ProjectName;
	Report.BuildingID = CompletedOperation.BuildingID;
	Report.QualityScore = CompletedOperation.QualityScore;
	Report.QualityGrade = CompletedOperation.QualityGrade;
	Report.DisciplineScores = CompletedOperation.DisciplineScores;
	Report.DisciplineTargets = CompletedOperation.DisciplineTargets;
	Report.DisciplineSlots = CompletedOperation.DisciplineSlots;
	Report.TotalRevenueEarned = CompletedOperation.TotalRevenueEarned;
	Report.TotalOperationTime = CompletedOperation.TotalOperationTime;

	// 해당 빌딩의 CompanyType을 리포트에 기록 (산업별 DataTable 조회/아이콘 로드용)
	// StageProgress.CompanyType은 SelectProject 시점부터 운영 종료 시점까지 유지됨.
	if (OfficeData)
	{
		Report.CompanyType = OfficeData->StageProgress.CompanyType;
	}

	// 시가총액 계산 — QualityScore × MarketCapMultiplier 강화 배율
	// 공식: BaseMarketCap(10000) × QualityScore × Multiplier(브랜드파워 강화)
	// 예: Quality 1.0 × 배율 1.0 → 10000원, Quality 1.5 × 배율 1.3 → 19500원
	constexpr int64 BaseMarketCapReward = 10000;
	const float McMult = GetBuildingEnhancementMultiplier(CompletedOperation.BuildingID, EBuildingEnhancementType::MarketCapMultiplier);
	// 건물 특성 (Growth/ESG/IP → 시총 배율)
	float McTraitFactor = 1.0f;
	if (UCGGameInstance* CGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* TraitMgr = CGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			McTraitFactor = TraitMgr->GetTraitFactor(CompletedOperation.BuildingID, EBuildingTraitTarget::MarketCap);
		}
	}
	const int64 MarketCapGain = FMath::RoundToInt64(static_cast<double>(BaseMarketCapReward) * CompletedOperation.QualityScore * McMult * McTraitFactor);
	Report.MarketCapGained = MarketCapGain;

	// 즉시 지급 (결산서 확인 전에 획득되게 — 자연스러운 즉시 보상)
	if (MarketCapGain > 0)
	{
		if (UResourceItemManager* ResourceMgr = GetResourceItemManager())
		{
			ResourceMgr->StoreResource(EResourceType::MarketCap, MarketCapGain);
		}
	}

	// 결산서 보관 = 건물별 세이브 한 곳 (전역 캐시 없음 — 다른 건물에 들어가도 안 뜨는 근거)
	if (OfficeData)
	{
		OfficeData->bHasPendingReport = true;
		OfficeData->PendingReport = Report;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProjectOperationManager] GenerateReport - Building %d 의 OfficeSaveData 없음, 결산서 유실"),
			CompletedOperation.BuildingID);
	}

	// 출시작 누적매출 — (프로젝트 행 인덱스 + 산업)으로 레코드를 특정. 재개발하면 같은 행에 매출이 이어서 쌓인다.
	// (구 ProjectName 매칭은 플레이어가 이름을 바꾸거나 재개발로 동명 행이 생기면 엉뚱한 곳에 붙었다.)
	if (UCGGameInstance* RevGI = UCGGameInstance::GetInstance())
	{
		if (USaveLoadManager* RevSaveMgr = RevGI->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* RevSave = RevSaveMgr->GetCurrentSaveData())
			{
				const int64 RevGain = FMath::Max<int64>(0, FMath::RoundToInt64(CompletedOperation.TotalRevenueEarned));
				const ECompanyType ShippedIndustry = Report.CompanyType;
				FShippedProjectRecord* RevRecord = RevSave->GameData.ShippedProjects.FindByPredicate(
					[&CompletedOperation, ShippedIndustry](const FShippedProjectRecord& R)
					{
						return R.ProjectIndex == CompletedOperation.ProjectNumber && R.Industry == ShippedIndustry;
					});
				if (RevRecord)
				{
					RevRecord->CumulativeRevenue += RevGain;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Report generated: %s (Quality: %s, Revenue: %.0f)"),
		*CompletedOperation.ProjectName,
		*QualityGradeToAlphabetString(CompletedOperation.QualityGrade),
		CompletedOperation.TotalRevenueEarned);

	// M10 미션 — 첫 자체개발 운영 완료(리포트 생성) 신호
	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->NotifyProjectOperationCompleted();
	}
}

bool UProjectOperationManager::HasPendingReport(int32 BuildingID) const
{
	const FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
	return OfficeData && OfficeData->bHasPendingReport;
}

const FProjectReportData* UProjectOperationManager::GetPendingReport(int32 BuildingID) const
{
	const FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
	return (OfficeData && OfficeData->bHasPendingReport) ? &OfficeData->PendingReport : nullptr;
}

FProjectReportData UProjectOperationManager::ConsumeReport(int32 BuildingID)
{
	FOfficeSaveData* OfficeData = GetOfficeSaveDataByBuildingID(BuildingID);
	if (!OfficeData || !OfficeData->bHasPendingReport)
	{
		return FProjectReportData();
	}

	FProjectReportData Report = OfficeData->PendingReport;
	OfficeData->bHasPendingReport = false;
	OfficeData->PendingReport = FProjectReportData();

	UE_LOG(LogTemp, Log, TEXT("[ProjectOperationManager] Report consumed (Building %d): %s"), BuildingID, *Report.ProjectName);
	return Report;
}
