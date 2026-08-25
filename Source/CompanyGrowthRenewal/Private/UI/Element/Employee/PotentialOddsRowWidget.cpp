#include "UI/Element/Employee/PotentialOddsRowWidget.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"

void UPotentialOddsRowWidget::SetOdds(ELootBoxRarity Rarity, float Percent)
{
	const FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(Rarity);

	if (GradeText)
	{
		GradeText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Rarity)));
		// 다크 면이라 등급색 원색 그대로 — ×0.55 다크 파생은 크림 플레이트 전용 처방이고 여기선 반대로 죽는다
		FLinearColor Ink = RarityColor;
		Ink.A = 1.f;
		GradeText->SetColorAndOpacity(FSlateColor(Ink));
	}

	if (PercentText)
	{
		// 0.5 같은 소수 확률이 반올림으로 0% 이 되지 않게 소수 1자리 고정
		PercentText->SetText(FText::FromString(FString::Printf(TEXT("%.1f%%"), Percent)));
	}

	if (OddsBar)
	{
		OddsBar->SetPercent(FMath::Max(Percent / 100.f, MinBarFraction));
		OddsBar->SetFillColorAndOpacity(RarityColor);
	}
}
