// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "CountrySellRowWidget.generated.h"

class UCommonTextBlock;
class UCommonButtonBase;
class UWidget;
class UProgressBar;
class UImage;
class UCountryMarketManager;
struct FCountryMarketState;

/**
 * UCountrySellRowWidget
 * 자유판매 모달 우측 11국 라디오의 한 행 + (Country, Industry) 셀의 수요 게이지.
 * 옛 UDemandGaugeWidget 의 모든 기능 흡수 — 게이지 element 와 라디오 행이 하나로 통합.
 *
 * 구성: RowButton(클릭) + SelectedBorder + 국기/국가명 + 게이지(ProgressBar/RatioText) + 효율%
 *
 * 구독 모델:
 *  - SetData() 시 UCountryMarketManager.OnDemandChanged 자동 구독
 *  - ConsumeDemand / Tick 회복 시 자동 UI 갱신 (NativeTick 폴링 안 함)
 *
 * 표시 항목 (모두 BindWidgetOptional — WBP에 있으면 채움):
 *  - ProgressBar       : 가로 게이지 (Ratio 0~1, 색상 lerp 자동)
 *  - RatioText         : "750 / 1,000" (Current / Capacity 수량 표시, hero 슬롯)
 *  - IndustryNameText  : "반도체" (산업 이름, DT_CompanyInfo.DisplayName)
 *  - CountryNameText   : "한국"
 *  - RecoveryText      : "+50/min" (분당 회복량)
 *  - CapacityText      : "750 / 1,000" (RatioText 와 동일 정보 — WBP 에서 둘 중 하나만 사용 권장)
 *  - HubBadge          : 무역 허브 별표 (USA/Singapore만 표시)
 *  - EfficiencyText    : "+25%" (외부에서 SetEfficiencyDelta 로 전달)
 *  - IconImage         : 국기 아이콘 (DT_CountryInfo.FlagIcon 자동 로드)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountrySellRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountryRowClicked, UCountrySellRowWidget*, Row);

	UPROPERTY(BlueprintAssignable, Category = "Country Row|Events")
	FOnCountryRowClicked OnRowClicked;

	/** 행 데이터 지정. Industry는 모달이 선택한 아이템의 산업과 일치해야 함. 매니저 구독 + 즉시 UI 갱신. */
	UFUNCTION(BlueprintCallable, Category = "Country Row")
	void SetData(ECountryType InCountry, ECompanyType InIndustry);

	UFUNCTION(BlueprintCallable, Category = "Country Row")
	void SetSelected(bool bInSelected);

	/** 모달이 11국 PreviewSell 비교 후 평균 대비 ±% 통보. */
	UFUNCTION(BlueprintCallable, Category = "Country Row")
	void SetEfficiencyDelta(float DeltaPercent);

	UFUNCTION(BlueprintPure, Category = "Country Row")
	ECountryType GetCountry() const { return Country; }

	UFUNCTION(BlueprintPure, Category = "Country Row")
	ECompanyType GetIndustry() const { return Industry; }

	/** 수량·주문과 무관한 판매 배율. MoneyBias × DemandMul × IndustryPriceMul × (허브면 PortPriceMul).
	 *  BulkMul/MatchMul 은 판매 시점에만 정해지므로 제외 — UTradePort::ComputeSell 과 같은 항 구성. */
	UFUNCTION(BlueprintPure, Category = "Country Row")
	float GetSellMultiplier() const;

	/** 외부에서 강제 갱신 (드물게 필요). */
	UFUNCTION(BlueprintCallable, Category = "Country Row")
	void RefreshFromManager();

	/** Ratio (0~1) → 색상 lerp. 0=붉은빛(포화), 0.5=노랑, 1=청록(만수요).
	 *  CountryNameWidget 의 닷 인디케이터 등 외부에서도 일관 색상을 위해 재사용. */
	static FLinearColor GetGaugeColor(float Ratio);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> RowButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RatioText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> IndustryNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CountryNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RecoveryText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CapacityText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HubBadge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> EfficiencyText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// 판매 컨텍스트 라벨 (DT_CountryMarketRole) — 카드 선택 시에만 노출, Industry None 이면 숨김.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MarketRoleText;

	// 기준가 (DT_CountryDemand.PriceMul). 정적이라 ApplyStaticTexts 에서 1회, 셀 없으면 Collapsed.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PriceMulText;

	// 실효 판매 배율 — 수요 의존이라 ApplyRatioVisuals 에서 매 갱신.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SellMulText;

	// 산업 시그니처색 스와치 (DT_CompanyInfo.AccentColor 틴트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IndustryAccentImage;

	/** 디자이너 프리뷰용 — NativePreConstruct에서 PreviewRatio 로 표시. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country Row|Preview", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float PreviewRatio = 0.6f;

private:
	UPROPERTY()
	ECountryType Country = ECountryType::None;

	UPROPERTY()
	ECompanyType Industry = ECompanyType::None;

	UPROPERTY()
	bool bIsTradeHub = false;

	UPROPERTY()
	TObjectPtr<UCountryMarketManager> MarketMgr;

	void HandleButtonClicked();

	UFUNCTION()
	void HandleDemandChanged(ECountryType InCountry, ECompanyType InIndustry, float NewRatio);

	void ApplyState(const FCountryMarketState& State);
	void ApplyRatioVisuals(float Ratio);
	void ApplyStaticTexts();
};
