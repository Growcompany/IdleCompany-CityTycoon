// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Settings/SettingToggleRowWidget.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

void USettingToggleRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ToggleCheck)
	{
		ToggleCheck->OnCheckStateChanged.AddDynamic(this, &USettingToggleRowWidget::HandleCheckChanged);
	}
}

void USettingToggleRowWidget::NativeDestruct()
{
	if (ToggleCheck)
	{
		ToggleCheck->OnCheckStateChanged.RemoveDynamic(this, &USettingToggleRowWidget::HandleCheckChanged);
	}
	Super::NativeDestruct();
}

void USettingToggleRowWidget::ConfigureToggle(const FText& InLabel, const FText& InCaption, bool bInitialOn)
{
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
	if (CaptionText)
	{
		CaptionText->SetText(InCaption);
		CaptionText->SetVisibility(InCaption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ToggleCheck)
	{
		ToggleCheck->SetIsChecked(bInitialOn);
	}
	RefreshSwitchVisual(bInitialOn);
}

bool USettingToggleRowWidget::IsOn() const
{
	return ToggleCheck && ToggleCheck->IsChecked();
}

void USettingToggleRowWidget::HandleCheckChanged(bool bIsChecked)
{
	RefreshSwitchVisual(bIsChecked);
	OnToggleChanged.Broadcast(bIsChecked);
}

void USettingToggleRowWidget::RefreshSwitchVisual(bool bOn)
{
	if (Knob)
	{
		if (UOverlaySlot* KnobSlot = Cast<UOverlaySlot>(Knob->Slot))
		{
			KnobSlot->SetHorizontalAlignment(bOn ? HAlign_Right : HAlign_Left);
		}
	}
	if (TrackBorder)
	{
		// On = 밝은 화이트 트랙 / Off = 어두운 트랙 (블루 없이 명도로 상태 구분)
		TrackBorder->SetBrushColor(bOn ? FLinearColor(1.0f, 1.0f, 1.0f, 0.30f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	}
}
