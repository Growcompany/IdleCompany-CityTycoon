#include "UI/Element/Employee/EquipmentCalloutLinesWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Spacer.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"

namespace EquipmentCallout
{
	// 원·원판은 둥근 상자 SDF 로 — 작은 원을 굵은 폴리라인으로 그리면 짧은 마디에서 미터가 쪼개진다(플레이북 §3)
	void DrawRoundBox(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, float HalfSize, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
	{
		if (HalfSize <= 0.f)
		{
			return;
		}
		const FSlateRoundedBoxBrush Brush(Fill, HalfSize, Outline, OutlineWidth);
		const FVector2D Size(HalfSize * 2.f, HalfSize * 2.f);
		// 둥근 상자 면 색 = MakeBox 틴트(브러시 TintColor 는 안 쓰인다). 테 색은 브러시 OutlineSettings 가 따로 낸다
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)), &Brush, ESlateDrawEffect::None, Fill);
	}

	// 반지름 = 테 가운데. 상자 테두리는 안쪽으로 그려지므로 상자를 테 절반만큼 키운다
	void DrawCircle(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness)
	{
		if (Radius <= 0.f || Thickness <= 0.f)
		{
			return;
		}
		// 빈 면은 테와 같은 RGB 의 알파 0 — 검은 투명이면 테 안쪽 가장자리가 어둡게 번진다
		DrawRoundBox(OutDrawElements, Layer, Geo, Center, Radius + Thickness * 0.5f, FLinearColor(Color.R, Color.G, Color.B, 0.f), Color, Thickness);
	}

	void DrawDisc(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		DrawRoundBox(OutDrawElements, Layer, Geo, Center, Radius, Color, FLinearColor::Transparent, 0.f);
	}

	FLinearColor HaloAt(float U, const FLinearColor& Inner, const FLinearColor& Mid, const FLinearColor& Outer)
	{
		return U < 0.5f ? FMath::Lerp(Inner, Mid, U / 0.5f) : FMath::Lerp(Mid, Outer, (U - 0.5f) / 0.5f);
	}

	// 방사 그라데이션 = 바깥부터 원판을 겹친다. 각 겹의 색·알파는 그 띠의 합성 결과가 목표값이 되도록 앞 겹을 빼서 정한다(premultiplied)
	// 겹마다 층을 올린다 — 같은 층의 둥근 상자는 배치 순서가 보장되지 않는다. 반환 = 마지막으로 쓴 층
	int32 DrawHalo(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo, const FVector2D& Center,
		float Radius, int32 Steps, const FLinearColor& Inner, const FLinearColor& Mid, const FLinearColor& Outer)
	{
		if (Radius <= 0.f || Steps <= 0)
		{
			return Layer;
		}
		FLinearColor Composite(0.f, 0.f, 0.f, 0.f);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float DiscRadius = Radius * (1.f - static_cast<float>(Step) / Steps);
			const float BandMid = (1.f - (Step + 0.5f) / Steps);
			const FLinearColor Target = HaloAt(BandMid, Inner, Mid, Outer);
			if (Target.A <= Composite.A + KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const float LayerAlpha = FMath::Clamp((Target.A - Composite.A) / FMath::Max(1.f - Composite.A, KINDA_SMALL_NUMBER), 0.f, 1.f);
			FLinearColor LayerColor(0.f, 0.f, 0.f, 0.f);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				const float TargetPremul = Target.Component(Channel) * Target.A;
				const float Value = (TargetPremul - Composite.Component(Channel) * (1.f - LayerAlpha)) / LayerAlpha;
				LayerColor.Component(Channel) = FMath::Clamp(Value, 0.f, 1.f);
			}
			LayerColor.A = LayerAlpha;
			DrawDisc(OutDrawElements, ++Layer, Geo, Center, DiscRadius, LayerColor);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				Composite.Component(Channel) = LayerColor.Component(Channel) * LayerAlpha + Composite.Component(Channel) * (1.f - LayerAlpha);
			}
			Composite.A = LayerAlpha + Composite.A * (1.f - LayerAlpha);
		}
		return Layer;
	}

	FVector2D QuadPoint(const FEquipmentCallout& Curve, float T)
	{
		const float U = 1.f - T;
		return Curve.Start * (U * U) + Curve.Control * (2.f * U * T) + Curve.End * (T * T);
	}

	// 곡선 길이를 따라 Spacing 마다 점 — 첫 점은 시작점(목업 dasharray 의 첫 대시)
	void BeadPositions(const FEquipmentCallout& Curve, float Spacing, TArray<FVector2D>& Out)
	{
		constexpr int32 Samples = 96;
		TArray<FVector2D> Points;
		TArray<float> Lengths;
		Points.Reserve(Samples + 1);
		Lengths.Reserve(Samples + 1);
		float Total = 0.f;
		for (int32 Index = 0; Index <= Samples; ++Index)
		{
			const FVector2D Point = QuadPoint(Curve, static_cast<float>(Index) / Samples);
			if (Index > 0)
			{
				Total += static_cast<float>(FVector2D::Distance(Points.Last(), Point));
			}
			Points.Add(Point);
			Lengths.Add(Total);
		}
		Out.Reset();
		int32 Segment = 1;
		for (float Distance = 0.f; Distance <= Total + 0.01f && Spacing > 0.f; Distance += Spacing)
		{
			while (Segment < Samples && Lengths[Segment] < Distance)
			{
				++Segment;
			}
			const float SegmentLength = Lengths[Segment] - Lengths[Segment - 1];
			const float Alpha = SegmentLength > KINDA_SMALL_NUMBER ? (Distance - Lengths[Segment - 1]) / SegmentLength : 0.f;
			Out.Add(FMath::Lerp(Points[Segment - 1], Points[Segment], FMath::Clamp(Alpha, 0.f, 1.f)));
		}
	}
}

TSharedRef<SWidget> UEquipmentCalloutLinesWidget::RebuildWidget()
{
	// 무인 자가 트리 — 크기는 호스트 캔버스 슬롯(전체 앵커)이 정한다
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("CalloutRoot"));
	}
	return Super::RebuildWidget();
}

void UEquipmentCalloutLinesWidget::SetCallout(const FEquipmentCallout& InCallout)
{
	bHasCallout = true;
	Callout = InCallout;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UEquipmentCalloutLinesWidget::ClearCallout()
{
	if (bHasCallout)
	{
		bHasCallout = false;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UEquipmentCalloutLinesWidget::SetFlash(const FVector2D& Center, float Scale, float Opacity)
{
	bFlashVisible = true;
	FlashCenter = Center;
	FlashScale = FMath::Clamp(Scale, 0.f, 1.f);
	FlashOpacity = FMath::Clamp(Opacity, 0.f, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UEquipmentCalloutLinesWidget::ClearFlash()
{
	if (bFlashVisible)
	{
		bFlashVisible = false;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UEquipmentCalloutLinesWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled) + 1;
	const FLinearColor White(1.f, 1.f, 1.f, 1.f);

	if (bHasCallout)
	{
		// 목업 순서: 후광 → 구슬 곡선 → 끝 점. 같은 레이어 안 순서는 배치(batch)가 바꿀 수 있어 층을 나눈다
		Layer = EquipmentCallout::DrawHalo(OutDrawElements, Layer, AllottedGeometry, Callout.Anchor, HaloRadius, HaloSteps,
			HaloInnerColor, HaloMidColor, HaloOuterColor);
		TArray<FVector2D> Beads;
		EquipmentCallout::BeadPositions(Callout, BeadSpacing, Beads);
		const float BeadRadius = BeadDiameter * 0.5f;
		for (const FVector2D& Bead : Beads)
		{
			EquipmentCallout::DrawDisc(OutDrawElements, Layer + 1, AllottedGeometry, Bead, BeadRadius + static_cast<float>(BeadGlowReach.Y),
				BeadGlowColor.CopyWithNewOpacity(static_cast<float>(BeadGlowAlpha.Y)));
			EquipmentCallout::DrawDisc(OutDrawElements, Layer + 2, AllottedGeometry, Bead, BeadRadius + static_cast<float>(BeadGlowReach.X),
				BeadGlowColor.CopyWithNewOpacity(static_cast<float>(BeadGlowAlpha.X)));
		}
		for (const FVector2D& Bead : Beads)
		{
			EquipmentCallout::DrawDisc(OutDrawElements, Layer + 3, AllottedGeometry, Bead, BeadRadius, BeadColor);
		}
		EquipmentCallout::DrawDisc(OutDrawElements, Layer + 4, AllottedGeometry, Callout.Anchor, EndDotRadius, White);
		Layer += 4;
	}

	if (bFlashVisible && FlashOpacity > 0.f)
	{
		const float Radius = FlashRadius * FlashScale;
		auto Faded = [this](FLinearColor Color, float Alpha)
		{
			Color.A = Alpha * FlashOpacity;
			return Color;
		};
		// 테 하나 + 바깥으로만 번지는 빛 두 겹, 가운데는 비워 얼굴·장비를 가리지 않는다. 회전 없음
		const float RingOuter = Radius + FlashThickness * 0.5f;
		++Layer;
		EquipmentCallout::DrawCircle(OutDrawElements, Layer, AllottedGeometry, FlashCenter, RingOuter + 7.f, Faded(FlashColor, 0.12f), 14.f);
		EquipmentCallout::DrawCircle(OutDrawElements, Layer, AllottedGeometry, FlashCenter, RingOuter + 3.f, Faded(FlashColor, 0.3f), 6.f);
		EquipmentCallout::DrawCircle(OutDrawElements, Layer, AllottedGeometry, FlashCenter, Radius, Faded(FlashColor, 1.f), FlashThickness);
		EquipmentCallout::DrawDisc(OutDrawElements, Layer, AllottedGeometry, FlashCenter, FlashDotRadius + 2.f, Faded(FlashColor, 1.f));
		EquipmentCallout::DrawDisc(OutDrawElements, Layer + 1, AllottedGeometry, FlashCenter, FlashDotRadius, Faded(White, 1.f));
		++Layer;
	}
	return Layer;
}
