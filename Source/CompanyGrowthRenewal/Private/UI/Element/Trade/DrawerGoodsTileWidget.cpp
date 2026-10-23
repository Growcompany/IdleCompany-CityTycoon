#include "UI/Element/Trade/DrawerGoodsTileWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UDrawerGoodsTileWidget::SetGoods(UTexture2D* Icon, const FText& Name, int64 Quantity)
{
	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if (NameText)
	{
		NameText->SetText(Name);
	}
	if (CountText)
	{
		CountText->SetText(CountryDrawer::GoodsQuantity(Quantity));
	}
}
