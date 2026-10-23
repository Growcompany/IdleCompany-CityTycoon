#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/ItemType.h"

class USaveLoadManager;

// 본사 레벨업 때 나가는 보상. 지급(USaveLoadManager::TryLevelUpHQ)과 본사 패널 보상 카드가 함께 읽는다.
namespace HQLevelUpReward
{
	// 새로 열린 업종 하나당
	constexpr EItemType IndustryTicketType = EItemType::RecruitTicketPremium;
	constexpr int32 IndustryTicketAmount = 1;
	constexpr int32 IndustryLootRolls = 1;
	constexpr const TCHAR* IndustryLootTable = TEXT("High");

	// 매 레벨 — 빌딩이 늘어 특성 칸이 늘어나는 만큼 꾸준히 채울 거리를 준다
	constexpr EItemType LevelTraitTicketType = EItemType::BuildingTraitTicketNormal;
	constexpr int32 LevelTraitTicketAmount = 1;

	// N레벨마다 한 번 더
	constexpr EItemType MilestoneTraitTicketType = EItemType::BuildingTraitTicketAdvanced;
	constexpr int32 MilestoneTraitTicketAmount = 1;
	constexpr int32 MilestoneLevelInterval = 5;

	inline bool IsMilestoneLevel(int32 HQLevel)
	{
		return HQLevel > 1 && HQLevel % MilestoneLevelInterval == 0;
	}

	// 그 레벨에서 새로 열리는 출시 가용 업종 (ECompanyType 순서)
	COMPANYGROWTHRENEWAL_API TArray<ECompanyType> GetIndustriesUnlockedAt(USaveLoadManager& SaveMgr, int32 HQLevel);
}
