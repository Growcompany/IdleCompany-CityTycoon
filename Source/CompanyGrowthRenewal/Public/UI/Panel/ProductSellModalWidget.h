// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "Enum/ProductSortMode.h"
#include "Data/WorldMapTypes.h"
#include "ProductSellModalWidget.generated.h"

class UPanelWidget;
class UScrollBox;
class UButton;
class UEditableTextBox;
class UImage;
class UCommonTextBlock;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UButtonWidget;
class UCloseButtonWidget;
class UItemCardSlotWidget;
class UCountrySellRowWidget;
class UStatRowWidget;
class UWidgetSwitcher;
class UTradePort;
class UCountryMarketManager;

/**
 * UProductSellModalWidget
 * 물품거래소 모달 (WBP: UI_ProductSellModal). 좌(인벤 카드 그리드) + 우(11국 라디오 + 미리보기 + 판매).
 *
 * 데이터 흐름:
 *  - SetItems(TArray<FSellableItem>) 외부에서 인벤 주입
 *  - 좌측 ItemCardSlot 클릭 → 우측 11국 라디오 활성화
 *  - 라디오 클릭 → TradePort.PreviewSell 호출 → 미리보기 갱신
 *  - 판매 버튼 → TradePort.SellProduct → 매니저가 트랜잭션 처리 (Money/MC + ConsumeDemand + 인벤)
 *
 * 구독 모델:
 *  - OnDemandChanged 자동 구독 → 11국 효율% 자동 갱신
 *  - BackgroundBtn 클릭 → 모달 닫기
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProductSellModalWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** 외부에서 인벤 데이터 주입. 위젯 활성화 후 호출. */
	UFUNCTION(BlueprintCallable, Category = "Product Sell")
	void SetItems(const TArray<FSellableItem>& InItems);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ── 헤더 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn;

	// ── 좌측 상단: 산업 필터 탭 (라디오, CommonButtonGroupBase) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> TabAllBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> TabElectronicsBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> TabSemiconductorBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> TabAutomobileBtn;

	// ── 좌측 상단: 정렬 토글 (라디오, CommonButtonGroupBase) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SortDefaultBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SortPriceBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SortQuantityBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SortNameBtn;

	// ── 11국 라디오 정렬 토글 (카드 선택 시 효율순/수요순 전환) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SortEfficiencyBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SortDemandBtn;

	// ── 좌측: 인벤 카드 그리드 (WrapBox / ScrollBox 둘 다 호환) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> InventoryContainer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> InventoryEmptyText;

	// ── 좌측 하단: 선택된 물품 요약 (B안 — 카드 그리드 아래) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectedIconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SelectedNameText;

	// StatRow 합성 위젯 사용 — WBP 측 UIE_StatRow_C 인스턴스 매칭. SetStatValue 로 값 갱신.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> SelectedHoldingText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> SelectedPriceText;

	// ── 좌측 하단: 확장 정보 (옵션 A) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LargeSelectedIconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SelectedGradeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> SelectedTotalValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SelectedDescriptionText;

	// 스켈레톤 상태 안내 (인벤 0 시 좌하단 안내 텍스트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> EmptyStateHintText;

	// 좌하단 정보 영역 swap (Index 0: 정상 정보, Index 1: 빈 상태 안내)
	// 있으면 SetActiveWidgetIndex 로 swap. 없으면 개별 Visibility 토글 fallback.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> SelectedInfoSwitcher;

	// ── 좌측 하단: 수량 컨트롤 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> QuantityInputBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> QuantityDecBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> QuantityIncBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> QuantityMaxBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> QuantityMinBtn;

	// ── 우측: 11국 라디오 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> CountryScrollBox;

	// ── 우측: 미리보기 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> PreviewMoneyText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> PreviewMarketCapText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> PreviewQuantityText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> SellButton;

private:
	UPROPERTY()
	TArray<FSellableItem> Items;

	UPROPERTY()
	TArray<TObjectPtr<UItemCardSlotWidget>> SpawnedItemCards;

	// SpawnedItemCards[k] 의 원본 Items 인덱스 (정렬/필터 후 표시 순서와 원본 순서가 다를 수 있음)
	TArray<int32> SpawnedCardOriginalIndices;

	UPROPERTY()
	TArray<TObjectPtr<UCountrySellRowWidget>> SpawnedCountryRows;

	UPROPERTY()
	TObjectPtr<UTradePort> TradePort;

	UPROPERTY()
	TObjectPtr<UCountryMarketManager> MarketMgr;

	// 산업 탭 / 정렬 토글 ButtonGroup (NewObject 로 생성)
	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> IndustryTabGroup;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> SortButtonGroup;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> CountrySortGroup;

	int32 SelectedItemIndex = INDEX_NONE;
	ECountryType SelectedCountry = ECountryType::None;
	int64 SelectedQuantity = 0;

	// 필터/정렬 상태
	ECompanyType IndustryFilter = ECompanyType::None;  // None = 전체
	// WBP 의 SortButtonGroup 첫 버튼이 SortPriceBtn 이라 default 도 Price 로 일치 (group selected 와 mode 매칭).
	// SortDefaultBtn 추가 시 default 를 EProductSortMode::Default 로 복귀.
	EProductSortMode SortMode = EProductSortMode::Price;
	ECountrySortMode CountrySortMode = ECountrySortMode::Efficiency;

	// 정렬 방향. false = 내림차순 (높은값 우선, default), true = 오름차순. 같은 모드 재클릭 시 토글.
	bool bInventorySortAscending = false;
	bool bCountrySortAscending = false;

	void RebuildInventoryList();
	void RebuildCountryList();
	void OnItemClickedByIndex(int32 ItemIndex);
	void OnCountryClicked(UCountrySellRowWidget* Row);

	void RefreshSelectedItemUI();
	void RefreshPreview();
	void RefreshAllEfficiencies();

	void SetSelectedQuantity(int64 NewQty);

	UFUNCTION()
	void HandleSellClicked();

	UFUNCTION()
	void HandleQtyDec();

	UFUNCTION()
	void HandleQtyInc();

	UFUNCTION()
	void HandleQtyMax();

	UFUNCTION()
	void HandleQtyMin();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleDimClicked();

	UFUNCTION()
	void HandleCountryRowClicked(UCountrySellRowWidget* Row);

	UFUNCTION()
	void HandleDemandChanged(ECountryType Country, ECompanyType Industry, float NewRatio);

	const FSellableItem* GetSelectedItem() const;

	// 자동 첫 카드 선택 — 인벤 0 / 표시 카드 0 이면 빈 상태 (멘트 한 줄)
	void AutoSelectFirstOrSkeleton();

	// 빈 상태 안내 ON/OFF: 좌하단 슬롯 collapse + 컨트롤 disable + 안내 텍스트 표시
	void SetEmptyState(bool bShow);

	// 산업 탭 / 정렬 그룹 셋업 (NativeOnActivated 에서 1회)
	void SetupIndustryTabGroup();
	void SetupSortButtonGroup();
	void SetupCountrySortGroup();

	// 11국 라디오 재정렬 (효율순 / 수요순). SpawnedCountryRows 순서 변경 + ScrollBox 자식 재배치.
	void SortCountriesByMode();

	UFUNCTION()
	void HandleIndustryTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void HandleSortChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void HandleCountrySortChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// 같은 모드 재클릭 → 방향 토글, 다른 모드 클릭 → mode 변경 + default 내림차순.
	// OnClicked 단일 경로로 처리 (OnSelectedButtonBaseChanged 와 순서 의존성 회피).
	void HandleSortDefaultClicked();
	void HandleSortPriceClicked();
	void HandleSortQuantityClicked();
	void HandleSortNameClicked();
	void HandleSortEfficiencyClicked();
	void HandleSortDemandClicked();

	// 현재 mode + ascending 상태에 따라 sort 버튼 텍스트에 방향 표시 (selected 만 "라벨 v"/"라벨 ^").
	void UpdateSortButtonTexts();
};
