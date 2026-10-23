// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/ItemCardWidget.h"
#include "Components/Image.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "UI/Element/Common/UIIconContentBounds.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"

void UItemCardWidget::SetIcon(UTexture2D* InIcon)
{
	SetIconResource(InIcon);
}

void UItemCardWidget::SetIconResource(UObject* InResource)
{
	CGIconBrush::ApplyResource(EntityImage, InResource);
	ApplyIconFit();
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
		Outline->SetBrushColor(SelectedOutlineColor);
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
	ApplyIconFit();
}

void UItemCardWidget::SetCardSize(FVector2D InCardSize, FVector2D InIconSize)
{
	CardSize = InCardSize;
	IconSize = InIconSize;
	ApplyCardSize();
	ApplyIconSize();
	ApplyIconFit();
}

void UItemCardWidget::SetIconTint(FLinearColor InTint)
{
	if (EntityImage) EntityImage->SetColorAndOpacity(InTint);
}

FLinearColor UItemCardWidget::GetIconTint() const
{
	return EntityImage ? EntityImage->GetColorAndOpacity() : FLinearColor::White;
}

void UItemCardWidget::ApplyIconFit()
{
	if (!bFitIconToContent || !EntityImage) return;

	FVector2D Box = IconSize;
	if (USizeBoxSlot* ImageSlot = Cast<USizeBoxSlot>(EntityImage->Slot))
	{
		// Fill 정렬이면 맞춘 크기가 상자 전체로 늘어나므로 가운데 정렬, 저작 패딩만큼 상자에서 뺀다
		ImageSlot->SetHorizontalAlignment(HAlign_Center);
		ImageSlot->SetVerticalAlignment(VAlign_Center);
		const FMargin Pad = ImageSlot->GetPadding();
		Box.X = FMath::Max(0.0, Box.X - static_cast<double>(Pad.Left + Pad.Right));
		Box.Y = FMath::Max(0.0, Box.Y - static_cast<double>(Pad.Top + Pad.Bottom));
	}
	else if (!bWarnedIconFitSlot)
	{
		bWarnedIconFitSlot = true;
		UE_LOG(LogTemp, Warning, TEXT("%s: bFitIconToContent 인데 EntityImage 부모 슬롯이 SizeBoxSlot 이 아님 ― 정렬 강제 불가, Fill 이면 아이콘이 늘어남"), *GetPathName());
	}
	FUIIconContentFit::Apply(EntityImage, EntityImage->GetBrush().GetResourceObject(), Box);
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
