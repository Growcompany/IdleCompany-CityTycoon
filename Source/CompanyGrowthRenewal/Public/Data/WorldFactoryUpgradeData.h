// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Manager/WorldFactoryManager.h"
#include "WorldFactoryUpgradeData.generated.h"

class UTexture2D;

/**
 * 세계지도 국가별 공장 강화 슬롯 정의 (DataTable Row)
 * 한 행 = 하나의 EWorldFactoryUpgradeType = 하나의 슬롯.
 * RowName 은 enum 식별자와 일치 권장 (예: "LineExpansion").
 *
 * 매니저(UWorldFactoryManager)는 이 DT 만 조회해 효과/비용/상한을 산출.
 * 밸런스/표시명/아이콘 변경은 CSV reimport 만으로 완결되어야 함 (DT 단일 진실 원천).
 */
USTRUCT(BlueprintType)
struct FWorldFactoryUpgradeDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	EWorldFactoryUpgradeType UpgradeType = EWorldFactoryUpgradeType::LineExpansion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	FText SubDescription;

	// 값 표시 단위 (예: "라인", "%", "개"). 빈 문자열이면 숫자만 표시.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	FString ValueUnit;

	// 정수 표시 여부 (true 면 소수점 생략).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	bool bIsInteger = false;

	// 패널 내 슬롯 정렬 순서 (오름차순).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Display")
	int32 SortOrder = 0;

	// 강화 비용 자원 종류 (Money / Diamond 등).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	EResourceType CostResourceType = EResourceType::Money;

	// 최대 레벨. 0 이면 무제한, 양수면 상한 (예: LineExpansion = 3).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Balance")
	int32 MaxLevel = 0;

	// 레벨 0 기준값. (LineExpansion: 기본 동시 가동 라인 수, StorageExpansion: 기본 저장 용량 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Balance")
	float BaseValue = 0.0f;

	// 레벨 1 마다 추가되는 양. (Automation: 0.10 = 속도 +10%/lvl, StorageExpansion: 500 = 용량 +500/lvl)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Balance")
	float PerLevel = 0.0f;

	// 1 → 2 레벨 업그레이드 비용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	int64 BaseCost = 100;

	// 비용 증가율. NextCost = BaseCost * pow(CostGrowthRate, CurrentLevel).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade|Cost")
	float CostGrowthRate = 1.15f;
};
