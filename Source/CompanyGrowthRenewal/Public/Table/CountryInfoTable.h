// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CountryType.h"
#include "Enum/CompanyType.h"
#include "Enum/ResourceType.h"
#include "Enum/MedallionAnchor.h"
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

	// ── 전략 지도 테이블 배치 (2026-09-24) ──

	// 판 기준 정규화 좌표 0~1. 실제 위경도가 아니라 메달리온 겹침을 피해 민 값이다.
	// (0,0) = 미기입 — 배치에서 제외되고 경고가 난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Map")
	FVector2D MapAnchor = FVector2D::ZeroVector;

	// 라벨을 메달리온 어느 쪽에 걸지. 이웃과 반대 방향으로 두어 겹침을 피한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Map")
	EMedallionAnchor LabelSide = EMedallionAnchor::Bottom;

	// 이 국가 영역 실루엣 마스크. 미진출·잠김일 때 판 위에 어둡게 덮는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Map")
	TSoftObjectPtr<UTexture2D> RegionMask;

	// 메달리온 링에 쓸 대표 산업. SupportedIndustries 의 순서는 대표를 뜻하지 않아
	// (중국·미국·일본이 모두 Electronics 로 시작) 그대로 쓰면 링이 한 색으로 수렴한다.
	// None 이면 SupportedIndustries 의 첫 유효 원소로 폴백하고 경고가 난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Map")
	ECompanyType RepresentativeIndustry = ECompanyType::None;
};
