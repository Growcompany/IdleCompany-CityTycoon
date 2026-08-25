// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CloseButtonWidget.generated.h"

class UButton;
class USizeBox;

/**
 * 재사용 가능한 닫기 버튼 위젯
 * - 각 패널에서 OnCloseClicked 델리게이트에 바인딩하여 사용
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCloseButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 닫기 버튼 클릭 델리게이트
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCloseClicked);

	UPROPERTY(BlueprintAssignable, Category = "CloseButton")
	FOnCloseClicked OnCloseClicked;

protected:
	UPROPERTY(meta = (BindWidget))
	USizeBox* RootSizeBox;

	UPROPERTY(meta = (BindWidget))
	UButton* CloseBtn;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleCloseButtonClicked();
};
