// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UTabButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (TabButton)
	{
		TabButton->SetButtonText(Text);
		TabButton->SetMinDimensions(MinWidth, MinHeight);
	}
	ApplyIcon();
	ApplySelectedColor(TabButton ? TabButton->GetSelected() : false);
}

void UTabButtonWidget::ApplyIcon()
{
	UTexture2D* Tex = IconTexture.IsNull() ? nullptr : IconTexture.LoadSynchronous();

	// 아이콘은 라벨 위에 겹쳐 놓이고 라벨이 왼쪽 패딩으로 자리를 비켜 준다 — 아이콘이 없으면
	// 그 여백도 걷어야 라벨이 가운데에 선다 (2026-09-24 탭 아이콘 제거 후 25px 치우침).
	// 세로 패딩은 그대로 둔다: 탭 라벨을 아래 내용판에서 띄워 두는 마감 값이다.
	if (TabButton)
	{
		FMargin LabelPadding = TabButton->GetTextPadding();
		LabelPadding.Left = Tex ? IconLabelInset : 0.0f;
		TabButton->SetTextPadding(LabelPadding);
	}

	if (!TabIcon)
	{
		return;
	}
	if (!Tex)
	{
		TabIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	TabIcon->SetBrushFromTexture(Tex, false);
	TabIcon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	TabIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	const FSlateColor& Ink = bIsSelected ? SelectedTextColor : NormalTextColor;
	if (TabButton)
	{
		TabButton->SetTextColor(Ink);
	}
	// 글리프는 글자색을 그대로 따라간다 — 노브를 나누면 둘이 다른 상태를 말하게 된다
	if (TabIcon && TabIcon->GetVisibility() != ESlateVisibility::Collapsed)
	{
		TabIcon->SetColorAndOpacity(Ink.GetSpecifiedColor());
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
