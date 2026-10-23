#include "UI/Element/Gacha/GachaOddsCellWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

void UGachaOddsCellWidget::SetOdds(const FText& Grade, float Percent, const FLinearColor& GradeColor, bool bShowSeparator, bool bLatinGrade)
{
	if (GradeText)
	{
		GradeText->SetVisibility(bLatinGrade ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		GradeText->SetText(Grade);
		GradeText->SetColorAndOpacity(FSlateColor(GradeColor));
	}
	if (LatinGradeText)
	{
		LatinGradeText->SetVisibility(bLatinGrade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		LatinGradeText->SetText(Grade);
		LatinGradeText->SetColorAndOpacity(FSlateColor(GradeColor));
	}
	else if (bLatinGrade)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaOddsCell] LatinGradeText missing from UIE_GachaOddsCell"));
	}
	if (PercentText)
	{
		const bool bWhole = Percent >= 10.f || FMath::IsNearlyEqual(Percent, FMath::RoundToFloat(Percent));
		const FString Label = bWhole
			? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Percent))
			: FString::Printf(TEXT("%.1f%%"), Percent);
		PercentText->SetText(FText::FromString(Label));
	}
	if (Separator)
	{
		Separator->SetVisibility(bShowSeparator ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
