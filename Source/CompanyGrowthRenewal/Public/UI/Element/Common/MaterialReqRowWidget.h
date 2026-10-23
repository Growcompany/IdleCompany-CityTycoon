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
class UTexture2D;

/**
 * UIE_MaterialReqRow
 * 재료 한 종의 카드: 아이콘 · 이름 / 1개당 필요(크게) · 보유(작게) / 사용 비율 막대(필요×수량÷보유).
 * 필요량을 앞에 크게 둔다 ― 보유가 큰 숫자라 앞에 오면 필요량이 안 읽힌다(2026-10-21 시안).
 * 최대 수량을 정하는 재료는 주황(막대·보유), 수량을 최대보다 올려 모자라면 빨강 + 「N개 부족」 글.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UMaterialReqRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @param InNeedPerUnit 1개 만들 때 필요량(카드에 크게 표시)
	 * @param InQuantity    고른 생산 수량 ― 막대 = 필요×수량 ÷ 보유
	 * @param InCapMax      이 재료가 제작 가능 개수를 정하는 병목이면 그 개수, 아니면 INDEX_NONE.
	 */
	void SetRequirement(EResourceType InType, int64 InHave, int64 InNeedPerUnit, int32 InQuantity, int32 InCapMax);

	/**
	 * 제품 입력(선행 제품) 행. 이름·아이콘이 DT_Resource 가 아니라 DT_Project_* 에서 오므로
	 * EResourceType 으로는 지목할 수 없어 진입점을 따로 둔다.
	 * @param InName   DT 행이 없으면 빈 FText 를 넘긴다 — 행이 비는 것이 코드 폴백보다 낫다.
	 * @param InCapMax SetRequirement 와 같다. 병목이면 그 개수, 아니면 INDEX_NONE.
	 */
	void SetProductRequirement(const FText& InName, const TSoftObjectPtr<UTexture2D>& InIcon,
		int64 InHave, int64 InNeedPerUnit, int32 InQuantity, int32 InCapMax);

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

	// 「보유 18」 ― 작게, 필요량 뒤
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HaveText;

	// 1개당 필요량 「5」 ― 크게, 앞
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NeedText;

	// 「2개 부족」. 충분하면 접는다 — N칸에 "충분"을 반복하면 부족분이 묻힌다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LackText;

	// "최대 20" 병목 표식 (Border + Text 쌍, 평시 Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CapMarkBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CapMarkText;

	// ===== 스타일 노브 (UIE_MaterialReqRow WBP Class Defaults 에서 튜닝) =====
	// 막대 채움·보유 글자 색만 코드가 바꾼다 — 값이 상태(여유/최대를 정함/부족)에 따라 달라지는 런타임 데이터 주입.

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor FillColorOk = FLinearColor(0.1022f, 0.3467f, 0.8714f, 1.0f);     // #5A9FF0

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor FillColorCap = FLinearColor(0.9301f, 0.3231f, 0.0423f, 1.0f);    // #F79A3A

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor FillColorLack = FLinearColor(0.6038f, 0.0423f, 0.0423f, 1.0f);   // #CC3A3A

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor InkOk = FLinearColor(0.1022f, 0.1470f, 0.2051f, 1.0f);           // #5A6B7D

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor InkCap = FLinearColor(0.5395f, 0.0513f, 0.0037f, 1.0f);          // #C2410C

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor InkLack = FLinearColor(0.5647f, 0.0273f, 0.0273f, 1.0f);         // #C62E2E

private:
	// 이름·아이콘을 어디서 구했는지와 무관한 공통 적용부 (재료 행 / 제품 입력 행 공유)
	void ApplyRequirement(const FText& InName, UTexture2D* InIcon, int64 InHave, int64 InNeedPerUnit, int32 InQuantity,
		int32 InCapMax);

	void ApplyLackVisual(bool bLack);
};
