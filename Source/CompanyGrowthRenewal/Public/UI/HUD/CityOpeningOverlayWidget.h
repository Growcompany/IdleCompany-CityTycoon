#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CityOpeningOverlayWidget.generated.h"

class UButtonWidget;
class UCommonTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnCityOpeningSkipped);

/** 도시에서 첫 공장으로 이동하는 짧은 오프닝의 자막과 건너뛰기 입력. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCityOpeningOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCityOpeningOverlayWidget(const FObjectInitializer& ObjectInitializer);

	void SetOpeningProgress(float ElapsedSeconds);

	FOnCityOpeningSkipped OnSkip;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CaptionText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> SkipButton = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Opening|Copy")
	FText CityCaption;

	UPROPERTY(EditDefaultsOnly, Category = "Opening|Copy")
	FText FactoryCaption;

private:
	void HandleSkipClicked();
};
