// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Settings/SettingToggleRowWidget.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Global/GlobalUtilFunctions.h"

void USettingToggleRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIconProperties();
}

void USettingToggleRowWidget::ApplyIconProperties()
{
	UGlobalUtilFunctions::ApplyWidgetIcon(RowIcon, IconTexture, IconSize, IconPadding, false);
}

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
	if (SelectionText)
	{
		SelectionText->SetText(FText::FromString(bOn ? TEXT("켬") : TEXT("끔")));
		SelectionText->SetColorAndOpacity(FSlateColor(bOn ? FLinearColor::White
			: FLinearColor::FromSRGBColor(FColor(0x97, 0xA3, 0xB6))));
		if (UOverlaySlot* TextSlot = Cast<UOverlaySlot>(SelectionText->Slot))
		{
			TextSlot->SetHorizontalAlignment(bOn ? HAlign_Left : HAlign_Right);
		}
	}
	if (TrackOn)
	{
		TrackOn->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TrackOff)
	{
		TrackOff->SetVisibility(bOn ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TrackBorder && !(TrackOn && TrackOff))
	{
		TrackBorder->SetBrushColor(bOn ? FLinearColor(1.0f, 1.0f, 1.0f, 0.30f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	}
}
