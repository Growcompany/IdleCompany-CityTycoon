// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Data/WorldMapTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/ResourceType.h"
#include "Entity/Country/CountryActor.h"
#include "CountryMarketManager.generated.h"

/**
 * UCountryMarketManager
 * 나라 × 산업 조합별 시장 수요 게이지 관리 (GameInstanceSubsystem).
 *
 * 역할:
 *  - DT_CountryDemand 기반으로 (Country, Industry) 셀 초기화 (Current=Capacity 만수요 시작)
 *  - 분당 RecoveryPerMin만큼 수요 자동 회복 (FTSTicker, 1초 간격)
 *  - 판매 시 ConsumeDemand로 수요 차감 → 시장 포화 시 페널티
 *  - DemandMul 곡선: 0.4 + 0.8 × ratio → 0.4× ~ 1.2×
 *  - GrowthMul: 플레이어 MarketCap 기반 진행도 스케일링 (초반 0.1× ~ 후반 2.0×)
 *    Effective Capacity = DT.Capacity × GrowthMul, RecoveryPerMin도 같은 비율로 스케일
 *  - SaveGame 직렬화 (오프라인 캐치업 지원)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountryMarketManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDemandChanged, ECountryType, Country, ECompanyType, Industry, float, NewRatio);

	UPROPERTY(BlueprintAssignable, Category = "Market|Events")
	FOnDemandChanged OnDemandChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** (Country, Industry) 셀의 현재 수요 비율 (0.0~1.0). 미정의 셀은 1.0 반환. */
	UFUNCTION(BlueprintPure, Category = "Market")
	float GetDemandRatio(ECountryType Country, ECompanyType Industry) const;

	/** 수요 비율 기반 판매 배율 (0.4× ~ 1.2×). 미정의 셀은 1.0 반환. */
	UFUNCTION(BlueprintPure, Category = "Market")
	float GetDemandMul(ECountryType Country, ECompanyType Industry) const;

	/** 판매 시 호출. Current에서 Qty 차감 (0 클램프). 변경 시 OnDemandChanged 브로드캐스트. */
	UFUNCTION(BlueprintCallable, Category = "Market")
	void ConsumeDemand(ECountryType Country, ECompanyType Industry, int64 Qty);

	/** 셀 상태 조회 (UI용). bOutFound=false면 미정의 셀. */
	UFUNCTION(BlueprintCallable, Category = "Market")
	FCountryMarketState GetMarketState(ECountryType Country, ECompanyType Industry, bool& bOutFound) const;

	/** 모든 셀 상태 (SaveGame 직렬화용). */
	UFUNCTION(BlueprintCallable, Category = "Market")
	TArray<FCountryMarketState> GetAllStates() const;

	/** SaveGame 로드 후 호출. 외부 상태로 일괄 덮어쓰기 + 오프라인 회복 시뮬. */
	void LoadStates(const TArray<FCountryMarketState>& InStates, const FDateTime& LastSaveTime);

	/** 현재 플레이어 MarketCap 기반 시장 성장 배율. */
	UFUNCTION(BlueprintPure, Category = "Market")
	float GetCurrentGrowthMultiplier() const;

	/** 임의 MarketCap 값에 대한 성장 배율 계산 (UI 미리보기 등에서 사용). DT_MarketBalance 사용. */
	UFUNCTION(BlueprintCallable, Category = "Market")
	float CalculateGrowthMultiplier(int64 MarketCap) const;

private:
	bool TickInternal(float DeltaTime);

	/** DT 기반 초기 셀 생성. SaveGame 로드 전 호출. */
	void InitializeStatesFromDT();

	/** ResourceItemManager.OnResourceChanged 구독 핸들러. MarketCap 변동 시 Capacity 라이브 재스케일. */
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	/** 마지막으로 적용된 GrowthMul. 미세 변화로 인한 Broadcast 폭증 방지용 임계값 비교에 사용. */
	float CachedGrowthMul = 0.0f;

	/** Country/Industry → FIntPoint 키 변환. */
	static FIntPoint MakeKey(ECountryType Country, ECompanyType Industry)
	{
		return FIntPoint(static_cast<int32>(Country), static_cast<int32>(Industry));
	}

	UPROPERTY()
	TMap<FIntPoint, FCountryMarketState> States;

	FTSTicker::FDelegateHandle TickerHandle;
};
