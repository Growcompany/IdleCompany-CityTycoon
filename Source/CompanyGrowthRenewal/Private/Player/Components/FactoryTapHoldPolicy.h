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
	StopProduction
};

inline EReleaseAction ResolveRelease(EPhase Phase, bool bDragging)
{
	if (bDragging)
	{
		return EReleaseAction::None;
	}

	if (Phase == EPhase::Pending)
	{
		return EReleaseAction::ClosePanel;
	}

	if (Phase == EPhase::Holding)
	{
		return EReleaseAction::StopProduction;
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
		return EDragAction::StopProduction;
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
