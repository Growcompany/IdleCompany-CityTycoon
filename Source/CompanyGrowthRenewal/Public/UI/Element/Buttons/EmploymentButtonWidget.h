// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "EmploymentButtonWidget.generated.h"

class UButtonWidget;
/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmploymentButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnEmploymentButtonClicked();

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EmploymentButton;

};