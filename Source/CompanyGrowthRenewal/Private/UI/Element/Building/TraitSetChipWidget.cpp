// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/TraitSetChipWidget.h"
#include "CommonTextBlock.h"

void UTraitSetChipWidget::SetChipData(const FText& InLabel, bool bInActive)
{
	if (ChipText)
	{
		ChipText->SetText(InLabel);
		// 활성 #2E7D3C / 뮤트 #5A6B7D (linear)
		ChipText->SetColorAndOpacity(FSlateColor(bInActive
			? FLinearColor(0.027f, 0.205f, 0.045f)
			: FLinearColor(0.102f, 0.147f, 0.205f)));
	}
	if (ActiveBG) ActiveBG->SetVisibility(bInActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (MutedBG)  MutedBG->SetVisibility(bInActive ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
