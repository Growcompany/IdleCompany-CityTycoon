#include "UI/Element/Employee/EmployeeRotateRingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Spacer.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"

namespace EmployeeRotateRing
{
	// 반 바퀴당 마디 수 — 양 끝(곡률 최대)에서도 꺾임이 미터 한계(74°)보다 한참 작다
	constexpr int32 HalfSegments = 36;
	// 바깥 번짐 두 겹 (굵기 배율, 알파) — 장착 빛 고리와 같은 2겹 구성
	constexpr float GlowWideScale = 4.5f;
	constexpr float GlowWideAlpha = 0.12f;
	constexpr float GlowNearScale = 2.2f;
	constexpr float GlowNearAlpha = 0.28f;
	// 끄는 동안 고리 번짐을 조금 올린다
	constexpr float DragGlowBoost = 1.6f;

	void BuildBrush(UTexture2D* Texture, FSlateBrush& OutBrush)
	{
		OutBrush = FSlateBrush();
		if (Texture)
		{
			OutBrush.SetResourceObject(Texture);
			OutBrush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			OutBrush.DrawAs = ESlateBrushDrawType::Image;
		}
	}

	// 위쪽(뒤) 반 = [π, 2π], 아래쪽(앞) 반 = [0, π]
	void BuildArc(const FVector2D& Center, float RadiusX, float RadiusY, float StartAngle, TArray<FVector2D>& OutPoints)
	{
		OutPoints.Reset(HalfSegments + 1);
		for (int32 Index = 0; Index <= HalfSegments; ++Index)
		{
			const float Angle = StartAngle + PI * static_cast<float>(Index) / HalfSegments;
			OutPoints.Add(Center + FVector2D(RadiusX * FMath::Cos(Angle), RadiusY * FMath::Sin(Angle)));
		}
	}

	void DrawArc(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color, /*bAntialias*/ true, Thickness);
	}

	void DrawImage(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FSlateBrush& Brush, const FVector2D& Center, const FVector2D& Size, const FLinearColor& Tint)
	{
		if (!Brush.GetResourceObject() || Size.X <= 0.f || Size.Y <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)), &Brush, ESlateDrawEffect::None, Tint);
	}
}

TSharedRef<SWidget> UEmployeeRotateRingWidget::RebuildWidget()
{
	// 무인 자가 트리 — 크기는 호스트 캔버스 슬롯이 정한다
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("RotateRingRoot"));
	}
	return Super::RebuildWidget();
}

void UEmployeeRotateRingWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	EmployeeRotateRing::BuildBrush(ArrowLeftTexture, ArrowLeftBrush);
	EmployeeRotateRing::BuildBrush(ArrowRightTexture, ArrowRightBrush);
	EmployeeRotateRing::BuildBrush(ShadowTexture, ShadowBrush);
}

void UEmployeeRotateRingWidget::SetDragging(bool bInDragging)
{
	if (bDragging != bInDragging)
	{
		bDragging = bInDragging;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UEmployeeRotateRingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled) + 1;
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 2.f || Size.Y < 2.f)
	{
		return Layer;
	}
	const FVector2D Center = Size * 0.5f;
	const float RadiusX = Size.X * 0.5f;
	const float RadiusY = Size.Y * 0.5f;

	EmployeeRotateRing::DrawImage(OutDrawElements, Layer, AllottedGeometry, ShadowBrush, Center, Size * ShadowScale, ShadowColor);
	++Layer;

	TArray<FVector2D> BackArc, FrontArc;
	EmployeeRotateRing::BuildArc(Center, RadiusX, RadiusY, PI, BackArc);
	EmployeeRotateRing::BuildArc(Center, RadiusX, RadiusY, 0.f, FrontArc);
	const float GlowBoost = bDragging ? EmployeeRotateRing::DragGlowBoost : 1.f;
	auto WithAlpha = [this](float Alpha)
	{
		FLinearColor Color = RingColor;
		Color.A = FMath::Clamp(RingColor.A * Alpha, 0.f, 1.f);
		return Color;
	};
	for (const TArray<FVector2D>* Arc : { &BackArc, &FrontArc })
	{
		const float ArcAlpha = Arc == &BackArc ? BackArcOpacity : 1.f;
		EmployeeRotateRing::DrawArc(OutDrawElements, Layer, AllottedGeometry, *Arc,
			WithAlpha(EmployeeRotateRing::GlowWideAlpha * GlowBoost * ArcAlpha), RingThickness * EmployeeRotateRing::GlowWideScale);
		EmployeeRotateRing::DrawArc(OutDrawElements, Layer, AllottedGeometry, *Arc,
			WithAlpha(EmployeeRotateRing::GlowNearAlpha * GlowBoost * ArcAlpha), RingThickness * EmployeeRotateRing::GlowNearScale);
		EmployeeRotateRing::DrawArc(OutDrawElements, Layer, AllottedGeometry, *Arc, WithAlpha(ArcAlpha), RingThickness);
	}
	++Layer;

	// 양 끝 화살표 — 타원 끝점에 가운데를 두어 굽은 꼬리가 고리 안쪽 위로, 촉이 바깥 아래로 향한다
	const FLinearColor ArrowTint = bDragging ? ArrowDragColor : ArrowColor;
	// 비율은 그릴 때 텍스처에서 — 에디터 빌드에선 생성 시점에 텍스처가 아직 컴파일 중이면 32×32 자리표시자다
	auto ArrowSize = [this](const UTexture2D* Texture)
	{
		const float Height = Texture ? Texture->GetSurfaceHeight() : 0.f;
		const float Aspect = Height > 0.f ? Texture->GetSurfaceWidth() / Height : 1.f;
		return FVector2D(ArrowHeight * Aspect, ArrowHeight);
	};
	EmployeeRotateRing::DrawImage(OutDrawElements, Layer, AllottedGeometry, ArrowLeftBrush,
		FVector2D(Center.X - RadiusX, Center.Y), ArrowSize(ArrowLeftTexture), ArrowTint);
	EmployeeRotateRing::DrawImage(OutDrawElements, Layer, AllottedGeometry, ArrowRightBrush,
		FVector2D(Center.X + RadiusX, Center.Y), ArrowSize(ArrowRightTexture), ArrowTint);
	return Layer;
}
