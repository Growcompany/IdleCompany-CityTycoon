// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/TeamPipItemWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"

void UTeamPipItemWidget::SetPips(const FText& Name, int32 FilledPips)
{
	if (Text_Name) { Text_Name->SetText(Name); }
	UImage* Pips[5] = { Pip1, Pip2, Pip3, Pip4, Pip5 };
	const int32 Filled = FMath::Clamp(FilledPips, 0, 5);
	for (int32 i = 0; i < 5; ++i)
	{
		if (Pips[i]) { Pips[i]->SetColorAndOpacity(i < Filled ? PipOnColor : PipOffColor); }
	}
}
