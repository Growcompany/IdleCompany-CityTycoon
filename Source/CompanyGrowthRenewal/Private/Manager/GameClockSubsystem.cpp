#include "Manager/GameClockSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Data/GameClockRules.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Global/CGDevSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameClock, Log, All);

void UGameClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UPlayFabManagerSubsystem>();

	if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PFMgr->OnServerTimeReceived.AddUObject(this, &UGameClockSubsystem::HandleServerTime);
	}

	GetGameInstance()->GetTimerManager().SetTimer(
		PollHandle, FTimerDelegate::CreateUObject(this, &UGameClockSubsystem::Poll), PollIntervalSeconds, true);
	if (UCGDevSettings::IsOfflineMode())
	{
		Poll();
	}
}

void UGameClockSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PollHandle);
		if (UPlayFabManagerSubsystem* PFMgr = GI->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			PFMgr->OnServerTimeReceived.RemoveAll(this);
		}
	}
	Super::Deinitialize();
}

bool UGameClockSubsystem::IsServerTimeValid() const
{
	// 개발용 로컬 모드에서만 기기 시간을 게임 진행 기준으로 사용한다.
	if (UCGDevSettings::IsOfflineMode())
	{
		return true;
	}
	if (!bSynced)
	{
		return false;
	}
	return (FPlatformTime::Seconds() - PlatformSecondsAtSync) < static_cast<double>(SyncStaleSeconds);
}

int64 UGameClockSubsystem::GetServerNowUnix() const
{
	int64 Base = FDateTime::UtcNow().ToUnixTimestamp();
	if (bSynced)
	{
		Base = ServerUnixAtSync + static_cast<int64>(FPlatformTime::Seconds() - PlatformSecondsAtSync);
	}
#if !UE_BUILD_SHIPPING
	Base += DebugOffsetSeconds;
#endif
	return Base;
}

FString UGameClockSubsystem::GetDayKey() const
{
	return FGameClockRules::DayKeyFromUnix(GetServerNowUnix());
}

FString UGameClockSubsystem::GetWeekKey() const
{
	return FGameClockRules::WeekKeyFromUnix(GetServerNowUnix());
}

int64 UGameClockSubsystem::GetSecondsUntilDayRoll() const
{
	return FGameClockRules::SecondsUntilNextDayKst(GetServerNowUnix());
}

int64 UGameClockSubsystem::GetSecondsUntilWeekRoll() const
{
	return FGameClockRules::SecondsUntilNextWeekKst(GetServerNowUnix());
}

int32 UGameClockSubsystem::GetHourKst() const
{
	return FGameClockRules::HourKst(GetServerNowUnix());
}

void UGameClockSubsystem::SetDebugOffsetSeconds(int64 InOffset)
{
	// 치트 극단값이 FDateTime 범위를 벗어나면 check() 로 죽는다 — ±10년 클램프
	InOffset = FMath::Clamp<int64>(InOffset, -3650LL * 86400LL, 3650LL * 86400LL);
	DebugOffsetSeconds = InOffset;
	UE_LOG(LogGameClock, Log, TEXT("debug offset=%lld now=%lld day=%s"), DebugOffsetSeconds, GetServerNowUnix(), *GetDayKey());
	BroadcastIfKeysChanged();
}

void UGameClockSubsystem::HandleServerTime(int64 ServerUnix)
{
	ServerUnixAtSync = ServerUnix;
	PlatformSecondsAtSync = FPlatformTime::Seconds();
	bSynced = true;
	UE_LOG(LogGameClock, Log, TEXT("synced unix=%lld day=%s week=%s"), ServerUnix, *GetDayKey(), *GetWeekKey());
	Poll();
}

void UGameClockSubsystem::Poll()
{
	const bool bValid = IsServerTimeValid();
	if (bValid != bLastValidity)
	{
		bLastValidity = bValid;
		OnServerTimeValidityChanged.Broadcast(bValid);
	}
	BroadcastIfKeysChanged();
}

void UGameClockSubsystem::BroadcastIfKeysChanged()
{
	// 첫 호출은 기준선만 잡는다 — 로드 직후 "키가 바뀌었다"는 거짓 방송을 막기 위해서다 (구독자는 로드 시 자기 저장 키와 비교한다)
	const FString DayKey = GetDayKey();
	const FString WeekKey = GetWeekKey();
	const bool bFirst = LastDayKey.IsEmpty();
	const bool bDayChanged = !bFirst && DayKey != LastDayKey;
	const bool bWeekChanged = !bFirst && WeekKey != LastWeekKey;
	LastDayKey = DayKey;
	LastWeekKey = WeekKey;
	if (bDayChanged)
	{
		UE_LOG(LogGameClock, Log, TEXT("day rolled -> %s"), *DayKey);
		OnDayKeyChanged.Broadcast(DayKey);
	}
	if (bWeekChanged)
	{
		UE_LOG(LogGameClock, Log, TEXT("week rolled -> %s"), *WeekKey);
		OnWeekKeyChanged.Broadcast(WeekKey);
	}
}
