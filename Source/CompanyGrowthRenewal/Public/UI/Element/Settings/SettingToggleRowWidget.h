// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingToggleRowWidget.generated.h"

class UBorder;
class UCheckBox;
class UImage;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingToggleChanged, bool);

// 설정 행 부품 — 라벨 + 보조 캡션 + 토글 (연출 감소 등 on/off 설정)
UCLASS()
class COMPANYGROWTHRENEWAL_API USettingToggleRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureToggle(const FText& InLabel, const FText& InCaption, bool bInitialOn);

	bool IsOn() const;

	FOnSettingToggleChanged OnToggleChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* LabelText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CaptionText;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* ToggleCheck;

	// On/Off 스위치 비주얼 — 노브가 좌(Off)/우(On)로 이동, 트랙 색도 전환 (있으면 구동)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* Knob;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* TrackBorder;

private:
	UFUNCTION()
	void HandleCheckChanged(bool bIsChecked);

	// 노브 위치 + 트랙 색을 on/off 상태에 맞춤
	void RefreshSwitchVisual(bool bOn);
};
