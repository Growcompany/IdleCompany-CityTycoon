#pragma once

#include "Fonts/SlateFontInfo.h"
#include "UI/Element/Common/TipBandWidget.h"
#include "HAL/CriticalSection.h"
#include "Internationalization/Text.h"
#include "Rendering/SlateResourceHandle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class UFont;
class UTexture2D;

enum class ELevelTransitionPhase : uint8
{
	Closing,
	Holding,
	Opening
};

struct FLevelTransitionSnapshot
{
	ELevelTransitionPhase Phase = ELevelTransitionPhase::Closing;
	float Progress = 0.f;
	bool bExit = false;
	bool bReduceMotion = false;
	double PatternStartTime = 0.0;
	// Holding 진입 시각. 팁 밴드 페이드인의 기준이며 POD 라 로딩 스레드에서 읽어도 안전하다
	double HoldStartTime = 0.0;
};

class FLevelTransitionVisualState
{
public:
	void SetSnapshot(const FLevelTransitionSnapshot& InSnapshot);
	FLevelTransitionSnapshot GetSnapshot() const;

private:
	mutable FCriticalSection SnapshotLock;
	FLevelTransitionSnapshot Snapshot;
};

using FLevelTransitionVisualStatePtr = TSharedPtr<FLevelTransitionVisualState, ESPMode::ThreadSafe>;

class SLevelTransitionOverlay final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLevelTransitionOverlay)
		: _PatternTexture(nullptr), _TipBodyFont(nullptr), _TipLabelFont(nullptr), _TipIcon(nullptr) {}
		SLATE_ARGUMENT(FLevelTransitionVisualStatePtr, VisualState)
		SLATE_ARGUMENT(UTexture2D*, PatternTexture)
		/** 전환 1회당 고정이므로 스냅샷이 아니라 생성 인자다 ― 로딩 스레드에서 FText 참조를 만지지 않는다 */
		SLATE_ARGUMENT(FText, TipText)
		SLATE_ARGUMENT(UFont*, TipBodyFont)
		SLATE_ARGUMENT(UFont*, TipLabelFont)
		SLATE_ARGUMENT(UTexture2D*, TipIcon)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FLevelTransitionVisualStatePtr VisualState;
	FSlateBrush PatternBrush;
	FSlateResourceHandle SolidHandle;
	FSlateResourceHandle PatternHandle;
	// Construct 에서 FString 으로 굳힌다 ― FText 복사는 refcount 를 건드리므로 로딩 스레드에서 피한다
	FString TipBody;
	const UFont* TipBodyFont = nullptr;
	const UFont* TipLabelFont = nullptr;
	FSlateBrush TipIconBrush;
	bool bHasTipIcon = false;
	// 외형 노브는 UMG 래퍼(UTipBandWidget)와 같은 구조체를 쓴다 ― 그림이 갈라질 수 없다
	FLoadingTipBandStyle TipStyle;
};
