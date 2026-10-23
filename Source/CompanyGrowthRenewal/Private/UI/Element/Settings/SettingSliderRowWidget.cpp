// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Settings/SettingSliderRowWidget.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Global/GlobalUtilFunctions.h"

void USettingSliderRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIconProperties();
}

void USettingSliderRowWidget::ApplyIconProperties()
{
	if (!IconTexture.IsNull())
	{
		UGlobalUtilFunctions::ApplyWidgetIcon(IconOn, IconTexture, IconSize, IconPadding, false);
	}
}

void USettingSliderRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ValueText && !bHasNormalValueStyle)
	{
		NormalValueFont = ValueText->GetFont();
		NormalValueColor = ValueText->GetColorAndOpacity();
		bHasNormalValueStyle = true;
	}

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
	if (FillBarMuted)
	{
		FillBarMuted->SetPercent(ClampedValue);
	}
	bMuted = bInMuted;
	RefreshMuteVisual();
	RefreshValueText(ClampedValue);
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
	if (FillBarMuted)
	{
		FillBarMuted->SetPercent(NewValue);
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
	RefreshValueText(GetValue());
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
	if (IconChipOn)
	{
		IconChipOn->SetVisibility(bMuted ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IconChipOff)
	{
		IconChipOff->SetVisibility(bMuted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (FillBar)
	{
		FillBar->SetVisibility(bMuted && FillBarMuted ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (FillBarMuted)
	{
		FillBarMuted->SetVisibility(bMuted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ValueSlider)
	{
		ValueSlider->SetRenderOpacity(1.0f);
	}
	if (ValueText)
	{
		ValueText->SetRenderOpacity(1.0f);
		ValueText->SetFont(bMuted && bOverrideMutedValueStyle ? MutedValueFont : NormalValueFont);
		ValueText->SetColorAndOpacity(bMuted && bOverrideMutedValueStyle ? FSlateColor(MutedValueColor) : NormalValueColor);
	}
}

void USettingSliderRowWidget::RefreshValueText(float Value01)
{
	if (ValueText)
	{
		ValueText->SetText(bMuted ? FText::FromString(TEXT("끔"))
			: FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value01 * 100.0f))));
	}
}
