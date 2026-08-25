#include "UI/Element/Common/DiamondProgressWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

void UDiamondProgressWidget::SetPercent(float In01)
{
	Percent = FMath::Clamp(In01, 0.f, 1.f);
	Invalidate(EInvalidateWidget::Paint);
}

void UDiamondProgressWidget::SetFillColor(const FLinearColor& InColor)
{
	if (FillColor.Equals(InColor)) { return; }
	FillColor = InColor;
	Invalidate(EInvalidateWidget::Paint);
}

TSharedRef<SWidget> UDiamondProgressWidget::RebuildWidget()
{
	// 디자이너가 루트를 배치하지 않은 경우 고정 크기 SizeBox 로 페인트 영역 확보 (프리뷰 지원)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DiamondRoot"));
		Root->SetWidthOverride(DiamondSize);
		Root->SetHeightOverride(DiamondSize);
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

int32 UDiamondProgressWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 TrackLayer = MaxLayer + 1;
	const int32 FillLayer  = MaxLayer + 2;

	const FVector2D Size   = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;

	// 꼭짓점 미터는 두께의 √2/2 만큼 바깥으로 뻗는다 — 두께만큼 안으로 물려야 스트로크가 박스 안에 남는다
	const float HalfX = Size.X * 0.5f - Thickness;
	const float HalfY = Size.Y * 0.5f - Thickness;

	if (HalfX <= 0.f || HalfY <= 0.f)
	{
		// 그릴 게 없으면 부모가 소비한 레이어만 반환 (레이어 ID 낭비 방지)
		return MaxLayer;
	}

	// 시계방향 상 → 우 → 하 → 좌 (Slate 로컬 Y 는 아래로 증가)
	const FVector2D V[4] = {
		Center + FVector2D(0.f, -HalfY),
		Center + FVector2D(HalfX, 0.f),
		Center + FVector2D(0.f, HalfY),
		Center + FVector2D(-HalfX, 0.f)
	};

	// 트랙: 둘레 전체. 닫힘점은 지역 배열 복사 — TrackPts.Add(TrackPts[0]) 은 재할당 시 dangling ref
	{
		TArray<FVector2D> TrackPts = { V[0], V[1], V[2], V[3], V[0] };
		FSlateDrawElement::MakeLines(OutDrawElements, TrackLayer, AllottedGeometry.ToPaintGeometry(),
			TrackPts, ESlateDrawEffect::None, TrackColor, true, Thickness);
	}

	// 필: 네 변의 길이가 모두 sqrt(HalfX²+HalfY²) 로 같아 Percent*4 의 정수부 = 완주한 변 수
	const float T = FMath::Clamp(Percent, 0.f, 1.f) * 4.f;
	const int32 FullEdges = FMath::Min(3, FMath::FloorToInt(T));
	const float Frac = T - static_cast<float>(FullEdges);

	TArray<FVector2D> FillPts;
	FillPts.Reserve(6);
	FillPts.Add(V[0]);
	for (int32 i = 0; i < FullEdges; ++i)
	{
		FillPts.Add(V[(i + 1) % 4]);
	}
	if (Frac > KINDA_SMALL_NUMBER)
	{
		// 마지막 변은 선형 보간으로 끊는다 — 변 단위로만 끊으면 25% 계단으로 튄다
		FillPts.Add(FMath::Lerp(V[FullEdges % 4], V[(FullEdges + 1) % 4], Frac));
	}

	if (FillPts.Num() >= 2)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, FillLayer, AllottedGeometry.ToPaintGeometry(),
			FillPts, ESlateDrawEffect::None, FillColor, true, Thickness);
	}

	return FillLayer;
}
