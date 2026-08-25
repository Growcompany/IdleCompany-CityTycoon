// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ProductionOrderData.h"
#include "Enum/ResourceType.h"
#include "ProductionCardWidget.generated.h"

class UImage;
class UWrapBox;
class UCommonTextBlock;
class UEditableTextBox;
class UButtonWidget;
class UStatRowWidget;
class UItemCardSlotWidget;
class UCommonButtonStyle;

/**
 * UIE_ProductionCard
 * 한 주문(FProductionOrder) 의 self-contained 제작 카드.
 * 큰 메인 이미지 + 제품명 + 재료 그리드 + 비용/시간 + 수량 +/- + 제작 버튼.
 *
 * ⚠ 휴면 (2026-07-29 ~) — 제작 시작 팝업이 마스터·디테일(A안)로 전환되며 호출자가 0이 되었다.
 *   현행 경로 = UProductionOrderRowWidget(레일 행) + UMaterialReqRowWidget(재료 행).
 *   WBP 자산이 있어 사용자 결정으로 존치 중. **여기에 새 기능을 붙이지 말 것** — 화면에 안 뜬다.
 */
DECLARE_DELEGATE_ThreeParams(FOnProduceRequested, int32 /*OrderID*/, int32 /*Quantity*/, float /*ProductionTimeSecPerUnit*/);
DECLARE_DELEGATE_TwoParams(FOnMaterialClicked, EResourceType /*Type*/, FVector2D /*AbsoluteScreenPos*/);

UCLASS()
class COMPANYGROWTHRENEWAL_API UProductionCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ProductionCard")
	void SetOrderData(const FProductionOrder& InOrder);

	UFUNCTION(BlueprintPure, Category = "ProductionCard")
	int32 GetOrderID() const { return CachedOrder.OrderID; }

	UFUNCTION(BlueprintPure, Category = "ProductionCard")
	int32 GetMaxQuantity() const { return CachedOrder.RemainingQuantity; }

	// 외부 자원 변화 시 부모 popup 이 호출 — 재료 부족 표시 갱신용
	UFUNCTION(BlueprintCallable, Category = "ProductionCard")
	void NotifyResourcesChanged() { RefreshMaterialQuantities(); }

	// StartProduction 실패 시 popup 이 호출 — Accept 시점에 차감한 재료를 환불
	UFUNCTION(BlueprintCallable, Category = "ProductionCard")
	void RefundMaterials(int32 Quantity);

	// 라인 여유 상태 propagate — popup 이 FactoryMgr 상태 변화 감지 시 호출
	UFUNCTION(BlueprintCallable, Category = "ProductionCard")
	void SetLineCapacityAvailable(bool bAvailable);

	FOnProduceRequested OnProduceRequested;

	// 재료 ItemCard 클릭 시 popup 으로 bubble up — popup 이 Tooltip 띄움
	FOnMaterialClicked OnMaterialClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> EntityImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductionNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> ItemWrapBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> Text_Time;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MinusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> PlusButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> NumberEditableTextBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> AcceptButton;

	// 재료 충분 시 적용할 AcceptButton 스타일 (WBP 에서 할당)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProductionCard|Style")
	TSubclassOf<UCommonButtonStyle> AcceptButtonStyle_Enabled;

	// 재료 부족 시 적용할 AcceptButton 스타일 (회색 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProductionCard|Style")
	TSubclassOf<UCommonButtonStyle> AcceptButtonStyle_Disabled;

private:
	FProductionOrder CachedOrder;
	int32 CurrentQuantity = 1;

	// popup 이 SetLineCapacityAvailable 로 갱신. 기본 true (안 갱신해도 정상 동작 — fail-open)
	bool bLineCapacityAvailable = true;

	// Recipe DT lookup 실패 시 true — HasEnoughMaterials 가 false 반환해 어뷰즈 차단 (fail-closed)
	bool bRecipeMissing = false;

	UPROPERTY()
	TArray<TObjectPtr<UItemCardSlotWidget>> SpawnedMaterialCards;

	TArray<TPair<EResourceType, int32>> CachedMaterials;

	float CachedProductionTimeSec = 0.0f;

	void RefreshAll();
	void RefreshQuantityUI();
	void RebuildMaterials();
	void RefreshMaterialQuantities();
	void ClampQuantity();

	bool HasEnoughMaterials() const;
	void UpdateAcceptButtonState();

	void HandleMinus();
	void HandlePlus();
	void HandleAccept();

	UFUNCTION()
	void HandleQuantityCommitted(const FText& Text, ETextCommit::Type CommitType);

	void LoadStageImageAsync();
};
