#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/Element/Common/TipBandWidget.h"
#include "Widgets/SLeafWidget.h"

class UFont;

/**
 * 팁 밴드를 그리는 **단일 구현**. 두 소비처가 이 함수 하나를 부른다:
 *   - `SLevelTransitionOverlay` (전환/로딩 ― 로딩 스레드라 UMG 불가)
 *   - `UTipBandWidget` (에디터에서 보고 편집하는 UMG 래퍼)
 * 그려지는 그림이 갈라질 수 없게 하려고 뗐다.
 *
 * @param Alpha  0~1 페이드. 0 이면 아무것도 안 그린다.
 * @return 사용한 마지막 레이어.
 */
int32 PaintLoadingTipBand(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FLoadingTipBandStyle& Style, const FString& Body,
	const UFont* BodyFont, const UFont* LabelFont, const FSlateBrush* IconBrush, float Alpha);

/** 위 함수를 그대로 그리는 리프 위젯. UMG 래퍼가 이걸 품는다 */
class SLoadingTipBand final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLoadingTipBand)
		: _BodyFont(nullptr), _LabelFont(nullptr), _IconTexture(nullptr) {}
		SLATE_ARGUMENT(FLoadingTipBandStyle, Style)
		SLATE_ARGUMENT(FString, Body)
		SLATE_ARGUMENT(const UFont*, BodyFont)
		SLATE_ARGUMENT(const UFont*, LabelFont)
		SLATE_ARGUMENT(UTexture2D*, IconTexture)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetBody(const FString& InBody) { Body = InBody; }
	void SetStyle(const FLoadingTipBandStyle& InStyle) { Style = InStyle; }
	void SetFonts(const UFont* InBody, const UFont* InLabel) { BodyFont = InBody; LabelFont = InLabel; }
	void SetIcon(UTexture2D* InIcon);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FLoadingTipBandStyle Style;
	FString Body;
	const UFont* BodyFont = nullptr;
	const UFont* LabelFont = nullptr;
	FSlateBrush IconBrush;
	bool bHasIcon = false;
};
