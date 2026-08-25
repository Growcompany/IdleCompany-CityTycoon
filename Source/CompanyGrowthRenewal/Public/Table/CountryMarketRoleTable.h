// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "CountryMarketRoleTable.generated.h"

/**
 * 나라 × 산업 조합별 판매 시장 라벨 테이블.
 *
 * Row Name = "{Country}_{Industry}" (예: "Korea_Electronics", "USA_Automobile")
 * Country × Industry = 11 × 3 = 33행.
 *
 * 판매 모달에서 "이 나라의 이 산업 시장은 어떤 성격인가" 한 줄 라벨 표시용.
 * 채광/생산 모달은 별도 컬럼(SpecialtyName 등) 사용 — 본 테이블은 판매 컨텍스트 전용.
 */
USTRUCT(BlueprintType)
struct FCountryMarketRoleTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MarketRole")
	ECountryType Country = ECountryType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MarketRole")
	ECompanyType Industry = ECompanyType::None;

	/** 판매 모달에서 국가명 옆에 노출되는 한 줄 라벨 (예: "거대 소비처", "프리미엄 본거지"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MarketRole")
	FText MarketRole;
};
