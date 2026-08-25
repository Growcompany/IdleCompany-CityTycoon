// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingSegmentRowWidget.generated.h"

class UButtonWidget;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingSegmentChanged, int32);

// 설정 행 부품 — 라벨 + 배타선택 세그먼트 버튼 최대 3개 (품질/FPS 등)
UCLASS()
class COMPANYGROWTHRENEWAL_API USettingSegmentRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureSegments(const FText& InLabel, const FText& InCaption, const TArray<FText>& Options, int32 InitialIndex);

	FOnSettingSegmentChanged OnSegmentChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

private:
	UFUNCTION()
	void HandleSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UPROPERTY()
	UCommonButtonGroupBase* SegmentGroup = nullptr;

	// 프로그래매틱 선택(Configure)이 사용자 콜백으로 새지 않게 가드
	bool bConfiguring = false;
};
