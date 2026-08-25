// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/NamedSlot.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

void UEntityCardFrameWidget::SetEntityImage(UTexture2D* Texture)
{
	if (EntityImage && Texture)
	{
		EntityImage->SetBrushFromTexture(Texture);
	}
}

void UEntityCardFrameWidget::SetLocked(bool bIsLocked, const FText& LockReason)
{
	if (LockBorder)
	{
		LockBorder->SetVisibility(bIsLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (LockText && bIsLocked)
	{
		LockText->SetText(LockReason);
	}
}

void UEntityCardFrameWidget::SetSelected(bool bSelected)
{
	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UEntityCardFrameWidget::SetShadowLift(float LiftPixels)
{
	if (DropShadow)
	{
		DropShadow->SetRenderTranslation(FVector2D(0.0f, LiftPixels));
	}
}

void UEntityCardFrameWidget::SetPressSink(float SinkPixels)
{
	if (CardEdge)
	{
		CardEdge->SetRenderTranslation(FVector2D(0.0f, -SinkPixels));
	}
}

void UEntityCardFrameWidget::SetSizeBadge(int32 WidthCells, int32 DepthCells)
{
	if (!SizeBadgeGrid || !WidgetTree)
	{
		return;
	}

	SizeBadgeGrid->ClearChildren();

	// 칸수가 비정상이면 캡슐째 숨김(빈 테두리가 남지 않게)
	if (WidthCells <= 0 || DepthCells <= 0)
	{
		SizeBadgeGrid->SetVisibility(ESlateVisibility::Collapsed);
		if (SizeBadgePill)
		{
			SizeBadgePill->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	SizeBadgeGrid->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (SizeBadgePill)
	{
		SizeBadgePill->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	FSlateBrush DotBrush;
	DotBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	// 캡슐이 밴드색(#2A2E34)이라 점은 밝게 — 밝은 썸네일 판 위에서 캡슐째 떨어져 보이게 한다.
	DotBrush.TintColor = FSlateColor(FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.0f));
	DotBrush.OutlineSettings.CornerRadii = FVector4(2.0f, 2.0f, 2.0f, 2.0f);
	DotBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	DotBrush.SetImageSize(FVector2D(9.0f, 9.0f));

	for (int32 Row = 0; Row < DepthCells; ++Row)
	{
		for (int32 Col = 0; Col < WidthCells; ++Col)
		{
			UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (!Dot)
			{
				continue;
			}
			Dot->SetBrush(DotBrush);
			// 지역변수명 Slot 금지(C4458 셰도잉) — GridSlot 사용
			if (UUniformGridSlot* GridSlot = SizeBadgeGrid->AddChildToUniformGrid(Dot, Row, Col))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
}
