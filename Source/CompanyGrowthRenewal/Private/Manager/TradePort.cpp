// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/TradePort.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TradeOrderManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/CountryMarketManager.h"
#include "Table/ProductRecipeTable.h"
#include "Table/CountryInfoTable.h"
#include "Table/CountryDemandTable.h"
#include "Enum/ResourceType.h"
#include "Engine/GameInstance.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Enum/BuildingTraitTarget.h"

// ─────────────────────────────────────────────
// 기본 단가 공식
// ─────────────────────────────────────────────
int64 UTradePort::GetBasePrice(ECompanyType /*Company*/, int32 ProjectIndex) const
{
	// MVP: 후반부로 갈수록 단가가 의미있게 상승하도록 2차 곡선 채택
	// Base = 100 + 50 * Idx^2  (Idx=1 → 150, Idx=10 → 5100, Idx=50 → 125100, Idx=100 → 500100)
	const int64 Idx = FMath::Max<int64>(1, static_cast<int64>(ProjectIndex));
	return FMath::Max<int64>(100, 100 + 50 * Idx * Idx);
}

float UTradePort::GetPortPriceMultiplier(ECountryType Port) const
{
	// 허브 전용 가격 배율. 허브 여부 판정은 호출자(ComputeSell)에서 선수행.
	// USA = +35% (고단가), Singapore = 중립 (속도로 차별화), 그 외 = 1.0 (안전한 기본값).
	switch (Port)
	{
	case ECountryType::USA:       return WorldMapConstants::USAPriceBonus; // 1.35
	case ECountryType::Singapore: return 1.0f;
	default:                      return 1.0f;
	}
}

float UTradePort::GetPortFeeMultiplier(ECountryType Port) const
{
	// MVP 단가 공식에는 미반영. 추후 연출 속도 또는 실판매가 조정 시 사용.
	switch (Port)
	{
	case ECountryType::Singapore: return WorldMapConstants::SingaporeFeeBonus; // 0.5
	default:                      return 1.0f;
	}
}

float UTradePort::GetBulkBonus(int64 Qty) const
{
	if (Qty >= 100) return WorldMapConstants::BulkBonus100; // 1.50
	if (Qty >= 50)  return WorldMapConstants::BulkBonus50;  // 1.25
	if (Qty >= 10)  return WorldMapConstants::BulkBonus10;  // 1.10
	return 1.0f;
}

float UTradePort::GetGradeMultiplier(EQualityGrade Grade) const
{
	return GetQualityGradeRevenueMultiplier(Grade);
}

bool UTradePort::IsTradeHub(ECountryType Port) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return false;

	bool bOk = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Port, bOk);
	return bOk && Info.bIsTradeHub;
}

float UTradePort::GetMoneyBias(ECountryType Port) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 1.0f;

	bool bOk = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Port, bOk);
	return bOk ? Info.MoneyBias : 1.0f;
}

float UTradePort::GetMarketCapBias(ECountryType Port) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return 1.0f;

	bool bOk = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Port, bOk);
	return bOk ? Info.MarketCapBias : 1.0f;
}

// ─────────────────────────────────────────────
// 공개 API
// ─────────────────────────────────────────────
FSellResult UTradePort::SellProduct(ECountryType Port, ECompanyType Company, int32 ProjectIndex, int64 RequestedQty)
{
	return ComputeSell(Port, Company, ProjectIndex, RequestedQty, /*bApply=*/true);
}

FSellResult UTradePort::PreviewSell(ECountryType Port, ECompanyType Company, int32 ProjectIndex, int64 RequestedQty) const
{
	// const 래퍼 — 내부 ComputeSell을 mutable-cast로 호출 (bApply=false라 상태 변경 없음)
	return const_cast<UTradePort*>(this)->ComputeSell(Port, Company, ProjectIndex, RequestedQty, /*bApply=*/false);
}

// ─────────────────────────────────────────────
// 내부 공통 계산
// ─────────────────────────────────────────────
FSellResult UTradePort::ComputeSell(ECountryType Port, ECompanyType Company, int32 ProjectIndex, int64 RequestedQty, bool bApply)
{
	FSellResult Result;
	const FIntPoint Key = MakeProductKey(Company, ProjectIndex);

	// 1. 매니저 레졸브
	UGameInstance* GI = GetGameInstance();
	UWorldMapManager* WorldMap = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
	UTradeOrderManager* TradeOrderMgr = GI ? GI->GetSubsystem<UTradeOrderManager>() : nullptr;
	UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCountryMarketManager* MarketMgr = GI ? GI->GetSubsystem<UCountryMarketManager>() : nullptr;

	// 2. 무역 허브 여부 판정 (DT lookup). 허브=벌크/주문서/Diamond 혜택, 비허브=기본 단가만.
	const bool bIsHub = IsTradeHub(Port);

	if (!WorldMap)
	{
		UE_LOG(LogTemp, Error, TEXT("[TradePort] WorldMapManager 없음"));
		if (bApply) OnSaleFailed.Broadcast(Key, TEXT("내부 오류: WorldMap 없음"));
		return Result;
	}

	// 3. 재고 확인 및 판매 수량 클램프
	const int64 Stock = WorldMap->GetProductAmount(Key);
	const int64 Qty = FMath::Min<int64>(FMath::Max<int64>(0, RequestedQty), Stock);
	if (Qty <= 0)
	{
		if (bApply)
		{
			OnSaleFailed.Broadcast(Key, TEXT("재고 없음"));
		}
		return Result;
	}

	// 4. 기본 단가
	const int64 Base = GetBasePrice(Company, ProjectIndex);

	// 5. 주문서 매칭 (VIP > Urgent > Normal) — 허브에서만 시도
	bool bMatched = false;
	FTradeOrder Match;
	int64 SellToOrderQty = 0;
	float MatchMul = 1.0f;
	if (bIsHub && TradeOrderMgr)
	{
		Match = TradeOrderMgr->FindMatchingOrder(Company, ProjectIndex, bMatched);
		if (bMatched)
		{
			MatchMul = Match.RewardMultiplier > 0.0f ? Match.RewardMultiplier : 1.0f;
			SellToOrderQty = FMath::Min<int64>(Qty, Match.RemainingQuantity);
		}
	}

	// 6. 배율 조합 — 허브만 포트 가격 배율/벌크 보너스 적용, 일반국가는 기본 단가(1.0). 품질은 공통.
	//    바이어스는 모든 나라 공통(허브는 1.0 중립, 비허브는 나라별 성향) — DT 값이 단일 진실.
	//    DemandMul은 시장 포화도 (0.4×~1.2×). 모든 나라 공통 적용.
	const float PriceMul         = bIsHub ? GetPortPriceMultiplier(Port) : 1.0f;
	const float BulkMul          = bIsHub ? GetBulkBonus(Qty) : 1.0f;
	const float GradeMul         = GetGradeMultiplier(WorldMap->GetProductGrade(Key));
	const float MoneyBiasMul     = GetMoneyBias(Port);
	const float MarketCapBiasMul = GetMarketCapBias(Port);
	const float DemandMul        = MarketMgr ? MarketMgr->GetDemandMul(Port, Company) : 1.0f;

	// (Country, Industry) 매트릭스 가격 가중 — DT_CountryDemand.PriceMul. 산업별 차등의 정적 layer.
	// 셀 미정의 시 1.0 fallback (Loud failure 는 DT 에디터에서 행 누락이 즉시 보임).
	float IndustryPriceMul = 1.0f;
	if (TableMgr)
	{
		bool bDemandOk = false;
		const FCountryDemandTable DemandRow = TableMgr->GetCountryDemand(Port, Company, bDemandOk);
		if (bDemandOk)
		{
			IndustryPriceMul = DemandRow.PriceMul;
		}
	}

	// 7. Money = Base * Qty * PriceMul * BulkMul * MatchMul * MoneyBias * DemandMul * IndustryPriceMul
	//    (FeeMul은 MVP 단가 공식에 미반영 — 싱가포르 장점은 속도/연출로 분리)
	// 글로벌 특성 — 유일한 전사 적용 대상. 항구는 건물을 인자로 받지 않아(재고가 Company/ProjectIndex 로 집계됨)
	// 건물별 집계가 불가능하다. 합산 대신 최댓값이라 건물 도배가 지배 전략이 되지 않는다.
	double TradeTraitMul = 1.0;
	if (UBuildingTraitManagerSubsystem* TradeTraitMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr)
	{
		TradeTraitMul = 1.0 + static_cast<double>(TradeTraitMgr->GetCompanyWideTraitPercent(EBuildingTraitTarget::TradeValue)) / 100.0;
	}
	const double MoneyD = static_cast<double>(Base) * static_cast<double>(Qty)
		* static_cast<double>(PriceMul) * static_cast<double>(BulkMul) * static_cast<double>(MatchMul)
		* static_cast<double>(MoneyBiasMul) * static_cast<double>(DemandMul) * static_cast<double>(IndustryPriceMul)
		* TradeTraitMul;
	const int64 Money = FMath::RoundToInt64(MoneyD);

	// 8. MarketCap = BasePerUnit * Qty * GradeMul * MatchMul * MarketCapBias * DemandMul
	int32 MarketCapPerUnit = 10;
	if (TableMgr)
	{
		bool bRecipeOk = false;
		const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Company, ProjectIndex, bRecipeOk);
		if (bRecipeOk && Recipe.BaseMarketCap > 0)
		{
			MarketCapPerUnit = Recipe.BaseMarketCap;
		}
	}
	const double MarketCapD = static_cast<double>(MarketCapPerUnit) * static_cast<double>(Qty)
		* static_cast<double>(GradeMul) * static_cast<double>(MatchMul)
		* static_cast<double>(MarketCapBiasMul) * static_cast<double>(DemandMul);
	const int64 MarketCapGain = FMath::RoundToInt64(MarketCapD);

	// 9. 결과 값 세팅 (Diamond는 주문서 완전 충족 시에만 지급 — 아래 ConsumeOrderQuantity 후 결정)
	Result.QuantitySold      = Qty;
	Result.MoneyGained       = Money;
	Result.MarketCapGained   = MarketCapGain;
	Result.bMatchedOrder     = bMatched;
	Result.MatchedOrderId    = bMatched ? Match.OrderId : INDEX_NONE;
	Result.DiamondGained     = 0;

	// 10. Apply 단계: 실제 재고 차감 및 보상 지급
	if (bApply)
	{
		WorldMap->ConsumeProduct(Key, Qty);

		// 시장 수요 차감 (판매가 시장을 포화시키는 효과). 모든 나라 공통.
		if (MarketMgr)
		{
			MarketMgr->ConsumeDemand(Port, Company, Qty);
		}

		// 주문서 차감: 이 판매로 주문이 완전히 충족되었는지 판단하여 Diamond 지급 여부 결정
		if (bMatched && TradeOrderMgr && SellToOrderQty > 0)
		{
			const int64 BeforeRemaining = Match.RemainingQuantity;
			TradeOrderMgr->ConsumeOrderQuantity(Match.OrderId, SellToOrderQty);
			// 이번 판매가 주문서를 완전히 닫았는지: BeforeRemaining <= SellToOrderQty 이면 완료
			const bool bFulfilledNow = (BeforeRemaining <= SellToOrderQty);
			if (bFulfilledNow && Match.DiamondBonus > 0)
			{
				Result.DiamondGained = Match.DiamondBonus;
			}
		}

		// 보상 지급
		if (ResourceMgr)
		{
			if (Money > 0)
			{
				ResourceMgr->StoreResource(EResourceType::Money, Money);
			}
			if (MarketCapGain > 0)
			{
				ResourceMgr->StoreResource(EResourceType::MarketCap, MarketCapGain);
			}
			if (Result.DiamondGained > 0)
			{
				ResourceMgr->StoreResource(EResourceType::Diamond, Result.DiamondGained);
			}
		}

		OnProductSold.Broadcast(Port, Key, Result);
	}
	else
	{
		// 미리보기: Diamond 보너스도 "지금 팔면 완전 충족되는가" 기준으로 노출
		if (bMatched && SellToOrderQty > 0)
		{
			const bool bWouldFulfill = (Match.RemainingQuantity <= SellToOrderQty);
			if (bWouldFulfill && Match.DiamondBonus > 0)
			{
				Result.DiamondGained = Match.DiamondBonus;
			}
		}
	}

	return Result;
}
