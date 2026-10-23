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

	// 표시 항목 1개의 아이콘(자원 Icon 소프트 참조 / 아이템 아이콘). 없으면 nullptr — 호출부는 수치만 표시한다.
	UObject* ResolveIcon(UGameInstance* GameInstance, const FDisplayEntry& Entry);

	// 보상 1개를 표시 단위로 펼친 첫 항목의 아이콘 (보상 1종 전제인 출석 칸·메일 행용)
	UObject* ResolveIcon(UGameInstance* GameInstance, const FMissionReward& Reward);

	// 수량 표기 통일 — 아이템 "x{N}", 자원 축약. 출석 타일 라벨과 보상 칩이 같은 문자열을 쓴다
	FText FormatQuantity(const FDisplayEntry& Entry);

	// RewardBox 를 비우고 [아이콘+축약수치] 페어를 다시 채운다. 표시한 항목이 하나라도 있으면 true.
	bool RenderIcons(UWidgetTree& InWidgetTree, UGameInstance* GameInstance,
		UHorizontalBox& InRewardBox, const TArray<FMissionReward>& Rewards);
}
