// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "CountryDemandTable.generated.h"

/**
 * 나라 × 산업 조합별 수요 게이지 정의 테이블.
 *
 * Row Name = "{Country}_{Industry}" (예: "Korea_Semiconductor", "USA_Automobile")
 * Country × Industry = 11 × 3 = 33행.
 */
USTRUCT(BlueprintType)
struct FCountryDemandTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demand")
	ECountryType Country = ECountryType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demand")
	ECompanyType Industry = ECompanyType::None;

	/** 시장 수요 용량. 큰 값일수록 큰 시장. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demand")
	int32 Capacity = 1000;

	/** 분당 자동 회복량. 큰 시장일수록 빨리 회복. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demand")
	float RecoveryPerMin = 10.0f;

	/** 산업별 가격 가중. 1.0 = 중립, > 1.0 = 해당 산업 프리미엄 지불, < 1.0 = 저가 선호.
	 *  ComputeSell 의 Money 공식에 곱해짐. (Country, Industry) 매트릭스의 핵심 차등 항. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demand")
	float PriceMul = 1.0f;
};
