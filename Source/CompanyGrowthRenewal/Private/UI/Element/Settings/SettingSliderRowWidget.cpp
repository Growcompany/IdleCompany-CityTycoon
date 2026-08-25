// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Settings/SettingSliderRowWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

void USettingSliderRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.AddDynamic(this, &USettingSliderRowWidget::HandleSliderChanged);
		ValueSlider->OnMouseCaptureEnd.AddDynamic(this, &USettingSliderRowWidget::HandleSliderCaptureEnd);
	}
	if (MuteButton)
	{
		MuteButton->OnClicked.AddDynamic(this, &USettingSliderRowWidget::HandleMuteClicked);
	}
}

void USettingSliderRowWidget::NativeDestruct()
{
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.RemoveDynamic(this, &USettingSliderRowWidget::HandleSliderChanged);
		ValueSlider->OnMouseCaptureEnd.RemoveDynamic(this, &USettingSliderRowWidget::HandleSliderCaptureEnd);
	}
	if (MuteButton)
	{
		MuteButton->OnClicked.RemoveDynamic(this, &USettingSliderRowWidget::HandleMuteClicked);
	}
	Super::NativeDestruct();
}

void USettingSliderRowWidget::ConfigureSlider(const FText& InLabel, float InValue01, bool bInMuted)
{
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
	const float ClampedValue = FMath::Clamp(InValue01, 0.0f, 1.0f);
	if (ValueSlider)
	{
		bConfiguring = true;
		ValueSlider->SetValue(ClampedValue);
		bConfiguring = false;
	}
	if (FillBar)
	{
		FillBar->SetPercent(ClampedValue);
	}
	bMuted = bInMuted;
	RefreshValueText(ClampedValue);
	RefreshMuteVisual();
}

float USettingSliderRowWidget::GetValue() const
{
	return ValueSlider ? ValueSlider->GetValue() : 0.0f;
}

void USettingSliderRowWidget::HandleSliderChanged(float NewValue)
{
	if (bConfiguring)
	{
		return;
	}
	if (FillBar)
	{
		FillBar->SetPercent(NewValue);
	}
	RefreshValueText(NewValue);
	OnValueChangedDelegate.Broadcast(NewValue);
}

void USettingSliderRowWidget::HandleSliderCaptureEnd()
{
	OnSliderReleased.Broadcast();
}

void USettingSliderRowWidget::HandleMuteClicked()
{
	bMuted = !bMuted;
	RefreshMuteVisual();
	OnMuteToggledDelegate.Broadcast(bMuted);
}

void USettingSliderRowWidget::RefreshMuteVisual()
{
	if (IconOn)
	{
		IconOn->SetVisibility(bMuted ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IconOff)
	{
		IconOff->SetVisibility(bMuted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const float DimAlpha = bMuted ? 0.32f : 1.0f;
	if (ValueSlider)
	{
		ValueSlider->SetRenderOpacity(DimAlpha);
	}
	if (ValueText)
	{
		ValueText->SetRenderOpacity(DimAlpha);
	}
}

void USettingSliderRowWidget::RefreshValueText(float Value01)
{
	if (ValueText)
	{
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value01 * 100.0f))));
	}
}
