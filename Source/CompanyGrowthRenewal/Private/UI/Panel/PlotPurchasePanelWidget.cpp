#include "UI/Panel/PlotPurchasePanelWidget.h"
#include "UI/UIBase.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Enum/ResourceType.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"

UPlotPurchasePanelWidget::UPlotPurchasePanelWidget()
{
	bIsBackHandler = true;
	AppearPopTargetName = NAME_None;
}

void UPlotPurchasePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::HandleConfirmClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
		CloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
	ApplyPurchaseDetails();
}

void UPlotPurchasePanelWidget::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	Super::NativeDestruct();
}

void UPlotPurchasePanelWidget::SetPurchaseDetails(const FText& Headline, const FText& Message, int64 Price, bool bInCanAfford, bool bInCanPurchase)
{
	CachedHeadline = Headline;
	CachedMessage = Message;
	CachedPrice = Price;
	bCanAfford = bInCanAfford;
	bCanPurchase = bInCanPurchase;
	ApplyPurchaseDetails();
}

void UPlotPurchasePanelWidget::ApplyPurchaseDetails()
{
	if (TitleText) { TitleText->SetText(CachedHeadline); }
	if (MessageText) { MessageText->SetText(CachedMessage); }
	if (LockIcon) { LockIcon->SetVisibility(bCanPurchase ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
	if (PriceResource)
	{
		PriceResource->SetResourceType(EResourceType::Money, false);
		PriceResource->SetValue(CachedPrice);
		PriceResource->SetCanAfford(bCanAfford);
	}
	if (!ConfirmButton) { return; }

	ConfirmButton->SetButtonText(NSLOCTEXT("CityPlot", "ConfirmPurchase", "부지 인수"));
	ConfirmButton->SetIsEnabled(true);
	if (bCanPurchase)
	{
		ConfirmButton->ClearRejectReason();
		if (AvailableButtonStyle) { ConfirmButton->SetStyle(AvailableButtonStyle); }
	}
	else
	{
		ConfirmButton->SetRejectReason(CachedHeadline);
		if (BlockedButtonStyle) { ConfirmButton->SetStyle(BlockedButtonStyle); }
	}
}

void UPlotPurchasePanelWidget::HandleConfirmClicked()
{
	if (!bCanPurchase) { return; }
	DeactivateWidget();
	OnConfirm.Broadcast();
}

UWidget* UPlotPurchasePanelWidget::GetCloseButtonWidget() const
{
	return CloseButton;
}

UWidget* UPlotPurchasePanelWidget::GetConfirmButtonWidget() const
{
	return ConfirmButton;
}

void UPlotPurchasePanelWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

bool UPlotPurchasePanelWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	DeactivateWidget();
	return true;
}
