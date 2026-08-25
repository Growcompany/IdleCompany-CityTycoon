#include "UI/Element/Building/VaultGaugeWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"

void UVaultGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LastBarOpacity = -1.0f;

	if (FillBar)
	{
		FillBar->SetFillColorAndOpacity(ActiveFillColor);
	}
	if (EndingRim)
	{
		FSlateBrush EndingRimBrush = EndingRim->GetBrush();
		EndingRimBrush.OutlineSettings.Color = FSlateColor(EndingRimColor);
		EndingRim->SetBrush(EndingRimBrush);
	}
	ApplyBarOpacity(LastHealth);
}

void UVaultGaugeWidget::SetPresentation(
	float FillPct,
	EGaugeHealth Health,
	EVaultGaugePresentation Presentation)
{
	if (!GaugeBox || !FillBar || !FillTip || !EndingRim || !PearlBox)
	{
		return;
	}

	ApplyBarOpacity(Health);

	// 정지 상태는 호출자가 이전 채움값을 보유해도 빈 Bar만 남기고 Pearl은 숨긴다.
	const float P = (Health == EGaugeHealth::Idle)
		? 0.0f
		: FMath::Clamp(FillPct, 0.0f, 1.0f);
	const bool bBar = Presentation == EVaultGaugePresentation::Bar;
	const bool bPearl = Presentation == EVaultGaugePresentation::Pearl
		&& Health != EGaugeHealth::Idle;
	const ESlateVisibility GaugeVisibility = bBar
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	const ESlateVisibility PearlVisibility = bPearl
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	const ESlateVisibility RimVisibility = bBar && Health == EGaugeHealth::EndingSoon
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;

	if (GaugeBox->GetVisibility() != GaugeVisibility)
	{
		GaugeBox->SetVisibility(GaugeVisibility);
	}
	if (PearlBox->GetVisibility() != PearlVisibility)
	{
		PearlBox->SetVisibility(PearlVisibility);
	}

	if (!FMath::IsNearlyEqual(P, LastFill))
	{
		FillBar->SetPercent(P);
		LastFill = P;
	}
	if (EndingRim->GetVisibility() != RimVisibility)
	{
		EndingRim->SetVisibility(RimVisibility);
	}

	LastHealth = Health;
	LastPresentation = Presentation;
	RefreshFillTipGeometry();
}

void UVaultGaugeWidget::ApplyBarOpacity(EGaugeHealth Health)
{
	if (!GaugeBox)
	{
		return;
	}

	const float BarOpacity = ResolveVaultGaugeBarOpacity(Health);
	if (!FMath::IsNearlyEqual(BarOpacity, LastBarOpacity))
	{
		GaugeBox->SetRenderOpacity(BarOpacity);
		LastBarOpacity = BarOpacity;
	}
}

void UVaultGaugeWidget::SetGaugeWidth(float Width)
{
	if (!GaugeBox)
	{
		return;
	}

	LastWidth = FMath::Max(0.0f, Width);

	// GetWidthOverride()는 override 플래그와 무관하게 raw 값을 반환하므로 engage 여부를 먼저 확인한다.
	if (!GaugeBox->IsWidthOverride()
		|| !FMath::IsNearlyEqual(GaugeBox->GetWidthOverride(), LastWidth, 0.5f))
	{
		GaugeBox->SetWidthOverride(LastWidth);
	}

	RefreshFillTipGeometry();
}

void UVaultGaugeWidget::RefreshFillTipGeometry()
{
	if (!FillTip)
	{
		return;
	}

	const bool bShow = LastPresentation == EVaultGaugePresentation::Bar
		&& LastHealth != EGaugeHealth::Idle
		&& LastFill > 0.05f;
	const ESlateVisibility TipVisibility = bShow
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (FillTip->GetVisibility() != TipVisibility)
	{
		FillTip->SetVisibility(TipVisibility);
	}

	if (bShow)
	{
		const FVector2D TipTranslation(
			ComputeFillTipOffset(LastWidth, LastFill, 3.0f, 5.0f),
			0.0f);
		if (!FillTip->GetRenderTransform().Translation.Equals(TipTranslation, KINDA_SMALL_NUMBER))
		{
			FillTip->SetRenderTranslation(TipTranslation);
		}
	}
}
