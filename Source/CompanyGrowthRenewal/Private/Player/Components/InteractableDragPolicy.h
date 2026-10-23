#pragma once

#include "Player/MainMapPlayerController.h"

namespace CGRInteractableDragPolicy
{
inline constexpr float DragThresholdPixels = 30.0f;

inline bool ShouldCancelAircraftTap(bool bAlreadyDragging, bool bValidPointerPositions,
    const FVector2D& PressPosition, const FVector2D& ReleasePosition)
{
    return bAlreadyDragging || !bValidPointerPositions
        || FVector2D::DistSquared(PressPosition, ReleasePosition) > FMath::Square(DragThresholdPixels);
}

inline bool ShouldCancelFactoryHold(
	bool bAlreadyDragging,
	float DragDistance,
	float DragThreshold,
	EInputMode CurrentMode,
	bool bHitFactory)
{
	return !bAlreadyDragging
		&& DragDistance > DragThreshold
		&& CurrentMode == EInputMode::Factory
		&& bHitFactory;
}

template <typename TObjectType, typename TCancelCallback>
inline bool TryCancelFactoryHold(
	bool bShouldCancel,
	TObjectType*& InOutHitObject,
	bool bImplementsInputHandler,
	TCancelCallback&& CancelCallback)
{
	if (!bShouldCancel || !IsValid(InOutHitObject) || !bImplementsInputHandler)
	{
		return false;
	}

	CancelCallback(InOutHitObject);
	InOutHitObject = nullptr;
	return true;
}
}
