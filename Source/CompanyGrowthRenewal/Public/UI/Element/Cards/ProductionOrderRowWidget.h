// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "Data/ProductionOrderData.h"
#include "Enum/ResourceType.h"
#include "ProductionOrderRowWidget.generated.h"

class UImage;
class UBorder;
class UCommonTextBlock;

DECLARE_DELEGATE_OneParam(FOnOrderRowClicked, int32 /*OrderID*/);

/**
 * UIE_ProductionOrderRow
 * 제작 시작 팝업 좌측 레일의 주문서 한 줄.
 * 커버 · 이름 · 등급 · 남은수량/개당시간 + "재료로 N개까지" 류 상한 힌트.
 *
 * 열어보기 전에 만들 수 있는 것이 골라지도록 상한을 행에서 미리 알려준다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UProductionOrderRowWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UProductionOrderRowWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * @param InMaxProducible 재료·제품 입력·주문서 종합 제작 가능 최대 (0 = 1개도 불가)
	 * @param InBottleneck    상한을 정한 재료 (None = 재료가 상한을 정하지 않음)
	 * @param bInSupplyBound  재료 또는 제품 입력이 주문서보다 먼저 막는가
	 * @param InProductInputName 제품 입력(칩)이 상한을 정했을 때 그 제품 이름, 그 밖에는 빈 FText.
	 *                           이 자리에서 "재료"라고 쓰면 칩이 모자랄 때 광석을 캐러 간다.
	 * @param InEffectiveTimePerUnitSec 현재 국가의 자동화 배율을 반영한 1개 생산 시간.
	 */
	void SetOrderData(const FProductionOrder& InOrder, int32 InMaxProducible,
		EResourceType InBottleneck, bool bInSupplyBound, const FText& InProductInputName,
		float InEffectiveTimePerUnitSec);

	// 상한/병목만 다시 칠한다 — 자원이 바뀔 때마다 이름·커버·레시피를 다시 조회하지 않기 위해
	// SetOrderData(정적 1회) 와 분리했다 (플레이북 "부분 갱신, 전체 rebuild 금지").
	void UpdateCapacityHint(int32 InMaxProducible, EResourceType InBottleneck, bool bInSupplyBound,
		const FText& InProductInputName);

	void SetRowSelected(bool bInSelected);

	int32 GetOrderID() const { return CachedOrderID; }

	// 등급색 단일 출처 — 팝업 상세 배지도 이걸 경유해 두 곳이 갈리지 않게 한다
	FLinearColor GetGradeColor(EQualityGrade Grade) const;

	FOnOrderRowClicked OnOrderRowClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CoverImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GradeBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> GradeText;

	// 「C 등급 · 남은 8개」, 1개도 못 만들면 주황 「재료 부족」(spec 2026-10-21 §2.10)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MetaText;

	// 상한/불가 안내. 여유가 있으면 Collapsed (경고가 상시면 경고가 아니다)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> HintText;

	// 선택 표시 = 링 하나. 블루는 기능색이라 선택 상태에만 쓴다.
	// ⚠ 좌측 액센트 바 재도입 금지 (2026-07-29 사용자 지시) — 링과 중복이고 라운드 실루엣과 충돌한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectRing;

	// ===== 스타일 노브 (WBP Class Defaults) =====
	// 등급색 전용 DT 가 없어 여기가 SOT. 블루 회피 — 블루는 선택 상태 전용.
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	TMap<EQualityGrade, FLinearColor> GradeColors;

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor HintInkTight = FLinearColor(0.8632f, 0.5647f, 0.1518f, 1.0f);   // #F0C46B 상한

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor HintInkBlocked = FLinearColor(0.8632f, 0.2232f, 0.1946f, 1.0f); // #F0857F 불가

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor MetaInk = FLinearColor(0.4233f, 0.4793f, 0.5647f, 1.0f);        // #AEB8C6

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor MetaInkBlocked = FLinearColor(0.8879f, 0.3813f, 0.1022f, 1.0f); // #F2A65A

private:
	void HandleClicked();
	void LoadCoverAsync(ECompanyType CompanyType, int32 ProjectIndex);

	int32 CachedOrderID = 0;
	EQualityGrade CachedGrade = EQualityGrade::C;
	int32 CachedRemaining = 0;

	void ApplyMetaLine(int32 InMaxProducible, EResourceType InBottleneck, bool bInSupplyBound, const FText& InProductInputName);
};
