#pragma once

#include "CoreMinimal.h"
#include "ProjectTrait.generated.h"

/**
 * 프로젝트 특성
 * 프로젝트마다 랜덤으로 1~2개 부여되어 실행 조건을 변경
 *
 * 제약형 (수주 풀에 주로 등장): Rush, ClientPressure, BudgetCut, StrictQA
 * 기회형 (자체개발 풀에 주로 등장): TrendRiding, Innovation, HighRisk, LargeProject
 * 공통 (양쪽 모두): Focus 계열, StabilityOriented
 */
UENUM(BlueprintType)
enum class EProjectTrait : uint8
{
	None UMETA(DisplayName = "None"),

	// 제약형 (수주 풀)
	RushDelivery UMETA(DisplayName = "급한 납기"),
	ClientPressure UMETA(DisplayName = "클라이언트 압박"),
	BudgetCut UMETA(DisplayName = "예산 삭감"),
	StrictQA UMETA(DisplayName = "엄격한 검수"),

	// 공통 (Focus 계열)
	DevFocus UMETA(DisplayName = "개발 집중"),
	QAFocus UMETA(DisplayName = "QA 집중"),
	PlanningFocus UMETA(DisplayName = "기획 집중"),

	// 기회형 (자체개발 풀)
	TrendRiding UMETA(DisplayName = "트렌드 탑승"),
	InnovationRequired UMETA(DisplayName = "혁신 요구"),
	StabilityOriented UMETA(DisplayName = "안정 지향"),
	HighRiskHighReturn UMETA(DisplayName = "고위험 고수익"),
	LargeProject UMETA(DisplayName = "대형 프로젝트"),

	// 도메인형 (프로젝트 데이터로부터 자동 부여 — 부서 가중치 프리셋)
	// 금융 4종
	RegulatoryDomain UMETA(DisplayName = "규제 심사"),
	PerformanceDomain UMETA(DisplayName = "성과 검증"),
	InsuranceDomain UMETA(DisplayName = "약관 승인"),
	AuditDomain UMETA(DisplayName = "기술 감사"),
	// IT 3종
	ITGeneralDomain UMETA(DisplayName = "일반 SW"),
	ITAIDomain UMETA(DisplayName = "AI ML"),
	ITInfraDomain UMETA(DisplayName = "인프라"),

	Max UMETA(Hidden)
};

inline FString ProjectTraitToString(EProjectTrait Trait)
{
	switch (Trait)
	{
	case EProjectTrait::RushDelivery: return TEXT("급한 납기");
	case EProjectTrait::ClientPressure: return TEXT("클라이언트 압박");
	case EProjectTrait::BudgetCut: return TEXT("예산 삭감");
	case EProjectTrait::StrictQA: return TEXT("엄격한 검수");
	case EProjectTrait::DevFocus: return TEXT("개발 집중");
	case EProjectTrait::QAFocus: return TEXT("QA 집중");
	case EProjectTrait::PlanningFocus: return TEXT("기획 집중");
	case EProjectTrait::TrendRiding: return TEXT("트렌드 탑승");
	case EProjectTrait::InnovationRequired: return TEXT("혁신 요구");
	case EProjectTrait::StabilityOriented: return TEXT("안정 지향");
	case EProjectTrait::HighRiskHighReturn: return TEXT("고위험 고수익");
	case EProjectTrait::LargeProject: return TEXT("대형 프로젝트");
	case EProjectTrait::RegulatoryDomain: return TEXT("규제 심사");
	case EProjectTrait::PerformanceDomain: return TEXT("성과 검증");
	case EProjectTrait::InsuranceDomain: return TEXT("약관 승인");
	case EProjectTrait::AuditDomain: return TEXT("기술 감사");
	case EProjectTrait::ITGeneralDomain: return TEXT("일반 SW");
	case EProjectTrait::ITAIDomain: return TEXT("AI ML");
	case EProjectTrait::ITInfraDomain: return TEXT("인프라");
	default: return TEXT("None");
	}
}

// 특성 간 충돌 여부 판별
inline bool AreTraitsConflicting(EProjectTrait A, EProjectTrait B)
{
	// 타이머 방향 반대: 급한 납기 vs 대형 프로젝트
	if ((A == EProjectTrait::RushDelivery && B == EProjectTrait::LargeProject) ||
		(A == EProjectTrait::LargeProject && B == EProjectTrait::RushDelivery))
		return true;

	// Focus 계열 중복 불가
	auto IsFocus = [](EProjectTrait T) {
		return T == EProjectTrait::DevFocus || T == EProjectTrait::QAFocus || T == EProjectTrait::PlanningFocus;
	};
	if (IsFocus(A) && IsFocus(B) && A != B)
		return true;

	// 모순: 안정 지향 vs 혁신 요구
	if ((A == EProjectTrait::StabilityOriented && B == EProjectTrait::InnovationRequired) ||
		(A == EProjectTrait::InnovationRequired && B == EProjectTrait::StabilityOriented))
		return true;

	// 클라이언트 압박 vs 급한 납기 (압박 + 시간 압박 중복)
	if ((A == EProjectTrait::ClientPressure && B == EProjectTrait::RushDelivery) ||
		(A == EProjectTrait::RushDelivery && B == EProjectTrait::ClientPressure))
		return true;

	return false;
}
