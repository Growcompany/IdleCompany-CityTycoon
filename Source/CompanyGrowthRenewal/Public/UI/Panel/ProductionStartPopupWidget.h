// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/ResourceType.h"
#include "Manager/WorldFactoryManager.h"
#include "ProductionStartPopupWidget.generated.h"

class UPanelWidget;
class UButton;
class UBorder;
class UImage;
class UCommonTextBlock;
class UButtonWidget;
class UCloseButtonWidget;
class UProductionOrderRowWidget;
class UMaterialReqRowWidget;
class UResourceItemManager;
class UProductionOrderManager;
class UWorldFactoryManager;

/**
 * UI_ProductionStartPopup
 * 공장 탭에서 "+ 추가 제작" 클릭 시 뜨는 모달. 마스터·디테일 2단.
 *
 * 좌 = 주문서 레일(선택), 우 = 선택된 주문의 상세 + 재료 + 수량 + CTA.
 * 수량 상한은 min(주문서 잔여, 재료 기준 최대) 이고 라인은 별개 게이트다.
 *
 * 소프트 캡: 스텝퍼는 주문서 잔여까지 올라간다. 하드 클램프하면 "얼마나 모자란지"를
 * 보여줄 자리가 사라져, 무엇을 더 캐야 하는지가 화면에서 없어진다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProductionStartPopupWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ProductionStart")
	void InitializeForCountry(ECountryType InCountry);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── 셸 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> CloseButton;

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

	// ── 재료 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> MaterialGridBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MaterialLabelText;

	// ── 액션 웰 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MinusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> PlusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> QuantityText;

	// 배율 칩 3종. UIE_BulkModeSelector 는 세로 자가 트리(플레이트+VBox+체크3)라 이 가로 웰에 안 맞아 미사용.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> BulkBtn_x1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> BulkBtn_x10;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> BulkBtn_x50;

	// 재료 상한까지 한 번에 — 강화 배율의 '최대' 기각과는 다른 축(반복 손맛이 아니라 배치 투자 상한)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MaxJumpButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CapacityText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SummaryText;

	// 막힌 사유는 인라인 라벨이 아니라 클릭 시 토스트로 알린다 (HandleAccept).
	// 상시 라벨은 CTA 를 위로 밀어 올리고, 이미 재료 행·헤더 칩이 같은 사실을 말하고 있다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> AcceptButton;

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
	int32 BulkStep = 1;

	UPROPERTY()
	TArray<TObjectPtr<UProductionOrderRowWidget>> SpawnedRows;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialReqRowWidget>> SpawnedMatRows;

	UPROPERTY()
	TObjectPtr<UResourceItemManager> ResourceMgr;

	UPROPERTY()
	TObjectPtr<UWorldFactoryManager> FactoryMgr;

	UPROPERTY()
	TObjectPtr<UProductionOrderManager> OrderMgr;

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

	bool TryGetSelectedOrder(struct FProductionOrder& OutOrder) const;
	int32 GetSelectedMaxProducible(EResourceType& OutBottleneck, bool& bOutMaterialBound) const;
	bool IsLineAvailable() const;
	FText GetResourceDisplayName(EResourceType Type) const;

	void SetQuantity(int32 NewQuantity);
	void SetBulkStep(int32 NewStep);
	void RefreshBulkVisuals();

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
