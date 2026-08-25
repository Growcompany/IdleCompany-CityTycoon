#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DiamondProgressWidget.generated.h"

// 자가 트리 마름모 게이지 위젯 (URadialProgressWidget 패턴 미러).
// NativePaint 로 트랙(마름모 둘레 전체) + 필(상→우→하→좌 시계방향, Percent 비율)을 MakeLines 로 드로잉.
// WBP 트리에서 45° 회전 뱃지 위에 겹쳐 배치하면 뱃지 테두리가 곧 진행 게이지가 된다(크기 변화 0).
// RebuildWidget: 디자이너가 루트를 안 넣으면 SizeBox(DiamondSize) 자동 생성 (프리뷰 지원).
UCLASS()
class COMPANYGROWTHRENEWAL_API UDiamondProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetPercent(float In01);

	UFUNCTION(BlueprintCallable)
	void SetFillColor(const FLinearColor& InColor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Percent = 0.6f;

	// 트랙(배경 마름모) 색 — linear. 기본 흰 7%
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TrackColor = FLinearColor(1.f, 1.f, 1.f, 0.07f);

	// 필 색 — linear. 기본 액센트 블루 #3D9BE0 (소비처가 상태색을 주입한다)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FillColor = FLinearColor(0.046625f, 0.327835f, 0.745404f, 1.f);

	// ⚠ 3~4px 유지 — 마름모 꼭짓점은 90° 꺾임이라 굵으면 Slate FLineBuilder 미터 한계(76.5°)를 넘어
	// 꼭짓점이 미터 대신 캡 2개로 분할돼 모양이 깨진다 (SelectionChevron 2026-06-04 실측 선례)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float Thickness = 3.5f;

	// 자가 트리 루트 SizeBox 크기 (디자이너 미배치 시 프리뷰용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DiamondSize = 64.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
