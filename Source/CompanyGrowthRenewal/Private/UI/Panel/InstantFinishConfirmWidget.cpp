#include "UI/Panel/InstantFinishConfirmWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"

void UInstantFinishConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCloseButtonWidget* FrameClose = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		FrameClose->OnCloseClicked.AddDynamic(this, &UInstantFinishConfirmWidget::HandleFrameCloseClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UInstantFinishConfirmWidget::HandleCancelClicked);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &UInstantFinishConfirmWidget::HandleConfirmClicked);
	}
}

void UInstantFinishConfirmWidget::NativeDestruct()
{
	if (UCloseButtonWidget* FrameClose = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		FrameClose->OnCloseClicked.RemoveDynamic(this, &UInstantFinishConfirmWidget::HandleFrameCloseClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UInstantFinishConfirmWidget::Configure(UTexture2D* Icon, const FText& Name, int64 Current, int64 Target, double Unit01,
	const FTimespan& Remaining, int32 Diamonds, bool bMine)
{
	bResolved = false;

	if (ModalFrame)
	{
		ModalFrame->SetTitle(CountryDrawer::InstantFinishTitle());
	}
	if (ProductIcon)
	{
		CGIconBrush::ApplyResource(ProductIcon, Icon);
		ProductIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (ProductName)
	{
		ProductName->SetText(Name);
	}
	if (StatusText)
	{
		StatusText->SetText(bMine ? CountryDrawer::MineFullEtaLabel(Remaining) : CountryDrawer::FactoryRemainingLabel(Remaining));
	}
	if (ProgressBar)
	{
		ProgressBar->SetPercent(CountryDrawer::LineProgress01(Current, Target, Unit01));
	}
	if (BarText)
	{
		BarText->SetText(CountryDrawer::BarCountLabel(Current, Target));
	}
	if (MessageText)
	{
		MessageText->SetText(CountryDrawer::InstantFinishMessage(Diamonds, FMath::Max<int64>(Target - Current, 0), bMine));
	}
	if (CancelButton)
	{
		CancelButton->SetButtonText(CountryDrawer::CancelLabel());
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetButtonText(CountryDrawer::InstantFinishTitle());
		// 가격은 부족하면 빨갛게 보이되, 누름은 구매 경로로 보낸다 ― 부족 안내가 두 번 뜨지 않게
		ConfirmButton->SetCost(Diamonds, EResourceType::Diamond);
		ConfirmButton->SetEnabled(true);
	}
}

void UInstantFinishConfirmWidget::Dismiss()
{
	bResolved = true;
	RemoveFromParent();
}

void UInstantFinishConfirmWidget::HandleConfirmClicked()
{
	if (bResolved) return;
	bResolved = true;
	ConfirmedEvent.Broadcast();
	RemoveFromParent();
}

void UInstantFinishConfirmWidget::HandleCancelClicked()
{
	if (bResolved) return;
	bResolved = true;
	CancelledEvent.Broadcast();
	RemoveFromParent();
}

void UInstantFinishConfirmWidget::HandleFrameCloseClicked()
{
	HandleCancelClicked();
}
