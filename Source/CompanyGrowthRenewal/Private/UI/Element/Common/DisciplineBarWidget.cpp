#include "UI/Element/Common/DisciplineBarWidget.h"

#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
	FText DisciplinePercentText(int32 Pct)
	{
		FNumberFormattingOptions Opts;
		Opts.SetUseGrouping(false);
		return FText::Format(NSLOCTEXT("DisciplineBar", "PctFmt", "{0}%"), FText::AsNumber(Pct, &Opts));
	}
}

void UDisciplineBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIconProperties();
}

void UDisciplineBarWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyIconProperties();
}

void UDisciplineBarWidget::ApplyIconProperties()
{
	UGlobalUtilFunctions::ApplyWidgetIcon(DiscIcon, IconTexture, IconSize, IconPadding);
}

FLinearColor UDisciplineBarWidget::GetLightWellSlotFill(int32 SlotIdx)
{
	FLinearColor Fill = UGlobalUtilFunctions::GetStepColor(SlotIdx + 1) * 0.55f;
	Fill.A = 1.0f;   // 스칼라 곱이 알파까지 깎으므로 복원
	return Fill;
}

void UDisciplineBarWidget::SetInfo(const FText& InName, float InAcquired, float InTarget, const FLinearColor& InFillColor)
{
	UnlockRevealValueWidth();
	const bool bMiss = InAcquired < InTarget;
	AchievedRatio = InTarget > 0.0f ? InAcquired / InTarget : 0.0f;
	RevealFillColor = InFillColor;
	bActiveDiscipline = true;

	if (Text_Name)
	{
		Text_Name->SetText(InName);
		Text_Name->SetColorAndOpacity(FSlateColor(NameInk));
	}
	if (Text_Value)
	{
		// 바만 0~1 클램프 — 획득 점수는 상한 없이 누적되므로 오버필(137%)을 텍스트엔 그대로 노출한다
		Text_Value->SetText(DisciplinePercentText(InTarget > 0.f ? FMath::FloorToInt(InAcquired / InTarget * 100.f) : 0));
		Text_Value->SetColorAndOpacity(FSlateColor(bMiss ? MissInk : ValueInk));
	}
	if (Bar)
	{
		Bar->SetPercent(InTarget > 0.0f ? FMath::Clamp(InAcquired / InTarget, 0.0f, 1.0f) : 0.0f);
		Bar->SetFillColorAndOpacity(InFillColor);
	}
}

void UDisciplineBarWidget::SetRevealProgress(float Alpha)
{
	if (!bActiveDiscipline) { return; }
	const float Progress = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float Shown = AchievedRatio * Progress;
	if (Bar)
	{
		Bar->SetPercent(FMath::Min(Shown, 1.0f));
	}
	if (Text_Value)
	{
		if (Progress < 1.0f) { LockRevealValueWidth(); }
		Text_Value->SetText(DisciplinePercentText(FMath::FloorToInt(Shown * 100.0f)));
		const FLinearColor& Ink = Shown >= 1.0f ? HitInk : (Progress >= 1.0f ? MissInk : ValueInk);
		Text_Value->SetColorAndOpacity(FSlateColor(Ink));
		if (Progress >= 1.0f) { UnlockRevealValueWidth(); }
	}
}

// 값 글자는 DownOnly ScaleBox 안이라 글자 수가 늘면 줄어든다. 세는 동안 최종 값의 폭을 최소 폭으로 잡아
// 처음부터 최종 배율로 그린다 — 100%를 넘는 순간 크기가 튀지 않는다.
void UDisciplineBarWidget::LockRevealValueWidth()
{
	if (bRevealWidthLocked || !Text_Value) { return; }
	RevealBaseMinWidth = Text_Value->GetMinDesiredWidth();
	Text_Value->SetText(DisciplinePercentText(FMath::FloorToInt(AchievedRatio * 100.0f)));
	Text_Value->ForceLayoutPrepass();
	Text_Value->SetMinDesiredWidth(FMath::Max(RevealBaseMinWidth, static_cast<float>(Text_Value->GetDesiredSize().X)));
	bRevealWidthLocked = true;
}

void UDisciplineBarWidget::UnlockRevealValueWidth()
{
	if (!bRevealWidthLocked) { return; }
	if (Text_Value) { Text_Value->SetMinDesiredWidth(RevealBaseMinWidth); }
	bRevealWidthLocked = false;
}

void UDisciplineBarWidget::SetFillFlash(float Amount)
{
	if (!Bar || !bActiveDiscipline) { return; }
	FLinearColor Fill = FMath::Lerp(RevealFillColor, FLinearColor::White, FMath::Clamp(Amount, 0.0f, 1.0f) * 0.75f);
	Fill.A = RevealFillColor.A;
	Bar->SetFillColorAndOpacity(Fill);
}

void UDisciplineBarWidget::SetEmpty(const FText& InName)
{
	UnlockRevealValueWidth();
	AchievedRatio = 0.0f;
	bActiveDiscipline = false;
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
