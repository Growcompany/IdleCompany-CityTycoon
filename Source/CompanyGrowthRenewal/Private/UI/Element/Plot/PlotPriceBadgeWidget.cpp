// 부지 가격 배지의 데이터 표시와 인수 입력.

#include "UI/Element/Plot/PlotPriceBadgeWidget.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Enum/ResourceType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"

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

void UPlotPriceBadgeWidget::SetAcquisitionRequired(bool bRequired)
{
	bAcquisitionRequired = bRequired;
	ApplyPriceToWidget();
}

void UPlotPriceBadgeWidget::ApplyPriceToWidget()
{
	if (TitleText)
	{
		TitleText->SetText(bAcquisitionRequired
			? NSLOCTEXT("PlotPriceBadge", "AcquisitionRequired", "회사 정리 필요")
			: NSLOCTEXT("PlotPriceBadge", "PlotAcquisition", "부지 인수"));
	}
	if (ContentHBox)
	{
		ContentHBox->SetVisibility(bAcquisitionRequired ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (BadgeSizing)
	{
		BadgeSizing->SetMinDesiredHeight(bAcquisitionRequired ? 0.0f : 64.0f);
	}
	if (PriceText)
	{
		PriceText->SetText(CachedPrice);
		PriceText->SetColorAndOpacity(FSlateColor(bCachedCanAfford ? AffordablePriceColor : InsufficientPriceColor));
	}
	if (ShortageText)
	{
		ShortageText->SetVisibility(bAcquisitionRequired || bCachedCanAfford ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UPlotPriceBadgeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MoneyIcon)
	{
		UGameInstance* OwningGameInstance = GetGameInstance();
		UTableManagerSubsystem* TableManager = OwningGameInstance ? OwningGameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		bool bFound = false;
		const FResourceInfo ResourceInfo = TableManager ? TableManager->GetResourceInfo(EResourceType::Money, bFound) : FResourceInfo();
		UTexture2D* IconTexture = bFound ? ResourceInfo.Icon.LoadSynchronous() : nullptr;
		MoneyIcon->SetBrushFromTexture(IconTexture);
		MoneyIcon->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!IconTexture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlotPriceBadge] DT_Resource Money icon is missing"));
		}
	}

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
