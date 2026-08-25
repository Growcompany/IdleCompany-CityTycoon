// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Enum/ItemType.h"
#include "ItemInventoryManager.generated.h"

/**
 * 아이템 인벤토리 매니저
 * - 채용권, 강화 재료, 특수 아이템 등 수량 관리
 * - ResourceItemManager(재화)와 별개로 소비성 아이템 전용
 * - TMap<EItemType, int32> 기반 저장/로드
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UItemInventoryManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 아이템 변경 델리게이트 (타입, 변경 후 수량, 변경량)
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnItemChanged, EItemType, int32 /*NewCount*/, int32 /*Delta*/);
	FOnItemChanged OnItemChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 아이템 수량 조회
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemCount(EItemType Type) const;

	// 아이템 보유 여부 (Amount개 이상)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasItem(EItemType Type, int32 Amount = 1) const;

	// 아이템 추가
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItem(
		EItemType Type,
		int32 Amount,
		bool bShouldSave = true,
		bool bShouldBroadcast = true);

	// 아이템 소비 (부족 시 false 반환, 차감 안 함)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SpendItem(EItemType Type, int32 Amount, bool bShouldSave = true);

	// 저장/로드용 전체 데이터 접근
	const TMap<EItemType, int32>& GetAllItems() const { return ItemStorage; }
	void SetAllItems(
		const TMap<EItemType, int32>& InItems,
		bool bShouldBroadcast = true);

private:
	TMap<EItemType, int32> ItemStorage;

	void SaveGameData();
};
