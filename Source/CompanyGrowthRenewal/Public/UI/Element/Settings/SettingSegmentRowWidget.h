// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingSegmentRowWidget.generated.h"

class UButtonWidget;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UCommonButtonStyle;
class UTextBlock;
class UImage;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingSegmentChanged, int32);

// 설정 행 부품 — 라벨 + 배타선택 세그먼트 버튼 최대 3개 (품질/FPS 등)
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API USettingSegmentRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureSegments(const FText& InLabel, const FText& InCaption, const TArray<FText>& Options, int32 InitialIndex);

	FOnSettingSegmentChanged OnSegmentChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;
	void ApplyIconProperties();
	void RefreshOptionTextStyles();

	UPROPERTY(meta = (BindWidget))
	UTextBlock* LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* CaptionText;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Seg0;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Seg1;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Seg2;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* RowIcon;

	// 행마다 다른 글리프 — UI_SettingsPanel 의 인스턴스 Details 에서 지정한다
	UPROPERTY(EditAnywhere, Category = "Setting Row|Icon")
	TSoftObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Setting Row|Icon")
	FVector2D IconSize = FVector2D(38.0f, 38.0f);

	UPROPERTY(EditAnywhere, Category = "Setting Row|Icon")
	FMargin IconPadding = FMargin(0.0f);

	UPROPERTY(EditAnywhere, Category = "Setting Row|Options")
	TSubclassOf<UCommonButtonStyle> OptionButtonStyle;

private:
	UFUNCTION()
	void HandleSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UPROPERTY()
	UCommonButtonGroupBase* SegmentGroup = nullptr;

	// 프로그래매틱 선택(Configure)이 사용자 콜백으로 새지 않게 가드
	bool bConfiguring = false;
};
