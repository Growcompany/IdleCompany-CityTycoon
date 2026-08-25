#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/LootBoxRarity.h"
#include "RecruitmentResultPanelWidget.generated.h"

class UButtonWidget;
class UResourceWidget;
class UTextBlock;
class UImage;

DECLARE_MULTICAST_DELEGATE(FOnConfirmExitRequested);
DECLARE_MULTICAST_DELEGATE(FOnRetryRequested);

UCLASS()
class COMPANYGROWTHRENEWAL_API URecruitmentResultPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	FOnConfirmExitRequested OnConfirmExitRequested;

	void RequestClose() { CloseWithAnimation(); }

	void InitResourceDisplay(int32 BuildingIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButtonWidget* OutButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Employee;

private:
	UFUNCTION()
	void OnConfirmClicked();
};
