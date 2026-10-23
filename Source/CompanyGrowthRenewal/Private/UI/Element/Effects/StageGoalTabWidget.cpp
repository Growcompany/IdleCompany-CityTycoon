#include "UI/Element/Effects/StageGoalTabWidget.h"

#include "Components/Image.h"

namespace
{
	float GoalTabEaseOutCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }
	float GoalTabEaseOutQuad(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Square(1.f - T); }
	float GoalTabEaseInQuad(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T; }
}

void UStageGoalTabWidget::Play(bool bReducedMotion)
{
	bReduced = bReducedMotion;
	Elapsed = 0.f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Apply(0.f);
}

float UStageGoalTabWidget::GetBodyWidthFor(float CellWidth)
{
	UWidget* ContentRow = GetWidgetFromName(TEXT("Content"));
	if (!ContentRow) return CellWidth;
	ForceLayoutPrepass();
	return FMath::Max(CellWidth, ContentRow->GetDesiredSize().X + ContentInset * 2.f);
}

void UStageGoalTabWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Elapsed < 0.f || IsFinished()) return;
	// 개발 배속과 무관하게 실제 시간으로 돈다 — 2배속에서 0.5초로 줄면 글자를 읽을 틈이 없다
	Elapsed = FMath::Min(RetractEnd, Elapsed + InDeltaTime);
	Apply(Elapsed);
	if (IsFinished()) SetVisibility(ESlateVisibility::Collapsed);
}

void UStageGoalTabWidget::Apply(float Time)
{
	if (!TabRoot) return;
	if (bReduced)
	{
		const float Alpha = FMath::Min(FMath::Clamp(Time / .1f, 0.f, 1.f), FMath::Clamp((RetractEnd - Time) / .12f, 0.f, 1.f));
		TabRoot->SetRenderTranslation(FVector2D::ZeroVector);
		TabRoot->SetRenderOpacity(Alpha);
		if (CheckImage) { CheckImage->SetRenderScale(FVector2D::UnitVector); CheckImage->SetRenderOpacity(1.f); }
		if (SheenImage) SheenImage->SetRenderOpacity(0.f);
		return;
	}

	// 0.1초에 내려와 살짝 지나쳤다가 제자리에 걸린다. 크기는 그대로 두고 위치만 움직인다(1배 초과 금지)
	float Offset = 0.f;
	if (Time < DropEnd) Offset = FMath::Lerp(-DesignHeight, Overshoot, GoalTabEaseOutCubic(Time / DropEnd));
	else if (Time < SettleEnd) Offset = FMath::Lerp(Overshoot, 0.f, GoalTabEaseOutQuad((Time - DropEnd) / (SettleEnd - DropEnd)));
	else if (Time >= HoldEnd) Offset = FMath::Lerp(0.f, -DesignHeight - Overshoot, GoalTabEaseInQuad((Time - HoldEnd) / (RetractEnd - HoldEnd)));
	TabRoot->SetRenderOpacity(1.f);
	TabRoot->SetRenderTranslation(FVector2D(0.f, Offset));

	if (CheckImage)
	{
		const float Pop = Time < DropEnd ? .9f : FMath::Lerp(.9f, 1.f, GoalTabEaseOutCubic((Time - DropEnd) / .08f));
		CheckImage->SetRenderScale(FVector2D(Pop, Pop));
		CheckImage->SetRenderOpacity(FMath::Clamp((Time - .12f) / .1f, 0.f, 1.f));
	}

	if (SheenImage)
	{
		const float Sweep = (Time - DropEnd) / .2f;
		const float Width = TabRoot->GetCachedGeometry().GetLocalSize().X;
		const bool bLive = Sweep > 0.f && Sweep < 1.f && Width > 1.f;
		SheenImage->SetRenderOpacity(bLive ? FMath::Sin(Sweep * PI) * .9f : 0.f);
		if (bLive) SheenImage->SetRenderTranslation(FVector2D(FMath::Lerp(-.6f, .6f, GoalTabEaseOutQuad(Sweep)) * Width, 0.f));
	}
}
