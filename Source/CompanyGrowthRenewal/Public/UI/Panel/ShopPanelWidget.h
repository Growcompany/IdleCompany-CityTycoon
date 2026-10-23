#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/ShopTypes.h"
#include "Enum/ResourceType.h"
#include "Data/EquipmentTicketShopData.h"
#include "ShopPanelWidget.generated.h"

class UTabButtonWidget;
class UPanelHeaderWidget;
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
	void SetEquipmentTicketContext(EItemType TicketType, int32 RequiredCount);

	// 흐름 밖에 열린 상점을 닫는 가이드의 헤더 X 대상
	UWidget* GetCloseButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== 상단바 =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Diamond;

	// ===== 좌측 탭 레일 =====
	// 패키지 탭은 WBP 저작 전까지 Optional — required 로 두면 트리에 없는 동안 WBP 컴파일이 깨진다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTabButtonWidget* PackageTab;

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

private:
	friend struct FEquipmentTicketUIValidation;
	void ConfirmEquipmentTicketPurchase(FEquipmentTicketShopRequest Request);
	EItemType RefillTicketType = EItemType::None;
	int32 RefillRequiredCount = 0;
	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseButtonClicked();

	void HandlePurchaseRequested(FName RowName);
	void ConfirmPurchase(FName RowName);

	void RebuildGrid();
	void RefreshAllCardStates();
	void RefreshCurrencyChips();
	void UpdateInfoText();

	void HandleStockChanged();
	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	void HandleMileageChanged(int32 NewPoints);

	// 인덱스↔탭은 실제로 그룹에 등록된 탭만으로 만든다 — Optional 탭이 없을 때 인덱스가 밀리지 않게
	EShopTab TabForIndex(int32 Index) const;
	int32 IndexForTab(EShopTab Tab) const;

	// 그룹 등록 순서와 1:1 (레일 표시 순서)
	TArray<EShopTab> TabOrder;

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
