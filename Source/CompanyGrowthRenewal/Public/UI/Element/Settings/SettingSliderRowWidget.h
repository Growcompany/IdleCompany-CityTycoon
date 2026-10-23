// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Blueprint/UserWidget.h"
#include "SettingSliderRowWidget.generated.h"

class UButton;
class UBorder;
class UImage;
class UProgressBar;
class USlider;
class UTextBlock;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingSliderValueChanged, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingSliderMuteToggled, bool);
DECLARE_MULTICAST_DELEGATE(FOnSettingSliderReleased);

/**
 * 설정 행 부품 — 스피커 뮤트 버튼 + 라벨 + 슬라이더 + 퍼센트.
 * 매니저를 모른다: Configure + 델리게이트만 노출, 배선은 소유 패널 책임.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API USettingSliderRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureSlider(const FText& InLabel, float InValue01, bool bInMuted);

	float GetValue() const;
	bool IsMutedState() const { return bMuted; }

	FOnSettingSliderValueChanged OnValueChangedDelegate;
	FOnSettingSliderMuteToggled OnMuteToggledDelegate;
	FOnSettingSliderReleased OnSliderReleased;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UButton* MuteButton;

	// 스피커 온/오프 글리프 — 뮤트 상태에 따라 둘 중 하나만 표시
	UPROPERTY(meta = (BindWidget))
	UImage* IconOn;

	UPROPERTY(meta = (BindWidget))
	UImage* IconOff;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* IconChipOn;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* IconChipOff;

	UPROPERTY(EditAnywhere, Category = "Setting Row|Icon")
	TSoftObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, Category = "Setting Row|Icon")
	FVector2D IconSize = FVector2D(42.0f, 42.0f);

	UPROPERTY(EditAnywhere, Category = "Setting Row|Icon")
	FMargin IconPadding = FMargin(0.0f);

	UPROPERTY(meta = (BindWidget))
	UTextBlock* LabelText;

	UPROPERTY(meta = (BindWidget))
	USlider* ValueSlider;

	// 채워진 게이지 — 슬라이더 뒤에 깔려 현재 값까지 채움(USlider 자체엔 fill 개념 없음)
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* FillBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* FillBarMuted;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ValueText;

	UPROPERTY(EditAnywhere, Category = "Setting Row|Muted Value")
	bool bOverrideMutedValueStyle = false;

	UPROPERTY(EditAnywhere, Category = "Setting Row|Muted Value", meta = (EditCondition = "bOverrideMutedValueStyle"))
	FSlateFontInfo MutedValueFont;

	UPROPERTY(EditAnywhere, Category = "Setting Row|Muted Value", meta = (EditCondition = "bOverrideMutedValueStyle"))
	FLinearColor MutedValueColor = FLinearColor::FromSRGBColor(FColor(0x97, 0xA3, 0xB6));

private:
	UFUNCTION()
	void HandleSliderChanged(float NewValue);

	UFUNCTION()
	void HandleSliderCaptureEnd();

	UFUNCTION()
	void HandleMuteClicked();

	void RefreshMuteVisual();
	void RefreshValueText(float Value01);
	void ApplyIconProperties();

	FSlateFontInfo NormalValueFont;
	FSlateColor NormalValueColor;
	bool bHasNormalValueStyle = false;

	bool bMuted = false;

	// 프로그래매틱 SetValue(Configure)가 외부 콜백으로 새지 않게 가드 — UE5.4 USlider::SetValue는 델리게이트를 발동시킴
	bool bConfiguring = false;
};
