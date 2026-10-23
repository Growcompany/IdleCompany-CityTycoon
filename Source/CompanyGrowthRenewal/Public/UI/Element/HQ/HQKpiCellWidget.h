// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HQKpiCellWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

/**
 * 본사 운영 현황 탭의 회사 지표 한 칸 (WBP = UIE_HQKpiCell).
 * 라벨·아이콘은 인스턴스 정적 값, 수치만 패널이 주입한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQKpiCellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetValue(const FText& Value, const FText& Unit);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "HQ|Kpi")
	FText Label;

	UPROPERTY(EditAnywhere, Category = "HQ|Kpi")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> UnitText;
};
