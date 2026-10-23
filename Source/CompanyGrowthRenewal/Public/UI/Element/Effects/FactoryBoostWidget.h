// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactoryBoostWidget.generated.h"

class UCanvasPanel;
class UCommonTextBlock;
class UImage;

/** 공장 홀드 배수 배지. 화면 배치는 InGameLayer가 담당한다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UFactoryBoostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BeginBoost(float Multiplier, const FVector2D& PressAbsolutePosition);
	void EndBoost();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* BadgeRoot = nullptr;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Txt_Multiplier = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* Img_PressArrow = nullptr;

	UPROPERTY(EditAnywhere, Category = "Boost", meta = (ClampMin = "0.01"))
	float EntranceDuration = 0.33f;

	UPROPERTY(EditAnywhere, Category = "Boost", meta = (ClampMin = "0.0"))
	float EntranceRise = 36.f;

	UPROPERTY(EditAnywhere, Category = "Boost|PressFeedback", meta = (ClampMin = "0.01"))
	float PressFeedbackPeriod = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Boost|PressFeedback", meta = (ClampMin = "0.0"))
	float PressFeedbackRise = 18.f;

	UPROPERTY(EditAnywhere, Category = "Boost|PressFeedback", meta = (ClampMin = "0.0"))
	float PressFeedbackGap = 10.f;

	UPROPERTY(EditAnywhere, Category = "Boost|PressFeedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PressFeedbackOpacity = 0.32f;

private:
	void ApplyEntrance(float TranslationY, float Scale, float Opacity);
	void UpdatePressFeedback(const FGeometry& MyGeometry, float InDeltaTime);

	TWeakObjectPtr<UCanvasPanel> CachedBadgeRoot;
	FWidgetTransform AuthoredBadgeTransform;
	float AuthoredBadgeOpacity = 1.f;
	float EntranceElapsed = 0.f;
	bool bEntrancePlaying = false;
	FVector2D PressAnchorAbsolute = FVector2D::ZeroVector;
	float PressFeedbackElapsed = 0.f;
	bool bBoostActive = false;
};
