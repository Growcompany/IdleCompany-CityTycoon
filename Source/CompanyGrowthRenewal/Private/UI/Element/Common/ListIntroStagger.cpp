// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/ListIntroStagger.h"

#include "Blueprint/UserWidget.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace
{
	float EaseOutCubic(float A)
	{
		const float T = 1.0f - A;
		return 1.0f - T * T * T;
	}
}

void FListIntroStagger::Play(UUserWidget& Widget, int32 OrderIndex)
{
	if (FMotionGate::IsReduced())
	{
		// 풀에서 재사용되는 항목이라 이전 재생의 트랜스폼/알파가 남아 있을 수 있다 — 제자리로 되돌린다
		Widget.SetRenderTransform(FWidgetTransform());
		Widget.SetRenderOpacity(1.f);
		bPlaying = false;
		return;
	}

	Delay = FMath::Min(OrderIndex * Stagger, MaxDelay);
	Elapsed = 0.f;
	bPlaying = true;

	FWidgetTransform Init;
	Init.Translation = FVector2D(SlideX, 0.f);
	Widget.SetRenderTransform(Init);
	Widget.SetRenderOpacity(0.f);
}

bool FListIntroStagger::Tick(UUserWidget& Widget, float DeltaTime)
{
	if (!bPlaying)
	{
		return false;
	}

	Elapsed += DeltaTime;
	if (Elapsed < Delay)
	{
		return true;
	}

	const float T = FMath::Clamp((Elapsed - Delay) / Duration, 0.f, 1.f);
	const float E = EaseOutCubic(T);

	FWidgetTransform Xform;
	Xform.Translation = FVector2D(SlideX * (1.f - E), 0.f);
	Widget.SetRenderTransform(Xform);
	Widget.SetRenderOpacity(E);

	if (T >= 1.f)
	{
		bPlaying = false;
		Widget.SetRenderTransform(FWidgetTransform());
		Widget.SetRenderOpacity(1.f);
	}
	return bPlaying;
}
