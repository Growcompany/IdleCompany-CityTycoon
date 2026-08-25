// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/ItemInventoryManager.h"
#include "Manager/SaveLoadManager.h"

void UItemInventoryManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[ItemInventoryManager] Initialized"));
}

int32 UItemInventoryManager::GetItemCount(EItemType Type) const
{
	if (const int32* Found = ItemStorage.Find(Type))
	{
		return *Found;
	}
	return 0;
}

bool UItemInventoryManager::HasItem(EItemType Type, int32 Amount) const
{
	return GetItemCount(Type) >= Amount;
}

void UItemInventoryManager::AddItem(
	EItemType Type,
	int32 Amount,
	bool bShouldSave,
	bool bShouldBroadcast)
{
	if (Amount <= 0 || Type == EItemType::None)
	{
		return;
	}

	int32& Current = ItemStorage.FindOrAdd(Type);
	Current += Amount;

	UE_LOG(LogTemp, Log, TEXT("[ItemInventoryManager] AddItem: Type=%d, Added=%d, NewTotal=%d"),
		static_cast<int32>(Type), Amount, Current);

	if (bShouldBroadcast)
	{
		OnItemChanged.Broadcast(Type, Current, Amount);
	}

	if (bShouldSave)
	{
		SaveGameData();
	}
}

bool UItemInventoryManager::SpendItem(EItemType Type, int32 Amount, bool bShouldSave)
{
	if (Amount <= 0 || Type == EItemType::None)
	{
		return false;
	}

	if (!HasItem(Type, Amount))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemInventoryManager] SpendItem failed: Type=%d, Requested=%d, Have=%d"),
			static_cast<int32>(Type), Amount, GetItemCount(Type));
		return false;
	}

	int32& Current = ItemStorage.FindOrAdd(Type);
	Current -= Amount;

	UE_LOG(LogTemp, Log, TEXT("[ItemInventoryManager] SpendItem: Type=%d, Spent=%d, NewTotal=%d"),
		static_cast<int32>(Type), Amount, Current);

	OnItemChanged.Broadcast(Type, Current, -Amount);

	if (bShouldSave)
	{
		SaveGameData();
	}

	return true;
}

void UItemInventoryManager::SetAllItems(
	const TMap<EItemType, int32>& InItems,
	bool bShouldBroadcast)
{
	ItemStorage = InItems;

	if (bShouldBroadcast)
	{
		// 각 아이템의 UI 업데이트 이벤트 발생
		for (const auto& Pair : ItemStorage)
		{
			OnItemChanged.Broadcast(Pair.Key, Pair.Value, 0);
		}
	}
}

void UItemInventoryManager::SaveGameData()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}
