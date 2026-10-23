#include "Manager/AttendanceSubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/AttendanceRules.h"
#include "Table/AttendanceTable.h"
#include "Data/RewardGrant.h"

DEFINE_LOG_CATEGORY_STATIC(LogAttendance, Log, All);

void UAttendanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UGameClockSubsystem>();
	Collection.InitializeDependency<UResourceItemManager>();
	Collection.InitializeDependency<UItemInventoryManager>();

	UGameInstance* GI = GetGameInstance();
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UAttendanceSubsystem::HandleGameDataLoaded);
	}
	if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnDayKeyChanged.AddUObject(this, &UAttendanceSubsystem::HandleDayKeyChanged);
		Clock->OnServerTimeValidityChanged.AddUObject(this, &UAttendanceSubsystem::HandleServerTimeValidityChanged);
	}
}

void UAttendanceSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->OnGameDataLoaded.RemoveAll(this);
		if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>()) { Clock->OnDayKeyChanged.RemoveAll(this); Clock->OnServerTimeValidityChanged.RemoveAll(this); }
	}
	Super::Deinitialize();
}

void UAttendanceSubsystem::HandleGameDataLoaded()
{
	// 세이브 핸드오프는 SetAttendanceData 하나뿐 — 여기서 다시 읽으면 스테일 캐시로 같은 방송의 앞선 쓰기를 덮는다
	bLoaded = true;
	OnAttendanceChanged.Broadcast();
}

// 날짜가 넘어가면 수령 가능 상태가 바뀌므로 배지/패널을 다시 그리게 한다
void UAttendanceSubsystem::HandleDayKeyChanged(const FString&)
{
	OnAttendanceChanged.Broadcast();
}

// CanClaimToday 가 권위 시각을 요구한다 — 유효성이 뒤집히면 패널·도트를 깨워야 한다
void UAttendanceSubsystem::HandleServerTimeValidityChanged(bool)
{
	OnAttendanceChanged.Broadcast();
}

bool UAttendanceSubsystem::CanClaimToday() const
{
	// 세이브 로드 전의 기본값 Save 로 판정하면 이미 받은 날도 "받을 수 있음" 으로 보인다 (ClaimToday 와 같은 가드)
	if (!bLoaded) return false;
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	return Clock && Clock->IsServerTimeValid() && FAttendanceRules::CanClaim(Save, Clock->GetDayKey());
}

bool UAttendanceSubsystem::HasClaimedToday() const
{
	// 권위 시각이 없으면 "오늘"이 없다 — 로컬 시계로 판정하면 완료 표시가 거짓이 된다
	if (!bLoaded) return false;
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	return Clock && Clock->IsServerTimeValid() && Save.LastClaimDayKey == Clock->GetDayKey();
}

int32 UAttendanceSubsystem::GetNextDay() const
{
	return FAttendanceRules::NextDay(Save);
}

bool UAttendanceSubsystem::ClaimToday()
{
	if (!bLoaded) return false;
	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	const UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>();
	if (!Clock || !Clock->IsServerTimeValid())
	{
		if (UIMgr) UIMgr->ShowRejectNotification(FText::FromString(TEXT("서버 시간을 확인하는 중입니다")), 0.4f, ENotificationType::Warning);
		return false;
	}

	const FString TodayKey = Clock->GetDayKey();
	if (!FAttendanceRules::CanClaim(Save, TodayKey))
	{
		if (UIMgr) UIMgr->ShowRejectNotification(FText::FromString(TEXT("오늘 출석 보상은 이미 받았습니다")), 0.4f, ENotificationType::Warning);
		return false;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const int32 TargetDay = FAttendanceRules::NextDay(Save);
	FAttendanceTable Row;
	if (!TableMgr || !TableMgr->GetAttendanceRow(TargetDay, Row))
	{
		// 행이 없으면 순환이 영영 못 넘어간다 — 조용히 죽지 말고 크게 알린다
		UE_LOG(LogAttendance, Warning, TEXT("DT_Attendance %d일차 행 없음 — 수령 불가"), TargetDay);
		if (UIMgr) UIMgr->ShowRejectNotification(FText::FromString(TEXT("출석 보상 정보를 불러오지 못했습니다")), 0.4f, ENotificationType::Warning);
		return false;
	}

	FRewardGrant::Commit(GI, Row.Rewards);

	FAttendanceRules::ApplyClaim(Save, TodayKey);
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	if (UIMgr) UIMgr->ShowRewardToast(Row.Rewards, FText::FromString(FString::Printf(TEXT("%d일차 출석 보상"), Save.CycleDay)), true);
	UE_LOG(LogAttendance, Log, TEXT("claimed day=%d total=%d key=%s"), Save.CycleDay, Save.TotalDays, *TodayKey);
	OnAttendanceChanged.Broadcast();
	return true;
}

void UAttendanceSubsystem::DevReset()
{
	Save = FAttendanceSaveData();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	OnAttendanceChanged.Broadcast();
}
