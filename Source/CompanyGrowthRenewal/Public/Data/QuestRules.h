#pragma once

#include "CoreMinimal.h"
#include "Enum/QuestTypes.h"
#include "Table/QuestTable.h"
#include "Data/QuestSaveData.h"

// 원장 1행의 표시 상태 스냅샷 (UI 소비용)
struct FQuestEntry
{
	FName QuestID;
	FQuestTable Row;
	EQuestState State = EQuestState::InProgress;
	int64 Current = 0;
	int64 Target = 1;
};

// 퀘스트 원장 순수 규칙 — 매니저는 값을 모아 이 함수들에 넘기고 결과만 저장한다
struct FQuestRules
{
	static bool IsIncrementCounter(EQuestCounter Counter)
	{
		return Counter >= EQuestCounter::RevenueCollect && Counter <= EQuestCounter::FloorRaise;
	}

	static bool IsClaimed(FName QuestID, const FQuestTable& Row, const FQuestSaveData& Save)
	{
		return Row.Period == EQuestPeriod::Lifetime ? Save.ClaimedLifetime.Contains(QuestID) : Save.Claimed.Contains(QuestID);
	}

	// 업적: 같은 Track 에서 수령 안 된 가장 낮은 Tier 만 노출. 트랙이 없거나 전부 수령이면 0
	static int32 VisibleLifetimeTier(FName Track, const TMap<FName, FQuestTable>& Rows, const FQuestSaveData& Save)
	{
		int32 Lowest = 0;
		for (const auto& Pair : Rows)
		{
			const FQuestTable& Row = Pair.Value;
			if (Row.Period != EQuestPeriod::Lifetime || Row.Track != Track || Save.ClaimedLifetime.Contains(Pair.Key)) continue;
			if (Lowest == 0 || Row.Tier < Lowest) Lowest = Row.Tier;
		}
		return Lowest;
	}

	static EQuestState StateFor(FName QuestID, const FQuestTable& Row, int64 Current, const FQuestSaveData& Save)
	{
		if (Row.Period == EQuestPeriod::Lifetime)
		{
			if (Save.ClaimedLifetime.Contains(QuestID)) return EQuestState::Hidden;
			// 낮은 티어 잔존 여부는 호출측(GetEntries)이 VisibleLifetimeTier 로 거른다 — 여기서는 수령 여부만
			return Current >= Row.Target ? EQuestState::Claimable : EQuestState::InProgress;
		}
		if (Save.Claimed.Contains(QuestID)) return EQuestState::Claimed;
		return Current >= Row.Target ? EQuestState::Claimable : EQuestState::InProgress;
	}

	// 주기 롤오버: 완료·미수령 보상을 OutExpired 로 넘기고 그 주기의 상태를 비운다.
	// 증분형 행만 본다 — 절대값형은 라이브 게터가 필요해 순수 규칙 밖(일일/주간 행은 전부 증분형).
	static void Rollover(FQuestSaveData& Save, const TMap<FName, FQuestTable>& Rows,
		EQuestPeriod Period, const FString& NewKey, TArray<FMissionReward>& OutExpired)
	{
		TArray<FName> PeriodIDs;
		for (const auto& Pair : Rows)
		{
			const FQuestTable& Row = Pair.Value;
			if (Row.Period != Period) continue;
			PeriodIDs.Add(Pair.Key);
			if (!IsIncrementCounter(Row.Counter) || Save.Claimed.Contains(Pair.Key)) continue;
			if (Save.Progress.FindRef(Pair.Key) >= Row.Target)
			{
				OutExpired.Append(Row.Rewards);
			}
		}
		for (const FName& ID : PeriodIDs) { Save.Progress.Remove(ID); Save.Claimed.Remove(ID); }
		if (Period == EQuestPeriod::Daily) { Save.DayKey = NewKey; }
		else if (Period == EQuestPeriod::Weekly) { Save.WeekKey = NewKey; }
	}
};
