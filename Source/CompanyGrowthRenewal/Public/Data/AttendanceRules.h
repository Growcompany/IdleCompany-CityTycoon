#pragma once

#include "CoreMinimal.h"
#include "Data/AttendanceSaveData.h"

// 관대형 순환 출석: 놓친 날은 건너뛰지 않고 다음 접속일이 다음 칸, 28 뒤 1
struct FAttendanceRules
{
	static constexpr int32 CycleLength = 28;

	static bool CanClaim(const FAttendanceSaveData& Save, const FString& TodayKey)
	{
		return !TodayKey.IsEmpty() && Save.LastClaimDayKey != TodayKey;
	}

	static int32 NextDay(const FAttendanceSaveData& Save)
	{
		return (FMath::Clamp(Save.CycleDay, 0, CycleLength) % CycleLength) + 1;
	}

	static void ApplyClaim(FAttendanceSaveData& Save, const FString& TodayKey)
	{
		Save.CycleDay = NextDay(Save);
		Save.LastClaimDayKey = TodayKey;
		Save.TotalDays += 1;
	}
};
