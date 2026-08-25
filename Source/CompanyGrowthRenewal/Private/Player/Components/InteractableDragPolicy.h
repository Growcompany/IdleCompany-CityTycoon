#pragma once

#include "Player/MainMapPlayerController.h"

namespace CGRInteractableDragPolicy
{
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
