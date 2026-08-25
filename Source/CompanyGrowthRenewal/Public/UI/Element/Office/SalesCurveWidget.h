#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SalesCurveWidget.generated.h"

struct FOperationData;

/**
 * 판매 감쇠 곡선 라이브 그래프 (UIE_SalesCurve) — 순수 Slate MakeLines, 텍스처/트리 0.
 * 실현선(지금까지 rps 샘플) + 투영 점선(감쇠 외삽) + 현재점 + 그리드.
 * 자가 트리(RebuildWidget) — WBP는 빈 껍데기(reparent만), 배치는 호스트 슬롯이 담당.
 * 피드: OfficeMainWidget이 OnOperationUpdated 틱마다 PushSample. 새 운영 감지 시 자동 리셋.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USalesCurveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void PushSample(const FOperationData& Op);
	void ResetCurve();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// 디자이너 크롬의 피크값 라벨 (있으면 PushSample 이 갱신). 위치/스타일=디자이너, 값만 코드.
	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* PeakLabel;

	// 재진입 등으로 히스토리가 빈 채 운영 도중부터 관측 시 — 매니저와 동일한 감쇠 모델로 0~현재 곡선 재구성.
	// (뷰가 히스토리를 소유해 재생성 시 소실 → 곡선이 중간부터 시작하던 것 방지)
	void BackfillHistory(const struct FOperationData& Op);

	// x=경과초, y=초당수익
	TArray<FVector2D> Samples;
	float TotalTime = 0.0f;
	float HalfLifeFrac = 0.5f;
	float PeakSeen = 1.0f;
	float LastSampleTime = -1.0f;

	static constexpr float MinSampleGapSec = 0.35f;
	static constexpr int32 MaxSamples = 240;
};
