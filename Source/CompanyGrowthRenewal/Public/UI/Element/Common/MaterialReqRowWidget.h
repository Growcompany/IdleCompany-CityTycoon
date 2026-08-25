// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/ResourceType.h"
#include "MaterialReqRowWidget.generated.h"

class UImage;
class UBorder;
class UProgressBar;
class UCommonTextBlock;

/**
 * UIE_MaterialReqRow
 * 재료 한 종의 요구 상태를 한 줄로: 아이콘 · 이름 · 보유/필요 · 부족분.
 * 행 배경 자체가 게이지(보유÷필요) — 폭을 안 먹으면서 길이와 숫자 두 채널로 부족을 전달한다.
 *
 * 구 UIE_ItemCard_QtyInside 방식(필요량만 표시 + 부족 시 틴트)의 대체.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMaterialReqRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @param InCapMax 이 재료가 제작 가능 개수를 정하는 병목이면 그 개수, 아니면 INDEX_NONE.
	 *                 병목일 때만 "최대 N" 표식이 붙는다.
	 */
	void SetRequirement(EResourceType InType, int64 InHave, int64 InNeed, int32 InCapMax = INDEX_NONE);

protected:
	virtual void NativePreConstruct() override;

	// 배경 게이지. 트랙은 투명(WBP), 채움만 코드가 비율·색을 주입.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> FillBar;

	// 부족 상태 전용 레이어 (평시 Collapsed) — 밑판 붉은 틴트 + 좌측 엣지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LackTint;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LackEdge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText;

	// "1,240 / 200" — 보유는 밝게, "/ 필요"는 뮤트. RichText 대신 두 블록으로 분리.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HaveText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> NeedText;

	// 부족분 "−1,050". 충분하면 빈 문자열 — N행에 "충분"을 반복하면 부족분이 묻힌다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LackText;

	// "최대 20" 병목 표식 (Border + Text 쌍, 평시 Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CapMarkBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CapMarkText;

	// ===== 스타일 노브 (UIE_MaterialReqRow WBP Class Defaults 에서 튜닝) =====
	// 채움 색만 코드가 바꾼다 — 값이 상태(충분/부족)에 따라 달라지는 런타임 데이터 주입.

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor FillColorOk = FLinearColor(1.0f, 1.0f, 1.0f, 0.085f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor FillColorLack = FLinearColor(0.7295f, 0.0884f, 0.0703f, 0.30f);   // #E0504A

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor InkOk = FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.0f);            // #ECEEF0

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor InkLack = FLinearColor(0.8632f, 0.2232f, 0.1946f, 1.0f);          // #F0857F

private:
	void ApplyLackVisual(bool bLack);
};
