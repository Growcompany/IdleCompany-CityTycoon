// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "UI/UISoundTags.h"
#include "Components/Image.h"
#include "UI/Element/Common/IconBrushUtils.h"
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

	// 위젯 풀 재사용/리빌드로 PreConstruct 가 다시 돌 때 주입된 웰 색이 증발하지 않게 복원한다.
	// 주입된 적 없으면 손대지 않는다 ― WBP 가 구운 웰 색이 정답이다.
	if (bWellColorSet && IsValid(WellBG))
	{
		WellBG->SetColorAndOpacity(WellColor);
	}

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
	SetIconResource(NewIcon);
}

void UIconWithButtonWidget::SetIconResource(UObject* NewResource)
{
	IconResource = NewResource;

	// 명시적 클리어는 디자이너 폴백까지 끈다 — 안 그러면 ApplyIconSettings 가 IconTexture 로
	// 되돌아가 숨겼던 WBP 기본 아이콘이 다시 나타난다.
	if (!NewResource)
	{
		IconTexture = nullptr;
	}

	if (IsValid(IconImage))
	{
		if (IsValid(NewResource))
		{
			CGIconBrush::ApplyResource(IconImage, NewResource);
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

void UIconWithButtonWidget::SetIconWellColor(FLinearColor NewColor)
{
	WellColor = NewColor;
	bWellColorSet = true;

	if (IsValid(WellBG))
	{
		WellBG->SetColorAndOpacity(NewColor);
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

void UIconWithButtonWidget::SetItemCost(int32 Have, int32 Need)
{
	const bool bShort = Have < Need;
	SetCountText(bShort
		? FText::Format(NSLOCTEXT("IconWithButton", "ItemCostShort", "{0}/{1}"), FText::AsNumber(Have), FText::AsNumber(Need))
		: FText::Format(NSLOCTEXT("IconWithButton", "ItemCost", "×{0}"), FText::AsNumber(Need)));
	if (!ShortageStyle) return;
	if (!RegularStyle && GetStyle()) RegularStyle = GetStyle()->GetClass();
	const TSubclassOf<UCommonButtonStyle> Wanted = bShort ? ShortageStyle : RegularStyle;
	if (Wanted && (!GetStyle() || GetStyle()->GetClass() != Wanted)) SetStyle(Wanted);
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

	// 아이콘 — 런타임 주입 리소스(IconResource) 우선, 없으면 디자이너 텍스처
	UObject* Resource = IsValid(IconResource) ? IconResource.Get() : static_cast<UObject*>(IconTexture);
	if (IsValid(Resource))
	{
		CGIconBrush::ApplyResource(IconImage, Resource);
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
