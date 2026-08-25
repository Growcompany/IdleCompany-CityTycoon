// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelectionChevronWidget.generated.h"

// 선택 건물 위 이중 V 셰브론 — 자기 로컬 공간에 선 드로잉 (절대좌표 왕복 없음 = 페인트 공간 버그 무관).
// 배치는 InGameLayer 가 캔버스 슬롯(하단중앙 정렬 = V 꼭짓점이 앵커)으로 수행.
// 무인 런타임 위젯이라 WBP/DT 등록 없이 StaticClass 직접 생성 (플레이북 '디자이너 비관여 = C++ 자가 트리' 분기)
UCLASS()
class COMPANYGROWTHRENEWAL_API USelectionChevronWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 선택/배치 상태는 호스트가 판정하고, 이 위젯은 같은 이중 V 실루엣의 색만 바꾼다.
	void SetChevronColors(const FLinearColor& InMainColor, const FLinearColor& InEchoColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FLinearColor MainColor = FLinearColor(1.f, 0.92f, 0.6f, 1.f);
	FLinearColor EchoColor = FLinearColor(1.f, 0.92f, 0.6f, 0.45f);
};
