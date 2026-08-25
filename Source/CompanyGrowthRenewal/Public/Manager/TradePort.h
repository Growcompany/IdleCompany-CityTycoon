// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/WorldMapTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "Entity/Country/CountryActor.h"
#include "TradePort.generated.h"

/**
 * UTradePort
 * 완성품 판매 엔진. 모든 나라에 판매 가능하며, 무역 허브(DT_CountryInfo.bIsTradeHub=true)에만 추가 혜택.
 *
 * 설계 메모:
 *  - 판매 관문 모델: 모든 나라 허용(allow-by-default), 허브 여부는 배율/주문서/Diamond 분기에만 반영
 *  - Money 공식: BasePrice * Qty * PriceMul * BulkMul * MatchMul * MoneyBias * DemandMul
 *    - 비허브: PriceMul=1.0, BulkMul=1.0, MatchMul=1.0 (기본 단가), MoneyBias=나라별 성향
 *    - 허브: USA=+35% 단가, Singapore=중립(속도/연출), 벌크/주문서/Diamond 혜택, 바이어스는 보통 1.0 중립
 *  - MarketCap 공식: BasePerUnit * Qty * GradeMul * MatchMul * MarketCapBias * DemandMul
 *  - DemandMul: 0.4× ~ 1.2× (UCountryMarketManager 시장 수요 게이지). 판매 시 ConsumeDemand 호출.
 *  - 나라별 바이어스는 DT_CountryInfo.MoneyBias / MarketCapBias 에서 조회 (단일 진실 소스).
 *  - Diamond 보너스는 VIP 주문서 완전 충족 시에만 지급(허브 한정).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTradePort : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ────────────────────────────────────────────
	// Blueprint 바인딩용 델리게이트
	// ────────────────────────────────────────────
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProductSold, ECountryType, Port, FIntPoint, ProductKey, FSellResult, Result);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaleFailed, FIntPoint, ProductKey, FString, Reason);

	UPROPERTY(BlueprintAssignable, Category = "TradePort|Events")
	FOnProductSold OnProductSold;

	UPROPERTY(BlueprintAssignable, Category = "TradePort|Events")
	FOnSaleFailed OnSaleFailed;

	// ────────────────────────────────────────────
	// Public API
	// ────────────────────────────────────────────

	/** 실제 판매 실행. 재고 없으면 OnSaleFailed 브로드캐스트 후 빈 FSellResult 반환. */
	UFUNCTION(BlueprintCallable, Category = "TradePort")
	FSellResult SellProduct(ECountryType Port, ECompanyType Company, int32 ProjectIndex, int64 RequestedQty);

	/** 판매 미리보기 — 자원 변동 없이 예상 보상만 계산. */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	FSellResult PreviewSell(ECountryType Port, ECompanyType Company, int32 ProjectIndex, int64 RequestedQty) const;

	/** 제품별 기본 단가 (MVP: ProjectIndex 기반 간단한 곡선) */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	int64 GetBasePrice(ECompanyType Company, int32 ProjectIndex) const;

	/** 무역항별 가격 배율 (USA 1.35, Singapore 1.0, 기타 0.9). */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	float GetPortPriceMultiplier(ECountryType Port) const;

	/** 무역항별 수수료 배율 (Singapore 0.5, 기타 1.0). MVP에서는 연출 속도에만 사용. */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	float GetPortFeeMultiplier(ECountryType Port) const;

	/** 일괄 판매 수량 보너스 배율 (100+=1.5, 50+=1.25, 10+=1.1). */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	float GetBulkBonus(int64 Qty) const;

	/** 무역 허브 여부. DT_CountryInfo의 bIsTradeHub 조회. 허브면 벌크/주문서/Diamond 혜택 대상. */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	bool IsTradeHub(ECountryType Port) const;

	/** 나라별 Money 획득 바이어스 (DT_CountryInfo.MoneyBias). 조회 실패 시 1.0. */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	float GetMoneyBias(ECountryType Port) const;

	/** 나라별 MarketCap 획득 바이어스 (DT_CountryInfo.MarketCapBias). 조회 실패 시 1.0. */
	UFUNCTION(BlueprintPure, Category = "TradePort")
	float GetMarketCapBias(ECountryType Port) const;

private:
	/** 등급별 보상 배율 wrapper (QualityGrade.h GetQualityGradeRevenueMultiplier 래핑) */
	float GetGradeMultiplier(EQualityGrade Grade) const;

	/** 내부 계산 공통 헬퍼. bApply=true면 실제 자원/재고 변동, false면 미리보기만. */
	FSellResult ComputeSell(ECountryType Port, ECompanyType Company, int32 ProjectIndex, int64 RequestedQty, bool bApply);
};
