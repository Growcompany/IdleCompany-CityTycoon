#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "PotentialOddsWidget.generated.h"

class UCloseButtonWidget;
class UCommonTextBlock;
class UVerticalBox;

/** 잠재능력 명함의 현재 등급별 공통 상승 확률표. */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UPotentialOddsWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<class UButton> BackgroundBtn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* OddsBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardName0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardName1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardName2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardCap0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardCap1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardCap2;

private:
	void Rebuild();
	void UpdateCardCeilings(class UTableManagerSubsystem* TableMgr);

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void HandleDimClicked();
};
