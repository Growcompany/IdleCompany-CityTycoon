#include "UI/Element/Effects/DevelopmentBannerWidget.h"

#include "Animation/UMGSequencePlayer.h"
#include "Animation/WidgetAnimation.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Materials/MaterialInstanceDynamic.h"

void UDevelopmentBannerWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (BackgroundImage)
	{
		if (UMaterialInstanceDynamic* BackgroundMaterial = BackgroundImage->GetDynamicMaterial())
		{
			BackgroundMaterial->SetVectorParameterValue(TEXT("TopColor"), BackgroundTopColor);
			BackgroundMaterial->SetVectorParameterValue(TEXT("BottomColor"), BackgroundBottomColor);
		}
	}
	if (OutlineImage) OutlineImage->SetColorAndOpacity(OutlineColor);
	if (ShadowImage) ShadowImage->SetColorAndOpacity(ShadowColor);
	if (HighlightImage) HighlightImage->SetColorAndOpacity(HighlightColor);
	if (LeftStarImage) LeftStarImage->SetColorAndOpacity(StarColor);
	if (RightStarImage) RightStarImage->SetColorAndOpacity(StarColor);
	if (LeftStarOutlineImage) LeftStarOutlineImage->SetColorAndOpacity(StarOutlineColor);
	if (RightStarOutlineImage) RightStarOutlineImage->SetColorAndOpacity(StarOutlineColor);
	if (CountBadgeImage) CountBadgeImage->SetColorAndOpacity(BadgeColor);
	if (TitleText) TitleText->SetColorAndOpacity(TitleColor);
	if (SubtitleText) SubtitleText->SetColorAndOpacity(SubtitleColor);
}

void UDevelopmentBannerWidget::SetBannerText(const FText& Title, const FText& Subtitle)
{
	if (TitleText) TitleText->SetText(Title);
	if (SubtitleText) SubtitleText->SetText(Subtitle);
}

void UDevelopmentBannerWidget::SetAccentColor(const FLinearColor& Color)
{
	BackgroundTopColor = Color;
	SynchronizeProperties();
}

void UDevelopmentBannerWidget::StartPresentation(bool bReducedMotion)
{
	bReduced = bReducedMotion;
	bPresentationPaused = false;
	PausedTime = 0.f;
	if (!Presentation) return;
	StopAnimation(Presentation);
	if (bReduced)
	{
		if (AnimationRoot)
		{
			AnimationRoot->SetRenderScale(FVector2D::UnitVector);
			AnimationRoot->SetRenderOpacity(1.f);
		}
		return;
	}
	PresentationPlayer = PlayAnimation(Presentation, 0.f, 1, EUMGSequencePlayMode::Forward, PresentationSpeed);
}

void UDevelopmentBannerWidget::SetPresentationPaused(bool bPaused)
{
	if (bReduced || bPresentationPaused == bPaused || !Presentation) return;
	bPresentationPaused = bPaused;
	if (bPaused)
	{
		PausedTime = PauseAnimation(Presentation);
	}
	else
	{
		PresentationPlayer = PlayAnimation(Presentation, PausedTime, 1, EUMGSequencePlayMode::Forward, PresentationSpeed);
	}
}

void UDevelopmentBannerWidget::SetPresentationSpeed(float Speed)
{
	PresentationSpeed = FMath::Max(.01f, Speed);
	if (PresentationPlayer && !bPresentationPaused) PresentationPlayer->SetPlaybackSpeed(PresentationSpeed);
}

float UDevelopmentBannerWidget::GetPresentationDuration() const
{
	return Presentation ? Presentation->GetEndTime() - Presentation->GetStartTime() : 0.f;
}
