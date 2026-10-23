#pragma once

#include "CoreMinimal.h"
#include "Table/LiveOpsTables.h"
#include "Data/LiveOpsSaveData.h"
#include "JsonObjectConverter.h"

struct FLiveOpsRules
{
	// 기간 창 [Start, End)
	static bool IsWithin(int64 StartUnix, int64 EndUnix, int64 NowUnix) { return NowUnix >= StartUnix && NowUnix < EndUnix; }
	static bool IsActive(const FLiveOpsEventDef& Def, int64 NowUnix) { return IsWithin(Def.StartUnix, Def.EndUnix, NowUnix); }
	static bool IsActive(const FLiveOpsMailDef& Def, int64 NowUnix) { return IsWithin(Def.StartUnix, Def.EndUnix, NowUnix); }

	// [Start, End) 시각 창. End < Start 는 자정 넘김. 여러 창이 겹치면 곱
	static float HotTimeMultiplier(const FLiveOpsEventDef& Def, int32 HourKst)
	{
		float Mult = 1.0f;
		for (const FLiveOpsHotTimeWindow& W : Def.HotTime)
		{
			// End == Start 는 24시간 창, Multiplier <= 0 은 수익 0 — 데이터 실수로 보고 무시한다
			if (W.EndHourKst == W.StartHourKst || W.Multiplier <= 0.0f) continue;

			const bool bInside = W.EndHourKst > W.StartHourKst
				? (HourKst >= W.StartHourKst && HourKst < W.EndHourKst)
				: (HourKst >= W.StartHourKst || HourKst < W.EndHourKst);
			if (bInside) Mult *= W.Multiplier;
		}
		return Mult;
	}

	static bool IsMilestoneClaimable(const FLiveOpsEventProgress& P, const FLiveOpsEventDef& Def, int32 Index)
	{
		return Def.Milestones.IsValidIndex(Index) && !P.ClaimedMilestones.Contains(Index) && P.Points >= Def.Milestones[Index].Threshold;
	}

	static void ExpiredMilestoneRewards(const FLiveOpsEventProgress& P, const FLiveOpsEventDef& Def, TArray<FMissionReward>& Out)
	{
		Out.Reset();
		for (int32 i = 0; i < Def.Milestones.Num(); ++i)
		{
			if (IsMilestoneClaimable(P, Def, i)) Out.Append(Def.Milestones[i].Rewards);
		}
	}

	// TitleData 정의는 DT 로더(InitializeLiveOpsTables)의 ensure 를 못 거치므로 같은 규칙을 여기서 본다
	template <typename T>
	static bool UniqueIds(const TArray<T>& Defs, const TCHAR* What, FString& OutError)
	{
		TSet<FString> Seen;
		for (const T& Def : Defs)
		{
			bool bAlready = false;
			Seen.Add(Def.Id, &bAlready);
			if (bAlready)
			{
				OutError = FString::Printf(TEXT("%s: duplicate Id %s"), What, *Def.Id);
				return false;
			}
		}
		return true;
	}

	static bool NoMoneyReward(const TArray<FMissionReward>& Rewards, const FString& Where, FString& OutError)
	{
		for (const FMissionReward& R : Rewards)
		{
			if (R.ResourceType == EResourceType::Money)
			{
				OutError = FString::Printf(TEXT("%s: Money reward is forbidden"), *Where);
				return false;
			}
		}
		return true;
	}

	static bool ValidateEvents(const TArray<FLiveOpsEventDef>& Defs, FString& OutError)
	{
		if (!UniqueIds(Defs, TEXT("Event"), OutError)) return false;
		for (const FLiveOpsEventDef& Def : Defs)
		{
			for (const FLiveOpsMilestone& M : Def.Milestones)
			{
				if (!NoMoneyReward(M.Rewards, FString::Printf(TEXT("Event %s"), *Def.Id), OutError)) return false;
			}
			for (const FLiveOpsHotTimeWindow& W : Def.HotTime)
			{
				if (W.StartHourKst < 0 || W.StartHourKst > 23 || W.EndHourKst < 0 || W.EndHourKst > 23)
				{
					OutError = FString::Printf(TEXT("Event %s: hot time hour out of [0, 23]"), *Def.Id);
					return false;
				}
				if (W.EndHourKst == W.StartHourKst)
				{
					OutError = FString::Printf(TEXT("Event %s: hot time End == Start"), *Def.Id);
					return false;
				}
				if (W.Multiplier <= 0.0f)
				{
					OutError = FString::Printf(TEXT("Event %s: hot time Multiplier must be > 0"), *Def.Id);
					return false;
				}
			}
		}
		return true;
	}

	static bool ValidateNotices(const TArray<FLiveOpsNoticeDef>& Defs, FString& OutError)
	{
		return UniqueIds(Defs, TEXT("Notice"), OutError);
	}

	static bool ValidateMails(const TArray<FLiveOpsMailDef>& Defs, FString& OutError)
	{
		if (!UniqueIds(Defs, TEXT("Mail"), OutError)) return false;
		for (const FLiveOpsMailDef& Def : Defs)
		{
			if (!NoMoneyReward(Def.Rewards, FString::Printf(TEXT("Mail %s"), *Def.Id), OutError)) return false;
		}
		return true;
	}

	template <typename T>
	static bool ParseArray(const FString& Json, TArray<T>& Out)
	{
		TArray<T> Tmp;
		if (!FJsonObjectConverter::JsonArrayStringToUStruct<T>(Json, &Tmp)) return false;
		Out = MoveTemp(Tmp);
		return true;
	}

	static bool ParseEvents(const FString& Json, TArray<FLiveOpsEventDef>& Out) { return ParseArray(Json, Out); }
	static bool ParseNotices(const FString& Json, TArray<FLiveOpsNoticeDef>& Out) { return ParseArray(Json, Out); }
	static bool ParseMails(const FString& Json, TArray<FLiveOpsMailDef>& Out) { return ParseArray(Json, Out); }
};
