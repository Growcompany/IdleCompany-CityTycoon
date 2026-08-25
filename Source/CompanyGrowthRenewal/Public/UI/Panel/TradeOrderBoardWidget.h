#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/WorldMapTypes.h"
#include "TradeOrderBoardWidget.generated.h"

class UScrollBox;
class USizeBox;
class UStatRowWidget;
class UButtonWidget;
class UTradeOrderManager;
class UTableManagerSubsystem;
class UWorldMapManager;
class UTradeOrderCardWidget;
class UAlertMarkWidget;

/**
 * 무역 게시판 — WorldMapLayer의 부모 Overlay(HAlign=Fill, VAlign=Bottom)에 배치.
 *
 * 상태 & 애니메이션 (코드 기반):
 *  - Expanded: CardAreaSizeBox.HeightOverride = ExpandedCardAreaHeight (기본 1150)
 *  - Collapsed: CardAreaSizeBox.HeightOverride = CollapsedCardAreaHeight (기본 183)
 *  - ExpandDuration 동안 EaseOutBack(쫄깃) / CollapseDuration 동안 EaseInQuad(가속 닫힘)
 *  - NativeTick에서 AnimElapsed 누적해 HeightOverride 매 프레임 갱신 (UWidgetAnimation 불사용)
 *  - Expand 시작 직후 카드 populate → 자라나는 프레임에 자연스럽게 카드 드러남
 *  - Collapse 종료 후 카드 clear
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTradeOrderBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TradeBoard")
	void RefreshBoard();

	UFUNCTION(BlueprintCallable, Category = "TradeBoard")
	void ToggleCollapse();

	UFUNCTION(BlueprintPure, Category = "TradeBoard")
	bool IsCollapsed() const { return bCollapsed; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativePreConstruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> ComboText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> SlotCountText = nullptr;

	// 미수락 긴급 주문 수 표시 ("긴급 / N건"). 0건이면 Hidden
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UStatRowWidget> UrgentText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> CollapseBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> CardAreaSizeBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> TradeOrderCardScrollBox = nullptr;

	// 보드 헤더 알림 도트 — 미열람 신규 주문 존재 시 표시. 접힌 상태에서도 시각적 신호.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> AlertMark = nullptr;

	// ── 애니메이션 튜닝 (에디터에서 조정 가능) ──
	UPROPERTY(EditAnywhere, Category = "TradeBoard|Style")
	float ExpandedCardAreaHeight = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "TradeBoard|Style")
	float CollapsedCardAreaHeight = 183.0f;

	UPROPERTY(EditAnywhere, Category = "TradeBoard|Animation", meta = (ClampMin = "0.0"))
	float ExpandDuration = 0.4f;

	UPROPERTY(EditAnywhere, Category = "TradeBoard|Animation", meta = (ClampMin = "0.0"))
	float CollapseDuration = 0.3f;

	// Back easing 오버슈트 강도 (1.7 = CSS 기본, 2.5+면 과한 튕김)
	UPROPERTY(EditAnywhere, Category = "TradeBoard|Animation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float BackEaseOvershoot = 1.7f;

	// Designer 전용 프리뷰 — 체크하면 에디터에서 접힌 모습, 해제하면 펴진 모습 (런타임 무관)
	UPROPERTY(EditAnywhere, Category = "TradeBoard|Preview",
	          meta = (DisplayName = "Preview Collapsed (Editor Only)"))
	bool bPreviewCollapsed = false;

private:
	bool bCollapsed = false;

	// 애니메이션 진행 상태
	bool bAnimating = false;
	bool bAnimatingToExpand = false;
	float AnimElapsed = 0.0f;

	UPROPERTY()
	TObjectPtr<UTradeOrderManager> TradeOrderMgr = nullptr;

	UPROPERTY()
	TObjectPtr<UTableManagerSubsystem> TableMgr = nullptr;

	UPROPERTY()
	TObjectPtr<UWorldMapManager> WorldMapMgr = nullptr;

	FTimerHandle RefreshTimerHandle;

	// 매니저 델리게이트
	UFUNCTION() void HandleOrderGenerated(FTradeOrder Order);
	UFUNCTION() void HandleOrderExpired(int32 OrderId);
	UFUNCTION() void HandleOrderCompleted(int32 OrderId);
	UFUNCTION() void HandleOrderAccepted(int32 OrderId);
	UFUNCTION() void HandleOrderDismissed(int32 OrderId, bool bWasAccepted);
	UFUNCTION() void HandleComboChanged(int32 NewCombo, float NewMultiplier);

	// 카드 델리게이트
	UFUNCTION() void HandleCardAcceptRequested(int32 OrderId);
	UFUNCTION() void HandleCardDismissRequested(int32 OrderId);

	// CollapseBtn 토글
	void OnCollapseBtnClicked();

	// 상태 적용 + 애니 시작
	void ApplyCollapsedState();

	// 애니 완료 후처리 (bCollapsed 참조해 분기)
	void OnAnimationFinished();

	// RefreshBoard 분해
	void RefreshStats();
	void RefreshCards();

	// 타이머용 경량 갱신: 기존 카드들에게 남은 시간만 다시 계산하라고 지시.
	// ClearChildren/CreateWidget을 하지 않아 TextBlock 재생성 깜빡임 없음.
	void TickCards();

	// Easing 함수 (static이라 상태 없음)
	static float EaseOutBack(float T, float Overshoot);
	static float EaseInQuad(float T);

	// 카드 생성 헬퍼
	UWidget* CreateOrderCard(const FTradeOrder& Order);

	FString FormatDuration(const FTimespan& Duration) const;
};
