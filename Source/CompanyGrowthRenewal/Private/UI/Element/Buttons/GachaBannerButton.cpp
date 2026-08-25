// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/GachaBannerButton.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UGachaBannerButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (TierIcon)
	{
		if (TierIconTexture)
		{
			TierIcon->SetBrushFromTexture(TierIconTexture);
			TierIcon->SetDesiredSizeOverride(TierIconSize);
			TierIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			TierIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (SubText)
	{
		SubText->SetText(SubTextValue);
		SubText->SetVisibility(SubTextValue.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (CountPlate)
	{
		CountPlate->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGachaBannerButton::SetOwnedCount(int32 Count)
{
	if (!CountText) { return; }

	if (CountPlate)
	{
		CountPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	CountText->SetText(FText::AsNumber(Count));
	CountText->SetColorAndOpacity(FSlateColor(Count > 0
		? FLinearColor(0.838f, 0.855f, 0.871f)
		: FLinearColor(0.188f, 0.228f, 0.275f)));
}
