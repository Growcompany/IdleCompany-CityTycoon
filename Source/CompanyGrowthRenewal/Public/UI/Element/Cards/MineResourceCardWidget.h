// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/CountryType.h"
#include "Enum/ResourceType.h"
#include "MineResourceCardWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UButtonWidget;
class UEditableTextBox;
class UStatRowWidget;
class UTexture2D;

/**
 * UMineResourceCardWidget
 * MinePicker 모달 안에서 자원 1종을 카드로 표시 (UIE_MineResourceCard).
 * +/- 버튼 + 직접 입력으로 라인 저장 한도(N)를 정해 CreateMineLine에 전달.
 * N 변경 시 1회 설치비와 한도 도달 시간을 갱신한다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UMineResourceCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResourceCardSelected, EResourceType, Resource, int64, Quantity);

	UPROPERTY(BlueprintAssignable, Category = "MineResourceCard|Events")
	FOnResourceCardSelected OnSelected;

	// MinePicker 가 카드 spawn 시 호출. Country 는 Plus 버튼 상한 + rate 쿼리용.
	UFUNCTION(BlueprintCallable, Category = "MineResourceCard")
	void SetResource(ECountryType Country, EResourceType Resource);

	UFUNCTION(BlueprintPure, Category = "MineResourceCard")
	EResourceType GetResource() const { return CachedResource; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> EntityImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductionNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> AcceptButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> NumberEditableTextBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MinusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> PlusButton;

	// 사용자 WBP — 1회 설치비 StatRow. N 에 비례 (BasePrice * N).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> Text_Cost;

	// 사용자 WBP — 한도 도달 시간 StatRow. N 에 비례 (N / RatePerSec).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> Text_Time;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineResourceCard|Quantity")
	int64 MinQuantity = 1;

private:
	ECountryType CachedCountry = ECountryType::None;
	EResourceType CachedResource = EResourceType::None;
	int64 CurrentQuantity = 1;

	void ClampQuantity();
	void RefreshQuantityUI();
	void RefreshCostText();
	void RefreshTimeText();
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	int64 ComputeCost(int64 Quantity) const;
	double ComputeProductionTimeSec(int64 Quantity) const;

	UFUNCTION()
	void HandleSelectClicked();

	UFUNCTION()
	void HandleMinusClicked();

	UFUNCTION()
	void HandlePlusClicked();

	UFUNCTION()
	void HandleQuantityCommitted(const FText& Text, ETextCommit::Type CommitType);
};
