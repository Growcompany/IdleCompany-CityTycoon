#pragma once

#include "CoreMinimal.h"

// 게임 하루/주 키 = KST(UTC+9) 자정 기준. 장비권 주간 키는 통합 시 이 함수로 연결한다.
struct FGameClockRules
{
	static constexpr int64 KstOffsetSeconds = 9 * 3600;
	static constexpr int64 DaySeconds = 86400;

	static FDateTime KstFromUnix(int64 Unix)
	{
		return FDateTime::FromUnixTimestamp(Unix) + FTimespan::FromSeconds(static_cast<double>(KstOffsetSeconds));
	}

	static FString DayKeyFromUnix(int64 Unix)
	{
		const FDateTime Kst = KstFromUnix(Unix);
		return FDateTime(Kst.GetYear(), Kst.GetMonth(), Kst.GetDay()).ToString(TEXT("%Y-%m-%d"));
	}

	// EDayOfWeek 는 Monday=0 — 이번 주 월요일 00:00(KST)
	static FString WeekKeyFromUnix(int64 Unix)
	{
		const FDateTime Kst = KstFromUnix(Unix);
		const FDateTime Midnight(Kst.GetYear(), Kst.GetMonth(), Kst.GetDay());
		return (Midnight - FTimespan::FromDays(static_cast<int32>(Kst.GetDayOfWeek()))).ToString(TEXT("%Y-%m-%d"));
	}

	static int64 SecondsIntoKstDay(int64 Unix)
	{
		const int64 Local = Unix + KstOffsetSeconds;
		return ((Local % DaySeconds) + DaySeconds) % DaySeconds;
	}

	static int64 SecondsUntilNextDayKst(int64 Unix)
	{
		return DaySeconds - SecondsIntoKstDay(Unix);
	}

	static int64 SecondsUntilNextWeekKst(int64 Unix)
	{
		const int32 DayOfWeek = static_cast<int32>(KstFromUnix(Unix).GetDayOfWeek());
		const int64 DaysLeft = 7 - DayOfWeek;
		return DaysLeft * DaySeconds - SecondsIntoKstDay(Unix);
	}

	static int32 HourKst(int64 Unix)
	{
		return KstFromUnix(Unix).GetHour();
	}

	// 24h 미만 "HH:MM:SS", 이상 "N일 HH:MM". 음수는 0 으로 클램프
	static FString FormatCountdown(int64 Seconds)
	{
		const int64 S = FMath::Max<int64>(0, Seconds);
		const int64 Days = S / DaySeconds;
		const int64 Rem = S % DaySeconds;
		const int64 H = Rem / 3600;
		const int64 M = (Rem % 3600) / 60;
		const int64 Sec = Rem % 60;
		if (Days > 0)
		{
			return FString::Printf(TEXT("%lld일 %02lld:%02lld"), Days, H, M);
		}
		return FString::Printf(TEXT("%02lld:%02lld:%02lld"), H, M, Sec);
	}
};
