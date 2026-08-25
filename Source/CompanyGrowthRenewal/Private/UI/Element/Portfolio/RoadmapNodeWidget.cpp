#include "UI/Element/Portfolio/RoadmapNodeWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

void URoadmapNodeWidget::Configure(int32 Tier, ERoadmapNodeState State, const FLinearColor& IndustryColor)
{
	const bool bDone = State == ERoadmapNodeState::Done;
	const bool bCur = State == ERoadmapNodeState::Current;
	const bool bGlow = bDone || bCur;

	static const FLinearColor DoneColor(0.180f, 0.520f, 0.290f, 1.0f);
	static const FLinearColor MutedDot(0.095f, 0.110f, 0.150f, 1.0f);
	static const FLinearColor LabelMuted(0.45f, 0.50f, 0.58f, 1.0f);
	const FLinearColor DotColor = bDone ? DoneColor : (bCur ? IndustryColor : MutedDot);

	if (NumberText)
	{
		NumberText->SetText(FText::AsNumber(Tier));
		NumberText->SetColorAndOpacity(FSlateColor(bGlow ? FLinearColor::White : LabelMuted));
	}
	if (TierLabel)
	{
		TierLabel->SetText(FText::FromString(FString::Printf(TEXT("T%d"), Tier)));
		TierLabel->SetColorAndOpacity(FSlateColor(bCur ? FMath::Lerp(IndustryColor, FLinearColor::White, 0.35f) : LabelMuted));
	}
	if (DotFill)
	{
		DotFill->SetBrushTintColor(FSlateColor(DotColor));
	}
	if (DotSizeBox)
	{
		const float DotSize = bCur ? 64.f : 54.f;
		DotSizeBox->SetWidthOverride(DotSize);
		DotSizeBox->SetHeightOverride(DotSize);
	}
	if (DotGloss)
	{
		DotGloss->SetVisibility(bGlow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (NodeGlow)
	{
		NodeGlow->SetVisibility(bGlow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		NodeGlow->SetColorAndOpacity(FLinearColor(DotColor.R, DotColor.G, DotColor.B, bCur ? 0.75f : 0.40f));
	}
}
