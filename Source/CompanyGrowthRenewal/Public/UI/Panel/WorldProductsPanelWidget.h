// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/CompanyType.h"
#include "WorldProductsPanelWidget.generated.h"

class UTextBlock;
class UButton;
class UButtonWidget;
class UCloseButtonWidget;
class UFont;
class UVerticalBox;
class UWorldMapManager;
class UTradePort;
class UTableManagerSubsystem;

/**
 * 월드맵 완성품 창고 패널 (우측 슬라이드인)
 *
 * 보유 완성품 목록 + 일괄 판매 (싱가포르 빠른 / 미국 +35%).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldProductsPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> ProductsContainer = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SellAllSingaporeBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SellAllUSABtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UWorldMapManager> WorldMapMgr = nullptr;

	UPROPERTY()
	TObjectPtr<UTradePort> TradePortMgr = nullptr;

	UPROPERTY()
	TObjectPtr<UTableManagerSubsystem> TableMgr = nullptr;

	// 코드 생성 TextBlock 전용 폰트 — plain UTextBlock 은 CUI_Style_Text_* 슬롯이 없어(CommonTextBlock 전용)
	// 폰트를 안 정하면 엔진 Roboto 폴백으로 한글이 깨진다.
	UPROPERTY()
	TObjectPtr<UFont> RowFont = nullptr;

	void Refresh();

	// 목록/빈 상태 행 1개 생성 (NEXON Regular + 시맨틱 스케일 사이즈 수동 일치)
	UTextBlock* MakeRowText(const FText& InText, int32 FontSize) const;

	UFUNCTION() void HandleProductChanged(FIntPoint ProductKey, int64 NewAmount);

	UFUNCTION() void OnSellAllSingaporeClicked();
	UFUNCTION() void OnSellAllUSAClicked();
	UFUNCTION() void OnCloseButtonClicked();
	UFUNCTION() void OnBackgroundClicked();

	FText GetProductDisplayName(ECompanyType Company, int32 ProjectIndex) const;
};
