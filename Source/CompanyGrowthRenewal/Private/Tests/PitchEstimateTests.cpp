#include "Misc/AutomationTest.h"
#include "Enum/QualityGrade.h"
#include "Data/StageProgressData.h"
#include "Data/EmployeeTypes.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRExpectedQualityTest,
	"CGR.Pitch.ExpectedQuality",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// 단일 직능 프로젝트 + 그 직능만 가진 직원 1명 = 배분이 100% 라 got 을 손으로 역산할 수 있다.
static FEstimateWorkerInput MakeSoloWorker(int32 Level, int32 Slot, int32 Points)
{
	FEstimateWorkerInput W;
	W.Level = Level;
	W.DisciplinePoints.Init(0, 6);
	W.DisciplinePoints[Slot] = Points;
	return W;
}

bool FCGRExpectedQualityTest::RunTest(const FString& Parameters)
{
	TArray<int32> Weights; Weights.Init(0, 6);
	Weights[1] = 5;                       // 개발 직능만 활성

	const int32 R1 = 60, R2 = 60, R3 = 60;
	const float TargetBase = FStageProgressData::ComputeDisciplineTargetBase(R1, R2, R3);
	const float Target = FStageProgressData::ComputeDisciplineTarget(5, 5, TargetBase);

	{
		TArray<FEstimateWorkerInput> Empty;
		const FProjectOutlookEstimate EmptyResult = FStageProgressData::EstimateProjectOutlook(
			Empty, Weights, R1, R2, R3, 15.f, 1.f);
		TestEqual(TEXT("빈 로스터는 하한"),
			EmptyResult.ExpectedQuality, 0.5f);
		TestFalse(TEXT("빈 로스터는 예측 불가"), EmptyResult.bIsValid);
		TestTrue(TEXT("예측 불가는 게이트 위험"), EmptyResult.bGateRisk);
		TestEqual(TEXT("직능 배열은 6칸"), EmptyResult.ExpectedDisciplineScores.Num(), 6);
	}
	{
		TArray<int32> ZeroW; ZeroW.Init(0, 6);
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 1, 10));
		const FProjectOutlookEstimate ZeroWeightResult = FStageProgressData::EstimateProjectOutlook(
			Roster, ZeroW, R1, R2, R3, 15.f, 1.f);
		TestEqual(TEXT("활성 직능 없으면 하한"),
			ZeroWeightResult.ExpectedQuality, 0.5f);
		TestFalse(TEXT("활성 직능 없으면 예측 불가"), ZeroWeightResult.bIsValid);
		TestTrue(TEXT("활성 직능 없으면 게이트 위험"), ZeroWeightResult.bGateRisk);
		TestEqual(TEXT("활성 직능 없어도 직능 배열은 6칸"),
			ZeroWeightResult.ExpectedDisciplineScores.Num(), 6);
		for (int32 DisciplineIndex = 0; DisciplineIndex < ZeroWeightResult.ExpectedDisciplineScores.Num(); ++DisciplineIndex)
		{
			TestEqual(*FString::Printf(TEXT("활성 직능 없는 예상 점수[%d]는 0"), DisciplineIndex),
				ZeroWeightResult.ExpectedDisciplineScores[DisciplineIndex], 0.0f);
		}
	}
	{
		// 목표를 정확히 채우는 Duration 을 역산해 넣으면 Q = 1.0 (이 스펙의 핵심 계약)
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 1, 10));
		const float Aff = UOfficeStageProgressManager::DisciplineAffinity(10);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval * (Aff * Aff / Aff);
		const float ExactDuration = Target / PerSec;
		const FProjectOutlookEstimate Outlook = FStageProgressData::EstimateProjectOutlook(
			Roster, Weights, R1, R2, R3, ExactDuration, 1.f);
		TestTrue(TEXT("목표 정확히 달성 = Q 1.0"), FMath::IsNearlyEqual(Outlook.ExpectedQuality, 1.0f, 0.01f));
		TestTrue(TEXT("유효한 로스터는 예측 가능"), Outlook.bIsValid);
		TestEqual(TEXT("유효 결과의 직능 배열은 6칸"), Outlook.ExpectedDisciplineScores.Num(), 6);
		if (Outlook.ExpectedDisciplineScores.IsValidIndex(1))
		{
			TestEqual(TEXT("활성 직능 예상 점수 반환"), Outlook.ExpectedDisciplineScores[1], Target, 0.001f);
		}
	}
	{
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 1, 10));
		const float Aff = UOfficeStageProgressManager::DisciplineAffinity(10);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval * Aff;
		const FProjectOutlookEstimate At49 = FStageProgressData::EstimateProjectOutlook(
			Roster, Weights, R1, R2, R3, Target * 0.49f / PerSec, 1.f);
		const FProjectOutlookEstimate At50 = FStageProgressData::EstimateProjectOutlook(
			Roster, Weights, R1, R2, R3, Target * 0.50f / PerSec, 1.f);
		TestTrue(TEXT("예상 점수 49%는 출시 위험"), At49.bGateRisk);
		TestFalse(TEXT("예상 점수 50%는 최소선 통과"), At50.bGateRisk);
		TestEqual(TEXT("49% 결과의 직능 배열은 6칸"), At49.ExpectedDisciplineScores.Num(), 6);
		TestEqual(TEXT("50% 결과의 직능 배열은 6칸"), At50.ExpectedDisciplineScores.Num(), 6);
		if (At49.ExpectedDisciplineScores.IsValidIndex(1))
		{
			TestEqual(TEXT("49% 예상 점수 반환"), At49.ExpectedDisciplineScores[1] / Target, 0.49f, 0.001f);
		}
		if (At50.ExpectedDisciplineScores.IsValidIndex(1))
		{
			TestEqual(TEXT("50% 예상 점수 반환"), At50.ExpectedDisciplineScores[1] / Target, 0.50f, 0.001f);
		}
	}
	TestEqual(TEXT("49.9% 표시는 49%로 내림"),
		FStageProgressData::ComputeDisciplinePercentDisplayValue(0.499f), 49);
	TestEqual(TEXT("50% 표시는 50%"),
		FStageProgressData::ComputeDisciplinePercentDisplayValue(0.5f), 50);
	TestEqual(TEXT("99.9% 표시는 99%로 내림"),
		FStageProgressData::ComputeDisciplinePercentDisplayValue(0.999f), 99);
	TestEqual(TEXT("100% 표시는 100%"),
		FStageProgressData::ComputeDisciplinePercentDisplayValue(1.0f), 100);
	TestEqual(TEXT("음수 달성률 표시는 0%"),
		FStageProgressData::ComputeDisciplinePercentDisplayValue(-0.01f), 0);
	TestEqual(TEXT("NaN 달성률 표시는 0%"),
		FStageProgressData::ComputeDisciplinePercentDisplayValue(
			std::numeric_limits<float>::quiet_NaN()), 0);
	{
		// 캡: 목표의 10배를 부어도 2.0 을 넘지 않는다
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 1, 10));
		const float Aff = UOfficeStageProgressManager::DisciplineAffinity(10);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval * Aff;
		const float Q = FStageProgressData::EstimateProjectOutlook(
			Roster, Weights, R1, R2, R3, (Target / PerSec) * 10.f, 1.f).ExpectedQuality;
		TestEqual(TEXT("상한 2.0 클램프"), Q, 2.0f);
	}
	{
		// 캡은 **직능별**이다 — 주력 직능을 목표의 4배로 채워도 방치 직능의 미달을 못 메운다.
		// 직능별 Min(,2.0) 이 빠지면 (4 + 0.16)/2 = 2.08 을 바깥 클램프가 2.0 으로 만들어 조용히 통과한다.
		TArray<int32> TwoW; TwoW.Init(0, 6); TwoW[0] = 5; TwoW[1] = 5;
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 0, 20));
		const float AffHi = UOfficeStageProgressManager::DisciplineAffinity(20);
		const float AffLo = UOfficeStageProgressManager::DisciplineAffinity(0);
		const float ShareHi = AffHi * AffHi / (AffHi + AffLo);
		const float ShareLo = AffLo * AffLo / (AffHi + AffLo);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval;
		const float OverDuration = 4.f * Target / (PerSec * ShareHi);
		const float ExpectedQ = (2.0f + 4.f * ShareLo / ShareHi) / 2.f;
		TestEqual(TEXT("캡은 직능별 — 초과분이 미달 직능을 못 메운다"),
			FStageProgressData::EstimateProjectOutlook(
				Roster, TwoW, R1, R2, R3, OverDuration, 1.f).ExpectedQuality,
			ExpectedQ, 0.001f);
	}
	{
		// weight 가중평균 + 목표의 가중 비례성. 단순평균이면 (1.0+0.2)/2 = 0.6,
		// 목표가 가중치에 비례하지 않으면 낮은 가중 직능 달성률이 0.2 가 아니라 0.04 가 된다 — 둘 다 어긋난다.
		TArray<int32> UnevenW; UnevenW.Init(0, 6); UnevenW[0] = 5; UnevenW[1] = 1;
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 0, 20));
		const float AffHi = UOfficeStageProgressManager::DisciplineAffinity(20);
		const float AffLo = UOfficeStageProgressManager::DisciplineAffinity(0);
		const float ShareHi = AffHi * AffHi / (AffHi + AffLo);
		const float ShareLo = AffLo * AffLo / (AffHi + AffLo);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval;
		const float TargetHi = FStageProgressData::ComputeDisciplineTarget(5, 5, TargetBase);
		const float TargetLo = FStageProgressData::ComputeDisciplineTarget(1, 5, TargetBase);
		const float ExactDuration = TargetHi / (PerSec * ShareHi);
		const float RateLo = (PerSec * ExactDuration * ShareLo) / TargetLo;
		const float ExpectedQ = (1.0f * 5 + RateLo * 1) / 6.f;
		TestEqual(TEXT("가중치 5:1 가중평균"),
			FStageProgressData::EstimateProjectOutlook(
				Roster, UnevenW, R1, R2, R3, ExactDuration, 1.f).ExpectedQuality,
			ExpectedQ, 0.001f);
	}
	{
		// PerWorkerOutputScale = 산출 선형 배수. 캡/하한이 안 닿는 구간이라 2배는 정확히 Q 2배여야 한다.
		TArray<FEstimateWorkerInput> Roster; Roster.Add(MakeSoloWorker(1, 1, 10));
		const float Aff = UOfficeStageProgressManager::DisciplineAffinity(10);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval * Aff;
		const float Duration = 0.75f * Target / PerSec;
		const float Q1 = FStageProgressData::EstimateProjectOutlook(
			Roster, Weights, R1, R2, R3, Duration, 1.f).ExpectedQuality;
		const float Q2 = FStageProgressData::EstimateProjectOutlook(
			Roster, Weights, R1, R2, R3, Duration, 2.f).ExpectedQuality;
		TestEqual(TEXT("산출 배수 2배 = Q 2배"), Q2, Q1 * 2.f, 0.001f);
	}
	{
		// OutputScale = 그 직원에게만 걸리는 배수. ⚠ 두 직원의 초당 산출이 서로 달라야 판별력이 생긴다 —
		// 똑같은 직원 둘에 배수만 [1,3] 이면 로스터 평균(2)으로 뭉개는 구현도 정확히 같은 값을 내 통과한다.
		const float Aff10 = UOfficeStageProgressManager::DisciplineAffinity(10);
		const float Aff30 = UOfficeStageProgressManager::DisciplineAffinity(30);
		const float PerSecUnit = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval;

		TArray<FEstimateWorkerInput> Mixed;
		Mixed.Add(MakeSoloWorker(1, 1, 10));
		Mixed.Add(MakeSoloWorker(1, 1, 30));
		Mixed[1].OutputScale = 3.f;

		// 목표를 정확히 채우는 Duration 을 역산 — 직원별로 곱하는 구현만 Q = 1.0
		const float Duration = Target / (PerSecUnit * (Aff10 + 3.f * Aff30));
		TestEqual(TEXT("직원별 배수가 그 직원 산출에만 곱해진다"),
			FStageProgressData::EstimateProjectOutlook(
				Mixed, Weights, R1, R2, R3, Duration, 1.f).ExpectedQuality, 1.0f, 0.001f);

		// 판별력 자체를 잰다 — 배수를 평균으로 접는 구현이 위 기대값과 갈리는지(=위 단언이 계약을 지키는지)
		TArray<FEstimateWorkerInput> Averaged = Mixed;
		Averaged[0].OutputScale = 2.f;
		Averaged[1].OutputScale = 2.f;
		TestFalse(TEXT("배수를 평균으로 접으면 같은 값이 안 나온다"),
			FMath::IsNearlyEqual(
				FStageProgressData::EstimateProjectOutlook(
					Averaged, Weights, R1, R2, R3, Duration, 1.f).ExpectedQuality,
				1.0f, 0.01f));
	}
	{
		// 2직능 프로젝트, 총 포인트는 같고 배분만 다른 두 직원.
		TArray<int32> TwoW; TwoW.Init(0, 6); TwoW[0] = 5; TwoW[1] = 5;
		FEstimateWorkerInput Spec; Spec.Level = 1; Spec.DisciplinePoints.Init(0, 6);
		Spec.DisciplinePoints[0] = 20;
		FEstimateWorkerInput Flat; Flat.Level = 1; Flat.DisciplinePoints.Init(0, 6);
		Flat.DisciplinePoints[0] = 10; Flat.DisciplinePoints[1] = 10;
		TArray<FEstimateWorkerInput> A; A.Add(Spec);
		TArray<FEstimateWorkerInput> B; B.Add(Flat);

		// 전문화의 대가 = 방치한 직능이 최소선(목표 절반)에 못 미쳐 출시 게이트가 막힌다.
		TestTrue(TEXT("전문화는 방치 직능이 게이트에 걸린다"),
			FStageProgressData::EstimateProjectOutlook(
				A, TwoW, R1, R2, R3, 15.f, 1.f).bGateRisk);
		TestFalse(TEXT("균등 배치는 게이트 통과"),
			FStageProgressData::EstimateProjectOutlook(
				B, TwoW, R1, R2, R3, 15.f, 1.f).bGateRisk);

		// 전문화의 이득 = affinity 제곱 배분이라 총 산출 자체가 크다. 배분이 선형(affinity/Σ)이면 두 직원의
		// 총 산출이 같아져 이 비교가 뒤집힌다. 직능당 캡(2.0)/Q 하한(0.5)이 총량을 가리지 않는 구간에서 잰다.
		const float AffHi = UOfficeStageProgressManager::DisciplineAffinity(20);
		const float AffLo = UOfficeStageProgressManager::DisciplineAffinity(0);
		const float SpecShare = AffHi * AffHi / (AffHi + AffLo);
		const float PerSec = UEmployeeTypeHelper::CalculateBaseOutput(1)
			* UEmployeeBehaviorComponent::ScoreNormInterval;
		const float UncappedDuration = 1.75f * Target / (PerSec * SpecShare);
		TestTrue(TEXT("전문화가 총 산출은 높다 (affinity 제곱)"),
			FStageProgressData::EstimateProjectOutlook(
				A, TwoW, R1, R2, R3, UncappedDuration, 1.f).ExpectedQuality
			> FStageProgressData::EstimateProjectOutlook(
				B, TwoW, R1, R2, R3, UncappedDuration, 1.f).ExpectedQuality);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRQualityRevenueMultTest,
	"CGR.Pitch.QualityRevenueMult",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRQualityRevenueMultTest::RunTest(const FString& Parameters)
{
	// Leverage 1.0 = 등급 배율 그대로
	TestEqual(TEXT("C 는 기준 1.0"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::C, 1.0f), 1.0f);
	TestEqual(TEXT("S 는 2.0"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::S, 1.0f), 2.0f);
	// Leverage 1.5(게임) = 등급 차이 증폭
	TestEqual(TEXT("게임 산업 S 는 2.5"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::S, 1.5f), 2.5f);
	// Leverage 0.6(IT) = 압축
	TestTrue(TEXT("IT S 는 1.6"),
		FMath::IsNearlyEqual(UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::S, 0.6f), 1.6f));
	// C 는 Leverage 무관 항상 1.0 (기준점이므로)
	TestEqual(TEXT("C 는 Leverage 무관"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::C, 1.5f), 1.0f);

	// ===== 수익 안정성(RevenueStability) — 낮은 등급을 그 산업의 S 쪽으로 당긴다 =====

	// 기본 인자 = 안정성 0. 이게 깨지면 특성 없는 플레이어의 경제가 조용히 바뀐다.
	TestEqual(TEXT("기본 인자는 안정성 0 과 같다"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::B, 1.0f),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::B, 1.0f, 0.0f));

	// S 는 이미 상단이라 당길 곳이 없다 — 어떤 안정성에서도 무효과
	TestEqual(TEXT("S 는 안정성 무효과(5%)"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::S, 1.5f, 0.05f), 2.5f);
	TestEqual(TEXT("S 는 안정성 무효과(100%)"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::S, 1.5f, 1.0f), 2.5f);

	// 게임 산업(L=1.5) Mythic 5% → C 1.0 이 1.075 로
	TestTrue(TEXT("게임 Mythic 5% — C 1.0 -> 1.075"),
		FMath::IsNearlyEqual(
			UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::C, 1.5f, 0.05f), 1.075f));

	// 중립 leverage B 등급 50% → 1.2 와 2.0 의 중점
	TestTrue(TEXT("B 50% 는 상단과의 중점"),
		FMath::IsNearlyEqual(
			UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::B, 1.0f, 0.5f), 1.6f));

	// 100% 면 상단과 같아지고, 그 위는 클램프
	TestEqual(TEXT("안정성 100% = 그 산업 상단"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::C, 1.5f, 1.0f), 2.5f);
	TestEqual(TEXT("100% 초과는 상단에서 멈춘다"),
		UProjectOperationManager::ComputeQualityRevenueMult(EQualityGrade::C, 1.5f, 2.0f), 2.5f);

	// 단조 증가 = "안정성 특성이 수익을 깎지 않는다". 등급×leverage 전 조합을 훑는다 —
	// Lerp 의 두 끝을 뒤집는 실수(상단↔현재)는 이 루프에서만 잡힌다.
	{
		const EQualityGrade AllGrades[] = { EQualityGrade::F, EQualityGrade::D, EQualityGrade::C,
			EQualityGrade::B, EQualityGrade::A, EQualityGrade::S };
		const float AllLeverages[] = { 0.0f, 0.6f, 1.0f, 1.5f };
		bool bMonotone = true;
		bool bNeverBelowBase = true;
		for (const EQualityGrade G : AllGrades)
		{
			for (const float L : AllLeverages)
			{
				const float Base = UProjectOperationManager::ComputeQualityRevenueMult(G, L, 0.0f);
				float Prev = Base;
				for (int32 i = 1; i <= 10; ++i)
				{
					const float Cur = UProjectOperationManager::ComputeQualityRevenueMult(G, L, i * 0.1f);
					if (Cur < Prev - 1e-6f)  { bMonotone = false; }
					if (Cur < Base - 1e-6f)  { bNeverBelowBase = false; }
					Prev = Cur;
				}
			}
		}
		TestTrue(TEXT("안정성은 단조 증가"), bMonotone);
		TestTrue(TEXT("안정성은 어떤 등급에서도 손해가 아니다"), bNeverBelowBase);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRDecayIntegralTest,
	"CGR.Pitch.DecayIntegral",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRDecayIntegralTest::RunTest(const FString& Parameters)
{
	// 닫힌 식을 그대로 다시 적으면 오타를 못 잡는다 — 감쇠 곡선을 수치적분해 독립 검산한다.
	auto NumericAverage = [](float HalfLifeFrac)
	{
		constexpr int32 N = 100000;
		double Sum = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const double NormT = (static_cast<double>(i) + 0.5) / N;   // t/T
			Sum += FMath::Pow(0.5, NormT / HalfLifeFrac);
		}
		return static_cast<float>(Sum / N);
	};

	// DT_IndustryProfile 실측 폭 — 게임 0.28(빨리 식음) / 중립 0.5 / IT 0.85(긴 꼬리)
	const float Cases[] = { 0.28f, 0.5f, 0.85f };
	for (const float HLF : Cases)
	{
		TestEqual(*FString::Printf(TEXT("HLF %.2f 닫힌식 = 수치적분"), HLF),
			UProjectOperationManager::ComputeDecayIntegralPerSec(HLF), NumericAverage(HLF), 0.001f);
	}

	// DT 결손(0/음수)은 중립 0.5 로 폴백 — 0 나눗셈/무한대 방지
	TestEqual(TEXT("0 이하는 0.5 폴백"),
		UProjectOperationManager::ComputeDecayIntegralPerSec(0.0f),
		UProjectOperationManager::ComputeDecayIntegralPerSec(0.5f));

	// 감쇠는 반드시 총수익을 깎는다 — 1.0 이 되면 결과 화면이 다시 피크 기준으로 부풀어 카드와 갈린다
	TestTrue(TEXT("게임(0.28) 감쇠 적분은 0.4 미만"),
		UProjectOperationManager::ComputeDecayIntegralPerSec(0.28f) < 0.4f);
	TestTrue(TEXT("긴 꼬리(0.85)도 1.0 미만"),
		UProjectOperationManager::ComputeDecayIntegralPerSec(0.85f) < 1.0f);

	// 반감기가 길수록 누적이 크다 (곡선 모양이 뒤집히면 여기서 터진다)
	TestTrue(TEXT("반감기가 길수록 누적이 크다"),
		UProjectOperationManager::ComputeDecayIntegralPerSec(0.28f)
		< UProjectOperationManager::ComputeDecayIntegralPerSec(0.85f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRLaunchRevenueAssemblyTest,
	"CGR.Pitch.LaunchRevenueAssembly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRLaunchRevenueAssemblyTest::RunTest(const FString& Parameters)
{
	// 게임 산업 예시 — Base 50(프로젝트 5) / PeakMult 2.2 / B등급 ReviewLeverage 1.5 = 1.375 / 운영 15분
	const float Base = 50.0f, PeakMult = 2.2f, QualityMult = 1.375f, OpSec = 900.0f;

	// 항 목록을 리터럴로 못박는다 — 피크·트렌드·품질 중 어느 항이 빠져도 여기서 터진다
	TestEqual(TEXT("피크 초당 = 규모 x 피크 x 트렌드 x 품질"),
		UProjectOperationManager::ComputePeakRevenuePerSec(Base, PeakMult, 1.0f, QualityMult), 151.25f, 0.001f);

	// 트렌드 항은 과거 추정부에만 있었다 — 결과 화면 조립식에서 빠지면 여기서 터진다
	TestEqual(TEXT("트렌드 1.5배 = 총수익 1.5배"),
		UProjectOperationManager::ComputeLaunchRevenue(Base, PeakMult, 1.5f, QualityMult, OpSec, 0.28f),
		UProjectOperationManager::ComputeLaunchRevenue(Base, PeakMult, 1.0f, QualityMult, OpSec, 0.28f) * 1.5f, 1.0f);

	// 총수익의 의미 = "피크 초당 수익을 런타임과 같은 감쇠 곡선(0.5^(t/(T·HLF)))을 따라 T 만큼 적분한 값".
	// 조립식을 다시 적지 않고 곡선을 직접 적분해 비교한다 — 감쇠나 운영시간이 빠지면 빨간불.
	auto IntegrateOperation = [](float PeakRps, float Seconds, float HalfLifeFrac)
	{
		constexpr int32 N = 20000;
		const double DecayHalf = static_cast<double>(Seconds) * HalfLifeFrac;
		double Sum = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const double T = (static_cast<double>(i) + 0.5) * Seconds / N;
			Sum += PeakRps * FMath::Pow(0.5, T / DecayHalf);
		}
		return static_cast<float>(Sum * Seconds / N);
	};

	const float PeakRps = UProjectOperationManager::ComputePeakRevenuePerSec(Base, PeakMult, 1.0f, QualityMult);
	const float HalfLives[] = { 0.28f, 0.85f };
	for (const float HLF : HalfLives)
	{
		const float Total = UProjectOperationManager::ComputeLaunchRevenue(Base, PeakMult, 1.0f, QualityMult, OpSec, HLF);

		TestEqual(*FString::Printf(TEXT("HLF %.2f 총수익 = 감쇠 곡선 적분"), HLF),
			Total, IntegrateOperation(PeakRps, OpSec, HLF), PeakRps * OpSec * 0.001f);

		// 감쇠가 빠지면 총수익 = 피크 x 시간 이 된다 — 결과 화면이 부풀어 있던 정확한 형태
		TestTrue(*FString::Printf(TEXT("HLF %.2f 총수익 < 피크x시간"), HLF), Total < PeakRps * OpSec * 0.99f);
	}

	return true;
}

#endif
