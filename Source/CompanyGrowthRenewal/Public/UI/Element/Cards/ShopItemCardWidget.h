#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Table/ShopItemTable.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "ShopItemCardWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UCostActionButtonWidget;
class UVerticalBox;

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

	// 공용 비용 버튼(공장·책상 강화와 같은 부품). 가격 칩 + 「구매」, 부족 시 입력 거부·안내는 버튼이 맡는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCostActionButtonWidget* BuyButton;

	// 잔여 수량 줄 ("구매 가능 1/2") — 무제한 상품은 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LimitText;

	// 신규 배지. DT bNew 가 켜진 행에서만 보인다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* NewRibbon;

	// 수량 배지 — Quantity 가 2 이상일 때만. 이름에 "x5" 를 쓰지 않기 위한 것
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* QtyBadge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* QtyText;

	// 패키지 구성품 목록. 번들이 아니면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* BundleList;

	// 아이콘 칸(무대) 바탕. DT BandTint 로 색만 갈아 끼운다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* BandImage;

	// 무대 아래 턱. BandImage 가 아래를 비워 드러나며, 색은 BandTint 를 어둡게 한 값
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* BandShade;

	// 무대 무늬 — 귀한 상품(DT bFeatured)은 빛줄기, 나머지는 오른쪽 아래 하프톤. 둘이 겹치면 시끄러워 하나만 켠다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* StageRays;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* StageDots;

	// 품절 무대색. 등급색이 남으면 아직 살 수 있는 것처럼 보인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Stage")
	FLinearColor SoldOutStageTint = FLinearColor(0.258f, 0.283f, 0.335f, 1.f);

	// 무대 아래 턱 = 무대색 × 이 값(linear)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Stage")
	float StageShadeScale = 0.7f;

private:
	// 공용 버튼은 화면에 붙을 때(PreConstruct) 가격 칸을 기본값으로 되돌린다 ― 붙은 뒤에 다시 입힌다
	void ApplyBuyButton();
	void ApplyStage(bool bSoldOut);
	void HandleBuyClicked();
	void RefreshQtyBadge();
	void RefreshBundleList();

	FName RowName;
	FShopItemTable CachedRow;

	// 재구성 때 마지막 상태를 다시 입히기 위한 값
	int32 CachedRemainingCount = -1;
	bool bCachedCanAfford = true;
	bool bHasState = false;

	// 등장 애니 상태
	bool bEntrancePlaying = false;
	float EntranceDelayRemaining = 0.f;
	float EntranceElapsed = 0.f;

	FScalePunchAnimation PurchasePunch;

	static constexpr float EntranceDuration = 0.28f;
	static constexpr float EntranceRise = 24.f;
};
