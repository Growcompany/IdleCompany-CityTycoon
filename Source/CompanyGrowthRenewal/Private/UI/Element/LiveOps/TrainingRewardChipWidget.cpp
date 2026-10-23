#include "UI/Element/LiveOps/TrainingRewardChipWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Table/ShopItemTable.h"
#include "Table/SideDungeonLootTable.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "UI/Element/Common/RewardChipUtils.h"

void UTrainingRewardChipWidget::ApplyIdentity(const CGRewardChip::FDisplayEntry& Entry)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FText DisplayName;
	bool bFound = false;
	if (TableMgr)
	{
		if (Entry.ItemType != EItemType::None)
		{
			FShopItemTable Item;
			bFound = TableMgr->GetShopItemByItemType(Entry.ItemType, Item);
			if (bFound) DisplayName = Item.DisplayName;
		}
		else if (Entry.ResourceType != EResourceType::None)
		{
			const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.ResourceType, bFound);
			if (bFound) DisplayName = Info.DisplayName;
		}
	}
	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Training reward display row missing: item=%d resource=%d"),
			static_cast<int32>(Entry.ItemType), static_cast<int32>(Entry.ResourceType));
	}
	if (NameText)
	{
		NameText->SetText(DisplayName);
		NameText->SetVisibility(DisplayName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Icon)
	{
		UObject* Resource = CGRewardChip::ResolveIcon(GI, Entry);
		CGIconBrush::ApplyResource(Icon, Resource);
		Icon->SetVisibility(Resource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UTrainingRewardChipWidget::ConfigurePreview(const FSideDungeonLootRow& Loot)
{
	CGRewardChip::FDisplayEntry Minimum;
	CGRewardChip::FDisplayEntry Maximum;
	if (Loot.ItemType != EItemType::None)
	{
		Minimum.ItemType = Maximum.ItemType = Loot.ItemType;
		Minimum.Quantity = FMath::Min(Loot.ItemAmountMin, Loot.ItemAmountMax);
		Maximum.Quantity = FMath::Max(Loot.ItemAmountMin, Loot.ItemAmountMax);
	}
	else
	{
		Minimum.ResourceType = Maximum.ResourceType = Loot.ResourceType;
		Minimum.Quantity = FMath::Min(Loot.ResourceAmountMin, Loot.ResourceAmountMax);
		Maximum.Quantity = FMath::Max(Loot.ResourceAmountMin, Loot.ResourceAmountMax);
	}
	ApplyIdentity(Maximum);
	if (QuantityText)
	{
		const FText RangeEnd = Maximum.ItemType != EItemType::None
			? FText::AsNumber(Maximum.Quantity) : CGRewardChip::FormatQuantity(Maximum);
		QuantityText->SetText(Minimum.Quantity == Maximum.Quantity
			? CGRewardChip::FormatQuantity(Maximum)
			: FText::Format(NSLOCTEXT("Training", "RewardRange", "{0}-{1}"),
				CGRewardChip::FormatQuantity(Minimum), RangeEnd));
	}
	if (OddsText)
	{
		OddsText->SetText(FText::Format(NSLOCTEXT("Training", "RewardOdds", "확률 {0}"), GetSideDropOddsDisplayName(Loot.Odds)));
		OddsText->SetColorAndOpacity(Loot.Odds == ESideDropOdds::High ? OddsHighColor
			: Loot.Odds == ESideDropOdds::Low ? OddsLowColor : OddsNormalColor);
		OddsText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
