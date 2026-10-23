#include "UI/Panel/WorkstationPromotionWidget.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"

void UWorkstationPromotionWidget::SetPromotionInfo(int32 OldGradeNumber, int32 NewGradeNumber,
	const FText& EquipmentChange,
	float GradeSpeedDeltaPercentPoints, TSoftObjectPtr<UTexture2D> NewGradeImage)
{
	SetTitlePromotion(
		FText::Format(NSLOCTEXT("WorkstationPromotion", "GradeNumber", "{0}등급"), OldGradeNumber),
		FText::Format(NSLOCTEXT("WorkstationPromotion", "GradeNumber", "{0}등급"), NewGradeNumber),
		OldGradeNumber, NewGradeNumber);
	CachedEquipmentChange = EquipmentChange;
	CachedGradeSpeedDeltaPercentPoints = GradeSpeedDeltaPercentPoints;
	CachedGradeImage = MoveTemp(NewGradeImage);
}

void UWorkstationPromotionWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	RevealElapsed = 0.f;
	if (BackgroundBtn)
	{
		BackgroundBtn->SetIsEnabled(false);
	}
	if (EyebrowText)
	{
		EyebrowText->SetText(NSLOCTEXT("WorkstationPromotion", "Eyebrow", "책상을 강화했습니다"));
	}
	if (LevelUpTitleText)
	{
		LevelUpTitleText->SetText(NSLOCTEXT("WorkstationPromotion", "Headline", "UPGRADE!"));
	}
	if (RewardDescText)
	{
		RewardDescText->SetText(CachedEquipmentChange);
	}
	if (RewardPlate)
	{
		RewardPlate->SetVisibility(CachedEquipmentChange.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (EffectDeltaText)
	{
		EffectDeltaText->SetText(FText::Format(
			NSLOCTEXT("WorkstationPromotion", "SpeedDelta", "작업 속도 +{0}%p"),
			FText::AsNumber(CachedGradeSpeedDeltaPercentPoints)));
	}
	if (RetainedNoteText)
	{
		RetainedNoteText->SetText(NSLOCTEXT("WorkstationPromotion", "RetainedEffects",
			"세 항목의 누적 효과는 유지됩니다. 항목 레벨은 Lv.1부터 다시 강화할 수 있습니다."));
	}
	if (MedalBox)
	{
		MedalBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PromotionImage)
	{
		UTexture2D* GradeTexture = CachedGradeImage.LoadSynchronous();
		if (GradeTexture)
		{
			FSlateBrush Brush = PromotionImage->GetBrush();
			Brush.SetResourceObject(GradeTexture);
			const float LongEdge = FMath::Max(GradeTexture->GetSizeX(), GradeTexture->GetSizeY());
			const FVector2D ImageSize = LongEdge > 0.f
				? FVector2D(GradeTexture->GetSizeX(), GradeTexture->GetSizeY()) * (520.f / LongEdge)
				: FVector2D(520.f, 520.f);
			Brush.ImageSize = ImageSize;
			PromotionImage->SetBrush(Brush);
			PromotionImage->SetDesiredSizeOverride(ImageSize);
			PromotionImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PromotionImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			PromotionImage->SetRenderScale(FVector2D(0.72f, 0.72f));
			PromotionImage->SetRenderOpacity(0.f);
		}
		else
		{
			PromotionImage->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Warning, TEXT("[WorkstationPromotion] Grade image is missing for %s"),
				*CachedGradeImage.ToSoftObjectPath().ToString());
		}
	}
}

void UWorkstationPromotionWidget::NativeOnDeactivated()
{
	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->SetIsEnabled(true);
	}
	Super::NativeOnDeactivated();
}

void UWorkstationPromotionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RevealElapsed += InDeltaTime;

	if (PromotionImage && PromotionImage->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const float Arrival = FMath::Clamp((RevealElapsed - 0.33f) / 0.43f, 0.f, 1.f);
		const float Punch = 1.f + 0.065f * FMath::Sin(Arrival * PI);
		const float Scale = FMath::Lerp(0.72f, 1.f, FMath::InterpEaseOut(0.f, 1.f, Arrival, 3.f)) * Punch;
		PromotionImage->SetRenderScale(FVector2D(Scale, Scale));
		PromotionImage->SetRenderOpacity(Arrival);
	}
	if (BackgroundBtn && RevealElapsed >= 1.3f && !BackgroundBtn->GetIsEnabled())
	{
		BackgroundBtn->SetIsEnabled(true);
	}
}
