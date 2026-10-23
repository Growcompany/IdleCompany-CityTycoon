#include "UI/Element/Employee/PotentialOddsRowWidget.h"
#include "CommonTextBlock.h"

void UPotentialOddsRowWidget::SetOdds(ELootBoxRarity Rarity, float Percent)
{
	GradeText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Rarity)));
	FLinearColor Ink = FLootBoxRarityUtility::GetRarityColor(Rarity);
	Ink.A = 1.f;
	GradeText->SetColorAndOpacity(FSlateColor(Ink));
	PercentText->SetText(FText::FromString(FString::Printf(TEXT("%g%%"), Percent)));
}
