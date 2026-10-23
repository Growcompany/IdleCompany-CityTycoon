// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Settings/SettingSegmentRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonTextBlock.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"

void USettingSegmentRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIconProperties();
	if (OptionButtonStyle)
	{
		UButtonWidget* Buttons[3] = { Seg0, Seg1, Seg2 };
		for (UButtonWidget* SegBtn : Buttons)
		{
			if (SegBtn)
			{
				SegBtn->SetStyle(OptionButtonStyle);
			}
		}
	}
}

void USettingSegmentRowWidget::ApplyIconProperties()
{
	// 아이콘 박스가 고정폭 68 이라 텍스처가 없어도 자리를 비워 둔다 — 접으면 이 행만 들여쓰기가 어긋난다
	UGlobalUtilFunctions::ApplyWidgetIcon(RowIcon, IconTexture, IconSize, IconPadding, /*bCollapseWhenEmpty*/ false);
}

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
	RefreshOptionTextStyles();
}

void USettingSegmentRowWidget::HandleSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	RefreshOptionTextStyles();
	if (bConfiguring)
	{
		return;
	}
	OnSegmentChanged.Broadcast(ButtonIndex);
}

void USettingSegmentRowWidget::RefreshOptionTextStyles()
{
	UButtonWidget* Buttons[3] = { Seg0, Seg1, Seg2 };
	for (UButtonWidget* SegBtn : Buttons)
	{
		if (SegBtn)
		{
			if (OptionButtonStyle)
			{
				if (UCommonTextBlock* OptionText = Cast<UCommonTextBlock>(SegBtn->GetWidgetFromName(TEXT("ButtonText"))))
				{
					OptionText->SetStyle(SegBtn->GetCurrentTextStyleClass());
				}
			}
			if (const UCommonTextStyle* TextStyle = SegBtn->GetCurrentTextStyle())
			{
				SegBtn->SetTextColor(TextStyle->Color);
			}
		}
	}
}
