// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/HQ/HQConditionCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UHQConditionCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 칸 크기는 WBP 저작값을 유지한다
	if (IconImage && Icon) IconImage->SetBrushFromTexture(Icon, false);
}

void UHQConditionCardWidget::SetCondition(const FText& Label, const FText& Current, const FText& Target, float Percent, bool bMet, const FText& MissingText)
{
	if (LabelText) LabelText->SetText(Label);
	if (CurrentText) CurrentText->SetText(Current);
	if (TargetText) TargetText->SetText(FText::Format(NSLOCTEXT("HQ", "ConditionTarget", "/ {0}"), Target));

	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
		ProgressBar->SetFillColorAndOpacity(bMet ? FillMetColor : FillPendingColor);
	}

	if (StatusStrip) StatusStrip->SetBrushColor(bMet ? StripMetColor : StripNeedColor);
	if (StatusText)
	{
		StatusText->SetText(bMet ? NSLOCTEXT("HQ", "ConditionMet", "달성") : MissingText);
		StatusText->SetColorAndOpacity(FSlateColor(bMet ? InkMetColor : InkNeedColor));
	}
	if (StatusCheck) StatusCheck->SetVisibility(bMet ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UHQConditionCardWidget::SetTierBadge(int32 Tier)
{
	if (TierBadge) TierBadge->SetVisibility(Tier > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (TierBadgeText && Tier > 0) TierBadgeText->SetText(FText::AsNumber(Tier));
}
