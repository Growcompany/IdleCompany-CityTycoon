#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameClockSubsystem.generated.h"

/**
 * 공용 게임 시계 — PlayFab 서버 시간의 유일한 구독자. KST 하루/주 키를 소유하고 바뀔 때만 방송한다.
 * 상점 리셋·출석·일일/주간 퀘스트·이벤트 활성 판정이 전부 이 시계를 본다 (로컬 기기 시간은 UI 폴백 외 사용 금지).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGameClockSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDayKeyChanged, const FString& /*NewDayKey*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeekKeyChanged, const FString& /*NewWeekKey*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnServerTimeValidityChanged, bool /*bValid*/);

	FOnDayKeyChanged OnDayKeyChanged;
	FOnWeekKeyChanged OnWeekKeyChanged;
	FOnServerTimeValidityChanged OnServerTimeValidityChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 서버 모드: 동기 1회 이상 && 마지막 동기 24h 이내. 개발 로컬 모드: 기기 UTC 사용 가능.
	UFUNCTION(BlueprintPure, Category = "GameClock")
	bool IsServerTimeValid() const;

	// 유효하면 서버 시각 외삽, 아니면 로컬 UTC (호출부가 IsServerTimeValid 로 권위 여부를 따로 판단)
	UFUNCTION(BlueprintPure, Category = "GameClock")
	int64 GetServerNowUnix() const;

	UFUNCTION(BlueprintPure, Category = "GameClock")
	FString GetDayKey() const;

	UFUNCTION(BlueprintPure, Category = "GameClock")
	FString GetWeekKey() const;

	UFUNCTION(BlueprintPure, Category = "GameClock")
	int64 GetSecondsUntilDayRoll() const;

	UFUNCTION(BlueprintPure, Category = "GameClock")
	int64 GetSecondsUntilWeekRoll() const;

	int32 GetHourKst() const;

	// 치트 전용: 서버 시각에 더하는 오프셋(누적). Shipping 에서는 무시
	void SetDebugOffsetSeconds(int64 InOffset);
	int64 GetDebugOffsetSeconds() const { return DebugOffsetSeconds; }
	bool HasSyncedOnce() const { return bSynced; }
	int64 GetLastSyncUnix() const { return ServerUnixAtSync; }

private:
	void HandleServerTime(int64 ServerUnix);
	void Poll();
	void BroadcastIfKeysChanged();

	bool bSynced = false;
	bool bLastValidity = false;
	int64 ServerUnixAtSync = 0;
	double PlatformSecondsAtSync = 0.0;
	int64 DebugOffsetSeconds = 0;
	FString LastDayKey;
	FString LastWeekKey;
	FTimerHandle PollHandle;

	static constexpr float PollIntervalSeconds = 15.0f;
	static constexpr int64 SyncStaleSeconds = 24 * 3600;
};
