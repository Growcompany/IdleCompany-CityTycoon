#pragma once

#include "Components/Widget.h"
#include "Player/MainMapPlayerController.h"

namespace CGRFactoryPanelInteraction
{
inline void ConfigureWorldPassThrough(UWidget* PanelRoot, UWidget* Background)
{
	if (PanelRoot)
	{
		PanelRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (Background)
	{
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

inline EInputMode ResolveOpeningInputMode(EInputMode CurrentMode)
{
	return CurrentMode == EInputMode::UI ? EInputMode::Normal : CurrentMode;
}
}
