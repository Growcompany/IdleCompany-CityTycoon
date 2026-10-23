#include "UI/Element/Trade/DrawerHeroRowWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UDrawerHeroRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyActionStyle();
}

void UDrawerHeroRowWidget::ApplyActionStyle()
{
	if (!ActionButton)
	{
		return;
	}
	if (ActionButtonStyle)
	{
		ActionButton->ButtonStyleClass = ActionButtonStyle;
		UCommonButtonBase* Inner = ActionButton->WidgetTree
			? Cast<UCommonButtonBase>(ActionButton->WidgetTree->FindWidget(TEXT("Btn"))) : nullptr;
		if (Inner)
		{
			Inner->SetStyle(ActionButtonStyle);
		}
	}
	ApplyActionLabelInk(bActionEnabled ? ActionLabelInk : DisabledLabelInk);
}

void UDrawerHeroRowWidget::ApplyActionLabelInk(const FSlateColor& Ink)
{
	if (!ActionButton)
	{
		return;
	}
	ActionButton->BtnTextColor = Ink;
	if (UTextBlock* Label = ActionButton->WidgetTree
		? Cast<UTextBlock>(ActionButton->WidgetTree->FindWidget(TEXT("BtnText"))) : nullptr)
	{
		Label->SetColorAndOpacity(Ink);
	}
}

void UDrawerHeroRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ActionButton)
	{
		ActionButton->OnClicked().AddUObject(this, &UDrawerHeroRowWidget::HandleActionClicked);
	}

	if (!bWarnedMissingBindings && (!TitleText || !DetailText || !ActionButton))
	{
		bWarnedMissingBindings = true;
		UE_LOG(LogTemp, Warning, TEXT("[DrawerHeroRow] %s 바인딩 누락 Title=%d Detail=%d Action=%d — author_drawer_onepage.py 저작 확인"),
			*GetClass()->GetName(), TitleText != nullptr, DetailText != nullptr, ActionButton != nullptr);
	}
}

void UDrawerHeroRowWidget::NativeDestruct()
{
	if (ActionButton)
	{
		ActionButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UDrawerHeroRowWidget::SetSummary(const FText& Title, const FText& Detail)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (DetailText)
	{
		DetailText->SetText(Detail);
		DetailText->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UDrawerHeroRowWidget::SetAction(const FText& Label, EResourceType AmountIcon, int64 Amount)
{
	if (!ActionButton)
	{
		return;
	}

	const bool bHasAmount = Amount > 0;
	bActionEnabled = bHasAmount;
	ActionButton->SetButtonText(Label);
	ActionButton->SetCost(FMath::Max<int64>(Amount, 0), AmountIcon, /*bCheckAfford=*/false);
	// 요약 줄 칩 = 받을 것(모두 받기)·들어올 돈(모두 판매) ― 시안 「+243」「+17.5만」
	CountryDrawer::StyleChipCount(ActionButton, /*bIncoming=*/true);
	ActionButton->SetCostVisible(bHasAmount);
	ActionButton->SetEnabled(bHasAmount);
	ApplyActionLabelInk(bHasAmount ? ActionLabelInk : DisabledLabelInk);
}

void UDrawerHeroRowWidget::SetFacilityTile(UTexture2D* Icon, FLinearColor TileColor)
{
	if (FacilityTile)
	{
		FacilityTile->SetBrushColor(TileColor);
	}
	if (FacilityIcon)
	{
		if (Icon)
		{
			FacilityIcon->SetBrushFromTexture(Icon);
			FacilityIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FacilityIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UDrawerHeroRowWidget::HandleActionClicked()
{
	ActionClicked.Broadcast();
}
