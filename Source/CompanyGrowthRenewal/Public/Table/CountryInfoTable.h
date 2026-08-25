// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "Enum/ResourceType.h"
#include "CountryInfoTable.generated.h"

/**
 * 나라별 기본 정보 데이터 테이블
 *
 * Row Name = "Korea", "China", "Japan", "Germany", "USA", "SouthAfrica"
 */
USTRUCT(BlueprintType)
struct FCountryInfoTable : public FTableRowBase
{
	GENERATED_BODY()

	// 나라 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country")
	ECountryType CountryType = ECountryType::None;

	// 표시 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country")
	FText DisplayName;

	// 국기 텍스처
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country")
	TSoftObjectPtr<UTexture2D> FlagIcon;

	// 해금에 필요한 시가총액 (0이면 시작부터 해금)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country")
	int64 RequiredMarketCap = 0;

	// 특산 산업 이름 (예: "전자", "자동차")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Specialty")
	FText SpecialtyName;

	// 공장 특화 설명 (특산 버프 문구로 재활용: "전자 제품 생산 속도 +20%")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Factory")
	FText FactoryDescription;

	// Port 보너스 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Port")
	FText PortDescription;

	// ── 탭 지원 여부 (CountryDetail 4탭 가시성 제어) ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Tabs")
	bool bSupportsFactory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Tabs")
	bool bSupportsMine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Tabs")
	bool bSupportsTrade = true;

	// 무역 허브 여부 — true면 벌크 보너스/주문서 매칭/Diamond 보상 대상 (USA/Singapore)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Trade")
	bool bIsTradeHub = false;

	// Money 획득 바이어스 — 나라별 성향 (1.0=중립, 1.3=양산시장, 0.9=소규모). 허브는 보통 1.0 중립.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Trade")
	float MoneyBias = 1.0f;

	// MarketCap 획득 바이어스 — 나라별 명성/브랜드 성향 (1.3=프리미엄 시장, 0.8=저가 양산 시장).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Trade")
	float MarketCapBias = 1.0f;

	// 이 국가 공장이 지원하는 산업군 (공장 탭에서 표시할 제품 필터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Factory")
	TArray<ECompanyType> SupportedIndustries;

	// 이 국가 채광장이 뽑을 수 있는 자원 (채광 탭용, 추후)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Mine")
	TArray<EResourceType> MinableResources;
};
