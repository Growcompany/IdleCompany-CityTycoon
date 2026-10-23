#include "Manager/LiveOpsSubsystem.h"
#include "Global/CGDevSettings.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/MailboxSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/LiveOpsRules.h"
#include "Data/RewardGrant.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogLiveOps, Log, All);

namespace
{
	const TCHAR* const TitleKeyEvents = TEXT("LiveOps.Events");
	const TCHAR* const TitleKeyNotices = TEXT("LiveOps.Notices");
	const TCHAR* const TitleKeyMails = TEXT("LiveOps.Mails");

	// 키가 있고 파싱·검증에 성공할 때만 통째 교체 — 빈 배열도 교체다(운영이 전부 내린 상태)
	template <typename T>
	void ApplyTitleKey(const TMap<FString, FString>& Data, const TCHAR* Key, TArray<T>& Out, bool (*Validate)(const TArray<T>&, FString&))
	{
		const FString* Json = Data.Find(Key);
		if (!Json) return;
		TArray<T> Parsed;
		if (!FLiveOpsRules::ParseArray(*Json, Parsed))
		{
			UE_LOG(LogLiveOps, Warning, TEXT("TitleData %s 파싱 실패 — 기존 정의를 유지합니다"), Key);
			return;
		}
		FString Error;
		if (!Validate(Parsed, Error))
		{
			UE_LOG(LogLiveOps, Warning, TEXT("TitleData %s 검증 실패(%s) — 기존 정의를 유지합니다"), Key, *Error);
			return;
		}
		Out = MoveTemp(Parsed);
		UE_LOG(LogLiveOps, Log, TEXT("TitleData %s 적용 %d건"), Key, Out.Num());
	}
}

void ULiveOpsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UGameClockSubsystem>();
	Collection.InitializeDependency<UQuestSubsystem>();
	Collection.InitializeDependency<UAttendanceSubsystem>();
	Collection.InitializeDependency<UMailboxSubsystem>();
	Collection.InitializeDependency<UPlayFabManagerSubsystem>();

	UGameInstance* GI = GetGameInstance();
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &ULiveOpsSubsystem::HandleGameDataLoaded);
	}
	if (UQuestSubsystem* QuestMgr = GI->GetSubsystem<UQuestSubsystem>())
	{
		QuestMgr->OnCounterReported.AddUObject(this, &ULiveOpsSubsystem::HandleCounter);
	}
	if (UAttendanceSubsystem* Attendance = GI->GetSubsystem<UAttendanceSubsystem>())
	{
		Attendance->OnAttendanceChanged.AddUObject(this, &ULiveOpsSubsystem::HandleAttendanceChanged);
	}
	if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnDayKeyChanged.AddUObject(this, &ULiveOpsSubsystem::HandleDayKey);
		Clock->OnServerTimeValidityChanged.AddUObject(this, &ULiveOpsSubsystem::HandleServerTimeValidityChanged);
	}
	if (UPlayFabManagerSubsystem* PlayFabMgr = GI->GetSubsystem<UPlayFabManagerSubsystem>())
	{
		PlayFabMgr->OnLoginComplete.AddUObject(this, &ULiveOpsSubsystem::HandleLogin);
		PlayFabMgr->OnTitleDataReceived.AddUObject(this, &ULiveOpsSubsystem::HandleTitleData);
	}

	LoadFromTables();
	// 활성 전환·핫타임 시각 경계는 초 단위로 정확할 필요가 없다 — 15초 폴링은 시계가 이미 한다
	GI->GetTimerManager().SetTimer(
		ContentTimerHandle, FTimerDelegate::CreateUObject(this, &ULiveOpsSubsystem::HandleMinuteTick), ContentTickSeconds, true);
}

void ULiveOpsSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(ContentTimerHandle);
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->OnGameDataLoaded.RemoveAll(this);
		if (UQuestSubsystem* QuestMgr = GI->GetSubsystem<UQuestSubsystem>()) QuestMgr->OnCounterReported.RemoveAll(this);
		if (UAttendanceSubsystem* Attendance = GI->GetSubsystem<UAttendanceSubsystem>()) Attendance->OnAttendanceChanged.RemoveAll(this);
		if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnDayKeyChanged.RemoveAll(this);
			Clock->OnServerTimeValidityChanged.RemoveAll(this);
		}
		if (UPlayFabManagerSubsystem* PlayFabMgr = GI->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			PlayFabMgr->OnLoginComplete.RemoveAll(this);
			PlayFabMgr->OnTitleDataReceived.RemoveAll(this);
		}
	}
	Super::Deinitialize();
}

void ULiveOpsSubsystem::LoadFromTables()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;
	Events = TableMgr->GetLiveOpsEvents();
	Notices = TableMgr->GetLiveOpsNotices();
	Mails = TableMgr->GetLiveOpsMails();
}

void ULiveOpsSubsystem::HandleGameDataLoaded()
{
	// 세이브 핸드오프는 SetLiveOpsData 하나뿐 — 여기서 다시 읽으면 같은 방송의 앞선 쓰기를 덮는다
	bLoaded = true;
	if (!ApplyContent()) OnLiveOpsContentChanged.Broadcast();
}

// 출석 수령은 출석 세이브만 바꾸므로 누적 출석 이벤트 진행은 여기서만 다시 그려진다 (로드 프레임의 중복 방송은 버린다)
void ULiveOpsSubsystem::HandleAttendanceChanged() { if (bLoaded) OnLiveOpsContentChanged.Broadcast(); }
void ULiveOpsSubsystem::HandleDayKey(const FString&) { ApplyContent(); }
// 권위 시각을 잃으면 다음 1분 틱을 기다리지 않고 배율을 바로 1.0 으로 내린다
void ULiveOpsSubsystem::HandleServerTimeValidityChanged(bool bValid) { if (bValid) ApplyContent(); else RecomputeHotTime(); }
void ULiveOpsSubsystem::HandleLogin(bool bSuccess) { if (bSuccess) RefreshFromServer(/*bForce=*/false); }
void ULiveOpsSubsystem::HandleMinuteTick() { ApplyContent(); }

void ULiveOpsSubsystem::HandleTitleData(const TMap<FString, FString>& Data)
{
	if (UCGDevSettings::IsOfflineMode()) return;
	ApplyTitleKey(Data, TitleKeyEvents, Events, &FLiveOpsRules::ValidateEvents);
	ApplyTitleKey(Data, TitleKeyNotices, Notices, &FLiveOpsRules::ValidateNotices);
	ApplyTitleKey(Data, TitleKeyMails, Mails, &FLiveOpsRules::ValidateMails);
	if (!ApplyContent()) OnLiveOpsContentChanged.Broadcast();
}

void ULiveOpsSubsystem::RefreshFromServer(bool bForce)
{
	if (UCGDevSettings::IsOfflineMode()) return;
	const double Now = RefreshClockSeconds();
	const double Elapsed = Now - LastRefreshSeconds;
	// 기준이 서버↔플랫폼 시간으로 바뀌면 경과가 음수가 된다 — 그때는 스로틀을 풀어준다
	if (!bForce && LastRefreshSeconds > 0.0 && Elapsed >= 0.0 && Elapsed < RefreshThrottleSeconds) return;

	UPlayFabManagerSubsystem* PlayFabMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>();
	if (!PlayFabMgr) return;
	// 보내지도 못한 요청으로 스로틀을 찍으면 로그인 직후 10분간 콘텐츠가 비어 있다
	if (!PlayFabMgr->IsLoggedIn())
	{
		UE_LOG(LogLiveOps, Verbose, TEXT("TitleData 요청 보류 — 로그인 전"));
		return;
	}
	LastRefreshSeconds = Now;
	PlayFabMgr->RequestTitleData({ FString(TitleKeyEvents), FString(TitleKeyNotices), FString(TitleKeyMails) });
}

double ULiveOpsSubsystem::RefreshClockSeconds() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	if (Clock && Clock->IsServerTimeValid()) return static_cast<double>(Clock->GetServerNowUnix());
	return FPlatformTime::Seconds();
}

bool ULiveOpsSubsystem::ApplyContent()
{
	// 조기 반환 경로에서도 배율은 최신이어야 한다 (시각이 무효면 안에서 1.0 으로 내린다)
	RecomputeHotTime();
	if (!bLoaded || !IsClockAuthoritative()) return false;

	const int64 Now = NowUnix();
	UGameInstance* GI = GetGameInstance();
	UMailboxSubsystem* Mailbox = GI->GetSubsystem<UMailboxSubsystem>();
	const UAttendanceSubsystem* Attendance = GI->GetSubsystem<UAttendanceSubsystem>();
	bool bChanged = false;

	if (Mailbox)
	{
		for (const FLiveOpsMailDef& Def : Mails)
		{
			if (Def.Id.IsEmpty() || !FLiveOpsRules::IsActive(Def, Now)) continue;
			Mailbox->TryDeliverGlobalMail(Def.Id, Def.Title, Def.Body, Def.Rewards, Def.ExpireDays);
		}
	}

	for (const FLiveOpsEventDef& Def : Events)
	{
		if (Def.Id.IsEmpty()) continue;
		if (FLiveOpsRules::IsActive(Def, Now))
		{
			// 활성 중 한 번이라도 접속했다는 기록 — 종료 회수가 이 항목을 보고 돈다
			if (!Save.Events.Contains(Def.Id))
			{
				Save.Events.Add(Def.Id);
				bChanged = true;
			}
			FLiveOpsEventProgress& Progress = Save.Events[Def.Id];
			if (Def.Type == ELiveOpsEventType::AttendanceEvent && Progress.StartAttendanceTotal < 0)
			{
				Progress.StartAttendanceTotal = Attendance ? Attendance->GetTotalDays() : 0;
				bChanged = true;
			}
			continue;
		}

		// 우편함이 없으면 회수분이 증발하므로 표시도 남기지 않고 다음 틱에 다시 시도한다
		if (!Mailbox) continue;
		FLiveOpsEventProgress* Progress = Save.Events.Find(Def.Id);
		if (!Progress || Progress->bExpiredHandled) continue;
		TArray<FMissionReward> Unclaimed;
		FLiveOpsRules::ExpiredMilestoneRewards(EffectiveProgress(Def), Def, Unclaimed);
		if (Unclaimed.Num() > 0)
		{
			Mailbox->AddSystemMail(
				FText::Format(FText::FromString(TEXT("종료된 이벤트 보상 회수 ― {0}")), Def.Title),
				FText::FromString(TEXT("받지 않은 이벤트 보상을 우편으로 보냈습니다.")), Unclaimed, 7);
		}
		Progress->bExpiredHandled = true;
		bChanged = true;
		UE_LOG(LogLiveOps, Log, TEXT("이벤트 종료 회수 %s rewards=%d"), *Def.Id, Unclaimed.Num());
	}

	if (bChanged)
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
		OnLiveOpsContentChanged.Broadcast();
	}
	return bChanged;
}

void ULiveOpsSubsystem::RecomputeHotTime()
{
	float Multiplier = 1.0f;
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	// 로컬 기기 시각으로 핫타임을 열어주면 안 된다 (스펙 D1)
	if (Clock && Clock->IsServerTimeValid())
	{
		const int64 Now = Clock->GetServerNowUnix();
		const int32 HourKst = Clock->GetHourKst();
		for (const FLiveOpsEventDef& Def : Events)
		{
			if (Def.Type != ELiveOpsEventType::HotTimeEvent || !FLiveOpsRules::IsActive(Def, Now)) continue;
			Multiplier *= FLiveOpsRules::HotTimeMultiplier(Def, HourKst);
		}
	}
	if (!FMath::IsNearlyEqual(Multiplier, CachedRevenueMultiplier))
	{
		CachedRevenueMultiplier = Multiplier;
		UE_LOG(LogLiveOps, Log, TEXT("핫타임 배율 %.2f"), Multiplier);
		OnHotTimeChanged.Broadcast(Multiplier);
	}
}

void ULiveOpsSubsystem::HandleCounter(EQuestCounter Counter, int64 Amount)
{
	if (!bLoaded || Counter == EQuestCounter::None || Amount <= 0) return;
	// 활성 창 판정이 권위 시각을 요구한다 — 로컬 시각으로 점수를 쌓지 않는다
	if (!IsClockAuthoritative()) return;

	const int64 Now = NowUnix();
	bool bChanged = false;
	for (const FLiveOpsEventDef& Def : Events)
	{
		if (Def.Type != ELiveOpsEventType::CollectEvent || Def.Counter != Counter) continue;
		if (Def.Id.IsEmpty() || !FLiveOpsRules::IsActive(Def, Now)) continue;
		Save.Events.FindOrAdd(Def.Id).Points += Amount;
		bChanged = true;
	}
	if (bChanged)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
		OnLiveOpsContentChanged.Broadcast();
	}
}

const FLiveOpsEventDef* ULiveOpsSubsystem::FindEvent(const FString& Id) const
{
	return Events.FindByPredicate([&Id](const FLiveOpsEventDef& Def) { return Def.Id == Id; });
}

FLiveOpsEventProgress ULiveOpsSubsystem::EffectiveProgress(const FLiveOpsEventDef& Def) const
{
	FLiveOpsEventProgress Progress;
	if (const FLiveOpsEventProgress* Stored = Save.Events.Find(Def.Id)) Progress = *Stored;
	if (Def.Type == ELiveOpsEventType::AttendanceEvent)
	{
		const UAttendanceSubsystem* Attendance = GetGameInstance()->GetSubsystem<UAttendanceSubsystem>();
		const int32 Total = Attendance ? Attendance->GetTotalDays() : 0;
		Progress.Points = Progress.StartAttendanceTotal < 0 ? 0 : FMath::Max(0, Total - Progress.StartAttendanceTotal);
	}
	return Progress;
}

FLiveOpsEventProgress ULiveOpsSubsystem::GetEventProgress(const FString& Id) const
{
	const FLiveOpsEventDef* Def = FindEvent(Id);
	return Def ? EffectiveProgress(*Def) : FLiveOpsEventProgress();
}

void ULiveOpsSubsystem::GetActiveEvents(TArray<FLiveOpsEventDef>& OutEvents) const
{
	OutEvents.Reset();
	// 목록 표시는 권위 시각이 없으면 로컬 폴백을 쓴다 — 수령·지급 경로는 따로 막혀 있다
	const int64 Now = NowUnix();
	for (const FLiveOpsEventDef& Def : Events)
	{
		if (!Def.Id.IsEmpty() && FLiveOpsRules::IsActive(Def, Now)) OutEvents.Add(Def);
	}
}

bool ULiveOpsSubsystem::HasClaimableEvent() const
{
	// 알림 도트는 실제로 수령이 되는 상태에서만 — 권위 시각이 없으면 수령이 막힌다
	if (!IsClockAuthoritative()) return false;
	TArray<FLiveOpsEventDef> Active;
	GetActiveEvents(Active);
	for (const FLiveOpsEventDef& Def : Active)
	{
		const FLiveOpsEventProgress Progress = EffectiveProgress(Def);
		for (int32 Index = 0; Index < Def.Milestones.Num(); ++Index)
		{
			if (FLiveOpsRules::IsMilestoneClaimable(Progress, Def, Index)) return true;
		}
	}
	return false;
}

bool ULiveOpsSubsystem::ClaimEventMilestone(const FString& Id, int32 Index)
{
	if (!bLoaded || !CheckClaimableOrNotify()) return false;
	const FLiveOpsEventDef* Def = FindEvent(Id);
	if (!Def) return false;

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!FLiveOpsRules::IsActive(*Def, NowUnix()))
	{
		if (UIMgr) UIMgr->ShowRejectNotification(FText::FromString(TEXT("이미 종료된 이벤트입니다")), 0.4f, ENotificationType::Warning);
		return false;
	}
	if (!FLiveOpsRules::IsMilestoneClaimable(EffectiveProgress(*Def), *Def, Index))
	{
		if (UIMgr) UIMgr->ShowRejectNotification(FText::FromString(TEXT("아직 달성하지 않았습니다")), 0.4f, ENotificationType::Warning);
		return false;
	}

	const TArray<FMissionReward>& Rewards = Def->Milestones[Index].Rewards;
	FRewardGrant::Commit(GetGameInstance(), Rewards);
	Save.Events.FindOrAdd(Id).ClaimedMilestones.AddUnique(Index);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	if (UIMgr) UIMgr->ShowRewardToast(Rewards, Def->Title, true);
	UE_LOG(LogLiveOps, Log, TEXT("마일스톤 수령 %s[%d]"), *Id, Index);
	OnLiveOpsContentChanged.Broadcast();
	return true;
}

void ULiveOpsSubsystem::GetNotices(TArray<FLiveOpsNoticeDef>& OutNotices) const
{
	OutNotices = Notices;
	OutNotices.StableSort([](const FLiveOpsNoticeDef& A, const FLiveOpsNoticeDef& B)
	{
		if (A.bPinned != B.bPinned) return A.bPinned;
		return A.PostedUnix > B.PostedUnix;
	});
}

bool ULiveOpsSubsystem::IsNoticeRead(const FString& Id) const { return Save.ReadNoticeIds.Contains(Id); }

void ULiveOpsSubsystem::MarkNoticeRead(const FString& Id)
{
	if (!bLoaded || Id.IsEmpty() || Save.ReadNoticeIds.Contains(Id)) return;
	Save.ReadNoticeIds.AddUnique(Id);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
	OnLiveOpsContentChanged.Broadcast();
}

bool ULiveOpsSubsystem::HasUnreadNotice() const
{
	for (const FLiveOpsNoticeDef& Def : Notices)
	{
		if (!Def.Id.IsEmpty() && !Save.ReadNoticeIds.Contains(Def.Id)) return true;
	}
	return false;
}

int64 ULiveOpsSubsystem::NowUnix() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	return Clock ? Clock->GetServerNowUnix() : FDateTime::UtcNow().ToUnixTimestamp();
}

bool ULiveOpsSubsystem::IsClockAuthoritative() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	return Clock && Clock->IsServerTimeValid();
}

bool ULiveOpsSubsystem::CheckClaimableOrNotify() const
{
	if (IsClockAuthoritative()) return true;
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowRejectNotification(FText::FromString(TEXT("서버 시간을 확인하는 중입니다")), 0.4f, ENotificationType::Warning);
	}
	return false;
}

bool ULiveOpsSubsystem::DevAddEventPoints(const FString& Id, int64 Amount)
{
	if (!bLoaded) return false;
	const FLiveOpsEventDef* Def = FindEvent(Id);
	if (!Def || Def->Type != ELiveOpsEventType::CollectEvent || !FLiveOpsRules::IsActive(*Def, NowUnix())) return false;
	Save.Events.FindOrAdd(Id).Points += Amount;
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
	OnLiveOpsContentChanged.Broadcast();
	return true;
}
