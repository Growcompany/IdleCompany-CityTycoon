#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/WorldMapTypes.h"
#include "TradeOrderCardWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UBorder;
class UImage;
class UButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeOrderAcceptRequested, int32, OrderId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeOrderDismissRequested, int32, OrderId);

/**
 * 무역 주문 카드 (WBP: UIE_TradeOrderCard)
 *
 * TradeOrderBoardWidget이 FTradeOrder마다 1개씩 생성해 ScrollBox에 꽂는 카드.
 * Accept/Deliver 클릭은 델리게이트로만 보고, 실제 매니저 호출은 상위 Board가 처리.
 * 카드는 외부 의존성 없이 전달받은 데이터만 표시한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTradeOrderCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 카드는 외부 의존성 없이 데이터만 받음 (관심사 분리).
	// ProductName / ProductIcon 은 Board 가 DT_Project_* 에서 lookup 해 주입.
	UFUNCTION(BlueprintCallable, Category = "TradeCard")
	void SetOrder(const FTradeOrder& InOrder, const FText& ProductName, const TSoftObjectPtr<UTexture2D>& ProductIcon, int64 HaveQty, bool bSlotFull);

	// 카드 전체 재생성 없이 남은 시간 텍스트만 갱신 (타이머 틱용)
	UFUNCTION(BlueprintCallable, Category = "TradeCard")
	void UpdateRemainingTime();

	UPROPERTY(BlueprintAssignable, Category = "TradeCard")
	FOnTradeOrderAcceptRequested OnAcceptRequested;

	// 거절(미수락) 또는 포기(수락됨) 공통 델리게이트 — 매니저가 bAccepted 보고 분기
	UPROPERTY(BlueprintAssignable, Category = "TradeCard")
	FOnTradeOrderDismissRequested OnDismissRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TierBorder = nullptr;

	// 산업별 StageImage 텍스처 표시 — DT_Project_*.Icon 을 Board 가 주입
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TierText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProductText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RemainingTimeText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText = nullptr;

	// ProgressBar 위에 얹히는 "현재 수량" 텍스트 ("12")
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CurrentStatValueText = nullptr;

	// ProgressBar 위에 얹히는 "/총수량" 텍스트 ("/30")
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatValueText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RewardText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiamondBonusText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> AcceptButton = nullptr;

	// 수락 전 = "거절"(회색), 수락 후 = "포기"(빨강) 단일 버튼 상태머신
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> DeclineButton = nullptr;

	UPROPERTY(EditAnywhere, Category = "TradeCard|Style")
	FLinearColor TierColor_Normal = FLinearColor(0.25f, 0.55f, 0.95f);

	UPROPERTY(EditAnywhere, Category = "TradeCard|Style")
	FLinearColor TierColor_Urgent = FLinearColor(0.95f, 0.30f, 0.30f);

	UPROPERTY(EditAnywhere, Category = "TradeCard|Style")
	FLinearColor TierColor_VIP = FLinearColor(0.85f, 0.70f, 0.20f);

	UPROPERTY(EditAnywhere, Category = "TradeCard|Style")
	FLinearColor DeclineTextColor_Idle = FLinearColor(0.80f, 0.80f, 0.80f);

	UPROPERTY(EditAnywhere, Category = "TradeCard|Style")
	FLinearColor DeclineTextColor_Abandon = FLinearColor(1.00f, 0.25f, 0.25f);

private:
	FTradeOrder CachedOrder;
	FText CachedProductName;
	TSoftObjectPtr<UTexture2D> CachedProductIcon;
	int64 CachedHaveQty = 0;
	bool bCachedSlotFull = false;
	bool bHasCachedData = false;

	void ApplyOrderData();

	UFUNCTION()
	void OnAcceptClicked();

	UFUNCTION()
	void OnDeclineClicked();

	FString FormatDuration(const FTimespan& Duration) const;
};
