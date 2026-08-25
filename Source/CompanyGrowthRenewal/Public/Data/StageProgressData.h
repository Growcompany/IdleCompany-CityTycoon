#pragma once

#include "CoreMinimal.h"
#include "Enum/QualityGrade.h"
#include "Enum/CompanyType.h"
#include "Enum/ProjectMode.h"
#include "Enum/ProjectTrait.h"
#include "Enum/ProjectDirection.h"
#include "Enum/ProductionDiscipline.h"
#include "StageProgressData.generated.h"

/**
 * 스트립 셀의 "목표 달성" 판정 ― 바 그린 전환과 달성 배지가 같은 문턱을 쓰게 하는 단일 출처.
 * 애니메이션 중인 표시값(Shown Pct)을 받으므로 FStepRoundData 멤버가 아니라 비율을 받는 자유 함수다.
 *
 * 0.999 인 이유 = 카운트업의 **조기 이탈**이다. OfficeMainWidget.cpp:299-301 의 NativeTick 이
 * IsNearlyEqual(Shown, Target, PctEps) 로 램프를 멈추고 PctEps 상한이 0.001 이라, Shown 은 Target 보다
 * 최대 0.001 뒤처진 채 정지할 수 있다. (FInterpConstantTo 자체는 Clamp(Dist,-Step,Step) 이라 남은 거리가
 * 한 스텝 이내면 Target 에 정확히 도달한다 ― 수렴 실패가 이유가 아니다.)
 * ⚠ 그 PctEps 상한 0.001 과 커플링돼 있다 ― 상한을 올리면 이 문턱도 같이 내려야 배지가 안 깨진다.
 *
 * 정합 보장 범위: 도장(권위 Pct)과 배지(표시 Pct)가 함께 서는 것은 **Pct >= 1.0** 에서다.
 * 최종 Pct 가 [0.999, 1.0) 에 안착하면 도장은 뜨는데 바/배지는 블루로 남을 수 있다(확률 극히 낮음).
 * 다만 배지와 바는 같은 bFull 을 공유하므로 둘 사이는 절대 갈라지지 않는다 ― 이 함수가 보장하는 것이 정확히 그것이다.
 */
FORCEINLINE bool IsStageGoalMet(float Pct)
{
	return Pct >= 0.999f;
}

/**
 * Step 라운드 데이터
 * 동시 진행 모드: 15초 동안 기획/개발/QA 3개 카테고리 점수가 동시 누적
 */
USTRUCT(BlueprintType)
struct FStepRoundData
{
	GENERATED_BODY()

	// 단계 번호 (1~4: 기획, 개발, QA, 출시)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Step")
	int32 StepNumber = 1;

	// 단계 이름 (회사 타입에 따라 다름)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Step")
	FString StepName;

	// 최소 점수 (통과 기준)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Score")
	float MinimumScore = 60.0f;

	// 목표 점수 (S등급 기준)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Score")
	float TargetScore = 120.0f;

	// 획득 점수
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Score")
	float AcquiredScore = 0.0f;

	// 완료 여부
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	bool bIsCompleted = false;

	// 이 스텝이 담당하는 직능 슬롯(EProductionDiscipline 인덱스). 출시 스텝은 INDEX_NONE.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Step")
	int32 DisciplineSlot = INDEX_NONE;

	// 프로젝트 이 직능 가중치(품질 집계·TargetScore 스케일용). 0=미사용.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Step")
	int32 Weight = 0;

	// 달성률 계산 (목표 점수 기준)
	FORCEINLINE float GetAchievementRate() const
	{
		return TargetScore > 0.0f ? AcquiredScore / TargetScore : 0.0f;
	}

	// 최소 점수 달성 여부
	FORCEINLINE bool HasPassedMinimum() const
	{
		return AcquiredScore >= MinimumScore
			|| FMath::IsNearlyEqual(AcquiredScore, MinimumScore, UE_KINDA_SMALL_NUMBER);
	}

	FStepRoundData()
		: StepNumber(1)
		, StepName(TEXT(""))
		, MinimumScore(60.0f)
		, TargetScore(120.0f)
		, AcquiredScore(0.0f)
		, bIsCompleted(false)
	{}

	FStepRoundData(int32 InStepNumber, const FString& InStepName, float InMinScore, float InTargetScore)
		: StepNumber(InStepNumber)
		, StepName(InStepName)
		, MinimumScore(InMinScore)
		, TargetScore(InTargetScore)
		, AcquiredScore(0.0f)
		, bIsCompleted(false)
	{}
};

/**
 * 예상 품질 추정기 입력 — 매니저/세이브 타입에 의존하지 않아야 순수 테스트가 가능하다.
 */
USTRUCT()
struct FEstimateWorkerInput
{
	GENERATED_BODY()

	int32 Level = 1;

	// EProductionDiscipline 슬롯 순 직능 포인트 (FEmployeeInstance::DisciplinePoints 와 같은 배치)
	TArray<int32> DisciplinePoints;

	// 이 직원만의 산출 배수 — 판이 시작되기 전에 이미 확정돼 판 내내 변하지 않는 항만 담는다(속도 배수·잠재큐브).
	// 피로/슬랙처럼 판이 도는 동안 변하는 항은 시간 적분 모델이 필요해 여기 넣지 않는다.
	float OutputScale = 1.0f;

	// 업무집중도 — 주 직능(DisciplinePoints argmax)이 강제 지정될 확률. 0 이면 순수 어피니티 가중 랜덤.
	// 기여 경로가 이 확률만큼 배분을 주 직능으로 몰아주므로, 추정기도 같이 반영하지 않으면 예상↔실제가 갈린다.
	float FocusChance = 0.0f;
};

struct FProjectOutlookEstimate
{
	bool bIsValid = false;
	float ExpectedQuality = 0.5f;
	bool bGateRisk = true;
	TArray<float> ExpectedDisciplineScores;
};

/**
 * 스테이지 진행 데이터
 * 하나의 프로젝트 진행 상태를 추적
 *
 * 동시 진행 모드:
 *   CurrentStep 0 = idle (프로젝트 대기)
 *   CurrentStep 1 = 동시 타이머 진행 중 (15초, Steps[0~2] 3개 카테고리 동시 점수 누적)
 *   타이머 종료 후 결과 팝업 → 즉시 출시/재도전/포기 결정
 */
USTRUCT(BlueprintType)
struct FStageProgressData
{
	GENERATED_BODY()

	// 프로젝트 번호 (몇 번째 프로젝트인지, 1부터 시작)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Stage")
	int32 ProjectNumber = 1;

	// 스테이지 번호 (프로젝트 내 스테이지, 현재 항상 1, 확장 가능)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Stage")
	int32 StageNumber = 1;

	// 프로젝트 ID (DataTable 연동, = ProjectNumber)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Stage")
	int32 ProjectID = 0;

	// 프로젝트 이름
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FString ProjectName;

	// 회사 타입
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Stage")
	ECompanyType CompanyType = ECompanyType::None;

	// 현재 Step (0=idle, 1=동시 타이머 진행 중)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	int32 CurrentStep = 0;

	// 남은 시간 (초)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Timer")
	float RemainingTime = 15.0f;

	// Step별 데이터 (1~4)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	TArray<FStepRoundData> Steps;

	// 컨티뉴(추가 개발) 사용 횟수 — 프로젝트당 1회 한정. ResetProjectState에서 0 복귀
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	int32 RetryCount = 0;

	// 타이머 진행 중 여부 (런타임 전용, 저장 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timer")
	bool bIsTimerRunning = false;

	// 제조업 여부 (CompanyType 기반, 양산 플로우 분기용)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Operation")
	bool bIsManufacturing = false;

	// 프로젝트 운영 모드 (수주/자체개발, 프로젝트형 산업군에서만 사용)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Board")
	EProjectMode ActiveMode = EProjectMode::None;

	// 현재 프로젝트의 활성 트레이트 (보드에서 부여된 특성)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Board")
	TArray<EProjectTrait> ActiveTraits;

	// 트레이트에 의한 보상 배율 (운영/출시 시 적용)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Board")
	float TraitRewardMultiplier = 1.0f;

	// 이벤트 선택지에 의한 보상 배율 (예: 규제 변경 "무시" → 0.8). 트레이트 배율과 분리 — 이벤트 효과만 정산 반영
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Board")
	float EventRewardMultiplier = 1.0f;

	// ===== GDS 착수 발견형 (장르 × 소재 × 방향성) =====

	// 축A — 장르
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	FName Genre = NAME_None;

	// 축B — 소재
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	FName Material = NAME_None;

	// 결단 — 개발 방향성
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	EProjectDirection Direction = EProjectDirection::Standard;

	// Step 타이머 길이(초). SelectProject가 ProjectData.Duration으로 저장 → ResolveStepDuration이 우선 사용.
	// 합성 프로젝트(테이블 인덱스 무관)가 올바른 길이를 갖게 함. 0이면 기존 폴백(테이블/기본값).
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	float StepDuration = 0.0f;

	// 착수 순간 트렌드 소재 매칭 여부 — 운영 피크 x1.5 (착수 후 트렌드가 바뀌어도 유지)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	bool bTrendMatched = false;

	// 리뷰 총점 /40 (개발 종료 시 계산, InHouse 프로젝트형만. 0=미계산) — 운영 피크에 ReviewLeverage 가중 주입
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	int32 ReviewScore = 0;

	// 비평가 4명 개별 점수 (각 1~10)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	TArray<int32> CriticScores;

	// 이 (장르,소재) 조합의 첫 발견 여부 — 리뷰 발표에서 "첫 발견!" 배지
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "GDS")
	bool bFirstDiscovery = false;

	// Step 1~3 달성률 (품질 점수 계산용)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	float Step1AchievementRate = 0.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	float Step2AchievementRate = 0.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	float Step3AchievementRate = 0.0f;

	/**
	 * 품질 점수 계산 (0.5 ~ 2.0) — 프로젝트 직능 가중치로 정규화.
	 * 활성 직능 스텝의 달성률(캡 2.0)을 weight 가중평균. 출시 스텝(DisciplineSlot=NONE) 제외.
	 * 반환 계약(0.5~2.0)은 경제 소비처(운영수익/리뷰/출시)와의 절연을 위해 불변.
	 */
	float CalculateQualityScore() const
	{
		float WeightedSum = 0.0f;
		float WeightTotal = 0.0f;
		for (const FStepRoundData& S : Steps)
		{
			if (S.DisciplineSlot == INDEX_NONE || S.Weight <= 0) { continue; }
			WeightedSum += FMath::Min(S.GetAchievementRate(), 2.0f) * S.Weight;
			WeightTotal += S.Weight;
		}
		if (WeightTotal <= 0.0f) { return 0.5f; }
		return FMath::Clamp(WeightedSum / WeightTotal, 0.5f, 2.0f);
	}

	/**
	 * 품질 등급 계산
	 */
	EQualityGrade CalculateQualityGrade() const
	{
		return QualityScoreToGrade(CalculateQualityScore());
	}

	/**
	 * 개발 완료 기준 통과 여부
	 * 모든 활성 직능 스텝(DisciplineSlot != NONE)이 MinimumScore 이상일 때만 true
	 */
	bool MeetsMinimumClearScore() const
	{
		bool bAnyDiscipline = false;
		for (const FStepRoundData& S : Steps)
		{
			if (S.DisciplineSlot == INDEX_NONE) { continue; }  // 출시 스텝 제외
			bAnyDiscipline = true;
			if (!S.HasPassedMinimum())
			{
				return false;
			}
		}
		return bAnyDiscipline;
	}

	/**
	 * Steps 배열 초기화 (레거시 4고정 — 착수 시 InitializeDisciplineSteps로 대체됨. 폴백/기본 생성용)
	 */
	void InitializeSteps()
	{
		Steps.Empty();
		Steps.Reserve(4);
		for (int32 i = 1; i <= 4; i++)
		{
			Steps.Add(FStepRoundData(i, TEXT(""), 60.0f, 120.0f));
		}
	}

	/**
	 * 직능 동적 스텝 생성 — weight>0 직능만 N스텝. 착수 시 프로젝트 가중치로 호출.
	 * @param Weights EProductionDiscipline 슬롯 순(Plan/Dev/Graphics/Sound/Server/QA)의 가중치(0~5)
	 * @param TargetBase 최대 가중 직능의 목표 점수(가중 비례로 각 스텝 스케일)
	 * 표시명(StepName)은 호출자(매니저)가 산업별 DT로 주입.
	 */
	void InitializeDisciplineSteps(const TArray<int32>& Weights, float TargetBase)
	{
		Steps.Empty();
		int32 MaxW = 1;
		for (int32 W : Weights) { MaxW = FMath::Max(MaxW, W); }
		int32 StepNo = 1;
		for (int32 Slot = 0; Slot < Weights.Num(); ++Slot)
		{
			if (Weights[Slot] <= 0) { continue; }
			const float StepTarget = ComputeDisciplineTarget(Weights[Slot], MaxW, TargetBase);
			FStepRoundData S(StepNo++, TEXT(""), StepTarget * MinimumScoreRatio, StepTarget);
			S.DisciplineSlot = Slot;
			S.Weight = Weights[Slot];
			Steps.Add(S);
		}
	}

	/**
	 * 프로젝트 규모 기준선 = 기존 3스텝 요구점수 평균 x2.
	 * 착수 시 만들어지는 스텝 목표와 피치 카드가 보여주는 수치가 같아야 하므로 식은 여기 한 곳에만 둔다
	 * (복사해 두면 갈리는 순간 카드가 거짓 수치를 보여주고, 플레이어는 착수 후에야 안다).
	 */
	static float ComputeDisciplineTargetBase(int32 Score1, int32 Score2, int32 Score3)
	{
		return FMath::Max(1.0f, static_cast<float>(Score1 + Score2 + Score3) / 3.0f) * 2.0f;
	}

	/** 직능 슬롯 하나의 목표 점수 — 최대 가중 직능이 TargetBase 를 받고 나머지는 가중 비례. */
	static float ComputeDisciplineTarget(int32 Weight, int32 MaxWeight, float TargetBase)
	{
		return (Weight > 0 && MaxWeight > 0) ? (TargetBase * Weight / MaxWeight) : 0.0f;
	}

	// 출시 게이트 하한 = 목표의 이 비율. 피치 카드 예상과 실제 스텝 MinimumScore 가
	// 갈리면 카드가 통과한다고 한 프로젝트가 출시에서 막힌다.
	static constexpr float MinimumScoreRatio = 0.5f;

	/** UI 퍼센트 표시는 실제 달성률보다 높게 보이지 않도록 소수점을 내린다. */
	static int32 ComputeDisciplinePercentDisplayValue(float AchievementRatio)
	{
		if (!FMath::IsFinite(AchievementRatio) || AchievementRatio <= 0.0f) { return 0; }

		const double ScaledPercent = static_cast<double>(AchievementRatio) * 100.0;
		if (ScaledPercent >= static_cast<double>(MAX_int32)) { return MAX_int32; }
		return static_cast<int32>(FMath::FloorToInt64(ScaledPercent));
	}

	/**
	 * 현재 로스터로 이 프로젝트를 했을 때의 직능별 예상 점수, 품질, 출시 게이트 전망.
	 * 산식 출처 = EmployeeBehaviorComponent 기여 경로. 몬테카를로가 아니라 기댓값 닫힌형이다.
	 * @param PerWorkerOutputScale 전원에게 똑같이 걸리는 산출 배수(건물 특성 등). 직원마다 다른 항은 FEstimateWorkerInput::OutputScale.
	 */
	static FProjectOutlookEstimate EstimateProjectOutlook(
		const TArray<FEstimateWorkerInput>& Workers, const TArray<int32>& Weights,
		int32 R1, int32 R2, int32 R3, float DurationSec, float PerWorkerOutputScale);

	/**
	 * 현재 Step 데이터 가져오기
	 */
	FStepRoundData* GetCurrentStepData()
	{
		int32 Index = CurrentStep - 1;
		if (Steps.IsValidIndex(Index))
		{
			return &Steps[Index];
		}
		return nullptr;
	}

	const FStepRoundData* GetCurrentStepData() const
	{
		int32 Index = CurrentStep - 1;
		if (Steps.IsValidIndex(Index))
		{
			return &Steps[Index];
		}
		return nullptr;
	}

	FStageProgressData()
		: ProjectNumber(1)
		, StageNumber(1)
		, ProjectID(0)
		, ProjectName(TEXT(""))
		, CompanyType(ECompanyType::None)
		, CurrentStep(0)
		, RemainingTime(15.0f)
		, bIsTimerRunning(false)
		, bIsManufacturing(false)
		, ActiveMode(EProjectMode::None)
		, TraitRewardMultiplier(1.0f)
		, EventRewardMultiplier(1.0f)
		, Step1AchievementRate(0.0f)
		, Step2AchievementRate(0.0f)
		, Step3AchievementRate(0.0f)
	{
		InitializeSteps();
	}
};
