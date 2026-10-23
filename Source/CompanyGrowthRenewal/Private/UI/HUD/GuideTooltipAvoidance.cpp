#include "UI/HUD/GuideTooltipAvoidance.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/HelpButtonWidget.h"

#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Components/Button.h"
#include "Components/ListViewBase.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Widget.h"
#include "Layout/Clipping.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace CGRGuideTooltipAvoidance
{
	constexpr int32 MaxVisitedWidgets = 1024;
	constexpr int32 MaxAvoidRects = 256;

	bool HasArea(const FSlateRect& Rect)
	{
		return Rect.Right > Rect.Left + 1.f && Rect.Bottom > Rect.Top + 1.f;
	}

	bool GetOverlayRect(const UWidget* Widget, const FGeometry& OverlayGeometry, FSlateRect& OutRect)
	{
		const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
		const FVector2D Size = WidgetGeometry.GetLocalSize();
		if (Size.X <= 1.f || Size.Y <= 1.f)
		{
			return false;
		}

		const FVector2D Corners[4] = {
			FVector2D::ZeroVector, FVector2D(Size.X, 0.f),
			Size, FVector2D(0.f, Size.Y)
		};
		FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
		FVector2D Max(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
		for (const FVector2D& Corner : Corners)
		{
			const FVector2D Local = OverlayGeometry.AbsoluteToLocal(WidgetGeometry.LocalToAbsolute(Corner));
			Min.X = FMath::Min(Min.X, Local.X);
			Min.Y = FMath::Min(Min.Y, Local.Y);
			Max.X = FMath::Max(Max.X, Local.X);
			Max.Y = FMath::Max(Max.Y, Local.Y);
		}
		OutRect = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
		return HasArea(OutRect);
	}

	bool IsSemanticControl(const UWidget* Widget)
	{
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			return !Text->GetText().IsEmpty();
		}
		if (const URichTextBlock* RichText = Cast<URichTextBlock>(Widget))
		{
			return !RichText->GetText().IsEmpty();
		}
		if (const UButton* Button = Cast<UButton>(Widget))
		{
			// 빈 투명 버튼은 바깥 클릭용 입력 영역이며 읽을 콘텐츠가 없다.
			const FSlateBrush& Face = Button->GetStyle().Normal;
			return Button->GetContent() || (Face.DrawAs != ESlateBrushDrawType::NoDrawType
				&& (!Face.TintColor.IsColorSpecified() || Face.TintColor.GetSpecifiedColor().A > 0.f));
		}
		return Widget->IsA<UCommonButtonBase>()
			|| Widget->IsA<UListViewBase>()
			|| Widget->IsA<UUpgradeSlot>()
			|| Widget->IsA<UProgressBar>() || Widget->IsA<USlider>();
	}

	void CollectVisible(UWidget* Widget, const FGeometry& OverlayGeometry, const FSlateRect& ParentClip,
		const FSlateRect& ParentHardClip,
		TSet<const UWidget*>& Visited, TArray<FSlateRect>& OutRects, TArray<FSlateRect>* OutProtected,
		TArray<FSlateRect>* OutControls)
	{
		if (!Widget || Visited.Num() >= MaxVisitedWidgets || OutRects.Num() >= MaxAvoidRects
			|| Visited.Contains(Widget))
		{
			return;
		}
		Visited.Add(Widget);
		const ESlateVisibility Visibility = Widget->GetVisibility();
		if (Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden)
		{
			return;
		}

		FSlateRect Bounds;
		const bool bHasBounds = GetOverlayRect(Widget, OverlayGeometry, Bounds);
		FSlateRect Clip = ParentClip;
		FSlateRect HardClip = ParentHardClip;
		const EWidgetClipping Clipping = Widget->GetClipping();
		const FVector2D AllocatedSize = Widget->GetCachedGeometry().GetLocalSize();
		const FVector2D DesiredSize = Widget->GetDesiredSize();
		const bool bOnDemandClip = Clipping == EWidgetClipping::OnDemand
			&& (DesiredSize.X > AllocatedSize.X + 1.f || DesiredSize.Y > AllocatedSize.Y + 1.f);
		if (bHasBounds && (Widget->IsA<UScrollBox>() || Clipping == EWidgetClipping::ClipToBounds
			|| Clipping == EWidgetClipping::ClipToBoundsAlways || bOnDemandClip))
		{
			Clip = Clip.IntersectionWith(Bounds);
			if (Clipping == EWidgetClipping::ClipToBoundsAlways)
			{
				HardClip = HardClip.IntersectionWith(Bounds);
			}
		}
		else if (bHasBounds && Clipping == EWidgetClipping::ClipToBoundsWithoutIntersecting)
		{
			Clip = HardClip.IntersectionWith(Bounds);
		}
		if (!HasArea(Clip))
		{
			return;
		}

		// 닫기(X)는 작아서 글자와 같은 무게면 가장 먼저 덮인다 ― 따로 모아 절대 덮지 않게 한다
		if (OutProtected && bHasBounds && Widget->IsA<UCloseButtonWidget>())
		{
			const FSlateRect Visible = Bounds.IntersectionWith(Clip);
			if (HasArea(Visible))
			{
				OutProtected->Add(Visible);
				return;
			}
		}

		// 도움말 ? 는 버튼 면이 투명해 아래 판정에서 빈 입력 영역으로 빠진다 ― 글자와 같은 무게의 내용으로 모은다
		// (버튼 무게로 올리면 코치마크가 ? 를 피하려고 설명하는 조건 카드를 덮는다)
		if (bHasBounds && Widget->IsA<UHelpButtonWidget>())
		{
			const FSlateRect Visible = Bounds.IntersectionWith(Clip);
			if (HasArea(Visible))
			{
				OutRects.Add(Visible);
				return;
			}
		}

		if (bHasBounds && IsSemanticControl(Widget))
		{
			const FSlateRect Visible = Bounds.IntersectionWith(Clip);
			const FVector2D ViewSize = OverlayGeometry.GetLocalSize();
			// Full-screen backdrop buttons carry input but no useful content to protect.
			if (HasArea(Visible) && !(Visible.Right - Visible.Left >= ViewSize.X * .8f
				&& Visible.Bottom - Visible.Top >= ViewSize.Y * .8f))
			{
				const bool bControl = Widget->IsA<UButton>() || Widget->IsA<UCommonButtonBase>();
				(OutControls && bControl ? *OutControls : OutRects).Add(Visible);
				return;
			}
		}

		if (UCommonActivatableWidgetContainerBase* Container = Cast<UCommonActivatableWidgetContainerBase>(Widget))
		{
			CollectVisible(Container->GetActiveWidget(), OverlayGeometry, Clip, HardClip, Visited, OutRects, OutProtected, OutControls);
			return;
		}
		if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget))
		{
			CollectVisible(Switcher->GetActiveWidget(), OverlayGeometry, Clip, HardClip, Visited, OutRects, OutProtected, OutControls);
			return;
		}
		if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			CollectVisible(UserWidget->GetRootWidget(), OverlayGeometry, Clip, HardClip, Visited, OutRects, OutProtected, OutControls);
			return;
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				CollectVisible(Panel->GetChildAt(Index), OverlayGeometry, Clip, HardClip, Visited, OutRects, OutProtected, OutControls);
			}
		}
	}
}

void CollectGuideTooltipAvoidRects(UWidget* Root, const FGeometry& OverlayGeometry, TArray<FSlateRect>& OutRects,
	TArray<FSlateRect>* OutProtected, TArray<FSlateRect>* OutControls)
{
	OutRects.Reset();
	const FVector2D Size = OverlayGeometry.GetLocalSize();
	if (!Root || Size.X <= 1.f || Size.Y <= 1.f)
	{
		return;
	}
	TSet<const UWidget*> Visited;
	const FSlateRect Viewport(0.f, 0.f, Size.X, Size.Y);
	CGRGuideTooltipAvoidance::CollectVisible(Root, OverlayGeometry, Viewport, Viewport, Visited, OutRects, OutProtected, OutControls);
}
