// 부지 가격 배지 — WBP 기반 UUserWidget (UIE_PlotPriceBadge).
// 판/글꼴/여백과 상태 색은 WBP 가 소유한다.
// C++ 는 가격/자금 상태와 DT Money 아이콘을 표시하고 클릭을 ACityPlotActor::TryPurchase 로 전달한다.
// EWidgetType::PlotPriceBadge + DT_WidgetClass 로 로드 (표준 패턴), InGameLayer 가 풀로 생성/배치.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlotPriceBadgeWidget.generated.h"

class UButton;
class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class USizeBox;
class ACityPlotActor;

UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UPlotPriceBadgeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이 배지가 대표하는 부지(클릭 시 이 부지 인수). 약참조.
	void SetPlot(ACityPlotActor* InPlot);

	// 가격과 자금 여부를 캐시한 뒤 표시한다(NativeConstruct 전 호출도 안전).
	void SetPrice(const FText& InPrice, bool bInCanAfford);
	void SetAcquisitionRequired(bool bRequired);

	// 매 틱 위치 추적용 — 이 배지가 대표하는 부지(InGameLayer 가 재투영). 부지 소멸 시 null.
	ACityPlotActor* GetPlot() const { return Plot.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 클릭 루트 버튼. 판과 눌림 스타일은 WBP 에서 지정한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BadgeButton;

	// 가격 숫자의 폰트는 WBP, 값과 자금 상태는 런타임이 소유한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PriceText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* MoneyIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ShortageText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* ContentHBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	USizeBox* BadgeSizing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FLinearColor AffordablePriceColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FLinearColor InsufficientPriceColor = FLinearColor::White;

private:
	UFUNCTION()
	void HandleBadgeClicked();

	// 캐시 표시 상태(위젯이 아직 없을 때 보관 → NativeConstruct 에서 반영).
	void ApplyPriceToWidget();

	FText CachedPrice;
	bool bCachedCanAfford = true;
	bool bAcquisitionRequired = false;

	TWeakObjectPtr<ACityPlotActor> Plot;
};
