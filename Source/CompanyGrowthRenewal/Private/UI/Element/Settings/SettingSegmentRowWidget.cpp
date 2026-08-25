// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Settings/SettingSegmentRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/TextBlock.h"

void USettingSegmentRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SegmentGroup = NewObject<UCommonButtonGroupBase>(this);
	SegmentGroup->SetSelectionRequired(true);

	UButtonWidget* Buttons[3] = { Seg0, Seg1, Seg2 };
	for (UButtonWidget* SegBtn : Buttons)
	{
		if (SegBtn)
		{
			SegmentGroup->AddWidget(SegBtn);
			SegBtn->SetIsSelectable(true);
		}
	}
	SegmentGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &USettingSegmentRowWidget::HandleSelectionChanged);
}

void USettingSegmentRowWidget::NativeDestruct()
{
	if (SegmentGroup)
	{
		SegmentGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void USettingSegmentRowWidget::ConfigureSegments(const FText& InLabel, const FText& InCaption, const TArray<FText>& Options, int32 InitialIndex)
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

	UButtonWidget* Buttons[3] = { Seg0, Seg1, Seg2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (!Buttons[i])
		{
			continue;
		}
		const bool bUsed = Options.IsValidIndex(i);
		Buttons[i]->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bUsed)
		{
			Buttons[i]->SetButtonText(Options[i]);
		}
	}

	if (SegmentGroup && Options.Num() > 0)
	{
		bConfiguring = true;
		SegmentGroup->SelectButtonAtIndex(FMath::Clamp(InitialIndex, 0, Options.Num() - 1));
		bConfiguring = false;
	}
}

void USettingSegmentRowWidget::HandleSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (bConfiguring)
	{
		return;
	}
	OnSegmentChanged.Broadcast(ButtonIndex);
}
