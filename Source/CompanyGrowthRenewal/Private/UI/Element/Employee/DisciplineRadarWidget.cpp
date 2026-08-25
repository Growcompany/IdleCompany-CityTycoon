// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Employee/DisciplineRadarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 AxisCount = 6;

	// 12시 기준 시계방향 (i=0 이 위 꼭짓점)
	FVector2D AxisPoint(const FVector2D& Center, float Radius, int32 AxisIndex)
	{
		const float Angle = -HALF_PI + AxisIndex * (2.f * PI / AxisCount);
		return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	}

	// 닫힌 폴리라인 (Pts.Add(Pts[0]) 재할당 dangling 함정 회피 — 값 복사로 닫음)
	void DrawClosedPolyline(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2D> Closed;
		Closed.Reserve(Points.Num() + 1);
		Closed.Append(Points);
		const FVector2D First = Points[0];
		Closed.Add(First);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), Closed,
			ESlateDrawEffect::None, Color, true, Thickness);
	}

	// 중심 팬 삼각분할 채움 (화이트 브러시 — 정점색이 곧 표시색)
	void FillFan(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, const TArray<FVector2D>& Outer, const FLinearColor& Color)
	{
		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
		const FSlateRenderTransform& RT = Geo.ToPaintGeometry().GetAccumulatedRenderTransform();
		const FColor VertColor = Color.ToFColor(true);

		TArray<FSlateVertex> Verts;
		Verts.Reserve(Outer.Num() + 1);
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Center), FVector2f(0.f, 0.f), VertColor));
		for (const FVector2D& P : Outer)
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P), FVector2f(0.f, 0.f), VertColor));
		}

		TArray<SlateIndex> Indexes;
		Indexes.Reserve(Outer.Num() * 3);
		for (int32 i = 0; i < Outer.Num(); ++i)
		{
			Indexes.Add(0);
			Indexes.Add(1 + i);
			Indexes.Add(1 + (i + 1) % Outer.Num());
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
	}

	void FillCircle(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		constexpr int32 Segments = 12;
		TArray<FVector2D> Outer;
		Outer.Reserve(Segments);
		for (int32 i = 0; i < Segments; ++i)
		{
			const float Angle = i * (2.f * PI / Segments);
			Outer.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FillFan(OutDrawElements, Layer, Geo, Center, Outer, Color);
	}
}

void UDisciplineRadarWidget::SetRadarData(const TArray<int32>& InValues, int32 InDisplayMax, const FLinearColor& InGradeColor)
{
	const float Max = FMath::Max(1, InDisplayMax);
	NormValues.SetNum(AxisCount);
	for (int32 i = 0; i < AxisCount; ++i)
	{
		const int32 Raw = InValues.IsValidIndex(i) ? InValues[i] : 0;
		// 절대 상한 정규화라 저포인트 구간이 점처럼 작아진다 — 반경만 압축(순서·비교 가능성은 보존).
		// 0.7~0.8 사이에서 취향 조절 가능(1.0 = 압축 없음).
		constexpr float RadiusCurvePower = 0.75f;
		const float Linear = FMath::Clamp(Raw / Max, 0.f, 1.f);
		NormValues[i] = FMath::Clamp(FMath::Pow(Linear, RadiusCurvePower), MinValueFrac, 1.f);
	}
	GradeColor = InGradeColor;
	GradeColor.A = 1.f;

	// 첫 데이터 주입 = 중심(0)에서 드로인 시작, 이후 주입 = 현재 표시값에서 이어서 트윈
	if (ShownValues.Num() != AxisCount)
	{
		ShownValues.Init(0.f, AxisCount);
	}

	if (TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		Cached->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UDisciplineRadarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ShownValues.Num() != AxisCount || NormValues.Num() != AxisCount)
	{
		return;
	}

	const float Alpha = (ValueTweenSpeed <= 0.f) ? 1.f : FMath::Clamp(InDeltaTime * ValueTweenSpeed, 0.f, 1.f);
	bool bDirty = false;
	for (int32 i = 0; i < AxisCount; ++i)
	{
		const float Diff = NormValues[i] - ShownValues[i];
		if (FMath::Abs(Diff) < 0.0015f)
		{
			if (Diff != 0.f)
			{
				ShownValues[i] = NormValues[i];
				bDirty = true;
			}
			continue;
		}
		ShownValues[i] += Diff * Alpha;
		bDirty = true;
	}

	if (bDirty)
	{
		if (TSharedPtr<SWidget> Cached = GetCachedWidget())
		{
			Cached->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
}

void UDisciplineRadarWidget::SetAxisLabels(const TArray<FText>& InLabels)
{
	AxisLabels = InLabels;

	if (TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		Cached->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

TSharedRef<SWidget> UDisciplineRadarWidget::RebuildWidget()
{
	// 자가 SizeBox 루트 (WBP 빈 트리 전제 — SelectionChevron 패턴)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RadarRoot"));
		Root->SetWidthOverride(RadarWidth);
		Root->SetHeightOverride(RadarHeight);
		WidgetTree->RootWidget = Root;
	}

	if (!LabelFont.FontObject)
	{
		TSoftObjectPtr<UFont> FontPath = TSoftObjectPtr<UFont>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font")));
		LabelFont = FSlateFontInfo(FontPath.LoadSynchronous(), 21);
	}

	return Super::RebuildWidget();
}

int32 UDisciplineRadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	int32 Layer = MaxLayer + 1;

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 40.f || Size.Y < 40.f)
	{
		return MaxLayer;
	}

	const FVector2D Center = Size * 0.5f;
	const float Radius = FMath::Max(10.f, FMath::Min(Size.X, Size.Y) * 0.5f - LabelInset);

	// 그리드 링 3단 + 스포크
	for (float Frac : { 1.f / 3.f, 2.f / 3.f, 1.f })
	{
		TArray<FVector2D> Ring;
		Ring.Reserve(AxisCount);
		for (int32 i = 0; i < AxisCount; ++i)
		{
			Ring.Add(AxisPoint(Center, Radius * Frac, i));
		}
		DrawClosedPolyline(OutDrawElements, Layer, AllottedGeometry, Ring, RingColor, RingThickness);
	}
	for (int32 i = 0; i < AxisCount; ++i)
	{
		TArray<FVector2D> Spoke = { Center, AxisPoint(Center, Radius, i) };
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Spoke,
			ESlateDrawEffect::None, RingColor, true, RingThickness);
	}
	++Layer;

	// 데이터 폴리곤 — fill(등급색 저알파) → 스트로크(다크 파생) → 꼭짓점 도트. 표시값 = 트윈(ShownValues)
	const TArray<float>& DrawValues = (ShownValues.Num() == AxisCount) ? ShownValues : NormValues;
	if (DrawValues.Num() == AxisCount)
	{
		TArray<FVector2D> Data;
		Data.Reserve(AxisCount);
		for (int32 i = 0; i < AxisCount; ++i)
		{
			Data.Add(AxisPoint(Center, Radius * DrawValues[i], i));
		}

		FLinearColor Fill = GradeColor;
		Fill.A = FillAlpha;
		FillFan(OutDrawElements, Layer, AllottedGeometry, Center, Data, Fill);
		++Layer;

		FLinearColor Stroke = GradeColor * StrokeDarken;
		Stroke.A = 1.f;
		DrawClosedPolyline(OutDrawElements, Layer, AllottedGeometry, Data, Stroke, StrokeThickness);
		for (const FVector2D& P : Data)
		{
			FillCircle(OutDrawElements, Layer, AllottedGeometry, P, DotRadius, Stroke);
		}
		++Layer;
	}

	// 축 라벨 — 축 방향별 앵커 보정 (위=중앙상단, 우측=좌정렬, 아래=중앙하단, 좌측=우정렬)
	if (AxisLabels.Num() == AxisCount && LabelFont.FontObject)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		for (int32 i = 0; i < AxisCount; ++i)
		{
			const FString Label = AxisLabels[i].ToString();
			if (Label.IsEmpty())
			{
				continue;
			}
			const FVector2D TextSize = Measure->Measure(Label, LabelFont);
			const FVector2D Anchor = AxisPoint(Center, Radius + LabelGap, i);

			FVector2D Pos;
			if (i == 0)      { Pos = Anchor + FVector2D(-TextSize.X * 0.5f, -TextSize.Y); }
			else if (i < 3)  { Pos = Anchor + FVector2D(4.f, -TextSize.Y * 0.5f); }
			else if (i == 3) { Pos = Anchor + FVector2D(-TextSize.X * 0.5f, 0.f); }
			else             { Pos = Anchor + FVector2D(-TextSize.X - 4.f, -TextSize.Y * 0.5f); }

			FSlateDrawElement::MakeText(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(Pos))),
				Label, LabelFont, ESlateDrawEffect::None, LabelColor);
		}
	}

	return Layer;
}
