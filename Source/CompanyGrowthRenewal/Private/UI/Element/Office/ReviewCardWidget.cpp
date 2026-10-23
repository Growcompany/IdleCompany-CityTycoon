#include "UI/Element/Office/ReviewCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"

void UReviewCardWidget::SetSnsData(const FText& InNick, const FText& InComment)
{
	if (NameText) { NameText->SetText(InNick); }
	if (QuoteText) { QuoteText->SetText(InComment); }
	if (ScoreBadge) { ScoreBadge->SetVisibility(ESlateVisibility::Collapsed); }
	if (ScoreText) { ScoreText->SetVisibility(ESlateVisibility::Collapsed); }
}
