// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/ProductListRowWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UProductListRowWidget::SetProductData(const FText& InProductName, int32 InQuantity)
{
	if (ProductNameText)
	{
		ProductNameText->SetText(InProductName);
	}
	if (QuantityText)
	{
		const FText Out = FText::Format(QuantityFormat, FText::AsNumber(InQuantity));
		QuantityText->SetText(Out);
	}
}

void UProductListRowWidget::SetProductIcon(UTexture2D* InIcon)
{
	if (ProductIconImage && InIcon)
	{
		ProductIconImage->SetBrushFromTexture(InIcon);
	}
}
