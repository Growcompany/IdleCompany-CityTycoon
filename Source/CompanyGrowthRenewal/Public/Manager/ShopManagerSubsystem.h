#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ShopSaveData.h"
#include "Enum/ShopTypes.h"
#include "ShopManagerSubsystem.generated.h"

/**
 * 상점 구매/한도/리셋 매니저
 * - 상품 정의 = DT_ShopItem(TableManagerSubsystem) 단일 진실
 * - 리셋 기준: 로컬 자정 / 월요일 00:00 (서버 시간 검증은 오프라인 보상 시스템 도입 시 통합)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UShopManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 상품 구매 — 리셋 체크 → 한도/재화 검증 → 차감+지급+세이브. 실패 시 false (차감 없음) */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool PurchaseItem(FName RowName);

	/** 해당 행 결제 재화를 감당 가능한지 */
	UFUNCTION(BlueprintPure, Category = "Shop")
	bool CanAfford(FName RowName) const;

	/** 남은 구매 가능 횟수 (-1 = 무제한, 행 없으면 0) */
	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetRemainingCount(FName RowName) const;

	/** 다이아 50으로 일일 상점 한도 즉시 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool ManualResetDaily();

	/** 자정/월요일 경과 시 한도 리셋 (패널 오픈·구매 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void CheckAndPerformResets();

	/** 다음 일일 리셋까지 남은 시간 */
	FTimespan GetTimeUntilDailyReset() const;

	// 한도/리셋 변경 브로드캐스트 (UI 갱신용)
	DECLARE_MULTICAST_DELEGATE(FOnShopStockChanged);
	FOnShopStockChanged OnShopStockChanged;

	// SaveLoadManager 연동용 (가챠 패턴과 동일한 평구조체 핸드오프)
	const FShopSaveData& GetShopData() const { return ShopData; }
	void SetShopData(const FShopSaveData& InData) { ShopData = InData; }

private:
	void ResetTabCounts(EShopTab Tab);
	void SaveGameData();

	FShopSaveData ShopData;

	static constexpr int64 ManualResetDiamondCost = 50;
};
