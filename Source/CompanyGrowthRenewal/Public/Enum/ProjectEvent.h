#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/ProjectMode.h"
#include "ProjectEvent.generated.h"

/**
 * 프로젝트 중간 이벤트 (30초 실행 중 1~3회 팝업)
 *
 * 카테고리:
 * - 공통: 모든 모드/산업에서 발동 (12종)
 * - 수주 전용: Commissioned 모드만 (3종)
 * - 자체개발 전용: InHouse 모드만 (3종)
 * - 산업별: 특정 ECompanyType만 (3종 — Game/Finance/IT)
 */
UENUM(BlueprintType)
enum class EProjectEvent : uint8
{
	None UMETA(DisplayName = "None"),

	// === 공통 (12종) ===
	BugFound          UMETA(DisplayName = "버그 발견"),
	GoodIdea          UMETA(DisplayName = "좋은 아이디어"),
	TeamConflict      UMETA(DisplayName = "팀원 갈등"),
	OvertimeRequest   UMETA(DisplayName = "야근 요청"),
	TechDebt          UMETA(DisplayName = "기술 부채"),
	PowerOutage       UMETA(DisplayName = "정전"),
	NewRecruit        UMETA(DisplayName = "신입 합류"),
	CoffeeTime        UMETA(DisplayName = "커피 타임"),
	Burnout           UMETA(DisplayName = "번아웃"),
	SuddenSuccess     UMETA(DisplayName = "깜짝 성과"),
	MentorVisit       UMETA(DisplayName = "멘토 방문"),
	EquipmentUpgrade  UMETA(DisplayName = "장비 업그레이드"),

	// === 수주 전용 (3종) ===
	ClientRevision      UMETA(DisplayName = "클라 수정 요청"),
	ClientCancellation  UMETA(DisplayName = "클라 취소 협박"),
	NegotiateExtension  UMETA(DisplayName = "기한 협상"),

	// === 자체개발 전용 (3종) ===
	MarketShift       UMETA(DisplayName = "시장 트렌드 변화"),
	InvestorInterest  UMETA(DisplayName = "투자자 관심"),
	Pivoting          UMETA(DisplayName = "피보팅"),

	// === 산업별 (6종 — 산업당 2개) ===
	PublisherFeedback   UMETA(DisplayName = "퍼블리셔 피드백"),    // Game
	PlaytestFeedback    UMETA(DisplayName = "플레이테스트"),       // Game
	RegulationChange    UMETA(DisplayName = "규제 변경"),          // Finance
	SecurityAudit       UMETA(DisplayName = "보안 감사"),          // Finance
	ServerIssue         UMETA(DisplayName = "서버 장애"),          // IT
	ScalingChallenge    UMETA(DisplayName = "트래픽 폭주"),        // IT

	Max UMETA(Hidden)
};

inline FString ProjectEventToString(EProjectEvent Event)
{
	switch (Event)
	{
	case EProjectEvent::BugFound: return TEXT("버그 발견!");
	case EProjectEvent::GoodIdea: return TEXT("좋은 아이디어!");
	case EProjectEvent::TeamConflict: return TEXT("팀원 갈등");
	case EProjectEvent::OvertimeRequest: return TEXT("야근 요청");
	case EProjectEvent::TechDebt: return TEXT("기술 부채 발견");
	case EProjectEvent::PowerOutage: return TEXT("정전 발생!");
	case EProjectEvent::NewRecruit: return TEXT("신입 합류");
	case EProjectEvent::CoffeeTime: return TEXT("커피 타임");
	case EProjectEvent::Burnout: return TEXT("번아웃 위험");
	case EProjectEvent::SuddenSuccess: return TEXT("깜짝 성과!");
	case EProjectEvent::MentorVisit: return TEXT("멘토 방문");
	case EProjectEvent::EquipmentUpgrade: return TEXT("장비 업그레이드");
	case EProjectEvent::ClientRevision: return TEXT("클라이언트 수정 요청");
	case EProjectEvent::ClientCancellation: return TEXT("클라이언트 취소 협박");
	case EProjectEvent::NegotiateExtension: return TEXT("기한 협상");
	case EProjectEvent::MarketShift: return TEXT("시장 트렌드 변화");
	case EProjectEvent::InvestorInterest: return TEXT("투자자 관심");
	case EProjectEvent::Pivoting: return TEXT("피보팅");
	case EProjectEvent::PublisherFeedback: return TEXT("퍼블리셔 피드백");
	case EProjectEvent::PlaytestFeedback: return TEXT("플레이테스트");
	case EProjectEvent::RegulationChange: return TEXT("규제 변경");
	case EProjectEvent::SecurityAudit: return TEXT("보안 감사");
	case EProjectEvent::ServerIssue: return TEXT("서버 장애");
	case EProjectEvent::ScalingChallenge: return TEXT("트래픽 폭주");
	default: return TEXT("None");
	}
}

// 이벤트의 산업 필터 (None이면 모든 산업에서 발생)
inline ECompanyType GetEventCompanyFilter(EProjectEvent Event)
{
	switch (Event)
	{
	case EProjectEvent::PublisherFeedback:
	case EProjectEvent::PlaytestFeedback:
		return ECompanyType::Game;
	case EProjectEvent::RegulationChange:
	case EProjectEvent::SecurityAudit:
		return ECompanyType::Finance;
	case EProjectEvent::ServerIssue:
	case EProjectEvent::ScalingChallenge:
		return ECompanyType::IT;
	default: return ECompanyType::None;
	}
}

// 이벤트의 모드 필터 (None이면 양쪽 모두 발생)
inline EProjectMode GetEventModeFilter(EProjectEvent Event)
{
	switch (Event)
	{
	case EProjectEvent::ClientRevision:
	case EProjectEvent::ClientCancellation:
	case EProjectEvent::NegotiateExtension:
		return EProjectMode::Commissioned;
	case EProjectEvent::MarketShift:
	case EProjectEvent::InvestorInterest:
	case EProjectEvent::Pivoting:
		return EProjectMode::InHouse;
	default:
		return EProjectMode::None;
	}
}
