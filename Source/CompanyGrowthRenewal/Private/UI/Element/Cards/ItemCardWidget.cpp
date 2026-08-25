// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/ItemCardWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"

void UItemCardWidget::SetIcon(UTexture2D* InIcon)
{
	if (!EntityImage || !InIcon) return;
	EntityImage->SetBrushFromTexture(InIcon);
}

void UItemCardWidget::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
	if (LockBorder) LockBorder->SetVisibility(bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (LockImage)  LockImage->SetVisibility(bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UItemCardWidget::SetInsufficient(bool bInInsufficient)
{
	bInsufficient = bInInsufficient;
	RefreshOutline();
}

void UItemCardWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	RefreshOutline();
}

void UItemCardWidget::RefreshOutline()
{
	if (!Outline) return;

	if (bInsufficient)
	{
		Outline->SetVisibility(ESlateVisibility::HitTestInvisible);
		Outline->SetBrushColor(FLinearColor(1.0f, 0.2f, 0.2f, 1.0f));  // 빨강 = 부족
	}
	else if (bSelected)
	{
		Outline->SetVisibility(ESlateVisibility::HitTestInvisible);
		Outline->SetBrushColor(FLinearColor::White);  // 흰색 = 머터리얼 글로우 기본
	}
	else
	{
		Outline->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UItemCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 미리보기 + WBP Class Defaults override 즉시 반영
	ApplyIconSize();
	ApplyCardSize();
	SetIcon(DefaultIcon);
}

void UItemCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 부모(Slot)나 호출 측이 NativeConstruct 전에 setter 를 부른 경우 현재 상태 그대로 재적용
	SetLocked(bLocked);
	RefreshOutline();
	ApplyIconSize();
	ApplyCardSize();
}

void UItemCardWidget::SetIconSize(FVector2D NewSize)
{
	IconSize = NewSize;
	ApplyIconSize();
}

void UItemCardWidget::SetCardSize(FVector2D InCardSize, FVector2D InIconSize)
{
	CardSize = InCardSize;
	IconSize = InIconSize;
	ApplyCardSize();
	ApplyIconSize();
}

void UItemCardWidget::ApplyIconSize()
{
	if (!IconSizeBox) return;
	IconSizeBox->SetWidthOverride(IconSize.X);
	IconSizeBox->SetHeightOverride(IconSize.Y);
}

void UItemCardWidget::ApplyCardSize()
{
	if (!CardSizeBox || CardSize.IsNearlyZero()) return;   // 0 = WBP 저작값 유지
	CardSizeBox->SetWidthOverride(CardSize.X);
	CardSizeBox->SetHeightOverride(CardSize.Y);
	// CommonButtonBase 자체 최소치(CDO 144)가 데시레드를 붙들면 내부만 줄고 버튼 셀이 144로 남아,
	// 좌상단 앵커(CardSizeBox 슬롯 저작값)로 아이콘이 셀 왼쪽 위로 쏠린다 — 버튼 최소치도 함께 축소
	SetMinDimensions(FMath::RoundToInt(CardSize.X), FMath::RoundToInt(CardSize.Y));
	// 어떤 이유로든 할당이 데시레드보다 커져도 내용물은 중앙에 오도록 명시 크기 모드에선 중앙 앵커로
	if (UOverlaySlot* CardBoxSlot = Cast<UOverlaySlot>(CardSizeBox->Slot))
	{
		CardBoxSlot->SetHorizontalAlignment(HAlign_Center);
		CardBoxSlot->SetVerticalAlignment(VAlign_Center);
	}
	// 저작 패딩 10 은 144 카드 기준 — 축소 카드에서 아이콘을 과소하게 만들므로 명시 크기 모드에선 제거
	if (EntityImage)
	{
		if (USizeBoxSlot* ImageSlot = Cast<USizeBoxSlot>(EntityImage->Slot))
		{
			ImageSlot->SetPadding(FMargin(0.f));
		}
	}
}

void UItemCardWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnItemCardClicked.ExecuteIfBound(ItemID);
}
