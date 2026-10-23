// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Effects/FactoryBoostWidget.h"

#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

void UFactoryBoostWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetClipping(EWidgetClipping::Inherit);
	SetPixelSnapping(EWidgetPixelSnapping::Disabled);

	if (BadgeRoot)
	{
		BadgeRoot->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
		if (CachedBadgeRoot.Get() != BadgeRoot)
		{
			CachedBadgeRoot = BadgeRoot;
			AuthoredBadgeTransform = BadgeRoot->GetRenderTransform();
			AuthoredBadgeOpacity = BadgeRoot->GetRenderOpacity();
		}
		else
		{
			BadgeRoot->SetRenderTransform(AuthoredBadgeTransform);
			BadgeRoot->SetRenderOpacity(AuthoredBadgeOpacity);
		}
	}

	if (Img_PressArrow)
	{
		Img_PressArrow->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
		Img_PressArrow->SetVisibility(ESlateVisibility::Collapsed);
	}

	bBoostActive = false;
	PressFeedbackElapsed = 0.f;
	bEntrancePlaying = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UFactoryBoostWidget::BeginBoost(float Multiplier, const FVector2D& PressAbsolutePosition)
{
	if (Txt_Multiplier)
	{
		Txt_Multiplier->SetText(FText::AsNumber(FMath::RoundToInt(Multiplier)));
	}

	EntranceElapsed = 0.f;
	PressAnchorAbsolute = PressAbsolutePosition;
	PressFeedbackElapsed = 0.f;
	bBoostActive = true;
	bEntrancePlaying = true;
	if (Img_PressArrow)
	{
		Img_PressArrow->SetRenderOpacity(0.f);
		Img_PressArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ApplyEntrance(EntranceRise, 0.86f, 0.f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFactoryBoostWidget::EndBoost()
{
	bBoostActive = false;
	PressFeedbackElapsed = 0.f;
	bEntrancePlaying = false;
	SetVisibility(ESlateVisibility::Collapsed);
	if (Img_PressArrow)
	{
		Img_PressArrow->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BadgeRoot)
	{
		BadgeRoot->SetRenderTransform(AuthoredBadgeTransform);
		BadgeRoot->SetRenderOpacity(AuthoredBadgeOpacity);
	}
}

void UFactoryBoostWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bEntrancePlaying)
	{
		if (bBoostActive)
		{
			UpdatePressFeedback(MyGeometry, InDeltaTime);
		}
		return;
	}

	EntranceElapsed += InDeltaTime;
	const float Progress = FMath::Clamp(EntranceElapsed / FMath::Max(EntranceDuration, 0.01f), 0.f, 1.f);
	if (Progress >= 1.f)
	{
		ApplyEntrance(0.f, 1.f, 1.f);
		bEntrancePlaying = false;
		if (bBoostActive)
		{
			UpdatePressFeedback(MyGeometry, InDeltaTime);
		}
		return;
	}

	constexpr float OvershootTime = 0.62f;
	if (Progress < OvershootTime)
	{
		const float Phase = Progress / OvershootTime;
		const float EaseOut = 1.f - FMath::Pow(1.f - Phase, 3.f);
		ApplyEntrance(
			FMath::Lerp(EntranceRise, -EntranceRise / 6.f, EaseOut),
			FMath::Lerp(0.86f, 1.035f, EaseOut),
			EaseOut);
	}
	else
	{
		const float Phase = (Progress - OvershootTime) / (1.f - OvershootTime);
		const float EaseOut = 1.f - FMath::Pow(1.f - Phase, 2.f);
		ApplyEntrance(
			FMath::Lerp(-EntranceRise / 6.f, 0.f, EaseOut),
			FMath::Lerp(1.035f, 1.f, EaseOut),
			1.f);
	}
	if (bBoostActive)
	{
		UpdatePressFeedback(MyGeometry, InDeltaTime);
	}
}

void UFactoryBoostWidget::UpdatePressFeedback(const FGeometry& MyGeometry, float InDeltaTime)
{
	if (!Img_PressArrow || MyGeometry.GetLocalSize().IsNearlyZero())
	{
		return;
	}

	UCanvasPanelSlot* ArrowSlot = Cast<UCanvasPanelSlot>(Img_PressArrow->Slot);
	UCanvasPanelSlot* BadgeSlot = BadgeRoot ? Cast<UCanvasPanelSlot>(BadgeRoot->Slot) : nullptr;
	if (!ArrowSlot || !BadgeSlot || Img_PressArrow->GetParent() != BadgeRoot)
	{
		return;
	}

	const float Period = FMath::Max(PressFeedbackPeriod, 0.01f);
	PressFeedbackElapsed = FMath::Fmod(PressFeedbackElapsed + FMath::Max(InDeltaTime, 0.f), Period);
	const float Bounce = FMath::Square(FMath::Sin(PI * PressFeedbackElapsed / Period));
	const FVector2D PressLocal = MyGeometry.AbsoluteToLocal(PressAnchorAbsolute);
	const FVector2D ArrowContact = ArrowSlot->GetPosition() +
		FVector2D(ArrowSlot->GetSize().X * (0.5f - ArrowSlot->GetAlignment().X),
			ArrowSlot->GetSize().Y * (1.f - ArrowSlot->GetAlignment().Y));
	const FWidgetTransform BadgeTransform = BadgeRoot->GetRenderTransform();
	const FVector2D Pivot = BadgeSlot->GetSize() * BadgeRoot->GetRenderTransformPivot();
	const FVector2D ContactOffset = Pivot +
		FVector2D((ArrowContact.X - Pivot.X) * BadgeTransform.Scale.X,
			(ArrowContact.Y - Pivot.Y) * BadgeTransform.Scale.Y) + BadgeTransform.Translation;
	BadgeSlot->SetPosition(PressLocal - ContactOffset - FVector2D(0.f,
		FMath::Max(PressFeedbackGap, 0.f) + FMath::Max(PressFeedbackRise, 0.f) * Bounce));
	Img_PressArrow->SetRenderOpacity(FMath::Clamp(PressFeedbackOpacity, 0.f, 1.f) * (0.82f + 0.18f * Bounce));
}

void UFactoryBoostWidget::ApplyEntrance(float TranslationY, float Scale, float Opacity)
{
	if (!BadgeRoot)
	{
		return;
	}

	FWidgetTransform BadgeTransform = AuthoredBadgeTransform;
	BadgeTransform.Translation.Y += TranslationY;
	BadgeTransform.Scale = FVector2D(
		AuthoredBadgeTransform.Scale.X * Scale,
		AuthoredBadgeTransform.Scale.Y * Scale);
	BadgeRoot->SetRenderTransform(BadgeTransform);
	BadgeRoot->SetRenderOpacity(AuthoredBadgeOpacity * Opacity);
}
