// 부지 가격 배지 — WBP 기반 UUserWidget. C++ 는 가격 텍스트/색 + 클릭→인수만.

#include "UI/Element/Plot/PlotPriceBadgeWidget.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Components/Button.h"
#include "CommonTextBlock.h"

namespace
{
	// 가격 텍스트 색 — 충분=웜화이트, 부족=뮤트 레드(형광 빨강 회피, 절제).
	const FLinearColor PriceWhite(0.96f, 0.96f, 0.98f, 1.0f);
	const FLinearColor PriceRed(0.92f, 0.32f, 0.30f, 1.0f);
}

void UPlotPriceBadgeWidget::SetPlot(ACityPlotActor* InPlot)
{
	Plot = InPlot;
}

void UPlotPriceBadgeWidget::SetPrice(const FText& InPrice, bool bInCanAfford)
{
	CachedPrice = InPrice;
	bCachedCanAfford = bInCanAfford;
	ApplyPriceToWidget();
}

void UPlotPriceBadgeWidget::ApplyPriceToWidget()
{
	if (!PriceText) return;

	PriceText->SetText(CachedPrice);
	PriceText->SetColorAndOpacity(FSlateColor(bCachedCanAfford ? PriceWhite : PriceRed));
}

void UPlotPriceBadgeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BadgeButton)
	{
		// 중복 누적 방지(재진입 시) — Remove 후 Add. NativeDestruct 에서 해제 쌍.
		BadgeButton->OnClicked.RemoveDynamic(this, &UPlotPriceBadgeWidget::HandleBadgeClicked);
		BadgeButton->OnClicked.AddDynamic(this, &UPlotPriceBadgeWidget::HandleBadgeClicked);
	}

	// 위젯이 막 생성됐으니 캐시된 가격을 즉시 반영(setter 가 NativeConstruct 전에 호출됐을 수 있음).
	ApplyPriceToWidget();
}

void UPlotPriceBadgeWidget::NativeDestruct()
{
	if (BadgeButton)
	{
		BadgeButton->OnClicked.RemoveDynamic(this, &UPlotPriceBadgeWidget::HandleBadgeClicked);
	}
	Super::NativeDestruct();
}

void UPlotPriceBadgeWidget::HandleBadgeClicked()
{
	if (ACityPlotActor* PlotPtr = Plot.Get())
	{
		PlotPtr->TryPurchase();
	}
}
