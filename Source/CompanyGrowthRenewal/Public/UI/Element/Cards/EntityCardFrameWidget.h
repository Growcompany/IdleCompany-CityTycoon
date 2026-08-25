// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EntityCardFrameWidget.generated.h"

class UImage;
class UBorder;
class UCommonTextBlock;
class UNamedSlot;
class UUniformGridPanel;

/**
 * 엔티티 카드 프레임 위젯
 * - 카드 공통 UI (이미지, 잠금 오버레이)
 * - Named Slot으로 콘텐츠 영역 제공
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEntityCardFrameWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 엔티티 이미지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* EntityImage;

	// 잠금 UI
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* LockBorder;

	// 선택 UI (FloorTile 등에서 선택 상태 표시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* SelectionBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LockImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LockText;

	// 콘텐츠 슬롯
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UNamedSlot* ContentSlot;

	// 접지 그림자 — 카드가 호버로 떠오를 때 홀로 제자리에 남아 부양감을 만든다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* DropShadow;

	// 카드 측면(두께). 눌릴 때 홀로 제자리에 남아 얼굴만 가라앉게 만든다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CardEdge;

	// 크기 배지 그리드 — 썸네일 우상단. 셀(W×D)은 SetSizeBadge 에서 런타임 생성.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UUniformGridPanel* SizeBadgeGrid;

	// 배지 캡슐 — 칸수 데이터가 없으면 그리드와 함께 접어야 빈 테두리가 안 남는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* SizeBadgePill;

public:
	// 이미지 설정
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetEntityImage(UTexture2D* Texture);

	// 잠금 UI 설정
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetLocked(bool bIsLocked, const FText& LockReason = FText::GetEmpty());

	// 선택 UI 설정
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetSelected(bool bSelected);

	// 크기 배지 — footprint 칸수를 점 그리드로. 등급색이 게임플레이 위계가 아니므로
	// 실제 위계인 칸수를 색 없이 형태로 보여준다. 칸수 0 이하면 배지 숨김.
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetSizeBadge(int32 WidthCells, int32 DepthCells);

	// 카드가 위로 LiftPixels 만큼 뜰 때, 그림자를 같은 값만큼 아래로 상쇄해 화면상 고정시킨다.
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetShadowLift(float LiftPixels);

	// 카드가 아래로 SinkPixels 만큼 눌릴 때 측면을 반대로 올려 제자리에 두면,
	// 드러난 두께가 그만큼 줄어 실제로 눌려 들어가는 것처럼 보인다.
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetPressSink(float SinkPixels);

	// 이미지 접근자
	UImage* GetEntityImage() const { return EntityImage; }
};
