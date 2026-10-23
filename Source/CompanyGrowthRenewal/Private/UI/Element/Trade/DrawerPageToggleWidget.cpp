#include "UI/Element/Trade/DrawerPageToggleWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Groups/CommonButtonGroupBase.h"

void UDrawerPageToggleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PageGroup = NewObject<UCommonButtonGroupBase>(this);
	// 등록 전에 켜야 첫 버튼이 조용히 선택된다(등록 뒤에 켜면 소리와 함께 0번을 고른다)
	PageGroup->SetSelectionRequired(true);
	for (UButtonWidget* Segment : { ProductionSegment.Get(), UpgradeSegment.Get() })
	{
		if (Segment)
		{
			PageGroup->AddWidget(Segment);
			Segment->SetIsSelectable(true);
			// 전환음은 서랍이 사용자 전환일 때만 낸다 — 버튼 자체 클릭음과 두 발이 되지 않게
			Segment->SetPlayClickSound(false);
		}
	}
	PageGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UDrawerPageToggleWidget::HandleSelectionChanged);

	// 재구성돼도 서랍이 기억한 페이지를 지킨다(첫 생성은 생산)
	SelectSegment(CurrentPage);
}

void UDrawerPageToggleWidget::NativeDestruct()
{
	if (PageGroup)
	{
		PageGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		// 선택 필수인 채로 비우면 엔진이 남은 버튼을 재선택한다 — 재구성 때 새 그룹이 같은 버튼을 받으므로 옛 그룹과 끊는다
		PageGroup->SetSelectionRequired(false);
		PageGroup->RemoveAll();
		PageGroup = nullptr;
	}
	Super::NativeDestruct();
}

void UDrawerPageToggleWidget::SetPage(CountryDrawer::EDrawerPage Page, bool bBroadcast)
{
	const bool bChanged = CurrentPage != Page;
	CurrentPage = Page;
	SelectSegment(Page);
	if (bBroadcast && bChanged)
	{
		PageChanged.Broadcast(Page);
	}
}

void UDrawerPageToggleWidget::SetProductionDot(bool bShow)
{
	if (ProductionDot)
	{
		ProductionDot->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UDrawerPageToggleWidget::HandleSelectionChanged(UCommonButtonBase* SelectedButton, int32 /*ButtonIndex*/)
{
	ApplySegmentInk();
	if (bApplyingPage)
	{
		return;
	}
	const CountryDrawer::EDrawerPage Page = SelectedButton && SelectedButton == UpgradeSegment.Get()
		? CountryDrawer::EDrawerPage::Upgrade : CountryDrawer::EDrawerPage::Production;
	if (Page == CurrentPage)
	{
		return;
	}
	CurrentPage = Page;
	PageChanged.Broadcast(Page);
}

UButtonWidget* UDrawerPageToggleWidget::SegmentFor(CountryDrawer::EDrawerPage Page) const
{
	return Page == CountryDrawer::EDrawerPage::Upgrade ? UpgradeSegment.Get() : ProductionSegment.Get();
}

void UDrawerPageToggleWidget::SelectSegment(CountryDrawer::EDrawerPage Page)
{
	UButtonWidget* Segment = SegmentFor(Page);
	if (!PageGroup || !Segment)
	{
		return;
	}
	{
		TGuardValue<bool> Guard(bApplyingPage, true);
		PageGroup->SelectButtonAtIndex(PageGroup->FindButtonIndex(Segment), /*bAllowSound=*/false);
	}
	// 이미 선택된 칸이면 그룹이 방송하지 않는다
	ApplySegmentInk();
}

void UDrawerPageToggleWidget::ApplySegmentInk()
{
	for (UButtonWidget* Segment : { ProductionSegment.Get(), UpgradeSegment.Get() })
	{
		if (Segment)
		{
			Segment->SetTextColor(Segment->GetSelected() ? SelectedInk : UnselectedInk);
		}
	}
}
