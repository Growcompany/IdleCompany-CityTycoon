// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Element/HQ/HQPanelTypes.h"
#include "HQIndustryCardWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UBorder;
class UMaterialInterface;
class UTexture2D;
struct FCompanyInfoTable;

/**
 * 본사 운영 현황 탭의 업종 카드 (WBP = UIE_HQIndustryCard, 런타임 생성 = EWidgetType::HQIndustryCard).
 * 이름·분류·특징·색·아이콘은 DT_CompanyInfo 행, 수치는 패널 집계를 받는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQIndustryCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetIndustry(const FCompanyInfoTable& Info, EHQIndustryState State, const FHQIndustryMetrics& Metrics);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> RibbonBorder;

	// 리본 위쪽 색 — 흰→투명 세로 그라데이션 텍스처에 틴트해 리본 아래쪽 색 위에 겹친다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> RibbonSheen;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> NotchRing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> LockBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CategoryText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> MetricsBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> BuildingsValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EmployeesValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TierValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> IncomeValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> FeatureBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> FeatureTitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> FeatureDescText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> FeatureSubText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> StatusStrip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StatusText;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Industry")
	FLinearColor LockedRibbonTop = FLinearColor(FColor(0x9A, 0xA5, 0xB3));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Industry")
	FLinearColor LockedRibbonBottom = FLinearColor(FColor(0x7F, 0x8B, 0x9A));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Industry")
	FLinearColor NeutralStripColor = FLinearColor(FColor(0xE3, 0xEA, 0xF3));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Industry")
	FLinearColor NeutralStripInk = FLinearColor(FColor(0x2C, 0x48, 0x65));

	// 잠김 아이콘 흑백 — 텍스처 파라미터 "Icon". 비면 불투명도만 낮춘다
	UPROPERTY(EditDefaultsOnly, Category = "HQ|Industry")
	TObjectPtr<UMaterialInterface> LockedIconMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Industry")
	float LockedIconOpacity = 0.55f;

private:
	void ApplyIcon(UTexture2D* Texture, bool bLocked);
};
