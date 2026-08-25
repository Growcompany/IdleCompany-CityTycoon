// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/CenterPivotWidgetStack.h"
#include "Slate/SCommonAnimatedSwitcher.h"

TSharedRef<SWidget> UCenterPivotWidgetStack::RebuildWidget()
{
	TSharedRef<SWidget> Result = Super::RebuildWidget();

	// SCommonAnimatedSwitcher 는 Zoom 전이에서 RenderTransform 만 세팅하고 pivot 은 SWidget 기본값(좌상단)을
	// 그대로 쓴다. UMG 가 pivot 을 밀어넣는 대상은 바깥 SOverlay 라 안쪽 스위처까지 닿지 않는다.
	if (MySwitcher.IsValid())
	{
		MySwitcher->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	return Result;
}
