#include "UI/Element/Common/GuideTooltipWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"

void UGuideTooltipWidget::SetContent(const FText& Eyebrow, const FText& Body)
{
	if (EyebrowText)
	{
		EyebrowText->SetText(Eyebrow);
		EyebrowText->SetVisibility(Eyebrow.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (BodyText)
	{
		BodyText->SetText(Body);
	}
}

void UGuideTooltipWidget::SetTail(EGuideTooltipDir Dir, float Offset)
{
	if (!TailImage || !TailBox)
	{
		return;
	}
	if (Dir == LastDir && FMath::IsNearlyEqual(Offset, LastOffset, 0.5f))
	{
		return;   // 같은 값 재대입 방어 ― Slate invalidate 누적 회피
	}

	UCanvasPanelSlot* TailSlot = Cast<UCanvasPanelSlot>(TailBox->Slot);
	if (!TailSlot)
	{
		// 캐시를 커밋하지 않고 나간다 ― 커밋하면 같은 값의 다음 호출이 위 조기 반환에 막혀 재시도조차 못 한다
		UE_LOG(LogTemp, Warning,
			TEXT("UGuideTooltipWidget: TailBox 가 CanvasPanel 직계 자식이 아니라 꼬리를 배치할 수 없습니다."));
		return;
	}

	const FGuideTailPlacement Placement = ResolveGuideTailPlacement(Dir);

	// 꼬리 텍스처 1장을 회전해 4방향을 만든다.
	TailImage->SetRenderTransformAngle(Placement.Angle);

	// 앵커/얼라인먼트까지 함께 옮겨야 꼬리가 해당 모서리에 밀착한다(교차축 0 고정은 반대쪽 모서리로 간다).
	TailSlot->SetAnchors(FAnchors(
		static_cast<float>(Placement.AnchorPoint.X),
		static_cast<float>(Placement.AnchorPoint.Y)));
	TailSlot->SetAlignment(Placement.Alignment);
	TailSlot->SetAutoSize(true);   // 얼라인먼트 보정이 desired 크기를 알아야 성립한다
	TailSlot->SetPosition(Placement.bAlongX
		? FVector2D(Offset, 0.0f) : FVector2D(0.0f, Offset));

	LastDir = Dir;
	LastOffset = Offset;
}
