// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/FatigueBarWidget.h"
#include "Components/ProgressBar.h"

void UFatigueBarWidget::SetFatigue01(float InFill01)
{
	const float Fill01 = FMath::Clamp(InFill01, 0.f, 1.f);

	// throttle 은 호출부(워커 1초 게이트)가 1차로 거르지만, 동일값 재호출도 차단
	if (FMath::IsNearlyEqual(Fill01, LastFill01, 0.005f))
	{
		return;
	}
	LastFill01 = Fill01;

	if (!FatigueBar)
	{
		return;
	}

	FatigueBar->SetPercent(Fill01);
	// UI_STYLE_CATALOG §4-A: fill 텍스처(GradientTexture0)는 WBP 유지, 색만 FillColorAndOpacity 로 교체
	FatigueBar->SetFillColorAndOpacity(FillColorForFatigue(Fill01));
}

FLinearColor UFatigueBarWidget::FillColorForFatigue(float Fill01)
{
	// 0=초록(활기) → 0.5=노랑(피곤) → 1=빨강(위험). 2구간 선형 보간.
	const FLinearColor Green(0.263f, 0.788f, 0.369f, 1.f);
	const FLinearColor Yellow(0.94f, 0.78f, 0.22f, 1.f);
	const FLinearColor Red(0.88f, 0.20f, 0.16f, 1.f);

	if (Fill01 < 0.5f)
	{
		return FMath::Lerp(Green, Yellow, Fill01 / 0.5f);
	}
	return FMath::Lerp(Yellow, Red, (Fill01 - 0.5f) / 0.5f);
}
