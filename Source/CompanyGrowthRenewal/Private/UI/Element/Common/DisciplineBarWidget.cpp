#include "UI/Element/Common/DisciplineBarWidget.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Global/GlobalUtilFunctions.h"

FLinearColor UDisciplineBarWidget::GetLightWellSlotFill(int32 SlotIdx)
{
	FLinearColor Fill = UGlobalUtilFunctions::GetStepColor(SlotIdx + 1) * 0.55f;
	Fill.A = 1.0f;   // 스칼라 곱이 알파까지 깎으므로 복원
	return Fill;
}

void UDisciplineBarWidget::SetInfo(const FText& InName, float InAcquired, float InTarget, const FLinearColor& InFillColor)
{
	const bool bMiss = InAcquired < InTarget;

	if (Text_Name)
	{
		Text_Name->SetText(InName);
		Text_Name->SetColorAndOpacity(FSlateColor(NameInk));
	}
	if (Text_Value)
	{
		// 바만 0~1 클램프 — 획득 점수는 상한 없이 누적되므로 오버필(137%)을 텍스트엔 그대로 노출한다
		const int32 Pct = InTarget > 0.f ? FMath::FloorToInt(InAcquired / InTarget * 100.f) : 0;
		FNumberFormattingOptions Opts;
		Opts.SetUseGrouping(false);
		Text_Value->SetText(FText::Format(
			NSLOCTEXT("DisciplineBar", "PctFmt", "{0}%"), FText::AsNumber(Pct, &Opts)));
		Text_Value->SetColorAndOpacity(FSlateColor(bMiss ? MissInk : ValueInk));
	}
	if (Bar)
	{
		Bar->SetPercent(InTarget > 0.0f ? FMath::Clamp(InAcquired / InTarget, 0.0f, 1.0f) : 0.0f);
		Bar->SetFillColorAndOpacity(InFillColor);
	}
}

void UDisciplineBarWidget::SetEmpty(const FText& InName)
{
	FLinearColor MutedValue = ValueInk;
	MutedValue.A = EmptyInkAlpha;
	FLinearColor MutedName = NameInk;
	MutedName.A = EmptyInkAlpha;

	if (Text_Name)
	{
		Text_Name->SetText(InName);
		Text_Name->SetColorAndOpacity(FSlateColor(MutedName));
	}
	if (Text_Value)
	{
		Text_Value->SetText(FText::FromString(TEXT("―")));
		Text_Value->SetColorAndOpacity(FSlateColor(MutedValue));
	}
	if (Bar)
	{
		Bar->SetPercent(0.0f);
	}
}
