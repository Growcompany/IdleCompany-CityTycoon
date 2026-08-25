#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RadialProgressWidget.generated.h"

// 자가 트리 원형 게이지 위젯 (SelectionChevronWidget 패턴 미러).
// NativePaint 로 트랙(전체 원) + 필 아크(Percent * 360)를 MakeLines 로 드로잉.
// WBP 트리에서 SizeBox 안에 배치하면 할당 크기 기준으로 반지름을 계산.
// RebuildWidget: 디자이너가 루트를 안 넣으면 SizeBox(RingSize) 자동 생성 (프리뷰 지원).
UCLASS()
class COMPANYGROWTHRENEWAL_API URadialProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetPercent(float In01);

	UFUNCTION(BlueprintCallable)
	void SetFillColor(const FLinearColor& InColor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Percent = 0.6f;

	// 트랙(배경 원) 색 — linear. 기본 흰 7%
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TrackColor = FLinearColor(1.f, 1.f, 1.f, 0.07f);

	// 필 아크 색 — linear. 기본 액센트 블루 #3D9BE0
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FillColor = FLinearColor(0.047f, 0.328f, 0.745f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Thickness = 16.f;

	// 자가 트리 루트 SizeBox 크기 (디자이너 미배치 시 프리뷰용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RingSize = 248.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
