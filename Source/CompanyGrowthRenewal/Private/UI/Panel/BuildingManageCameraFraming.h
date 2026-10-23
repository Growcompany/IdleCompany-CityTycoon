#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

namespace BuildingManageCameraFraming
{
	// 같은 절대 Slate 좌표계에서 오른쪽 UI가 시작하기 전 가시 영역의 중앙을 구한다.
	inline bool TryGetScreenCenterRatioX(const FSlateRect& ViewportRect, const FSlateRect& RightUIRect, float& OutRatio)
	{
		const auto IsFiniteRect = [](const FSlateRect& Rect)
		{
			return FMath::IsFinite(Rect.Left) && FMath::IsFinite(Rect.Top)
				&& FMath::IsFinite(Rect.Right) && FMath::IsFinite(Rect.Bottom);
		};

		if (!IsFiniteRect(ViewportRect) || !IsFiniteRect(RightUIRect)
			|| ViewportRect.Right <= ViewportRect.Left || ViewportRect.Bottom <= ViewportRect.Top
			|| RightUIRect.Right <= RightUIRect.Left || RightUIRect.Bottom <= RightUIRect.Top
			|| RightUIRect.Right <= ViewportRect.Left || RightUIRect.Left >= ViewportRect.Right
			|| RightUIRect.Bottom <= ViewportRect.Top || RightUIRect.Top >= ViewportRect.Bottom)
		{
			return false;
		}

		const float VisibleRight = FMath::Clamp(RightUIRect.Left, ViewportRect.Left, ViewportRect.Right);
		if (VisibleRight <= ViewportRect.Left)
		{
			return false;
		}

		const float Ratio = (VisibleRight - ViewportRect.Left) / (2.f * (ViewportRect.Right - ViewportRect.Left));
		if (!FMath::IsFinite(Ratio))
		{
			return false;
		}
		OutRatio = Ratio;
		return true;
	}
}
