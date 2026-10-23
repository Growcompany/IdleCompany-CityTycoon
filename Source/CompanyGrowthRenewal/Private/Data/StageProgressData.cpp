#include "Data/StageProgressData.h"

// 추정기가 런타임 기여 경로와 같은 상수/공식을 읽으려는 include. 헤더에 두면 매니저 헤더와 순환이 된다.
#include "Data/EmployeeTypes.h"
#include "Data/FatigueConfig.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Manager/OfficeStageProgressManager.h"

namespace
{
	struct FWorkWindow
	{
		float WeightedSeconds = 0.f;
		float SquaredWeightedSeconds = 0.f;
		float BoltTimeVariance = 0.f;
	};

	FWorkWindow EstimateWorkWindow(const FEstimateWorkerInput& Worker, float DurationSec, const UFatigueConfig& Config,
		bool bSuppressEquipmentDown)
	{
		FWorkWindow Window;
		float Time = 0.f;
		float Fatigue = FMath::Clamp(Worker.InitialFatigue, 0.f, 100.f);
		const float Gain = Config.GainPerSec * FMath::Max(0.f, Worker.FatigueGainScale) /
			(1.f + FMath::Max(0, Worker.EffectiveStamina) * Config.StaminaGainScale);
		const float SlackThreshold = FMath::Clamp(Config.SlackingThreshold, 0.f, 100.f);
		while (Time < DurationSec)
		{
			if (!bSuppressEquipmentDown && Fatigue >= SlackThreshold)
			{
				const float Telegraph = FMath::Min(Config.SlackTelegraphDuration, DurationSec - Time);
				Time += FMath::Max(0.f, Telegraph);
				const float Slump = FMath::Min(Config.SlackSlumpDuration, DurationSec - Time);
				Time += FMath::Max(0.f, Slump);
				Fatigue = FMath::Max(0.f, Fatigue - Config.SlumpRecoverPerSec * FMath::Max(0.f, Slump));
				if (Telegraph + Slump <= 0.f) { break; }
				continue;
			}
			const float Step = FMath::Min(0.25f, DurationSec - Time);
			const float MidFatigue = FMath::Min(100.f, Fatigue + Gain * Step * 0.5f);
			const float TiredFraction = FMath::Clamp((MidFatigue - Config.TiredThreshold) /
				FMath::Max(1.f, 100.f - Config.TiredThreshold), 0.f, 1.f);
			const float Factor = FMath::Max(0.f, 1.f - Config.MaxFatiguePenalty *
				Worker.FatiguePenaltyScale * TiredFraction);
			Window.WeightedSeconds += Factor * Step;
			Window.SquaredWeightedSeconds += Factor * Factor * Step;
			Fatigue = FMath::Min(100.f, Fatigue + Gain * Step);
			Time += Step;
		}
		const float HalfPoint = FMath::Max(1.f, Config.MaxBoltReduction /
			FMath::Max(KINDA_SMALL_NUMBER, Config.ComposureBoltScale));
		const float Composure = FMath::Max(0, Worker.EffectiveComposure);
		const float Reduction = Config.MaxBoltReduction * Composure / (Composure + HalfPoint);
		const float BoltChance = bSuppressEquipmentDown ? 0.f : FMath::Clamp(
			Config.BoltChancePerSec * (1.f - Reduction) * Worker.BoltChanceScale, 0.f, 1.f);
		// 초당 독립 굴림과 고정 복귀 시간의 장기 가동률. 화면 밖/수동 캐치는 보수적으로 제외한다.
		const float BoltDuration = FMath::Max(0.f, Config.BoltDuration * Worker.BoltDurationScale);
		const float Availability = 1.f / (1.f + BoltChance * BoltDuration);
		const float AverageFatigueFactor = Window.WeightedSeconds / FMath::Max(DurationSec, KINDA_SMALL_NUMBER);
		// 독립 초당 폭주 횟수의 흔들림. 인원/시간이 늘수록 상대 오차는 자연스럽게 작아진다.
		Window.BoltTimeVariance = BoltChance * FMath::Square(BoltDuration) * DurationSec *
			FMath::Cube(Availability) * FMath::Square(AverageFatigueFactor);
		Window.WeightedSeconds *= Availability;
		Window.SquaredWeightedSeconds *= Availability;
		return Window;
	}

	/**
	 * 직원별 기대 기여를 직능 슬롯에 누적. false = 추정 불가(로스터/직능/시간 없음).
	 *
	 * 1회 방출 점수에 Overflow(=SpeedFactor x WorkInterval)가 곱해지고 방출 빈도가 1/WorkInterval 이라
	 * 케이던스가 상쇄된다 → 초당 = BaseOutput x affinity x ScoreNormInterval x SpeedFactor.
	 * 직능 배분이 affinity 제곱인 이유 = 선택 확률이 affinity 비례인 데다 점수에 그 affinity 가 다시 곱해진다.
	 */
	bool AccumulateExpectedScores(
		const TArray<FEstimateWorkerInput>& Workers, const TArray<int32>& Weights,
		float DurationSec, float PerWorkerOutputScale, const UFatigueConfig* FatigueConfig,
		bool bSuppressEquipmentDown,
		TArray<float>& OutGot)
	{
		OutGot.Init(0.f, Weights.Num());
		TArray<float> ScoreVariance;
		ScoreVariance.Init(0.f, Weights.Num());
		if (Workers.Num() == 0 || DurationSec <= 0.f) { return false; }

		bool bAnyActive = false;
		for (int32 W : Weights) { if (W > 0) { bAnyActive = true; break; } }
		if (!bAnyActive) { return false; }

		for (const FEstimateWorkerInput& Worker : Workers)
		{
			const FWorkWindow Window = FatigueConfig ? EstimateWorkWindow(Worker, DurationSec, *FatigueConfig,
				bSuppressEquipmentDown)
				: FWorkWindow{DurationSec, DurationSec};
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

			const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(Worker.Level)
				* UEmployeeBehaviorComponent::ScoreNormInterval * PerWorkerOutputScale * Worker.OutputScale;
			const float CritChance = FMath::Clamp(Worker.CriticalChance, 0.f, 1.f);
			const float CritMult = FMath::Max(1.f, Worker.CriticalMultiplier);
			const float CritMean = 1.f + CritChance * (CritMult - 1.f);
			const float CritSecond = 1.f + CritChance * (CritMult * CritMult - 1.f);
			const float WorkInterval = FMath::Clamp(UEmployeeBehaviorComponent::BaseWorkInterval /
				FMath::Max(0.01f, Worker.WorkSpeedFactor), UEmployeeBehaviorComponent::MinWorkInterval,
				UEmployeeBehaviorComponent::BaseWorkInterval);
			for (int32 i = 0; i < Weights.Num(); ++i)
			{
				if (Weights[i] <= 0) { continue; }
				const float PickProb = Aff[i] / SumAff;
				OutGot[i] += PerSec * Window.WeightedSeconds * PickProb * Aff[i] * CritMean;
				if (FatigueConfig)
				{
					// 랜덤 팩터, 직능 선택, 크리의 히트별 분산을 합산한다.
					const float PerHit = PerSec * WorkInterval * Aff[i];
					const float VariancePerHit = FMath::Max(0.f,
						PickProb * CritSecond * Worker.RandomFactorSecondMoment -
						FMath::Square(PickProb * CritMean));
					ScoreVariance[i] += PerHit * PerHit * Window.SquaredWeightedSeconds /
						WorkInterval * VariancePerHit;
					ScoreVariance[i] += FMath::Square(PerSec * PickProb * Aff[i] * CritMean) *
						Window.BoltTimeVariance;
				}
			}
		}
		if (FatigueConfig)
		{
			for (int32 i = 0; i < OutGot.Num(); ++i)
			{
				OutGot[i] = FMath::Max(0.f, OutGot[i] - FMath::Sqrt(ScoreVariance[i]));
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
		const UFatigueConfig* FatigueConfig, bool bSuppressEquipmentDown,
		FStageProgressData& OutPreview, TArray<float>& OutGot)
	{
		if (!AccumulateExpectedScores(Workers, Weights, DurationSec, PerWorkerOutputScale, FatigueConfig,
			bSuppressEquipmentDown, OutGot))
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
	int32 R1, int32 R2, int32 R3, float DurationSec, float PerWorkerOutputScale,
	const UFatigueConfig* FatigueConfig, bool bSuppressEquipmentDown)
{
	FProjectOutlookEstimate Result;
	Result.ExpectedDisciplineScores.Init(0.f, Weights.Num());

	FStageProgressData Preview;
	if (!BuildPreviewProgress(
		Workers, Weights, R1, R2, R3, DurationSec, PerWorkerOutputScale, FatigueConfig, bSuppressEquipmentDown,
		Preview, Result.ExpectedDisciplineScores))
	{
		return Result;
	}

	Result.bIsValid = true;
	Result.ExpectedQuality = Preview.CalculateQualityScore();
	Result.bGateRisk = !Preview.MeetsMinimumClearScore();
	return Result;
}
