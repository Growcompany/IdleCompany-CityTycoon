#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "PlotPurchasePanelWidget.generated.h"

class UButtonWidget;
class UCloseButtonWidget;
class UCommonButtonStyle;
class UCommonTextBlock;
class UImage;
class UResourceWidget;

/** The selected plot's compact, nonmodal purchase card in the bottom stack. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPlotPurchasePanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UPlotPurchasePanelWidget();

	FSimpleMulticastDelegate OnConfirm;
	void SetPurchaseDetails(const FText& Headline, const FText& Message, int64 Price, bool bInCanAfford, bool bInCanPurchase);
	UWidget* GetCloseButtonWidget() const;
	UWidget* GetConfirmButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnHandleBackAction() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UResourceWidget> PriceResource;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MessageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> ConfirmButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Plot Purchase|Appearance")
	TSubclassOf<UCommonButtonStyle> AvailableButtonStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Plot Purchase|Appearance")
	TSubclassOf<UCommonButtonStyle> BlockedButtonStyle;

private:
	UFUNCTION()
	void HandleCloseClicked();

	void HandleConfirmClicked();
	void ApplyPurchaseDetails();

	FText CachedHeadline;
	FText CachedMessage;
	int64 CachedPrice = 0;
	bool bCanAfford = false;
	bool bCanPurchase = false;
};
