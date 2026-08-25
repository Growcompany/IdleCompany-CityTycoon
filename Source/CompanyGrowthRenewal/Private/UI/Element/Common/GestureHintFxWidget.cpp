#include "UI/Element/Common/GestureHintFxWidget.h"
#include "UI/Element/Common/GestureHintTypes.h"
#include "UI/Element/Common/GestureHintWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

namespace
{
	constexpr float HandHeightPx = 104.f;   // WBP HandImage 크기와 동일 — 점 펄스 반경의 기준
	constexpr int32 FxRingSegments = 36;
	constexpr float TrailHalfPx = 58.f;     // 드래그 점선 반폭
	constexpr float TrailDashPx = 8.f;
	constexpr float TrailGapPx = 8.f;
	constexpr float TrailThickness = 4.f;
	constexpr float ArrowPx = 8.f;

	void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness)
	{
		if (Radius <= 1.f) return;
		TArray<FVector2D> Pts;
		Pts.Reserve(FxRingSegments + 1);
		for (int32 i = 0; i <= FxRingSegments; ++i)
		{
			const float A = (2.f * PI) * (static_cast<float>(i) / FxRingSegments);
			Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thickness);
	}
}

void UGestureHintFxWidget::SetMotion(EGestureHintKind InKind, float InElapsed)
{
	Kind = InKind;
	Elapsed = InElapsed;
	Invalidate(EInvalidateWidget::Paint);
}

TSharedRef<SWidget> UGestureHintFxWidget::RebuildWidget()
{
	// 디자이너가 루트를 배치하지 않은 경우 고정 크기 SizeBox 로 페인트 영역 확보 (프리뷰 지원)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GestureFxRoot"));
		Root->SetWidthOverride(UGestureHintWidget::BoxSize);
		Root->SetHeightOverride(UGestureHintWidget::BoxSize);
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

int32 UGestureHintFxWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (Kind != EGestureHintKind::Tap && Kind != EGestureHintKind::Drag)
	{
		// 그릴 게 없으면 부모가 소비한 레이어만 반환 (레이어 ID 낭비 방지)
		return MaxLayer;
	}

	const int32 FxLayer = MaxLayer + 1;
	const FVector2D Origin = AllottedGeometry.GetLocalSize() * 0.5f;
	using namespace GestureHintMotion;
	if (Kind == EGestureHintKind::Tap)
	{
		float S = 0.f, A = 0.f;
		TapPulse(Elapsed, S, A);
		const float Base = HandHeightPx * 0.17f;
		DrawCircle(OutDrawElements, FxLayer, AllottedGeometry, Origin, Base * S, FLinearColor(1.f, 1.f, 1.f, A), 3.f);
		TapPulse(Elapsed - 0.12f, S, A);
		DrawCircle(OutDrawElements, FxLayer, AllottedGeometry, Origin, Base * S, FLinearColor(1.f, 1.f, 1.f, A * 0.8f), 3.f);
	}
	else
	{
		const FLinearColor C(1.f, 1.f, 1.f, 0.85f);
		const FVector2D Y(0.f, -2.f);
		for (float X = -TrailHalfPx; X < TrailHalfPx; X += TrailDashPx + TrailGapPx)
		{
			TArray<FVector2D> Dash = { Origin + Y + FVector2D(X, 0.f), Origin + Y + FVector2D(FMath::Min(X + TrailDashPx, TrailHalfPx), 0.f) };
			FSlateDrawElement::MakeLines(OutDrawElements, FxLayer, AllottedGeometry.ToPaintGeometry(), Dash, ESlateDrawEffect::None, C, true, TrailThickness);
		}
		// 양끝 화살촉 (V 2점 — 미터 한계 회피: 꺾임 90° 미만 유지용으로 두 선분 따로)
		for (float Dir : { -1.f, 1.f })
		{
			const FVector2D Tip = Origin + Y + FVector2D(Dir * (TrailHalfPx + 6.f), 0.f);
			TArray<FVector2D> Up = { Tip + FVector2D(-Dir * ArrowPx, -ArrowPx), Tip };
			TArray<FVector2D> Dn = { Tip + FVector2D(-Dir * ArrowPx, ArrowPx), Tip };
			FSlateDrawElement::MakeLines(OutDrawElements, FxLayer, AllottedGeometry.ToPaintGeometry(), Up, ESlateDrawEffect::None, C, true, TrailThickness);
			FSlateDrawElement::MakeLines(OutDrawElements, FxLayer, AllottedGeometry.ToPaintGeometry(), Dn, ESlateDrawEffect::None, C, true, TrailThickness);
		}
	}
	return FxLayer;
}
