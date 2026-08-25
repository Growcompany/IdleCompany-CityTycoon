// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TraitSetChipWidget.generated.h"

class UCommonTextBlock;

/**
 * UIE_TraitSetChip
 * 특성 탭 세트 보너스 요약 칩 1개. 활성(그린)/진행중(뮤트)은 WBP 소유 배경 2장의 가시성 토글.
 * 패널(SetBonusBand)이 CalculateSetBonuses 결과대로 런타임 생성.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTraitSetChipWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TraitSetChip")
	void SetChipData(const FText& InLabel, bool bInActive);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ChipText;

	// 활성/뮤트 배경 브러시는 WBP 소유 ― C++ 는 가시성만 토글
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActiveBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MutedBG;
};
