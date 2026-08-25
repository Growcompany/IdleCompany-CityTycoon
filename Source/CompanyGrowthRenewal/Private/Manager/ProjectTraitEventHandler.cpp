#include "Manager/ProjectTraitEventHandler.h"
#include "Core/CGGameInstance.h"
#include "Data/StageProgressData.h"
#include "Data/ProjectBoardData.h"
#include "Data/BuildingEnhancementData.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectEvent, Log, All);

UProjectTraitEventHandler::UProjectTraitEventHandler()
{
}

// ===== 티어/풀 헬퍼 =====

int32 UProjectTraitEventHandler::GetTierFromProjectIndex(int32 ProjectIndex)
{
	return FProjectTierProgress::GetTierForProject(ProjectIndex);
}

void UProjectTraitEventHandler::GetTraitCountRange(int32 Tier, int32& OutMin, int32& OutMax)
{
	if (Tier <= 2)      { OutMin = 0; OutMax = 1; }
	else if (Tier <= 5) { OutMin = 1; OutMax = 1; }
	else if (Tier <= 8) { OutMin = 1; OutMax = 2; }
	else                { OutMin = 2; OutMax = 2; }
}

void UProjectTraitEventHandler::GetEventCountRange(int32 Tier, int32& OutMin, int32& OutMax)
{
	// 30초 타이머 기준: 이벤트 빈도 약간 상향 (단조로움 방지)
	if (Tier <= 2)      { OutMin = 1; OutMax = 1; }
	else if (Tier <= 5) { OutMin = 1; OutMax = 2; }
	else if (Tier <= 8) { OutMin = 2; OutMax = 2; }
	else                { OutMin = 2; OutMax = 3; }
}

TArray<EProjectTrait> UProjectTraitEventHandler::BuildTraitPool(EProjectMode Mode, int32 Tier)
{
	TArray<EProjectTrait> Pool;

	// 공통 - 모든 티어
	Pool.Add(EProjectTrait::StabilityOriented);
	Pool.Add(EProjectTrait::DevFocus);
	Pool.Add(EProjectTrait::QAFocus);
	Pool.Add(EProjectTrait::PlanningFocus);

	if (Mode == EProjectMode::Commissioned)
	{
		// 수주: 제약형 위주
		Pool.Add(EProjectTrait::RushDelivery);
		Pool.Add(EProjectTrait::BudgetCut);
		if (Tier >= 3) Pool.Add(EProjectTrait::StrictQA);
		if (Tier >= 3) Pool.Add(EProjectTrait::ClientPressure);
		if (Tier >= 5) Pool.Add(EProjectTrait::HighRiskHighReturn);
		if (Tier >= 7) Pool.Add(EProjectTrait::InnovationRequired);
	}
	else // InHouse
	{
		// 자체개발: 기회형 위주
		Pool.Add(EProjectTrait::TrendRiding);
		Pool.Add(EProjectTrait::InnovationRequired);
		if (Tier >= 3) Pool.Add(EProjectTrait::RushDelivery);
		if (Tier >= 3) Pool.Add(EProjectTrait::BudgetCut);
		if (Tier >= 5) Pool.Add(EProjectTrait::HighRiskHighReturn);
		if (Tier >= 7) Pool.Add(EProjectTrait::ClientPressure);
		if (Tier >= 9) Pool.Add(EProjectTrait::LargeProject);
	}

	return Pool;
}

TArray<EProjectEvent> UProjectTraitEventHandler::BuildEventPool(EProjectMode Mode, ECompanyType CompanyType, int32 Tier)
{
	TArray<EProjectEvent> Pool;

	// 티어 1~2: 긍정적/가벼운 이벤트 위주
	Pool.Add(EProjectEvent::GoodIdea);
	Pool.Add(EProjectEvent::OvertimeRequest);
	Pool.Add(EProjectEvent::CoffeeTime);
	Pool.Add(EProjectEvent::NewRecruit);

	// 티어 3+: 양면적/성장 이벤트 추가
	if (Tier >= 3)
	{
		Pool.Add(EProjectEvent::BugFound);
		Pool.Add(EProjectEvent::TeamConflict);
		Pool.Add(EProjectEvent::SuddenSuccess);
		Pool.Add(EProjectEvent::MentorVisit);
	}

	// 티어 5+: 도전/장비/리스크
	if (Tier >= 5)
	{
		Pool.Add(EProjectEvent::EquipmentUpgrade);
		Pool.Add(EProjectEvent::PowerOutage);
		Pool.Add(EProjectEvent::Burnout);
	}

	// 티어 6+: 기술 부채 + 산업별 (2종씩)
	if (Tier >= 6)
	{
		Pool.Add(EProjectEvent::TechDebt);

		if (CompanyType == ECompanyType::Game)
		{
			Pool.Add(EProjectEvent::PublisherFeedback);
			Pool.Add(EProjectEvent::PlaytestFeedback);
		}
		else if (CompanyType == ECompanyType::Finance)
		{
			Pool.Add(EProjectEvent::RegulationChange);
			Pool.Add(EProjectEvent::SecurityAudit);
		}
		else if (CompanyType == ECompanyType::IT)
		{
			Pool.Add(EProjectEvent::ServerIssue);
			Pool.Add(EProjectEvent::ScalingChallenge);
		}
	}

	// 모드별 전용 이벤트
	if (Tier >= 5)
	{
		if (Mode == EProjectMode::Commissioned)
		{
			Pool.Add(EProjectEvent::ClientRevision);
			Pool.Add(EProjectEvent::NegotiateExtension);
			if (Tier >= 7) Pool.Add(EProjectEvent::ClientCancellation);
		}
		else
		{
			Pool.Add(EProjectEvent::MarketShift);
			if (Tier >= 7)
			{
				Pool.Add(EProjectEvent::InvestorInterest);
				Pool.Add(EProjectEvent::Pivoting);
			}
		}
	}

	return Pool;
}

// ===== 트레이트 API =====

void UProjectTraitEventHandler::AssignRandomTraits(ECompanyType CompanyType, int32 ProjectIndex, EProjectMode Mode)
{
	ActiveTraits.Empty();

	int32 Tier = GetTierFromProjectIndex(ProjectIndex);
	int32 MinCount, MaxCount;
	GetTraitCountRange(Tier, MinCount, MaxCount);

	int32 TraitCount = FMath::RandRange(MinCount, MaxCount);
	if (TraitCount <= 0) return;

	TArray<EProjectTrait> Pool = BuildTraitPool(Mode, Tier);
	if (Pool.Num() == 0) return;

	// 풀에서 랜덤 추첨 (충돌 검증 포함)
	TArray<EProjectTrait> Selected;
	int32 MaxAttempts = 50;

	for (int32 i = 0; i < TraitCount && MaxAttempts > 0; i++)
	{
		int32 RandomIndex = FMath::RandRange(0, Pool.Num() - 1);
		EProjectTrait Candidate = Pool[RandomIndex];

		// 이미 선택됐는지 체크
		if (Selected.Contains(Candidate))
		{
			i--;
			MaxAttempts--;
			continue;
		}

		// 충돌 체크
		bool bConflict = false;
		for (EProjectTrait Existing : Selected)
		{
			if (AreTraitsConflicting(Candidate, Existing))
			{
				bConflict = true;
				break;
			}
		}

		if (bConflict)
		{
			i--;
			MaxAttempts--;
			continue;
		}

		Selected.Add(Candidate);
	}

	// FProjectTraitData로 변환
	for (EProjectTrait Trait : Selected)
	{
		ActiveTraits.Add(FProjectTraitData::MakeTraitData(Trait));
	}
}

float UProjectTraitEventHandler::GetEffectiveTimerDuration(float DefaultDuration) const
{
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		if (Trait.TimerDurationOverride > 0.0f)
		{
			return Trait.TimerDurationOverride;
		}
	}
	return DefaultDuration;
}

float UProjectTraitEventHandler::GetRequiredScoreMultiplier() const
{
	float Multiplier = 1.0f;
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		Multiplier *= Trait.RequiredScoreMultiplier;
	}
	return Multiplier;
}

float UProjectTraitEventHandler::GetRewardMultiplier() const
{
	float Multiplier = 1.0f;
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		Multiplier *= Trait.RewardMultiplier;
	}
	return Multiplier;
}

float UProjectTraitEventHandler::GetBaseScoreMultiplier() const
{
	float Multiplier = 1.0f;
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		Multiplier *= Trait.BaseScoreMultiplier;
	}
	return Multiplier;
}

void UProjectTraitEventHandler::GetCategoryWeights(float& OutPlanning, float& OutDev, float& OutQA) const
{
	// 기본 가중치
	OutPlanning = 0.2f;
	OutDev = 0.5f;
	OutQA = 0.3f;

	// Focus 계열 트레이트가 있으면 오버라이드
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		if (Trait.PlanningWeight >= 0.0f && Trait.DevWeight >= 0.0f && Trait.QAWeight >= 0.0f)
		{
			OutPlanning = Trait.PlanningWeight;
			OutDev = Trait.DevWeight;
			OutQA = Trait.QAWeight;
			return;
		}
	}
}

float UProjectTraitEventHandler::GetRandomVariance() const
{
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		// 안정 지향: 변동 없음 (Min == Max == 1.0)
		if (FMath::IsNearlyEqual(Trait.RandomVarianceMin, Trait.RandomVarianceMax))
		{
			return Trait.RandomVarianceMin;
		}
	}

	// 기본 랜덤 범위 중 하나 반환 (첫 번째 트레이트 기준, 없으면 기본)
	float Min = 0.8f;
	float Max = 1.2f;
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		Min = Trait.RandomVarianceMin;
		Max = Trait.RandomVarianceMax;
		break;
	}
	return FMath::FRandRange(Min, Max);
}

float UProjectTraitEventHandler::GetCriticalMultiplier(float DefaultMultiplier) const
{
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		if (Trait.CriticalMultiplierOverride > 0.0f)
		{
			return Trait.CriticalMultiplierOverride;
		}
	}
	return DefaultMultiplier;
}

float UProjectTraitEventHandler::GetCriticalChanceMultiplier() const
{
	float Multiplier = 1.0f;
	for (const FProjectTraitData& Trait : ActiveTraits)
	{
		Multiplier *= Trait.CriticalChanceMultiplier;
	}
	return Multiplier;
}

// ===== 이벤트 API =====

void UProjectTraitEventHandler::PrepareEvents(ECompanyType CompanyType, int32 ProjectIndex, EProjectMode Mode)
{
	ScheduledEvents.Empty();
	EventTriggerTimings.Empty();
	NextEventIndex = 0;
	CurrentPendingEvent = FProjectEventData();

	int32 Tier = GetTierFromProjectIndex(ProjectIndex);
	int32 MinCount, MaxCount;
	GetEventCountRange(Tier, MinCount, MaxCount);

	int32 EventCount = FMath::RandRange(MinCount, MaxCount);
	if (EventCount <= 0) return;

	TArray<EProjectEvent> Pool = BuildEventPool(Mode, CompanyType, Tier);
	if (Pool.Num() == 0) return;

	// 풀에서 랜덤 선택 (중복 제거)
	TArray<EProjectEvent> Selected;
	for (int32 i = 0; i < EventCount && Pool.Num() > 0; i++)
	{
		int32 RandomIndex = FMath::RandRange(0, Pool.Num() - 1);
		Selected.Add(Pool[RandomIndex]);
		Pool.RemoveAt(RandomIndex);
	}

	if (Selected.Num() < EventCount)
	{
		UE_LOG(LogProjectEvent, Warning,
			TEXT("Event pool exhausted: requested %d but only %d available (Tier=%d Mode=%d CompanyType=%d)"),
			EventCount, Selected.Num(), Tier, (int32)Mode, (int32)CompanyType);
	}

	// 트리거 시점 결정 [0.15, 0.85] 구간을 N등분해서 각 슬롯 내 랜덤 위치에 배치
	// → 슬롯 경계 자체가 이벤트 간 최소 간격을 보장 (padding으로 경계 근접을 살짝 회피)
	TArray<float> Timings;
	const int32 EventN = Selected.Num();
	constexpr float MinProgress = 0.15f;
	constexpr float MaxProgress = 0.85f;
	constexpr float SlotPaddingRatio = 0.3f; // 슬롯 크기의 30%를 양 끝 여백으로 사용

	if (EventN == 1)
	{
		Timings.Add(FMath::FRandRange(MinProgress, MaxProgress));
	}
	else if (EventN >= 2)
	{
		const float Span = MaxProgress - MinProgress;
		const float SlotSize = Span / EventN;
		const float Padding = SlotSize * SlotPaddingRatio;

		for (int32 i = 0; i < EventN; i++)
		{
			const float SlotStart = MinProgress + SlotSize * i + Padding;
			const float SlotEnd = MinProgress + SlotSize * (i + 1) - Padding;
			Timings.Add(FMath::FRandRange(SlotStart, SlotEnd));
		}
	}

	// 정렬 (오름차순) — 슬롯 분할이라 이미 정렬되어 있지만 안전하게
	Timings.Sort();

	// 이벤트 데이터 + 트리거 시점 매핑
	for (int32 i = 0; i < Selected.Num() && i < Timings.Num(); i++)
	{
		ScheduledEvents.Add(FProjectEventData::MakeEventData(Selected[i]));
		EventTriggerTimings.Add(Timings[i]);
	}
}

bool UProjectTraitEventHandler::CheckEventTrigger(float ElapsedRatio)
{
	if (NextEventIndex >= ScheduledEvents.Num()) return false;
	if (NextEventIndex >= EventTriggerTimings.Num()) return false;
	if (bEventPaused) return false;

	if (ElapsedRatio >= EventTriggerTimings[NextEventIndex])
	{
		CurrentPendingEvent = ScheduledEvents[NextEventIndex];
		NextEventIndex++;
		return true;
	}

	return false;
}

void UProjectTraitEventHandler::ApplyEventChoice(int32 ChoiceIndex, FStageProgressData& ProgressData)
{
	if (!CurrentPendingEvent.Choices.IsValidIndex(ChoiceIndex)) return;

	const FProjectEventChoice& Choice = CurrentPendingEvent.Choices[ChoiceIndex];

	// 빌딩 강화 EventResistance (부정 효과 감소) / ViralBoost (긍정 효과 증폭) 배율 조회 — Phase 3
	float ResistMult = 1.0f;  // 음수 보너스(페널티)에 곱
	float ViralMult = 1.0f;   // 양수 보너스에 곱
	{
		if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
		{
			if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
			{
				if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
				{
					const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
					for (const FBuildingEntitySaveData& B : SaveData->GameData.Buildings)
					{
						if (B.BuildingIndex == BuildingIndex)
						{
							const int32 ResistLv = B.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::EventResistance);
							ResistMult = UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::EventResistance, ResistLv);
							const int32 ViralLv = B.BuildingData.EnhancementLevels.FindRef(EBuildingEnhancementType::ViralBoost);
							ViralMult = UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::ViralBoost, ViralLv);
							break;
						}
					}
				}
			}
		}
	}
	// EventResistance 는 "피해 감소" 의미이므로 페널티에 직접 곱하면 강화될수록 페널티가 더 커지는 역효과.
	// ResistMult(>=1.0) 의 역수를 페널티 잔존률로 사용 — Lv 1000 (Mult 1.5) → 페널티 67% 만 적용.
	const float PenaltyRetention = 1.0f / FMath::Max(ResistMult, KINDA_SMALL_NUMBER);
	auto AdjustBonus = [PenaltyRetention, ViralMult](float Bonus) -> float
	{
		return Bonus >= 0.0f ? (Bonus * ViralMult) : (Bonus * PenaltyRetention);
	};

	// 카테고리별 점수 보너스: 고정 점수 가산 (누적 무관, 항상 동일한 변화량 보장)
	if (ProgressData.Steps.IsValidIndex(0) && !FMath::IsNearlyZero(Choice.PlanningScoreBonus))
	{
		ProgressData.Steps[0].AcquiredScore = FMath::Max(0.0f,
			ProgressData.Steps[0].AcquiredScore + AdjustBonus(Choice.PlanningScoreBonus));
	}
	if (ProgressData.Steps.IsValidIndex(1) && !FMath::IsNearlyZero(Choice.DevScoreBonus))
	{
		ProgressData.Steps[1].AcquiredScore = FMath::Max(0.0f,
			ProgressData.Steps[1].AcquiredScore + AdjustBonus(Choice.DevScoreBonus));
	}
	if (ProgressData.Steps.IsValidIndex(2) && !FMath::IsNearlyZero(Choice.QAScoreBonus))
	{
		ProgressData.Steps[2].AcquiredScore = FMath::Max(0.0f,
			ProgressData.Steps[2].AcquiredScore + AdjustBonus(Choice.QAScoreBonus));
	}

	// 타이머 증감 — 양수(시간 추가) ViralBoost / 음수(시간 감소) EventResistance
	if (!FMath::IsNearlyZero(Choice.TimerAdjustment))
	{
		ProgressData.RemainingTime += AdjustBonus(Choice.TimerAdjustment);
		ProgressData.RemainingTime = FMath::Max(0.5f, ProgressData.RemainingTime);
	}

	// 랜덤 카테고리 페널티 (항상 음수 — EventResistance 로 감소)
	if (Choice.bRandomCategoryPenalty && !FMath::IsNearlyZero(Choice.RandomPenaltyAmount))
	{
		int32 RandomStep = FMath::RandRange(0, 2);
		if (ProgressData.Steps.IsValidIndex(RandomStep))
		{
			ProgressData.Steps[RandomStep].AcquiredScore = FMath::Max(0.0f,
				ProgressData.Steps[RandomStep].AcquiredScore + Choice.RandomPenaltyAmount * PenaltyRetention);
		}
	}

	// 일시정지 효과
	if (Choice.PauseDuration > 0.0f)
	{
		PauseRemaining = Choice.PauseDuration;
		bEventPaused = true;
	}
	else
	{
		bEventPaused = false;
	}

	// 직원 일시 버프 적용 (BuffType != None)
	if (Choice.BuffType != EBuffType::None && Choice.BuffDuration > 0.0f && Choice.BuffTargetCount > 0)
	{
		ApplyBuffToRandomWorkers(Choice.BuffType, Choice.BuffValue, Choice.BuffDuration, Choice.BuffTargetCount);
	}

	// 이벤트 보상 배율 (예: 규제 변경 "무시" → 0.8) — 트레이트 배율과 분리된 전용 필드에 누적, 정산 시 적용
	if (!FMath::IsNearlyEqual(Choice.RewardMultiplier, 1.0f))
	{
		ProgressData.EventRewardMultiplier *= Choice.RewardMultiplier;
	}
}

void UProjectTraitEventHandler::ApplyBuffToRandomWorkers(EBuffType Type, float Value, float Duration, int32 TargetCount)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 현재 플레이 중인 건물 기준으로만 버프 적용 (다른 건물 직원에게 전파 방지)
	UCGGameInstance* GI = Cast<UCGGameInstance>(World->GetGameInstance());
	if (!GI) return;
	const int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
	if (BuildingIndex < 0) return;

	UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmpMgr) return;

	TArray<AOfficeworker*> Workers = EmpMgr->GetSpawnedWorkersInBuilding(BuildingIndex);
	if (Workers.Num() == 0) return;

	// 랜덤 N명 추출
	const int32 PickCount = FMath::Min(TargetCount, Workers.Num());
	for (int32 i = 0; i < PickCount; i++)
	{
		const int32 RandomIndex = FMath::RandRange(0, Workers.Num() - 1);
		AOfficeworker* Picked = Workers[RandomIndex];
		Workers.RemoveAtSwap(RandomIndex);

		if (Picked && Picked->BehaviorComponent)
		{
			Picked->BehaviorComponent->AddBuff(Type, Value, Duration);
		}
	}

	UE_LOG(LogProjectEvent, Log, TEXT("Buff applied to %d workers in building %d: Type=%d Value=%.2f Duration=%.1f"),
		PickCount, BuildingIndex, (int32)Type, Value, Duration);
}

// ===== 일시정지 =====

void UProjectTraitEventHandler::SetEventPaused(bool bPaused)
{
	bEventPaused = bPaused;
	if (!bPaused)
	{
		PauseRemaining = 0.0f;
	}
}

void UProjectTraitEventHandler::TickPause(float DeltaTime)
{
	if (!bEventPaused) return;

	// PauseRemaining <= 0 → 유저 입력 대기 모드: 유저가 선택지를 누를 때까지 무한 대기
	// PauseRemaining > 0 → Choice로 부여된 시간제 일시정지: 카운트다운 후 자동 해제
	if (PauseRemaining <= 0.0f)
	{
		return;
	}

	PauseRemaining -= DeltaTime;
	if (PauseRemaining <= 0.0f)
	{
		PauseRemaining = 0.0f;
		bEventPaused = false;
		OnEventPauseEnded.Broadcast();
	}
}

// ===== 초기화 =====

void UProjectTraitEventHandler::ClearAll()
{
	ActiveTraits.Empty();
	ScheduledEvents.Empty();
	EventTriggerTimings.Empty();
	NextEventIndex = 0;
	bEventPaused = false;
	PauseRemaining = 0.0f;
	CurrentPendingEvent = FProjectEventData();
}
