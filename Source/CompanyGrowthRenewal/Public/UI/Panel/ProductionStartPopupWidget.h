// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/CountryType.h"
#include "Enum/ResourceType.h"
#include "Manager/WorldFactoryManager.h"
#include "Data/ProductionOrderData.h"
#include "ProductionStartPopupWidget.generated.h"

class UPanelWidget;
class UButton;
class UBorder;
class UImage;
class UCommonTextBlock;
class UButtonWidget;
class UCostActionButtonWidget;
class UModalFrameWidget;
class UProductionOrderRowWidget;
class UMaterialReqRowWidget;
class UResourceItemManager;
class UProductionOrderManager;
class UWorldFactoryManager;
class UWorldMapManager;

/**
 * UI_ProductionStartPopup
 * 공장 빈 라인의 "제품 선택" 클릭 시 뜨는 모달. 마스터·디테일 2단.
 *
 * 좌 = 주문서 레일(선택), 우 = 선택된 주문의 상세 + 재료 + 수량 + CTA.
 * 선택 수량은 주문 잔여까지 설정하고, 재료 가능 수량과 라인 상태를 별도로 보여준다.
 *
 * 소프트 캡: 스텝퍼는 주문서 잔여까지 올라간다. 하드 클램프하면 "얼마나 모자란지"를
 * 보여줄 자리가 사라져, 무엇을 더 캐야 하는지가 화면에서 없어진다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProductionStartPopupWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ProductionStart")
	void InitializeForCountry(ECountryType InCountry, int32 PreferredOrderID = 0);
	void CloseFromBack();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── 셸 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn;

	// ── 헤더 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> FactoryNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LineCountText;

	// 라인이 가득 차면 붉게 — "왜 못 누르는지"를 CTA 사유와 헤더 두 곳에서 잡는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> LineChipBorder;

	// ── 좌 레일 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> OrderRailBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> OrderCountText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyStateBox;

	// ── 우 상세 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DetailRoot;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DetailCoverImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DetailNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DetailSubText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DetailGradeBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DetailGradeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RemainChipText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PerUnitTimeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PerUnitCapText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PerUnitEnergyText;

	// "C 등급 · 주문 남은 8개 · 1개에 27초"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DetailInfoText;

	// ── 재료 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> MaterialGridBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MaterialLabelText;

	// 「+N종 더 있습니다」 ― 첫 줄 밖 칸이 스크롤에 숨을 때만 보인다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MaterialMoreText;

	// 판 높이상 스크롤 없이 보이는 재료 칸 수(3열 × 1줄). WBP 의 재료 칸 높이 상한과 함께 바꾼다
	UPROPERTY(EditAnywhere, Category = "Style|Layout")
	int32 MaterialVisibleCells = 3;

	// ── 액션 웰 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MinusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> PlusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> QuantityText;

	// 수량 프리셋 3종.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> BulkBtn_x1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> BulkBtn_x10;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> BulkBtn_x50;

	// 재료 상한까지 한 번에 — 강화 배율의 '최대' 기각과는 다른 축(반복 손맛이 아니라 배치 투자 상한)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MaxJumpButton;

	// 최대 수량을 정하는 재료가 있을 때만 펴지는 판 폭 주황 띠. CapacityText 가 그 안의 문장이다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> LimitBand;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CapacityText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SummaryText;

	// 생산 가능 수량 소비와 완료품 수령 후 라인 해제 안내.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ChainTimeText;

	// 막힌 사유는 인라인 라벨이 아니라 클릭 시 토스트로 알린다 (HandleAccept).
	// 칩 = 고른 수량("N개"), 라벨 = 시작 또는 막힌 사유. 회색이어도 클릭은 받는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCostActionButtonWidget> AcceptButton;

	// ===== 스타일 노브 (WBP Class Defaults) =====
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor CapacityInkNormal = FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.0f); // #ECEEF0

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor CapacityInkTight = FLinearColor(0.8632f, 0.5647f, 0.1518f, 1.0f);  // #F0C46B

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor CapacityInkOver = FLinearColor(0.8632f, 0.2232f, 0.1946f, 1.0f);   // #F0857F

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor LineChipNormal = FLinearColor(0.0241f, 0.0343f, 0.0603f, 1.0f);    // #2A3447 (S3)

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor LineChipFull = FLinearColor(0.0453f, 0.0241f, 0.0293f, 1.0f);      // #3A2A2E

	// CTA 시각 스왑 — 클릭은 항상 가능하고 막힌 사유는 문장으로 알린다
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<class UCommonButtonStyle> AcceptStyle_Enabled;

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<class UCommonButtonStyle> AcceptStyle_Disabled;

	// 파랑 위 크림 / 밝은 회색 위 차콜 (카탈로그 §6 라이트 면 라벨)
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	FSlateColor AcceptLabelInk = FSlateColor(FLinearColor(1.0f, 0.9387f, 0.8714f, 1.0f));      // #FFF8F0

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	FSlateColor AcceptDisabledLabelInk = FSlateColor(FLinearColor(0.0103f, 0.0168f, 0.0296f, 1.0f)); // #1B2330

	// 수량 칸 버튼(최소 · − · + · 최대) 글자 ― 못 누를 때는 흐린 잉크로 모양이 아니라 글자가 말한다
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	FSlateColor StepLabelInk = FSlateColor(FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.0f));      // #ECEEF0

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	FSlateColor StepDisabledLabelInk = FSlateColor(FLinearColor(0.1119f, 0.1301f, 0.1714f, 1.0f)); // #5E6573

	// 배율 칩 선택 표시 — 라디오 그룹 없이 스타일 스왑으로. CommonButtonGroupBase 는 탭 전용 규약이라
	// 여기선 과하고, SetIsSelected 수동 토글은 클릭 타이밍과 꼬인다(카탈로그 경고).
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<class UCommonButtonStyle> BulkStyle_Selected;

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<class UCommonButtonStyle> BulkStyle_Unselected;

private:
	ECountryType Country = ECountryType::None;

	// 선택된 주문. 0 = 미선택(주문서 없음)
	int32 SelectedOrderID = 0;
	int32 CurrentQuantity = 1;
	int32 InitialPreferredOrderID = 0;
	bool bStartingProduction = false;
	FTimerHandle OrderRailRefreshTimerHandle;

	UPROPERTY()
	TArray<TObjectPtr<UProductionOrderRowWidget>> SpawnedRows;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialReqRowWidget>> SpawnedMatRows;

	// 제품 입력(칩) 행. 재료 행과 개수 대응(SpawnedMatRows 안의 부품 칸) ― 이름·아이콘 출처가 DT_Project_* 라 따로 가리킨다.
	UPROPERTY()
	TObjectPtr<UMaterialReqRowWidget> SpawnedProductRow;

	// 칸 순서(원자재 + 부품 입력 = 키 None). 최대를 정하는 칸이 맨 앞 ― 판 높이상 첫 줄만 보일 때가 많다.
	TArray<TPair<EResourceType, int32>> MaterialOrder;

	UPROPERTY()
	TObjectPtr<UResourceItemManager> ResourceMgr;

	UPROPERTY()
	TObjectPtr<UWorldFactoryManager> FactoryMgr;

	UPROPERTY()
	TObjectPtr<UProductionOrderManager> OrderMgr;

	// 칩(완성품) 재고 주인. 제품 입력 상한·차감·재고 변화 구독이 전부 여기를 탄다.
	UPROPERTY()
	TObjectPtr<UWorldMapManager> WorldMapMgr;

	// 매니저 포인터 확보 (구독은 하지 않음). 호출자가 AddToViewport 전에 InitializeForCountry 를
	// 부르므로 NativeConstruct 보다 먼저 데이터가 필요하다 — 캐싱만 하면 그 시점에 전부 null 이다.
	void EnsureManagers();

	// ── 갱신 ──
	void RebuildOrderRail();          // 주문서 목록 전체 재구성 (열림 / 주문 소진 시)
	void SelectOrder(int32 OrderID);  // 선택 변경 → 상세 전체 갱신
	void RefreshDetail();             // 선택 주문의 정적 필드 (이름/커버/등급/개당 수치)
	void RefreshMaterials();          // 재료 행 재생성 (선택 변경 시에만)
	void RefreshDynamic();            // 수량·자원 변화마다 도는 경량 갱신
	void RefreshHeader();
	void RefreshOrderRailHints();     // 좌 레일 행의 상한/병목만 재도색 (이름·커버·선택 유지)

	bool TryGetSelectedOrder(struct FProductionOrder& OutOrder) const;
	int32 GetSelectedMaxProducible(EResourceType& OutBottleneck, bool& bOutSupplyBound) const;
	bool IsLineAvailable() const;
	FText GetResourceDisplayName(EResourceType Type) const;
	float GetEffectiveProductionTimeSec(float BaseTimePerUnitSec) const;

	// 제품 입력(칩)의 표시 이름. 입력이 없거나 DT 행이 없으면 빈 FText.
	FText GetProductInputName(const struct FProductionOrder& Order) const;

	// 최대 수량을 정한 재료의 띠 문장. 띠를 접어야 하면 빈 FText.
	FText BuildLimitBandText(int32 MaxProducible, EResourceType Bottleneck,
		bool bSupplyBound, const FText& ProductInputName, int64 ProductHave) const;
	void ApplyStepEnabled(UButtonWidget* Button, bool bEnabled) const;
	void ApplyAcceptLook(bool bCanProduce, const FText& Label, int32 Quantity);

	void SetQuantity(int32 NewQuantity);
	void RefreshBulkVisuals();
	void ScheduleOrderRailRefresh();
	void FlushOrderRailRefresh();

	void HandleMinus();
	void HandlePlus();
	void HandleMaxJump();
	void HandleAccept();
	void HandleBulkX1();
	void HandleBulkX10();
	void HandleBulkX50();
	void HandleOrderRowClicked(int32 OrderID);
	void HandleResourceChanged(EResourceType Type, int64 NewAmount);

	UFUNCTION()
	void HandleOrderCreated(int32 BuildingID, const FProductionOrder& Order);

	UFUNCTION()
	void HandleOrderConsumed(int32 OrderID, int32 ConsumedAmount, int32 RemainingQuantity);

	UFUNCTION()
	void HandleOrderDepleted(int32 OrderID);

	// OnProductAdded / OnProductConsumed 가 같은 시그니처라 핸들러 하나로 받는다
	UFUNCTION()
	void HandleProductInventoryChanged(FIntPoint ProductKey, int64 NewAmount);

	UFUNCTION()
	void HandleLineStarted(ECountryType InCountry, FWorldFactoryLineState LineState);

	UFUNCTION()
	void HandleLineCompleted(ECountryType InCountry, int32 LineId);

	UFUNCTION()
	void HandleLineClaimed(ECountryType InCountry, int32 LineId, int64 FinalQty);

	UFUNCTION()
	void HandleClose();

	UFUNCTION()
	void HandleBackgroundClicked();

	// 비스택(AddToViewport) 모달이라 UIBase 스택 닫힘사운드 hook 이 안 닿음 — 직접 재생
	void PlayCloseSound();
};
