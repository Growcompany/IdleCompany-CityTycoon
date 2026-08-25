#include "UI/Element/Office/ReviewCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"

void UReviewCardWidget::SetCriticData(const FText& InName, int32 InScore, const FText& InQuote)
{
	if (NameText) { NameText->SetText(InName); }
	if (ScoreText)
	{
		ScoreText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ScoreText->SetText(FText::FromString(FString::Printf(TEXT("%d/10"), InScore)));
	}
	if (ScoreBadge)
	{
		// 8-10 그린 / 5-7 잉크 / 1-4 레드 — 하락에 블루 금지 (UI_STYLE_CATALOG §9)
		ScoreBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
		ScoreBadge->SetBrushColor((InScore >= 8) ? HighColor : (InScore >= 5) ? MidColor : LowColor);
	}
	if (QuoteText) { QuoteText->SetText(InQuote); }
}

void UReviewCardWidget::SetSnsData(const FText& InNick, const FText& InComment)
{
	if (NameText) { NameText->SetText(InNick); }
	if (QuoteText) { QuoteText->SetText(InComment); }
	if (ScoreBadge) { ScoreBadge->SetVisibility(ESlateVisibility::Collapsed); }
	if (ScoreText) { ScoreText->SetVisibility(ESlateVisibility::Collapsed); }
}
