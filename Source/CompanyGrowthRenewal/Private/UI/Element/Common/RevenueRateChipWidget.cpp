#include "UI/Element/Common/RevenueRateChipWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Global/GlobalUtilFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Common/RevenueRatePulse.h"

void URevenueRateChipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LastChipMatSize = FVector2D::ZeroVector;
	bPulseActive = false;
	bHasActiveRate = false;
	PulseEpochSeconds = 0.0f;
	LastObservedRate = -1.0;
	LastDisplayedRate = INDEX_NONE;
	ResetPulseVisual();
}

void URevenueRateChipWidget::SetRate(double PerSec)
{
	if (GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		// 칩은 순수 표시물 — 클릭은 뒤로 통과
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	const int64 DisplayRate = GetRevenueRateDisplayKey(PerSec);
	const bool bNextActive = IsRevenueRateActive(PerSec);
	const bool bShouldPulse = ShouldPulseRevenueRate(
		LastObservedRate, PerSec, PulseRelativeThreshold);

	if (bHasActiveRate && !bNextActive)
	{
		bPulseActive = false;
	}
	if (bHasActiveRate != bNextActive)
	{
		bHasActiveRate = bNextActive;
		ResetPulseVisual();
	}
	if (bShouldPulse)
	{
		BeginPulse();
	}
	LastObservedRate = PerSec;

	// 같은 표시 정수면 문자열 재생성을 건너뛴다 (Slate invalidate + FText 할당 회피)
	if (RateText && ShouldRefreshRevenueRateText(LastDisplayedRate, PerSec))
	{
		LastDisplayedRate = DisplayRate;
		// AbbreviateNumber 경유 — 만/억/조 승격이라 값이 아무리 커도 5자를 안 넘는다
		const FText AbbreviatedRate = bNextActive
			? UGlobalUtilFunctions::AbbreviateNumber(DisplayRate)
			: FText::GetEmpty();
		RateText->SetText(FormatRevenueRateText(DisplayRate, AbbreviatedRate));
	}
}

void URevenueRateChipWidget::BeginPulse()
{
	if (const UWorld* CurrentWorld = GetWorld())
	{
		PulseEpochSeconds = CurrentWorld->GetTimeSeconds();
		bPulseActive = true;
	}
}

void URevenueRateChipWidget::ResetPulseVisual()
{
	if (TrendGlyph)
	{
		TrendGlyph->SetRenderScale(FVector2D(1.0f));
		const FLinearColor GlyphColor = bHasActiveRate
			? RateColor
			: MutedRateColor.CopyWithNewOpacity(MutedGlyphOpacity);
		TrendGlyph->SetColorAndOpacity(GlyphColor);
	}
	if (RateText)
	{
		RateText->SetColorAndOpacity(FSlateColor(
			bHasActiveRate ? RateColor : MutedRateColor));
	}
}

void URevenueRateChipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateChipMaterialSize(MyGeometry);

	if (!bPulseActive || GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	const UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		return;
	}

	const float Elapsed = FMath::Max(0.0f, CurrentWorld->GetTimeSeconds() - PulseEpochSeconds);
	const float Pulse = ComputeRevenuePulseStrength(Elapsed, PulseDuration);

	if (TrendGlyph)
	{
		const float Scale = 1.0f + Pulse * PulseScaleGain;
		TrendGlyph->SetRenderScale(FVector2D(Scale, Scale));
	}
	if (RateText)
	{
		RateText->SetColorAndOpacity(FSlateColor(
			FMath::Lerp(RateColor, RatePulseColor, Pulse)));
	}

	if (Elapsed >= PulseDuration)
	{
		bPulseActive = false;
		ResetPulseVisual();
	}
}

void URevenueRateChipWidget::UpdateChipMaterialSize(const FGeometry& MyGeometry)
{
	if (!ChipBG && !ChipLine)
	{
		return;
	}

	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastChipMatSize, 0.5f))
	{
		return;
	}
	LastChipMatSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UImage* ChipImage : { ChipBG, ChipLine })
	{
		if (!ChipImage)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* DynamicMaterial = ChipImage->GetDynamicMaterial())
		{
			DynamicMaterial->SetScalarParameterValue(WpxParam, LocalSize.X);
			DynamicMaterial->SetScalarParameterValue(HpxParam, LocalSize.Y);
		}
	}
}
