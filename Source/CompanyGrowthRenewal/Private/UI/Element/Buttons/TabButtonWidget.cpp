// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonButtonBase.h"

void UTabButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (TabButton)
	{
		TabButton->SetButtonText(Text);
		TabButton->SetMinDimensions(MinWidth, MinHeight);
		ApplySelectedColor(TabButton->GetSelected());
	}
}

void UTabButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TabButton)
	{
		TabButton->OnIsSelectedChanged.AddDynamic(this, &UTabButtonWidget::HandleSelectedChanged);
		// 그룹이 초기 선택을 NativeConstruct 후에 거는 케이스 대비, 현재 상태로 1회 동기화
		ApplySelectedColor(TabButton->GetSelected());
	}
}

void UTabButtonWidget::NativeDestruct()
{
	if (TabButton)
	{
		TabButton->OnIsSelectedChanged.RemoveDynamic(this, &UTabButtonWidget::HandleSelectedChanged);
	}
	Super::NativeDestruct();
}

void UTabButtonWidget::HandleSelectedChanged(bool bIsSelected)
{
	ApplySelectedColor(bIsSelected);
}

void UTabButtonWidget::ApplySelectedColor(bool bIsSelected)
{
	if (TabButton)
	{
		TabButton->SetTextColor(bIsSelected ? SelectedTextColor : NormalTextColor);
	}
}

UCommonButtonBase* UTabButtonWidget::GetButton() const
{
	return TabButton;
}

void UTabButtonWidget::SetTabText(const FText& NewText)
{
	Text = NewText;
	if (TabButton)
	{
		TabButton->SetButtonText(NewText);
	}
}
