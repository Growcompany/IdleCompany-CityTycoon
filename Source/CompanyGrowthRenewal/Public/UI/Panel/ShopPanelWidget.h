#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/ShopTypes.h"
#include "Enum/ResourceType.h"
#include "ShopPanelWidget.generated.h"

class UTabButtonWidget;
class UCloseButtonWidget;
class UUniformGridPanel;
class UResourceWidget;
class UCommonTextBlock;
class UButtonWidget;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UShopItemCardWidget;

/**
 * 상점 풀스크린 패널 (4탭: 일일/주간/다이아/마일리지)
 * - 진입: BuildOpenWidget::OnShopButtonClicked → PushPromptClass(EWidgetType::ShopPanel)
 * - 상품: DT_ShopItem → UShopItemCardWidget 동적 생성 (4열 UniformGrid)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UShopPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	/** 푸시 직후 호출해 시작 탭 지정 (가챠 화면의 [마일리지 상점] 링크용) */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetInitialTab(EShopTab Tab);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== 상단바 =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Money_1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Diamond_1;

	// ===== 좌측 탭 레일 =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* DailyTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* WeeklyTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* DiamondTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* MileageTab;

	// ===== 콘텐츠 =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUniformGridPanel* ItemGrid;

	// 탭별 안내줄 (일일=리셋 카운트다운 / 마일리지=보유 pt)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* InfoText;

	// 일일 탭 전용 수동 리셋 (다이아 50)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* ManualResetButton;

private:
	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseButtonClicked();

	void OnManualResetClicked();
	void HandlePurchaseRequested(FName RowName);
	void ConfirmPurchase(FName RowName);
	void ConfirmManualReset();

	void RebuildGrid();
	void RefreshAllCardStates();
	void RefreshCurrencyChips();
	void UpdateInfoText();

	void HandleStockChanged();
	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	void HandleMileageChanged(int32 NewPoints);

	static EShopTab TabForIndex(int32 Index);

	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	UPROPERTY()
	TArray<UShopItemCardWidget*> Cards;

	EShopTab CurrentTab = EShopTab::Daily;
	FTimerHandle InfoTextTimerHandle;

	// 패널 인트로 (페이드+라이즈) 상태. 음수 = 비활성
	float PanelIntroElapsed = -1.f;
	static constexpr float PanelIntroDuration = 0.22f;
	static constexpr float PanelIntroRise = 18.f;
};
