#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DevelopmentBannerWidget.generated.h"

class UCanvasPanel;
class UCommonTextBlock;
class UImage;
class UOverlay;
class UUMGSequencePlayer;
class UWidgetAnimation;

UCLASS()
class COMPANYGROWTHRENEWAL_API UDevelopmentBannerWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	UFUNCTION(BlueprintCallable, Category = "Development Banner")
	void SetBannerText(const FText& Title, const FText& Subtitle);
	UFUNCTION(BlueprintCallable, Category = "Development Banner")
	void SetAccentColor(const FLinearColor& Color);
	UFUNCTION(BlueprintCallable, Category = "Development Banner")
	void StartPresentation(bool bReducedMotion);
	void SetPresentationPaused(bool bPaused);
	void SetPresentationSpeed(float Speed);
	float GetPresentationDuration() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor BackgroundTopColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor BackgroundBottomColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor OutlineColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor ShadowColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor HighlightColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor StarColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor StarOutlineColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor TitleColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor SubtitleColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance") FLinearColor BadgeColor = FLinearColor::White;

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOverlay> AnimationRoot;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> BannerCanvas;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> BackgroundImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> OutlineImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> ShadowImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> HighlightImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> LeftStarImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> LeftStarOutlineImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> RightStarImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> RightStarOutlineImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SubtitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CountBadgeImage;
	UPROPERTY(Transient, meta = (BindWidgetAnim)) TObjectPtr<UWidgetAnimation> Presentation;

private:
	UPROPERTY(Transient) TObjectPtr<UUMGSequencePlayer> PresentationPlayer;
	float PausedTime = 0.f;
	float PresentationSpeed = 1.f;
	bool bPresentationPaused = false;
	bool bReduced = false;
};
