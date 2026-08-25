// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/SkinInfoWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Enum/LootBoxRarity.h"

void USkinInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터에서만 기본값 미리보기
	if (IsDesignTime())
	{
		SetBackgroundColor(DefaultBackgroundColor);
		SetSkinName(DefaultSkinName);
	}
}

void USkinInfoWidget::SetBackgroundColor(FLinearColor InColor)
{
	UE_LOG(LogTemp, Warning, TEXT("[SkinInfoWidget] SetBackgroundColor called: R=%.2f G=%.2f B=%.2f"), InColor.R, InColor.G, InColor.B);

	if (BackgroundColor)
	{
		BackgroundColor->SetColorAndOpacity(FLinearColor::White);
		BackgroundColor->SetBrushTintColor(FSlateColor(InColor));
		UE_LOG(LogTemp, Warning, TEXT("[SkinInfoWidget] BackgroundColor set successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SkinInfoWidget] BackgroundColor is NULL!"));
	}
}

void USkinInfoWidget::SetSkinName(const FText& InName)
{
	if (SkinName)
	{
		SkinName->SetText(InName);
	}
}

void USkinInfoWidget::SetSkinInfo(const FText& InSkinName, ELootBoxRarity InRarity)
{
	SetSkinName(InSkinName);

	FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(InRarity);
	SetBackgroundColor(RarityColor);
}
