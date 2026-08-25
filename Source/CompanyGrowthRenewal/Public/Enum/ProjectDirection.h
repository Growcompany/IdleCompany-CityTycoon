#pragma once

#include "CoreMinimal.h"
#include "ProjectDirection.generated.h"

// 착수 결단 — 개발 방향성. 기존 감쇠 경제(개발비/반감기)에 직접 매핑되는 글로벌 노브.
// 온보딩: T2(또는 튜토 마일스톤)에서 해금. 초반엔 숨김·표준 고정.
UENUM(BlueprintType)
enum class EProjectDirection : uint8
{
	Standard UMETA(DisplayName = "표준"),  // 균형
	Speed    UMETA(DisplayName = "속도"),  // 짧은 개발 + 가파른 감쇠(빨리 식음)
	Quality  UMETA(DisplayName = "품질"),  // 긴 개발 + 느린 꼬리(오래 감)
	Research UMETA(DisplayName = "연구"),  // 비싸지만 등급 상향 보정
};

// 방향성 → 경제 계수. industry 무관한 글로벌 튜너블이라 코드 상수로 둠(prototype DIRECTIONS와 동일).
// CostMult: 개발비 배율 — 개발비 폐지(2026-08-15)로 소비처 0 / HalfLifeMult: 판매 감쇠 반감기 배율(Phase 2) / GradeBias: 등급 보정(Phase 4 연구).
struct FProjectDirectionTuning
{
	float CostMult = 1.f;
	float HalfLifeMult = 1.f;
	float GradeBias = 0.f;
};

inline FProjectDirectionTuning GetDirectionTuning(EProjectDirection Dir)
{
	switch (Dir)
	{
	case EProjectDirection::Speed:    return { 0.8f, 0.6f, 0.f };
	case EProjectDirection::Quality:  return { 1.3f, 1.5f, 0.f };
	case EProjectDirection::Research: return { 1.5f, 1.1f, 0.15f };
	case EProjectDirection::Standard:
	default:                          return { 1.0f, 1.0f, 0.f };
	}
}
