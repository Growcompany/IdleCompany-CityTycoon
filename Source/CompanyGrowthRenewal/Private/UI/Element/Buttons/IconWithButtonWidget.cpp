// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "UI/UISoundTags.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "Components/BorderSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "CommonTextBlock.h"

void UIconWithButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyIconSettings();

	// 수량 텍스트 기본값 적용
	SetCount(Count);

	// 뱃지: 버튼 스코프 용도이므로 Size 는 항상 Large 강제. 색/표시 여부는 호출자가 제어.
	if (IsValid(AlertMark))
	{
		AlertMark->SetSize(EAlertMarkSize::Large);
		AlertMark->Hide();
	}
}

void UIconWithButtonWidget::SetIcon(UTexture2D* NewIcon)
{
	IconTexture = NewIcon;

	if (IsValid(IconImage))
	{
		if (IsValid(NewIcon))
		{
			IconImage->SetBrushFromTexture(NewIcon);
			IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UIconWithButtonWidget::SetIconSize(FVector2D NewSize)
{
	IconSize = NewSize;

	if (IsValid(IconImage))
	{
		IconImage->SetDesiredSizeOverride(NewSize);
	}
}

void UIconWithButtonWidget::SetIconPadding(FMargin NewPadding)
{
	IconPadding = NewPadding;

	if (IsValid(IconImage))
	{
		if (UPanelSlot* PanelSlot = IconImage->Slot)
		{
			// OverlaySlot인 경우
			if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
			{
				OverlaySlot->SetPadding(NewPadding);
			}
			// BorderSlot인 경우
			else if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(PanelSlot))
			{
				BorderSlot->SetPadding(NewPadding);
			}
			// HorizontalBoxSlot인 경우
			else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				HBoxSlot->SetPadding(NewPadding);
			}
			// VerticalBoxSlot인 경우
			else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
			{
				VBoxSlot->SetPadding(NewPadding);
			}
		}
	}
}

void UIconWithButtonWidget::SetIconVisibility(ESlateVisibility InVisibility)
{
	if (IsValid(IconImage))
	{
		IconImage->SetVisibility(InVisibility);
	}
}

void UIconWithButtonWidget::SetIconColor(FLinearColor NewColor)
{
	IconColor = NewColor;

	if (IsValid(IconImage))
	{
		IconImage->SetColorAndOpacity(NewColor);
	}
}

// ========== 수량 텍스트 ==========

void UIconWithButtonWidget::SetCount(int32 InCount)
{
	Count = InCount;

	if (IsValid(CountText))
	{
		if (InCount > 0)
		{
			CountText->SetText(FText::FromString(
				FString::Printf(TEXT("x %d"), InCount)));
			CountText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			CountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UIconWithButtonWidget::SetCountText(const FText& InText)
{
	if (IsValid(CountText))
	{
		CountText->SetText(InText);
		if (!InText.IsEmpty())
		{
			CountText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			CountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UIconWithButtonWidget::SetCountVisibility(ESlateVisibility InVisibility)
{
	if (IsValid(CountText))
	{
		CountText->SetVisibility(InVisibility);
	}
}

// ========== 알림 뱃지 ==========

void UIconWithButtonWidget::ShowBadge()
{
	if (IsValid(AlertMark))
	{
		AlertMark->Show();
	}
}

void UIconWithButtonWidget::HideBadge()
{
	if (IsValid(AlertMark))
	{
		AlertMark->Hide();
	}
}

bool UIconWithButtonWidget::IsBadgeVisible() const
{
	return IsValid(AlertMark) && AlertMark->IsShown();
}

FGameplayTag UIconWithButtonWidget::GetClickSoundTag() const
{
	return CGUISoundTags::ButtonMainAction;
}

void UIconWithButtonWidget::ApplyIconSettings()
{
	if (!IsValid(IconImage))
	{
		return;
	}

	// 아이콘 텍스처 설정
	if (IsValid(IconTexture))
	{
		IconImage->SetBrushFromTexture(IconTexture);
		IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 아이콘 크기 설정
	IconImage->SetDesiredSizeOverride(IconSize);

	// 아이콘 패딩 설정
	SetIconPadding(IconPadding);

	// 아이콘 색상 설정
	IconImage->SetColorAndOpacity(IconColor);
}
