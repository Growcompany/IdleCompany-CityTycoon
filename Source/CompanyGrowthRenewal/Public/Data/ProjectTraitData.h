#pragma once

#include "CoreMinimal.h"
#include "Enum/ProjectTrait.h"
#include "ProjectTraitData.generated.h"

/**
 * 프로젝트 특성 데이터
 * 특성 enum → 수치 변경 매핑
 * MakeTraitData() 팩토리 함수로 enum에서 생성
 */
USTRUCT(BlueprintType)
struct FProjectTraitData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	EProjectTrait Trait = EProjectTrait::None;

	// 타이머 시간 오버라이드 (-1 = 기본값 15초 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Timer")
	float TimerDurationOverride = -1.0f;

	// 요구 점수 배율 (1.0 = 기본)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Score")
	float RequiredScoreMultiplier = 1.0f;

	// 보상 배율 (1.0 = 기본)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Reward")
	float RewardMultiplier = 1.0f;

	// 카테고리 가중치 (-1 = 기본값 사용: 기획20/개발50/QA30)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Weight")
	float PlanningWeight = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Weight")
	float DevWeight = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Weight")
	float QAWeight = -1.0f;

	// 크리티컬 배율 오버라이드 (-1 = 기본값 2.0 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Critical")
	float CriticalMultiplierOverride = -1.0f;

	// 크리티컬 확률 배율 (1.0 = 기본)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Critical")
	float CriticalChanceMultiplier = 1.0f;

	// 랜덤 변동 범위 (기본 0.8~1.2)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Random")
	float RandomVarianceMin = 0.8f;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Random")
	float RandomVarianceMax = 1.2f;

	// 기본 점수 배율 (1.0 = 기본, 0.8 = -20%)
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Score")
	float BaseScoreMultiplier = 1.0f;

	// 표시용 이름 및 설명
	UPROPERTY(BlueprintReadOnly, Category = "Trait|Display")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Display")
	FText Description;

	FProjectTraitData()
		: Trait(EProjectTrait::None)
		, TimerDurationOverride(-1.0f)
		, RequiredScoreMultiplier(1.0f)
		, RewardMultiplier(1.0f)
		, PlanningWeight(-1.0f)
		, DevWeight(-1.0f)
		, QAWeight(-1.0f)
		, CriticalMultiplierOverride(-1.0f)
		, CriticalChanceMultiplier(1.0f)
		, RandomVarianceMin(0.8f)
		, RandomVarianceMax(1.2f)
		, BaseScoreMultiplier(1.0f)
	{}

	/**
	 * 특성 enum에서 수치 데이터 생성 (팩토리)
	 * 모든 밸런스 수치가 여기 집중 관리됨
	 */
	static FProjectTraitData MakeTraitData(EProjectTrait InTrait)
	{
		FProjectTraitData Data;
		Data.Trait = InTrait;
		Data.DisplayName = FText::FromString(ProjectTraitToString(InTrait));

		switch (InTrait)
		{
		case EProjectTrait::RushDelivery:
			Data.TimerDurationOverride = 10.0f;
			Data.RewardMultiplier = 1.5f;
			Data.Description = FText::FromString(TEXT("시간은 줄지만 보상은 늘어납니다"));
			break;

		case EProjectTrait::LargeProject:
			Data.TimerDurationOverride = 25.0f;
			Data.RequiredScoreMultiplier = 2.0f;
			Data.RewardMultiplier = 3.0f;
			Data.Description = FText::FromString(TEXT("시간과 난이도가 높지만 보상이 매우 큽니다"));
			break;

		case EProjectTrait::DevFocus:
			Data.PlanningWeight = 0.1f;
			Data.DevWeight = 0.8f;
			Data.QAWeight = 0.1f;
			Data.Description = FText::FromString(TEXT("개발 부서 기여도가 압도적으로 높습니다"));
			break;

		case EProjectTrait::QAFocus:
			Data.PlanningWeight = 0.1f;
			Data.DevWeight = 0.1f;
			Data.QAWeight = 0.8f;
			Data.Description = FText::FromString(TEXT("QA 부서 기여도가 압도적으로 높습니다"));
			break;

		case EProjectTrait::PlanningFocus:
			Data.PlanningWeight = 0.8f;
			Data.DevWeight = 0.1f;
			Data.QAWeight = 0.1f;
			Data.Description = FText::FromString(TEXT("기획 부서 기여도가 압도적으로 높습니다"));
			break;

		case EProjectTrait::InnovationRequired:
			Data.CriticalMultiplierOverride = 3.0f;
			Data.BaseScoreMultiplier = 0.8f;
			Data.Description = FText::FromString(TEXT("기본 점수는 낮지만 크리티컬 효과가 큽니다"));
			break;

		case EProjectTrait::StabilityOriented:
			Data.RandomVarianceMin = 1.0f;
			Data.RandomVarianceMax = 1.0f;
			Data.RewardMultiplier = 1.3f;
			Data.Description = FText::FromString(TEXT("변동 없이 안정적으로 진행됩니다"));
			break;

		case EProjectTrait::HighRiskHighReturn:
			Data.RequiredScoreMultiplier = 1.5f;
			Data.RewardMultiplier = 2.0f;
			Data.Description = FText::FromString(TEXT("요구 점수가 높지만 보상도 큽니다"));
			break;

		case EProjectTrait::TrendRiding:
			Data.CriticalChanceMultiplier = 2.0f;
			Data.RewardMultiplier = 1.5f;
			Data.Description = FText::FromString(TEXT("트렌드를 탔습니다! 크리티컬 확률과 보상 증가"));
			break;

		case EProjectTrait::ClientPressure:
			Data.TimerDurationOverride = 12.0f;
			Data.RewardMultiplier = 1.8f;
			Data.Description = FText::FromString(TEXT("클라이언트가 재촉합니다. 시간 부담 대신 높은 보상"));
			break;

		case EProjectTrait::BudgetCut:
			Data.BaseScoreMultiplier = 0.85f;
			Data.RewardMultiplier = 1.4f;
			Data.Description = FText::FromString(TEXT("예산이 줄어 기본 점수가 낮지만 효율적 보상"));
			break;

		case EProjectTrait::StrictQA:
			Data.RequiredScoreMultiplier = 1.3f;
			Data.RewardMultiplier = 1.5f;
			Data.QAWeight = -1.0f;
			Data.Description = FText::FromString(TEXT("검수 기준이 높아 통과가 어렵지만 보상 증가"));
			break;

		// ────────────────────────────────────────
		// 도메인 트레이트 (프로젝트 데이터로부터 자동 부여)
		// 부서 가중치 프리셋 — 보상 / 시간은 변경하지 않음
		// ────────────────────────────────────────

		// 금융 — 규제심사형 (결제/은행/대출): QA(인사팀) 비중 높음
		case EProjectTrait::RegulatoryDomain:
			Data.PlanningWeight = 0.2f;
			Data.DevWeight = 0.4f;
			Data.QAWeight = 0.4f;
			Data.Description = FText::FromString(TEXT("규제 심사 통과가 핵심. 법무 / 컴플라이언스 비중 높음"));
			break;

		// 금융 — 성과검증형 (투자/자산관리): 기획 + 개발 양쪽 비중
		case EProjectTrait::PerformanceDomain:
			Data.PlanningWeight = 0.4f;
			Data.DevWeight = 0.4f;
			Data.QAWeight = 0.2f;
			Data.Description = FText::FromString(TEXT("투자 전략 설계와 모델 구현 비중 큼"));
			break;

		// 금융 — 약관승인형 (보험/연금): 기획 비중 압도적
		case EProjectTrait::InsuranceDomain:
			Data.PlanningWeight = 0.5f;
			Data.DevWeight = 0.2f;
			Data.QAWeight = 0.3f;
			Data.Description = FText::FromString(TEXT("상품 설계가 핵심. 약관 작성 / 승인 절차 비중 큼"));
			break;

		// 금융 — 기술감사형 (가상자산/블록체인): 개발 비중 압도적
		case EProjectTrait::AuditDomain:
			Data.PlanningWeight = 0.2f;
			Data.DevWeight = 0.5f;
			Data.QAWeight = 0.3f;
			Data.Description = FText::FromString(TEXT("스마트컨트랙트 구현 비중 큼. 보안 감사 동반"));
			break;

		// IT — 일반SW형 (웹/앱/엔터프라이즈): 게임과 동일 가중치
		case EProjectTrait::ITGeneralDomain:
			Data.PlanningWeight = 0.2f;
			Data.DevWeight = 0.5f;
			Data.QAWeight = 0.3f;
			Data.Description = FText::FromString(TEXT("표준 SW 개발. 기획 / 개발 / QA 균형"));
			break;

		// IT — AI ML형 (ML/DL/LLM): 개발(모델학습) 비중 압도적
		case EProjectTrait::ITAIDomain:
			Data.PlanningWeight = 0.2f;
			Data.DevWeight = 0.6f;
			Data.QAWeight = 0.2f;
			Data.Description = FText::FromString(TEXT("모델 학습이 핵심. 데이터 / 컴퓨팅 비중 큼"));
			break;

		// IT — 인프라형 (클라우드/MSA/IaaS): 설계 + 구축 균형
		case EProjectTrait::ITInfraDomain:
			Data.PlanningWeight = 0.3f;
			Data.DevWeight = 0.5f;
			Data.QAWeight = 0.2f;
			Data.Description = FText::FromString(TEXT("아키텍처 설계와 인프라 구축이 핵심"));
			break;

		default:
			break;
		}

		return Data;
	}
};
