// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Cards/CardInfoWidget.h"
#include "BuildCardInfoWidget.generated.h"

class UStatRowWidget;
class UResourceWidget;

/**
 * 빌딩 전용 카드 정보 위젯 (정석 3존 카드의 가치/비용존).
 * - 공유 UCardInfoWidget(4종 카드 공유)을 오염시키지 않으려 파생.
 * - 등급 배경 틴트 무력화(D3) + 크기/인원 StatRow + 멀티코스트(최대 2칩).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildCardInfoWidget : public UCardInfoWidget
{
	GENERATED_BODY()

public:
	// 가치존 값 주입(코드는 값만 — 라벨은 WBP DefaultStatName 으로 베이크).
	// NewBuildEmployees = 증축 0층 기준 인원. 상한이 아니라 출발점이다.
	void SetBuildingStats(int32 WidthCells, int32 DepthCells, int32 NewBuildEmployees);

	// 베이스 호출 후 등급 틴트 중립화 + 2차 비용칩 표시 분기.
	virtual void SetBuildableInfo(const FBuildableCardTable& buildableInfo) override;

	// 2차 비용칩까지 자원부족(빨강) 토글.
	virtual void UpdateCostAffordability(EResourceType ResourceType, bool bCanAfford) override;

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* SizeStatRow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* CapacityStatRow;

	// 2차 비용칩(비용이 2종일 때만 표시). 1차는 베이스 UIE_Resource.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* SecondaryCostResource;
};
