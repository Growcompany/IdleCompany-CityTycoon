#pragma once

#include "CoreMinimal.h"
#include "Data/MailboxSaveData.h"

struct FMailboxRules
{
	static constexpr int32 MaxMails = 50;

	static bool IsExpired(const FMailEntry& E, int64 NowUnix) { return E.ExpireUnix > 0 && NowUnix >= E.ExpireUnix; }

	static int32 PurgeExpired(FMailboxSaveData& Save, int64 NowUnix)
	{
		return Save.Mails.RemoveAll([NowUnix](const FMailEntry& E) { return IsExpired(E, NowUnix); });
	}

	static bool CanReceiveGlobal(const FMailboxSaveData& Save, const FString& SourceKey)
	{
		return !SourceKey.IsEmpty() && !Save.ConsumedGlobalMailIds.Contains(SourceKey);
	}

	// 상한 초과 시 가장 오래된 것(배열 앞)부터 버린다. 글로벌 메일은 소비 목록에 기록
	static void Push(FMailboxSaveData& Save, const FMailEntry& Entry)
	{
		if (!Entry.SourceKey.IsEmpty()) Save.ConsumedGlobalMailIds.AddUnique(Entry.SourceKey);
		Save.Mails.Add(Entry);
		while (Save.Mails.Num() > MaxMails) Save.Mails.RemoveAt(0);
	}

	static const FMailEntry* FindMail(const FMailboxSaveData& Save, const FGuid& Id)
	{
		return Save.Mails.FindByPredicate([&Id](const FMailEntry& E) { return E.Id == Id; });
	}
};
