#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Enum/QuestTypes.h"
#include "Table/QuestTable.h"
#include "Data/QuestSaveData.h"
#include "Data/QuestRules.h"
#include "QuestSubsystem.generated.h"

struct FGachaResultData;
struct FBuildingTraitGachaResult;
struct FBuildingSkinGachaResult;

/**
 * 퀘스트 원장 — 일일/주간/업적 한 원장. 진행은 ReportCounter 단일 버스, 정의는 DT_Quest, 하루/주 경계는 UGameClockSubsystem.
 * 미션판(GoalBoard)과 무관하다. 롤오버로 만료된 보상은 OnQuestRewardsExpired 로 내보내며 우편함이 구독한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnQuestBoardChanged);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCounterReported, EQuestCounter, int64);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQuestRewardsExpired, const FText& /*Title*/, const TArray<FMissionReward>&);

	FOnQuestBoardChanged OnQuestBoardChanged;
	FOnCounterReported OnCounterReported;
	FOnQuestRewardsExpired OnQuestRewardsExpired;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 단일 진입 버스. 증분형만 누적하고, 어떤 카운터든 OnCounterReported 로 재방송(이벤트 시스템용)
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void ReportCounter(EQuestCounter Counter, int64 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool ClaimQuest(FName QuestID);
	// 일괄 수령은 지급마다 토스트를 띄우지 않고 성공한 보상만 모아 한 번 표시한다.
	bool ClaimQuestWithPresentation(FName QuestID, bool bShowRewardToast);

	// 받을 것 → 진행 중(진행률 높은 순) → 완료 순, 같은 순위는 DT 순서
	void GetEntries(EQuestPeriod Period, TArray<FQuestEntry>& OutEntries) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasClaimable(EQuestPeriod Period) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasAnyClaimable() const;

	// 받을 수 있는 미션 수(전 주기 합산, HasAnyClaimable 과 같은 기준) — 메인 우상단 [미션] 배지 숫자
	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetClaimableCount() const;

	// SaveLoadManager 핸드오프 (평구조체)
	const FQuestSaveData& GetQuestData() const { return Save; }
	void SetQuestData(const FQuestSaveData& InData) { Save = InData; }

	// 치트: 전부 초기화 + 저장
	void DevReset();

private:
	void HandleGameDataLoaded();
	void HandleDayKeyChanged(const FString& NewDayKey);
	void HandleWeekKeyChanged(const FString& NewWeekKey);
	void HandleServerTimeValidityChanged(bool bValid);
	void HandleRecruitGacha(const FGachaResultData& Result);
	void HandleTraitGacha(const FBuildingTraitGachaResult& Result);
	void HandleSkinGacha(const FBuildingSkinGachaResult& Result);

	// 저장 키와 시계 키가 다르면 롤오버 (로드 직후 · 키 변경 방송 · 수령 직전)
	bool RolloverIfNeeded(bool bShouldSave);
	void EmitExpired(const FText& Title, const TArray<FMissionReward>& Rewards);
	int64 ReadAbsolute(EQuestCounter Counter) const;
	int64 CurrentFor(FName QuestID, const FQuestTable& Row) const;
	// 수령 가능하면 true, 아니면 토스트를 띄우고 false
	bool CheckClaimableOrNotify() const;
	void SaveNow();

	FQuestSaveData Save;
	// 서버 시각이 세이브 로드보다 먼저 도착할 수 있다 — 기본값 Save 로 롤오버·저장하면 실제 세이브를 덮는다
	bool bLoaded = false;
};
