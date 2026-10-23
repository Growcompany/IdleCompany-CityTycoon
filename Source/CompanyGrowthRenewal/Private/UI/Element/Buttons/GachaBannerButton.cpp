// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/GachaBannerButton.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "Engine/Texture2D.h"

void UGachaBannerButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshTierIcon();
	if (SubText)
	{
		SubText->SetText(SubTextValue);
		SubText->SetVisibility(SubTextValue.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (CountPlate)
	{
		CountPlate->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshFace();
	ApplySelectionVisual();
}

void UGachaBannerButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	ApplySelectionVisual();
}

void UGachaBannerButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	ApplySelectionVisual();
}

void UGachaBannerButton::RefreshFace()
{
	if (FaceImage && FaceTexture)
	{
		FSlateBrush Brush = FaceImage->GetBrush();
		Brush.SetResourceObject(FaceTexture);
		FaceImage->SetBrush(Brush);
	}
}

void UGachaBannerButton::ApplySelectionVisual()
{
	const bool bIsSelected = GetSelected();
	// RoundedBox 외곽선은 알파를 따로 들고 있어 투명도만으로는 안 꺼진다 — Visibility 까지 같이 바꾼다
	for (UWidget* Layer : {SelectBand, SelectSheen})
	{
		if (!Layer) continue;
		Layer->SetRenderOpacity(bIsSelected ? 1.f : 0.f);
		Layer->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (UnselectedVeil)
	{
		UnselectedVeil->SetVisibility(bIsSelected ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}

void UGachaBannerButton::SetBannerPresentation(const FText& InText, UObject* IconResource)
{
	SetButtonText(InText);
	TierIconMaterial = Cast<UMaterialInterface>(IconResource);
	TierIconTexture = Cast<UTexture2D>(IconResource);
	RefreshTierIcon();
}

void UGachaBannerButton::RefreshTierIcon()
{
	if (TierIcon)
	{
		UObject* IconResource = TierIconMaterial ? static_cast<UObject*>(TierIconMaterial) : static_cast<UObject*>(TierIconTexture);
		if (IconResource)
		{
			CGIconBrush::ApplyResource(TierIcon, IconResource);
			TierIcon->SetDesiredSizeOverride(TierIconSize);
			TierIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			TierIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGachaBannerButton::SetOwnedCount(int32 Count, bool bBoxUnit)
{
	if (!CountText) { return; }

	if (CountPlate)
	{
		CountPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
		CountPlate->SetRenderOpacity(Count > 0 ? 1.f : CountZeroOpacity);
	}
	CountText->SetText(FText::AsNumber(Count));
	if (CountUnit)
	{
		CountUnit->SetText(bBoxUnit
			? NSLOCTEXT("GachaBanner", "BoxUnit", "개")
			: NSLOCTEXT("GachaBanner", "TicketUnit", "장"));
	}
	CountText->SetColorAndOpacity(FSlateColor(Count > 0 ? CountColor : CountZeroColor));
}
