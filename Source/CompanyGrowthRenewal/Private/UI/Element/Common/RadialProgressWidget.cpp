#include "UI/Element/Common/RadialProgressWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

void URadialProgressWidget::SetPercent(float In01)
{
	Percent = FMath::Clamp(In01, 0.f, 1.f);
	Invalidate(EInvalidateWidget::Paint);
}

void URadialProgressWidget::SetFillColor(const FLinearColor& InColor)
{
	if (FillColor.Equals(InColor)) { return; }
	FillColor = InColor;
	Invalidate(EInvalidateWidget::Paint);
}

TSharedRef<SWidget> URadialProgressWidget::RebuildWidget()
{
	// 디자이너가 루트를 배치하지 않은 경우 고정 크기 SizeBox 로 페인트 영역 확보 (프리뷰 지원)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RingRoot"));
		Root->SetWidthOverride(RingSize);
		Root->SetHeightOverride(RingSize);
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

int32 URadialProgressWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 TrackLayer = MaxLayer + 1;
	const int32 FillLayer  = MaxLayer + 2;

	const FVector2D Size   = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const float Radius     = FMath::Min(Size.X, Size.Y) * 0.5f - Thickness;

	if (Radius <= 0.f)
	{
		// 그릴 게 없으면 부모가 소비한 레이어만 반환 (레이어 ID 낭비 방지)
		return MaxLayer;
	}

	// 트랙: 전체 원 (64 세그먼트, 닫기 위해 복사 후 Add — dangling ref 방지)
	{
		constexpr int32 N = 64;
		TArray<FVector2D> TrackPts;
		TrackPts.Reserve(N + 1);
		for (int32 i = 0; i <= N; ++i)
		{
			const float Angle = (2.f * PI * i) / N;
			TrackPts.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, TrackLayer, AllottedGeometry.ToPaintGeometry(),
			TrackPts, ESlateDrawEffect::None, TrackColor, true, Thickness);
	}

	// 필 아크: -PI/2(12시 방향)에서 시계방향으로 Percent * 2PI
	const float SweepRad = FMath::Clamp(Percent, 0.f, 1.f) * 2.f * PI;
	if (SweepRad > KINDA_SMALL_NUMBER)
	{
		const int32 N = FMath::Max(2, FMath::CeilToInt(64.f * SweepRad / (2.f * PI)));
		TArray<FVector2D> FillPts;
		FillPts.Reserve(N + 1);
		for (int32 i = 0; i <= N; ++i)
		{
			const float Angle = (-PI * 0.5f) + (SweepRad * i) / N;
			FillPts.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, FillLayer, AllottedGeometry.ToPaintGeometry(),
			FillPts, ESlateDrawEffect::None, FillColor, true, Thickness);
	}

	return FillLayer;
}
