#include "UI/Element/Common/GestureHintWidget.h"
#include "UI/Element/Common/GestureHintFxWidget.h"
#include "UI/Element/Common/RadialProgressWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UGestureHintWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 링 색/굵기는 WBP Class Defaults 소관 — 여기서 덮으면 정적 스타일이 코드에 갇힌다
	if (HoldRing)
	{
		HoldRing->SetPercent(0.f);
	}
	ApplyGestureVisibility();
}

void UGestureHintWidget::SetGesture(EGestureHintKind Kind)
{
	if (Gesture == Kind) return;
	Gesture = Kind;
	Elapsed = 0.f;
	// 숨김→표시 전환에서 직전 위상이 한 프레임 비치지 않도록 즉시 리셋 주입
	if (MotionFx) MotionFx->SetMotion(Gesture, 0.f);
	ApplyGestureVisibility();
}

void UGestureHintWidget::SetLabel(const FText& Label)
{
	if (!LabelText) return;
	LabelText->SetText(Label);
	LabelText->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

bool UGestureHintWidget::GetHandRectLocal(const FGeometry& OverlayGeometry, FSlateRect& OutRect) const
{
	if (!HandImage || Gesture == EGestureHintKind::None || !IsVisible())
	{
		return false;
	}
	const FGeometry& HandGeometry = HandImage->GetCachedGeometry();
	const FVector2D Size = HandGeometry.GetLocalSize();
	if (Size.X <= 1.f || Size.Y <= 1.f)
	{
		return false;
	}
	// 지금 위치는 왕복 중 한 점이다 ― 움직이는 폭 전체를 덮어야 반대쪽 끝에서 다시 겹치지 않는다
	const float SweepX = Gesture == EGestureHintKind::Drag ? GestureHintMotion::DragAmpPx * 2.f : 0.f;
	const float SweepY = Gesture == EGestureHintKind::Tap ? GestureHintMotion::TapDipPx : 0.f;
	const FVector2D Corners[4] = {
		FVector2D(-SweepX, -SweepY), FVector2D(Size.X + SweepX, -SweepY),
		FVector2D(-SweepX, Size.Y + SweepY), FVector2D(Size.X + SweepX, Size.Y + SweepY)};
	FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Max(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	for (const FVector2D& Corner : Corners)
	{
		const FVector2D Local = OverlayGeometry.AbsoluteToLocal(HandGeometry.LocalToAbsolute(Corner));
		Min = FVector2D::Min(Min, Local);
		Max = FVector2D::Max(Max, Local);
	}
	OutRect = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	return true;
}

void UGestureHintWidget::ApplyGestureVisibility()
{
	const bool bShow = Gesture != EGestureHintKind::None;
	const bool bHasFx = Gesture == EGestureHintKind::Tap || Gesture == EGestureHintKind::Drag;
	if (HandImage) HandImage->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (HoldRing) HoldRing->SetVisibility(Gesture == EGestureHintKind::Hold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (MotionFx) MotionFx->SetVisibility(bHasFx ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UGestureHintWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Gesture == EGestureHintKind::None || !HandImage) return;
	Elapsed += InDeltaTime;
	if (MotionFx) MotionFx->SetMotion(Gesture, Elapsed);
	using namespace GestureHintMotion;
	switch (Gesture)
	{
	case EGestureHintKind::Tap:
		HandImage->SetRenderTranslation(FVector2D(0.f, TapOffsetY(Elapsed)));
		HandImage->SetRenderScale(FVector2D(1.f, 1.f));
		break;
	case EGestureHintKind::Hold:
		HandImage->SetRenderTranslation(FVector2D(0.f, HoldScale(Elapsed) < 1.f ? 5.f : 0.f));
		HandImage->SetRenderScale(FVector2D(HoldScale(Elapsed), HoldScale(Elapsed)));
		if (HoldRing) HoldRing->SetPercent(HoldPercent(Elapsed));
		break;
	case EGestureHintKind::Drag:
		HandImage->SetRenderTranslation(FVector2D(DragOffsetX(Elapsed), 4.f));
		HandImage->SetRenderScale(FVector2D(0.96f, 0.96f));
		break;
	default: break;
	}
}
