#pragma once

#include "CoreMinimal.h"
#include "ProjectLifecycle.generated.h"

/**
 * 프로젝트 라이프사이클 (수주/자체개발 통합 FSM)
 *
 * 수주 경로:    Idle → Developing → LaunchPending → Idle
 *                                  └ 납품/취소 → 즉시 종료
 *
 * 자체개발 경로: Idle → Developing → LaunchPending → Operating → ReportPending → Idle
 *                                  └ 취소시 LaunchPending → Idle
 *
 * 상태별 UI:
 *  - Idle           : CTA 버튼, "프로젝트를 선택해주세요"
 *  - Developing     : Current 카드, "수주/자체 X 개발 중", 버튼 숨김
 *  - LaunchPending  : Current 카드, LaunchConfirm 모달
 *  - Operating      : Current 카드(Operation), "프로젝트N X [A] 운영 중", 버튼 보임
 *  - ReportPending  : Operating UI 위 ProjectReport 모달
 */
UENUM(BlueprintType)
enum class EProjectLifecycle : uint8
{
	Idle           UMETA(DisplayName = "Idle"),
	Developing     UMETA(DisplayName = "Developing"),
	LaunchPending  UMETA(DisplayName = "LaunchPending"),
	Operating      UMETA(DisplayName = "Operating"),
	ReportPending  UMETA(DisplayName = "ReportPending"),
};

inline FString LexToString(EProjectLifecycle Value)
{
	switch (Value)
	{
	case EProjectLifecycle::Idle:           return TEXT("Idle");
	case EProjectLifecycle::Developing:     return TEXT("Developing");
	case EProjectLifecycle::LaunchPending:  return TEXT("LaunchPending");
	case EProjectLifecycle::Operating:      return TEXT("Operating");
	case EProjectLifecycle::ReportPending:  return TEXT("ReportPending");
	default:                                return TEXT("Unknown");
	}
}
