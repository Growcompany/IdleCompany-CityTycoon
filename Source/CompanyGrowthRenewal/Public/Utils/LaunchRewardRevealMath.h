#pragma once

#include "CoreMinimal.h"
#include "Enum/ItemType.h"
#include "Table/MissionTable.h"

// 출시 보상 리빌 순수 계산 — 월드 의존 0(자동화 테스트 대상). 소비처: ULaunchRewardRevealWidget.
namespace LaunchRewardRevealMath
{
	// ULaunchLootManagerSubsystem::GetLootPreview 의 "메인 보상 우선" 서열과 동일. 숫자가 클수록 귀하다.
	inline int32 RewardPriority(EItemType Type)
	{
		switch (Type)
		{
		case EItemType::RecruitTicketNormal: return 1;
		case EItemType::RecruitTicketAdvanced: return 2;
		case EItemType::BuildingTraitTicketNormal: return 3;
		case EItemType::RecruitTicketPremium: return 4;
		case EItemType::BuildingTraitTicketAdvanced: return 5;
		case EItemType::SkinTicketNormal: return 6;
		case EItemType::SkinTicketAdvanced: return 7;
		default: return 9;
		}
	}

	// 같은 ItemType 은 1장으로 합치고(수량은 자원=Amount·아이템=ItemAmount 각각 합산), 서열 오름차순 — 최고 보상이 마지막에 찍힌다.
	inline TArray<FMissionReward> BuildRevealOrder(const TArray<FMissionReward>& Loot)
	{
		TArray<FMissionReward> Out;
		for (const FMissionReward& R : Loot)
		{
			if (R.ItemType == EItemType::None && R.ResourceType == EResourceType::None) { continue; }
			FMissionReward* Existing = Out.FindByPredicate([&R](const FMissionReward& E)
			{
				return E.ItemType == R.ItemType && E.ResourceType == R.ResourceType;
			});
			if (Existing) { Existing->Amount += R.Amount; Existing->ItemAmount += R.ItemAmount; }
			else { Out.Add(R); }
		}
		Out.StableSort([](const FMissionReward& A, const FMissionReward& B)
		{
			return RewardPriority(A.ItemType) < RewardPriority(B.ItemType);
		});
		return Out;
	}

	// 도장 연타 시작 시각 — 간격이 Decay 배로 가속하되 MinStagger 아래로는 안 줄어든다("뭐가 나왔는지" 놓침 방지).
	inline TArray<float> StampRunTimes(int32 Count, float Start, float Stagger, float Decay, float MinStagger)
	{
		TArray<float> Times;
		float Acc = Start;
		for (int32 i = 0; i < Count; ++i)
		{
			Times.Add(Acc);
			Acc += FMath::Max(MinStagger, Stagger * FMath::Pow(Decay, static_cast<float>(i)));
		}
		return Times;
	}
}
