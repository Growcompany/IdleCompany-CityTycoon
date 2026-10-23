// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/HQ/HQKpiCellWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UHQKpiCellWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (LabelText) LabelText->SetText(Label);
	// 칸 크기는 WBP 저작값을 유지한다
	if (IconImage && Icon) IconImage->SetBrushFromTexture(Icon, false);
}

void UHQKpiCellWidget::SetValue(const FText& Value, const FText& Unit)
{
	if (ValueText) ValueText->SetText(Value);
	if (UnitText)
	{
		UnitText->SetText(Unit);
		UnitText->SetVisibility(Unit.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}
