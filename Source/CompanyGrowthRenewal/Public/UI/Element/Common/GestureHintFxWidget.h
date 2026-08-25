#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Element/Common/GestureHintTypes.h"
#include "GestureHintFxWidget.generated.h"

/**
 * 제스처 힌트 절차식 이펙트 — 탭 점 펄스 2겹 + 드래그 점선/화살촉.
 * 별도 위젯인 이유: SObjectWidget::OnPaint 는 자식 트리를 먼저 그린 뒤 NativePaint 를 호출하므로,
 * 부모가 직접 그리면 무조건 손 글리프 "위"에 얹힌다. 손 아래에 깔려면 그리기를 자식으로 분리해
 * WBP z-order 에서 HandImage 보다 먼저(아래) 배치해야 한다.
 * 상태는 소유하지 않는다 — 부모(UGestureHintWidget)가 매 틱 SetMotion 으로 주입.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGestureHintFxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMotion(EGestureHintKind InKind, float InElapsed);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	EGestureHintKind Kind = EGestureHintKind::None;
	float Elapsed = 0.f;
};
