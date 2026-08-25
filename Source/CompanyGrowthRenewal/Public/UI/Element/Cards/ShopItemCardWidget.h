#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Table/ShopItemTable.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "ShopItemCardWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UButtonWidget;

/**
 * 상점 상품 카드 (DT_ShopItem 1행 = 1카드, ShopPanel이 동적 생성)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UShopItemCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitCard(FName InRowName, const FShopItemTable& InRow);

	/** 한도/재화 상태 반영 (RemainingCount -1 = 무제한) */
	void RefreshState(int32 RemainingCount, bool bCanAfford);

	/** 등장 캐스케이드 (Delay 후 페이드+라이즈 인) */
	void PlayEntrance(float Delay);

	/** 구매 성공 펀치 */
	void PlayPurchasePunch();

	FName GetRowName() const { return RowName; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPurchaseRequested, FName /*RowName*/);
	FOnPurchaseRequested OnPurchaseRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* NameText;

	// 구매 CTA (가격 표시는 PriceResource 칩 담당 — 버튼은 행동만)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* PriceButton;

	// 한정 수량 뱃지 ("일일 1/2") — 무제한 상품은 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LimitText;

	// 품절 오버레이 (반투명 크림 + 안내)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* SoldOutOverlay;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* SoldOutText;

private:
	void HandlePriceClicked();
	FText BuildPriceText() const;

	FName RowName;
	FShopItemTable CachedRow;

	// 등장 애니 상태
	bool bEntrancePlaying = false;
	float EntranceDelayRemaining = 0.f;
	float EntranceElapsed = 0.f;

	FScalePunchAnimation PurchasePunch;

	static constexpr float EntranceDuration = 0.28f;
	static constexpr float EntranceRise = 24.f;
};
