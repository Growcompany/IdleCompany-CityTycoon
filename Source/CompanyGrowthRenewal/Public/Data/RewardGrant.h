#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Table/MissionTable.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"

// 라이브옵스 보상 지급의 단일 경계 — 출석/퀘스트/우편/이벤트가 전부 이 함수만 부른다
struct FRewardGrant
{
	static void Commit(UGameInstance* GI, const TArray<FMissionReward>& Rewards)
	{
		if (!GI) return;
		for (const FMissionReward& R : Rewards)
		{
			// 데이터/호출부 실수를 지급 시점에 크게 알린다 (스펙 §0.5)
			ensureMsgf(R.ResourceType != EResourceType::Money, TEXT("FRewardGrant::Commit: Money reward is forbidden"));
		}

		// 지급 경계 — Cloud Function 으로 옮길 때 이 블록만 바뀐다
		UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
		UItemInventoryManager* ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
		for (const FMissionReward& R : Rewards)
		{
			if (ResMgr && R.ResourceType != EResourceType::None && R.Amount > 0) ResMgr->StoreResource(R.ResourceType, R.Amount, /*bShouldSave=*/false);
			if (ItemMgr && R.ItemType != EItemType::None && R.ItemAmount > 0) ItemMgr->AddItem(R.ItemType, R.ItemAmount, /*bShouldSave=*/false);
		}
	}
};
