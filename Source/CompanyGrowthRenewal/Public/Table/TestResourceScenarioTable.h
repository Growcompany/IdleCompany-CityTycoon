// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TestResourceScenarioTable.generated.h"

/**
 * DT_TestResourceScenarios 의 Row.
 * 한 행 = 한 시나리오 (Default/Rich/Poor/Empty 등).
 * RowName = 시나리오 키 (콘솔 명령어 인자).
 *
 * 일괄 set 방식 — GrantTestResources 호출 시 ResourceItemManager.SetAllResources 로 행 값 그대로 적용.
 */
USTRUCT(BlueprintType)
struct FTestResourceScenarioRow : public FTableRowBase
{
	GENERATED_BODY()

	// ── 통화 ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency")
	int64 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency")
	int64 Brick = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency")
	int64 Diamond = 0;

	// 시가총액 — 국가/콘텐츠 해금 게이트, 회사 등급 평가 기준
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency")
	int64 MarketCap = 0;

	// ── 채광 원자재 ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 IronOre = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Aluminum = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Copper = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Oil = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 RareEarth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Silicon = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Wood = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 Lithium = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	int64 DiamondOre = 0;

	// ── 뽑기권 (EItemType — ItemInventoryManager 로 일괄 set) ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 RecruitTicketNormal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 RecruitTicketAdvanced = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 RecruitTicketPremium = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 DepartmentTicket = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 BuildingTraitTicketNormal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 BuildingTraitTicketAdvanced = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 SkinTicketNormal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GachaTicket")
	int32 SkinTicketAdvanced = 0;

	FTestResourceScenarioRow() = default;
};
