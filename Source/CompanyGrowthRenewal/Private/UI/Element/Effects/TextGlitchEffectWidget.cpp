// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Effects/TextGlitchEffectWidget.h"
#include "Components/TextBlock.h"
#include "Components/RetainerBox.h"

void UTextGlitchEffectWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDefaultText();
}

void UTextGlitchEffectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyDefaultText();
}

void UTextGlitchEffectWidget::SetText(const FText& InText)
{
	if (GlitchText)
	{
		GlitchText->SetText(InText);
	}
}

void UTextGlitchEffectWidget::SetTextFromString(const FString& InText)
{
	SetText(FText::FromString(InText));
}

FText UTextGlitchEffectWidget::GetText() const
{
	if (GlitchText)
	{
		return GlitchText->GetText();
	}
	return FText::GetEmpty();
}

void UTextGlitchEffectWidget::ApplyDefaultText()
{
	if (GlitchText && !DefaultText.IsEmpty())
	{
		GlitchText->SetText(DefaultText);
	}
}
