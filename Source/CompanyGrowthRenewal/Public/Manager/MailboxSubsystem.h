#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/MailboxSaveData.h"
#include "MailboxSubsystem.generated.h"

/**
 * 우편함 — 시스템 메일(퀘스트/이벤트 만료 보상)과 글로벌 메일(SourceKey 당 1회 수신)을 한 곳에 담는다.
 * 수령하면 즉시 목록에서 사라지고, 만료 정리는 우편함을 열 때 한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMailboxSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnMailboxChanged);
	FOnMailboxChanged OnMailboxChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void AddSystemMail(const FText& Title, const FText& Body, const TArray<FMissionReward>& Rewards, int32 ExpireDays = 7);
	// 같은 SourceKey 는 한 번만 들어온다 (운영 일괄 지급용). 이미 받았으면 false
	bool TryDeliverGlobalMail(const FString& SourceKey, const FText& Title, const FText& Body, const TArray<FMissionReward>& Rewards, int32 ExpireDays);

	const TArray<FMailEntry>& GetMails() const { return Save.Mails; }

	UFUNCTION(BlueprintPure, Category = "Mailbox")
	int32 GetUnclaimedCount() const { return Save.Mails.Num(); }

	bool ClaimMail(const FGuid& Id);

	UFUNCTION(BlueprintCallable, Category = "Mailbox")
	int32 ClaimAll();

	// SaveLoadManager 핸드오프 (평구조체)
	const FMailboxSaveData& GetMailboxData() const { return Save; }
	void SetMailboxData(const FMailboxSaveData& InData) { Save = InData; }

	// 치트: 우편함 비우기 + 저장
	void DevClear();

	// 우편함을 열 때 호출 — 만료분을 정리하고 변경이 있으면 저장·방송
	void PurgeExpiredNow();

private:
	void HandleGameDataLoaded();
	void HandleQuestRewardsExpired(const FText& Title, const TArray<FMissionReward>& Rewards);
	void HandleServerTimeValidityChanged(bool bValid);
	// 수령 가능하면 true, 아니면 토스트를 띄우고 false
	bool CheckClaimableOrNotify() const;
	// 권위 시각 여부 — 만료 판정과 수신 시각 기록은 이게 참일 때만 한다 (스펙 D1)
	bool IsClockAuthoritative() const;
	int64 NowUnix() const;

	FMailboxSaveData Save;
};
