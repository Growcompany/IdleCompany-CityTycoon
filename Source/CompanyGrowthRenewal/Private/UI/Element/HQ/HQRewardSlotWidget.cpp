// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/HQ/HQRewardSlotWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "UI/Element/Common/IconBrushUtils.h"

namespace
{
	// UMG 는 한글을 음절 단위로 줄바꿈한다 — 폭을 넘는 이름만 두 줄 폭이 가장 고른 띄어쓰기 한 곳에서 직접 나눈다
	FText BreakHQRewardCaption(const FText& Caption, const FSlateFontInfo& Font, float WrapWidth)
	{
		const FString Source = Caption.ToString();
		if (!FSlateApplication::IsInitialized() || !Source.Contains(TEXT(" "))) return Caption;
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		if (Measure->Measure(Source, Font).X <= WrapWidth) return Caption;

		int32 BestSplit = INDEX_NONE;
		float BestWidth = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Source.Len(); ++Index)
		{
			if (Source[Index] != TEXT(' ')) continue;
			const float Wider = FMath::Max(Measure->Measure(Source.Left(Index), Font).X, Measure->Measure(Source.Mid(Index + 1), Font).X);
			if (Wider < BestWidth)
			{
				BestWidth = Wider;
				BestSplit = Index;
			}
		}
		return BestSplit == INDEX_NONE ? Caption : FText::FromString(Source.Left(BestSplit) + TEXT("\n") + Source.Mid(BestSplit + 1));
	}
}

void UHQRewardSlotWidget::SetReward(const FHQRewardSlot& Reward, int32 RewardCountInCard, float HorizontalGap)
{
	const bool bSingle = RewardCountInCard <= 1;
	const float Side = bSingle ? SingleTileSize : RewardCountInCard <= 3 ? FewTileSize : ManyTileSize;
	if (TileSize)
	{
		TileSize->SetWidthOverride(Side);
		TileSize->SetHeightOverride(Side + LipHeight);
	}

	switch (Reward.Style)
	{
	case EHQRewardSlotStyle::Industry:
		ApplyTileBrushes(PlainFace, PlainSheen, HQIndustryPalette::RibbonTop(Reward.Accent), IndustryRimWidth, HQIndustryPalette::RibbonBottom(Reward.Accent));
		break;
	case EHQRewardSlotStyle::Feature:
		ApplyTileBrushes(PlainFace, PlainSheen, FeatureRim, FeatureRimWidth, FeatureLip);
		break;
	case EHQRewardSlotStyle::Menu:
		ApplyTileBrushes(MenuFace, MenuSheen, MenuRim, PlainRimWidth, MenuLip);
		break;
	default:
		ApplyTileBrushes(PlainFace, PlainSheen, PlainRim, PlainRimWidth, PlainLip);
		break;
	}
	if (TileGlow)
	{
		TileGlow->SetVisibility(Reward.Style == EHQRewardSlotStyle::Feature ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const bool bPhoto = Reward.Style == EHQRewardSlotStyle::Building;
	if (PhotoImage)
	{
		PhotoImage->SetVisibility(bPhoto && Reward.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bPhoto && Reward.Icon)
		{
			const float Inner = Side - 2.f * PlainRimWidth;
			CGIconBrush::ApplyResource(PhotoImage, Reward.Icon, FVector2D(Inner, Inner));
		}
	}
	if (IconImage)
	{
		IconImage->SetVisibility(!bPhoto && Reward.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bPhoto && Reward.Icon)
		{
			ApplyIcon(Reward.Icon, Side * (Reward.Style == EHQRewardSlotStyle::Menu ? MenuIconFill : IconFill));
		}
	}
	if (!Reward.Icon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQRewardSlot] Icon missing for reward '%s'"), *Reward.Caption.ToString());
	}

	if (CountBadge) CountBadge->SetVisibility(Reward.Count.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (CountText) CountText->SetText(Reward.Count);

	if (CaptionText)
	{
		FSlateFontInfo Font = CaptionText->GetFont();
		Font.Size = bSingle ? SingleCaptionFontSize : CaptionFontSize;
		CaptionText->SetFont(Font);
		// 이름 상자는 WBP 의 음수 슬롯 여백만큼 칸보다 넓지만, 글자는 칸 + 칸 사이 간격 안에서만 쓴다 —
		// 이웃 칸 이름과 맞닿지 않게. 줄바꿈 폭을 고정해야 긴 이름이 칸 폭을 밀어내지도 않는다
		const UVerticalBoxSlot* CaptionSlot = Cast<UVerticalBoxSlot>(CaptionText->Slot);
		const float BoxWidth = Side + 2.f * (CaptionSlot ? FMath::Max(0.f, -CaptionSlot->GetPadding().Left) : 0.f);
		const float WrapWidth = FMath::Min(BoxWidth, Side + FMath::Max(0.f, HorizontalGap));
		CaptionText->SetAutoWrapText(false);
		CaptionText->SetWrapTextAt(WrapWidth);
		CaptionText->SetText(BreakHQRewardCaption(Reward.Caption, Font, WrapWidth));
	}
}

void UHQRewardSlotWidget::ApplyTileBrushes(const FLinearColor& Face, const FLinearColor& Sheen, const FLinearColor& Rim, float RimWidth, const FLinearColor& Lip)
{
	// 모서리 반경은 WBP 저작값 그대로 두고 브러시 색만 바꾼다 (BrushColor 는 저작 색에 곱해진다)
	if (TileLip)
	{
		FSlateBrush LipBrush = TileLip->Background;
		LipBrush.TintColor = FSlateColor(Lip);
		TileLip->SetBrush(LipBrush);
	}
	if (TileFace)
	{
		// 안쪽 여백 = 테 두께라 빌딩 사진이 테를 덮지 않는다
		FSlateBrush FaceBrush = TileFace->Background;
		FaceBrush.TintColor = FSlateColor(Face);
		FaceBrush.OutlineSettings.Color = FSlateColor(Rim);
		FaceBrush.OutlineSettings.Width = RimWidth;
		TileFace->SetBrush(FaceBrush);
		TileFace->SetPadding(FMargin(RimWidth));
	}
	if (TileSheen) TileSheen->SetColorAndOpacity(Sheen);
}

void UHQRewardSlotWidget::ApplyIcon(UObject* Icon, float BoxSize)
{
	// 텍스처는 원본 비율을 지켜 정사각 칸 안에 맞춘다 (머티리얼 아이콘은 정사각 전제)
	FVector2D Size(BoxSize, BoxSize);
	if (const UTexture2D* Texture = Cast<UTexture2D>(Icon))
	{
		const float Width = Texture->GetSurfaceWidth();
		const float Height = Texture->GetSurfaceHeight();
		if (Width > 0.f && Height > 0.f)
		{
			Size = Width >= Height ? FVector2D(BoxSize, BoxSize * Height / Width) : FVector2D(BoxSize * Width / Height, BoxSize);
		}
	}
	CGIconBrush::ApplyResource(IconImage, Icon, Size);
}
