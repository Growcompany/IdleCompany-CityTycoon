// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Effects/SelectionChevronWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

namespace
{
	// 위젯 로컬 크기/모양 (하단중앙 = V 꼭짓점)
	constexpr float BoxW = 130.f;
	constexpr float BoxH = 118.f;
	// 주의: 비율 HalfW/Depth 는 1.30 이상 유지할 것 — Slate FLineBuilder 는 꺾임각 76.5° 초과 시
	// 미터 대신 스트립 분할(캡 2개)로 그려서 꼭짓점이 깨짐 (48/38=1.263 으로 0.0013 차 탈락 실측)
	constexpr float HalfW = 48.f;      // V 반폭
	constexpr float Depth = 35.f;      // V 깊이 (꺾임각 72.2°, 미터 마진 확보)
	constexpr float EchoGap = 6.f;     // 메인-잔상 간격
	constexpr float EchoScale = 0.8f;

	// 단일 3점 폴리라인 — 꺾임각이 미터 한계(76.5°) 안이면 엔진이 꼭짓점을 깔끔하게 미터 처리.
	// 한 번에 그리므로 알파 자기중첩(겹침 진해짐)도 없음. 비율 규칙은 위 HalfW/Depth 주석 참조
	void DrawChevronV(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Tip, float InHalfW, float InDepth, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2D> V;
		V.Reserve(3);
		V.Add(Tip + FVector2D(-InHalfW, -InDepth));
		V.Add(Tip);
		V.Add(Tip + FVector2D(InHalfW, -InDepth));
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), V,
			ESlateDrawEffect::None, Color, true, Thickness);
	}
}

TSharedRef<SWidget> USelectionChevronWidget::RebuildWidget()
{
	// 무인 자가 트리 — 페인트 영역 확보용 고정 크기 루트
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChevronRoot"));
		Root->SetWidthOverride(BoxW);
		Root->SetHeightOverride(BoxH);
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void USelectionChevronWidget::SetChevronColors(const FLinearColor& InMainColor, const FLinearColor& InEchoColor)
{
	MainColor = InMainColor;
	EchoColor = InEchoColor;
	InvalidateLayoutAndVolatility();
}

int32 USelectionChevronWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 DrawLayer = MaxLayer + 1;

	// 메인 V (아래 방향, 꼭짓점 = 하단중앙)
	const FVector2D Tip(BoxW * 0.5f, BoxH - 8.f);
	DrawChevronV(OutDrawElements, DrawLayer, AllottedGeometry, Tip, HalfW, Depth,
		MainColor, 6.5f);

	// 위쪽 잔상 V (작고 옅게)
	const FVector2D Tip2 = Tip + FVector2D(0.f, -Depth - EchoGap);
	DrawChevronV(OutDrawElements, DrawLayer, AllottedGeometry, Tip2, HalfW * EchoScale, Depth * EchoScale,
		EchoColor, 5.f);

	return DrawLayer;
}
