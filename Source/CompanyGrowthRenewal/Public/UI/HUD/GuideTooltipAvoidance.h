#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

class UWidget;

/** Collect visible content bounds from one current UI owner in overlay-local coordinates.
 *  OutProtected (optional) receives close buttons instead of OutRects - they must never be covered.
 *  OutControls (optional) receives other pressable buttons instead of OutRects - weighted heavier than text. */
void CollectGuideTooltipAvoidRects(UWidget* Root, const FGeometry& OverlayGeometry, TArray<FSlateRect>& OutRects,
	TArray<FSlateRect>* OutProtected = nullptr, TArray<FSlateRect>* OutControls = nullptr);
