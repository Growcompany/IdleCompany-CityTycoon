#include "Manager/MailboxSubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/MailboxRules.h"
#include "Data/RewardGrant.h"

DEFINE_LOG_CATEGORY_STATIC(LogMailbox, Log, All);

void UMailboxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USaveLoadManager>();
	Collection.InitializeDependency<UGameClockSubsystem>();
	Collection.InitializeDependency<UResourceItemManager>();
	Collection.InitializeDependency<UItemInventoryManager>();
	Collection.InitializeDependency<UQuestSubsystem>();

	UGameInstance* GI = GetGameInstance();
	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UMailboxSubsystem::HandleGameDataLoaded);
	}
	if (UQuestSubsystem* QuestMgr = GI->GetSubsystem<UQuestSubsystem>())
	{
		QuestMgr->OnQuestRewardsExpired.AddUObject(this, &UMailboxSubsystem::HandleQuestRewardsExpired);
	}
	if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnServerTimeValidityChanged.AddUObject(this, &UMailboxSubsystem::HandleServerTimeValidityChanged);
	}
}

void UMailboxSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>()) SaveMgr->OnGameDataLoaded.RemoveAll(this);
		if (UQuestSubsystem* QuestMgr = GI->GetSubsystem<UQuestSubsystem>()) QuestMgr->OnQuestRewardsExpired.RemoveAll(this);
		if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>()) Clock->OnServerTimeValidityChanged.RemoveAll(this);
	}
	Super::Deinitialize();
}

int64 UMailboxSubsystem::NowUnix() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	return Clock ? Clock->GetServerNowUnix() : FDateTime::UtcNow().ToUnixTimestamp();
}

void UMailboxSubsystem::HandleGameDataLoaded()
{
	// 세이브 핸드오프는 SetMailboxData 하나뿐 — 여기서 다시 읽으면 같은 방송의 앞선 쓰기(퀘스트 만료 메일)를 덮는다
	if (IsClockAuthoritative())
	{
		// 로드 직후 저장 금지(BACKEND_PLAYBOOK §2.6) — 만료분은 메모리에서만 정리, 다음 저장 트리거에 실린다
		FMailboxRules::PurgeExpired(Save, NowUnix());
	}
	OnMailboxChanged.Broadcast();
}

// 권위 시각이 붙은 순간에야 만료 판정이 가능해진다 — 그때까지 미뤄둔 정리를 여기서 한다
void UMailboxSubsystem::HandleServerTimeValidityChanged(bool bValid)
{
	if (bValid) PurgeExpiredNow();
}

void UMailboxSubsystem::HandleQuestRewardsExpired(const FText& Title, const TArray<FMissionReward>& Rewards)
{
	AddSystemMail(Title, FText::FromString(TEXT("받지 않은 보상을 우편으로 보냈습니다.")), Rewards, 7);
}

void UMailboxSubsystem::AddSystemMail(const FText& Title, const FText& Body, const TArray<FMissionReward>& Rewards, int32 ExpireDays)
{
	if (Rewards.Num() == 0) return;
	if (!IsClockAuthoritative())
	{
		UE_LOG(LogMailbox, Warning, TEXT("system mail dropped (server time invalid): %s"), *Title.ToString());
		return;
	}
	for (const FMissionReward& R : Rewards)
	{
		// 데이터/호출부 실수를 투입 시점에 크게 알린다 (스펙 §0.5)
		ensureMsgf(R.ResourceType != EResourceType::Money, TEXT("AddSystemMail %s: Money reward is forbidden"), *Title.ToString());
	}
	FMailEntry E;
	E.Id = FGuid::NewGuid();
	E.Title = Title;
	E.Body = Body;
	E.Rewards = Rewards;
	E.ReceivedUnix = NowUnix();
	E.ExpireUnix = E.ReceivedUnix + static_cast<int64>(FMath::Max(1, ExpireDays)) * 86400;
	FMailboxRules::Push(Save, E);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
	UE_LOG(LogMailbox, Log, TEXT("system mail: %s (%d rewards)"), *Title.ToString(), Rewards.Num());
	OnMailboxChanged.Broadcast();
}

bool UMailboxSubsystem::TryDeliverGlobalMail(const FString& SourceKey, const FText& Title, const FText& Body, const TArray<FMissionReward>& Rewards, int32 ExpireDays)
{
	if (!FMailboxRules::CanReceiveGlobal(Save, SourceKey)) return false;
	if (!IsClockAuthoritative())
	{
		UE_LOG(LogMailbox, Warning, TEXT("global mail dropped (server time invalid): %s"), *SourceKey);
		return false;
	}
	for (const FMissionReward& R : Rewards)
	{
		// 데이터/호출부 실수를 투입 시점에 크게 알린다 (스펙 §0.5)
		ensureMsgf(R.ResourceType != EResourceType::Money, TEXT("TryDeliverGlobalMail %s: Money reward is forbidden"), *SourceKey);
	}
	FMailEntry E;
	E.Id = FGuid::NewGuid();
	E.SourceKey = SourceKey;
	E.Title = Title;
	E.Body = Body;
	E.Rewards = Rewards;
	E.ReceivedUnix = NowUnix();
	E.ExpireUnix = E.ReceivedUnix + static_cast<int64>(FMath::Max(1, ExpireDays)) * 86400;
	FMailboxRules::Push(Save, E);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
	UE_LOG(LogMailbox, Log, TEXT("global mail: %s"), *SourceKey);
	OnMailboxChanged.Broadcast();
	return true;
}

void UMailboxSubsystem::PurgeExpiredNow()
{
	// 권위 시각이 없으면 로컬 UTC 폴백으로 멀쩡한 메일을 지울 수 있다 — 아무것도 지우지 않는다
	if (!IsClockAuthoritative()) return;
	if (FMailboxRules::PurgeExpired(Save, NowUnix()) > 0)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->RequestDeferredSave();
		OnMailboxChanged.Broadcast();
	}
}

bool UMailboxSubsystem::IsClockAuthoritative() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	return Clock && Clock->IsServerTimeValid();
}

bool UMailboxSubsystem::CheckClaimableOrNotify() const
{
	if (IsClockAuthoritative()) return true;
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->ShowRejectNotification(FText::FromString(TEXT("서버 시간을 확인하는 중입니다")), 0.4f, ENotificationType::Warning);
	}
	return false;
}

bool UMailboxSubsystem::ClaimMail(const FGuid& Id)
{
	if (!CheckClaimableOrNotify()) return false;
	const FMailEntry* Found = FMailboxRules::FindMail(Save, Id);
	if (!Found)
	{
		UE_LOG(LogMailbox, Log, TEXT("claim rejected — 우편 없음 %s"), *Id.ToString());
		return false;
	}
	if (FMailboxRules::IsExpired(*Found, NowUnix()))
	{
		UE_LOG(LogMailbox, Log, TEXT("claim rejected — 만료됨 %s"), *Found->Title.ToString());
		return false;
	}
	// 배열에서 지우면 Found 가 죽으므로 먼저 복사한다
	const FMailEntry Copy = *Found;
	FRewardGrant::Commit(GetGameInstance(), Copy.Rewards);
	Save.Mails.RemoveAll([&Id](const FMailEntry& E) { return E.Id == Id; });
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UIMgr->ShowRewardToast(Copy.Rewards, Copy.Title, true);
	UE_LOG(LogMailbox, Log, TEXT("claimed mail: %s"), *Copy.Title.ToString());
	OnMailboxChanged.Broadcast();
	return true;
}

int32 UMailboxSubsystem::ClaimAll()
{
	if (!CheckClaimableOrNotify())
	{
		UE_LOG(LogMailbox, Log, TEXT("claim all rejected — 서버 시간 무효"));
		return 0;
	}
	// 아래 Reset() 이 통째로 비우므로, 건너뛴 만료분이 남아 있으면 조용히 삼켜진다 — 먼저 지운다(저장·방송까지)
	PurgeExpiredNow();
	const int64 Now = NowUnix();
	TArray<FMissionReward> Merged;
	int32 Count = 0;
	for (const FMailEntry& E : Save.Mails)
	{
		if (FMailboxRules::IsExpired(E, Now)) continue;
		Merged.Append(E.Rewards);
		++Count;
	}
	if (Count == 0)
	{
		UE_LOG(LogMailbox, Log, TEXT("claim all rejected — 수령 가능한 우편 없음 (보유 %d)"), Save.Mails.Num());
		return 0;
	}
	FRewardGrant::Commit(GetGameInstance(), Merged);
	Save.Mails.Reset();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UIMgr->ShowRewardToast(Merged, FText::FromString(TEXT("우편 보상")), true);
	UE_LOG(LogMailbox, Log, TEXT("claimed all: %d mails"), Count);
	OnMailboxChanged.Broadcast();
	return Count;
}

void UMailboxSubsystem::DevClear()
{
	Save = FMailboxSaveData();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->SaveGameData();
	OnMailboxChanged.Broadcast();
}
