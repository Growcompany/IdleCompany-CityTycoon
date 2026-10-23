// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HQConditionCardWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UBorder;
class UImage;
class UTexture2D;

/**
 * 본사 성장 탭의 레벨업 조건 카드 (WBP = UIE_CondStatRow).
 * 아이콘은 인스턴스마다 정적이라 WBP 가 소유하고, 값·진행·판정 띠만 패널이 주입한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQConditionCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCondition(const FText& Label, const FText& Current, const FText& Target, float Percent, bool bMet, const FText& MissingText);

	// 0 이하 = 배지 숨김
	void SetTierBadge(int32 Tier);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "HQ|Condition")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CurrentText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TargetText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> StatusStrip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> StatusCheck;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TierBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TierBadgeText;

	// 판정 상태 색 — 런타임 주입값이라 클래스 기본값으로 둔다(정적 면색은 WBP 소유)
	UPROPERTY(EditDefaultsOnly, Category = "HQ|Condition")
	FLinearColor FillPendingColor = FLinearColor(0.047f, 0.328f, 0.745f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Condition")
	FLinearColor FillMetColor = FLinearColor(0.263f, 0.788f, 0.369f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Condition")
	FLinearColor StripNeedColor = FLinearColor(FColor(0xE3, 0xEA, 0xF3));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Condition")
	FLinearColor StripMetColor = FLinearColor(FColor(0xDD, 0xF2, 0xD2));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Condition")
	FLinearColor InkNeedColor = FLinearColor(FColor(0x2C, 0x48, 0x65));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Condition")
	FLinearColor InkMetColor = FLinearColor(FColor(0x2B, 0x74, 0x19));
};
