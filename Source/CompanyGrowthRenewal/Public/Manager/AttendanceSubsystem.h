#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/AttendanceSaveData.h"
#include "AttendanceSubsystem.generated.h"

/**
 * 28일 순환 출석. 하루 1회 수령, 놓친 날은 건너뛰지 않는다. 서버 시간이 유효할 때만 수령 가능하다.
 * 보상 정의는 DT_Attendance, 하루 경계는 UGameClockSubsystem.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAttendanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnAttendanceChanged);
	FOnAttendanceChanged OnAttendanceChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Attendance")
	bool CanClaimToday() const;

	UFUNCTION(BlueprintPure, Category = "Attendance")
	bool HasClaimedToday() const;

	UFUNCTION(BlueprintPure, Category = "Attendance")
	int32 GetNextDay() const;

	UFUNCTION(BlueprintPure, Category = "Attendance")
	int32 GetCycleDay() const { return Save.CycleDay; }

	UFUNCTION(BlueprintPure, Category = "Attendance")
	int32 GetTotalDays() const { return Save.TotalDays; }

	UFUNCTION(BlueprintCallable, Category = "Attendance")
	bool ClaimToday();

	// SaveLoadManager 핸드오프 (평구조체)
	const FAttendanceSaveData& GetAttendanceData() const { return Save; }
	void SetAttendanceData(const FAttendanceSaveData& InData) { Save = InData; }

	// 치트: 출석 이력 초기화 + 저장
	void DevReset();

private:
	void HandleGameDataLoaded();
	void HandleDayKeyChanged(const FString& NewDayKey);
	void HandleServerTimeValidityChanged(bool bValid);

	FAttendanceSaveData Save;
	// 서버 시각이 세이브 로드보다 먼저 도착할 수 있다 — 기본값 Save 로 수령하면 실제 세이브를 덮는다
	bool bLoaded = false;
};
