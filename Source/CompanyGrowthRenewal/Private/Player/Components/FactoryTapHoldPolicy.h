#pragma once

#include "CoreTypes.h"

namespace CGRFactoryTapHoldPolicy
{
inline constexpr float HoldThresholdSeconds = 0.2f;

enum class EPhase : uint8
{
	None,
	Pending,
	Holding
};

enum class EReleaseAction : uint8
{
	None,
	ClosePanel,
	StopProduction
};

enum class EDragAction : uint8
{
	None,
	CancelPending,
	// 확정된 홀드는 드래그로 끊지 않는다 — None 과 달리 레거시 취소 경로까지 비켜가야 해서 별도 값이다.
	KeepProduction
};

inline EReleaseAction ResolveRelease(EPhase Phase, bool bDragging)
{
	// 드래그로 카메라를 돌렸어도 생산은 손을 뗄 때 끝낸다 — 여기서 빠지면 잡부 2배속·연기·잔돈 타이머가 영구히 돈다.
	if (Phase == EPhase::Holding)
	{
		return EReleaseAction::StopProduction;
	}

	if (Phase == EPhase::Pending && !bDragging)
	{
		return EReleaseAction::ClosePanel;
	}

	return EReleaseAction::None;
}

inline EDragAction ResolveDrag(EPhase Phase)
{
	if (Phase == EPhase::Pending)
	{
		return EDragAction::CancelPending;
	}

	if (Phase == EPhase::Holding)
	{
		return EDragAction::KeepProduction;
	}

	return EDragAction::None;
}

inline bool ShouldStartHold(EPhase Phase, bool bDragging, bool bPanelActive, bool bFactoryValid)
{
	return Phase == EPhase::Pending
		&& !bDragging
		&& bPanelActive
		&& bFactoryValid;
}
}
