// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/NamedSlot.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UEntityCardFrameWidget::ApplyCompactCatalogLayout()
{
	bUsesBuildModalLayout = false;
	if (!WidgetTree) return;
	if (USizeBox* CardSize = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("SizeBox_37"))))
	{
		CardSize->SetWidthOverride(300.f);
		CardSize->SetHeightOverride(400.f);
	}
	if (ContentSlot)
	{
		if (UVerticalBoxSlot* ContentBoxSlot = Cast<UVerticalBoxSlot>(ContentSlot->Slot))
		{
			ContentBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
	if (EntityImage)
	{
		EntityImage->SetRenderScale(FVector2D(1.f));
		EntityImage->SetDesiredSizeOverride(FVector2D(224.f, 224.f));
		if (UOverlaySlot* ImageSlot = Cast<UOverlaySlot>(EntityImage->Slot))
			ImageSlot->SetPadding(FMargin(20.f, 4.f, 20.f, 4.f));
	}
}

void UEntityCardFrameWidget::ApplyBuildModalLayout()
{
	bUsesBuildModalLayout = true;
	if (!WidgetTree) return;
	if (BuildModalThumbnailBrush.GetResourceObject() && BuildModalThumbnailBrush.DrawAs != ESlateBrushDrawType::NoDrawType)
	{
		if (UBorder* ThumbnailBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_81"))))
		{
			ThumbnailBorder->SetBrush(BuildModalThumbnailBrush);
			if (UWidget* Sheen = WidgetTree->FindWidget(TEXT("PlateSheen")))
				Sheen->SetVisibility(ESlateVisibility::Collapsed);
			if (UWidget* Shadow = WidgetTree->FindWidget(TEXT("GroundShadow")))
				Shadow->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Build modal thumbnail brush is authored, but Border_81 is missing from EntityCardFrame"));
		}
	}
	if (USizeBox* CardSize = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("SizeBox_37"))))
	{
		CardSize->SetWidthOverride(BuildModalCardWidth);
		CardSize->SetHeightOverride(BuildModalCardHeight);
	}
	if (EntityImage)
	{
		if (UTexture2D* IconTexture = Cast<UTexture2D>(EntityImage->GetBrush().GetResourceObject()))
		{
			SetEntityImage(IconTexture);
		}
	}
}

void UEntityCardFrameWidget::SetEntityImage(UTexture2D* Texture)
{
	if (EntityImage && Texture)
	{
		EntityImage->SetBrushFromTexture(Texture);
		FSlateBrush ImageBrush = EntityImage->GetBrush();
		ImageBrush.SetUVRegion(FBox2f(ForceInit));
		EntityImage->SetBrush(ImageBrush);
		if (!bUsesBuildModalLayout) return;

		const FVector4* IconBounds = BuildModalIconBounds.Find(FSoftObjectPath(Texture));
		if (!IconBounds || IconBounds->Z <= IconBounds->X || IconBounds->W <= IconBounds->Y)
		{
			UE_LOG(LogTemp, Warning, TEXT("Build modal icon bounds are missing or invalid: %s"), *Texture->GetPathName());
			return;
		}
		const FVector2D ArtworkSize(
			(IconBounds->Z - IconBounds->X) * Texture->GetSizeX(),
			(IconBounds->W - IconBounds->Y) * Texture->GetSizeY());
		const double FitScale = FMath::Min(BuildModalImageMaxSize.X / ArtworkSize.X,
			BuildModalImageMaxSize.Y / ArtworkSize.Y);
		const FVector2D DisplaySize = ArtworkSize * FitScale;
		ImageBrush.SetUVRegion(FBox2d(FVector2D(IconBounds->X, IconBounds->Y),
			FVector2D(IconBounds->Z, IconBounds->W)));
		EntityImage->SetBrush(ImageBrush);
		EntityImage->SetDesiredSizeOverride(DisplaySize);
		EntityImage->SetRenderScale(FVector2D(1.f));
		EntityImage->SetRenderTranslation(FVector2D::ZeroVector);
		if (UOverlaySlot* ImageSlot = Cast<UOverlaySlot>(EntityImage->Slot))
		{
			ImageSlot->SetHorizontalAlignment(HAlign_Center);
			ImageSlot->SetVerticalAlignment(VAlign_Bottom);
			ImageSlot->SetPadding(FMargin(0.f, 0.f, 0.f, BuildModalImageBottomInset));
		}
		if (WidgetTree)
		{
			if (UBorder* ThumbnailBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_81"))))
			{
				if (UMaterialInstanceDynamic* Backdrop = ThumbnailBorder->GetDynamicMaterial())
				{
					const float ThumbnailHeight = BuildModalThumbnailBrush.GetImageSize().Y;
					if (ThumbnailHeight > 0.f)
					{
						Backdrop->SetScalarParameterValue(TEXT("ContactCenterY"),
							(ThumbnailHeight - BuildModalImageBottomInset) / ThumbnailHeight);
						Backdrop->SetScalarParameterValue(TEXT("ContactHalfWidthPx"), static_cast<float>(DisplaySize.X * 0.4));
					}
				}
			}
		}
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
