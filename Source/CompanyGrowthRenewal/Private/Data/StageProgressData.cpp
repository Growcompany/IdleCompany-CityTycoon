#include "Data/StageProgressData.h"

// 추정기가 런타임 기여 경로와 같은 상수/공식을 읽으려는 include. 헤더에 두면 매니저 헤더와 순환이 된다.
#include "Data/EmployeeTypes.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Manager/OfficeStageProgressManager.h"

namespace
{
	/**
	 * 직원별 기대 기여를 직능 슬롯에 누적. false = 추정 불가(로스터/직능/시간 없음).
	 *
	 * 1회 방출 점수에 Overflow(=SpeedFactor x WorkInterval)가 곱해지고 방출 빈도가 1/WorkInterval 이라
	 * 케이던스가 상쇄된다 → 초당 = BaseOutput x affinity x ScoreNormInterval x SpeedFactor.
	 * 직능 배분이 affinity 제곱인 이유 = 선택 확률이 affinity 비례인 데다 점수에 그 affinity 가 다시 곱해진다.
	 *
	 * 업무집중도 p 가 있으면 선택 확률만 바뀐다(점수에 곱해지는 affinity 는 그대로):
	 *   주 직능 : (p + (1-p) x Aff_j/SumAff) x Aff_j
	 *   그 외   :      (1-p) x Aff_i/SumAff  x Aff_i
	 */
	bool AccumulateExpectedScores(
		const TArray<FEstimateWorkerInput>& Workers, const TArray<int32>& Weights,
		float DurationSec, float PerWorkerOutputScale, TArray<float>& OutGot)
	{
		OutGot.Init(0.f, Weights.Num());
		if (Workers.Num() == 0 || DurationSec <= 0.f) { return false; }

		bool bAnyActive = false;
		for (int32 W : Weights) { if (W > 0) { bAnyActive = true; break; } }
		if (!bAnyActive) { return false; }

		for (const FEstimateWorkerInput& Worker : Workers)
		{
			float SumAff = 0.f;
			TArray<float> Aff;
			Aff.Init(0.f, Weights.Num());
			for (int32 i = 0; i < Weights.Num(); ++i)
			{
				if (Weights[i] <= 0) { continue; }
				const int32 Pts = Worker.DisciplinePoints.IsValidIndex(i) ? Worker.DisciplinePoints[i] : 0;
				Aff[i] = UOfficeStageProgressManager::DisciplineAffinity(Pts);
				SumAff += Aff[i];
			}
			if (SumAff <= 0.f) { continue; }

			// 주 직능이 이번 프로젝트에서 비활성이면 기여 경로도 강제 지정을 포기한다 — 같은 조건으로 p 를 무효화
			// argmax 는 비활성 직능까지 포함해 구해야 GetPrimaryDisciplineSlot 과 같은 답이 나온다
			int32 PrimarySlot = INDEX_NONE, PrimaryPts = 0;
			for (int32 i = 0; i < Worker.DisciplinePoints.Num(); ++i)
			{
				if (Worker.DisciplinePoints[i] > PrimaryPts) { PrimaryPts = Worker.DisciplinePoints[i]; PrimarySlot = i; }
			}
			const float Focus = (PrimarySlot != INDEX_NONE && Weights.IsValidIndex(PrimarySlot) && Weights[PrimarySlot] > 0)
				? FMath::Clamp(Worker.FocusChance, 0.f, 1.f) : 0.f;

			const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(Worker.Level)
				* UEmployeeBehaviorComponent::ScoreNormInterval * PerWorkerOutputScale * Worker.OutputScale;
			for (int32 i = 0; i < Weights.Num(); ++i)
			{
				if (Weights[i] <= 0) { continue; }
				const float PickProb = (1.f - Focus) * (Aff[i] / SumAff) + (i == PrimarySlot ? Focus : 0.f);
				OutGot[i] += PerSec * DurationSec * PickProb * Aff[i];
			}
		}
		return true;
	}

	/**
	 * 착수 때와 같은 경로로 스텝을 만들고 기대 점수를 채워 넣은 "가상 진행 데이터".
	 * 집계와 게이트 판정을 실물 함수(CalculateQualityScore / MeetsMinimumClearScore)에 그대로 위임하려는 것 —
	 * 카드용 식을 따로 두면 캡·클램프·최소선을 리튠할 때 카드만 옛 값을 쓴다.
	 */
	bool BuildPreviewProgress(
		const TArray<FEstimateWorkerInput>& Workers, const TArray<int32>& Weights,
		int32 R1, int32 R2, int32 R3, float DurationSec, float PerWorkerOutputScale,
		FStageProgressData& OutPreview, TArray<float>& OutGot)
	{
		if (!AccumulateExpectedScores(Workers, Weights, DurationSec, PerWorkerOutputScale, OutGot))
		{
			return false;
		}

		OutPreview.InitializeDisciplineSteps(
			Weights, FStageProgressData::ComputeDisciplineTargetBase(R1, R2, R3));
		for (FStepRoundData& S : OutPreview.Steps)
		{
			if (OutGot.IsValidIndex(S.DisciplineSlot)) { S.AcquiredScore = OutGot[S.DisciplineSlot]; }
		}
		return true;
	}
}

FProjectOutlookEstimate FStageProgressData::EstimateProjectOutlook(
	const TArray<FEstimateWorkerInput>& Workers, const TArray<int32>& Weights,
	int32 R1, int32 R2, int32 R3, float DurationSec, float PerWorkerOutputScale)
{
	FProjectOutlookEstimate Result;
	Result.ExpectedDisciplineScores.Init(0.f, Weights.Num());

	FStageProgressData Preview;
	if (!BuildPreviewProgress(
		Workers, Weights, R1, R2, R3, DurationSec, PerWorkerOutputScale,
		Preview, Result.ExpectedDisciplineScores))
	{
		return Result;
	}

	Result.bIsValid = true;
	Result.ExpectedQuality = Preview.CalculateQualityScore();
	Result.bGateRisk = !Preview.MeetsMinimumClearScore();
	return Result;
}
