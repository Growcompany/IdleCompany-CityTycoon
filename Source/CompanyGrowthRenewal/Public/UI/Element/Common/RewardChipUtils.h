#pragma once

#include "CoreMinimal.h"
#include "Table/MissionTable.h"

class UGameInstance;
class UHorizontalBox;
class UWidgetTree;

// 미션 트래커와 미션판 행이 같은 보상 표시 규약을 공유하도록 한 곳에 둔 렌더 헬퍼
namespace CGRewardChip
{
	struct FDisplayEntry
	{
		EResourceType ResourceType = EResourceType::None;
		EItemType ItemType = EItemType::None;
		int64 Quantity = 0;
	};

	// FMissionReward 는 자원과 아이템을 동시에 담을 수 있으므로 표시 단위로 각각 펼친다.
	TArray<FDisplayEntry> BuildDisplayEntries(const TArray<FMissionReward>& Rewards);

	// RewardBox 를 비우고 [아이콘+축약수치] 페어를 다시 채운다. 표시한 항목이 하나라도 있으면 true.
	bool RenderIcons(UWidgetTree& InWidgetTree, UGameInstance* GameInstance,
		UHorizontalBox& InRewardBox, const TArray<FMissionReward>& Rewards);
}
