#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Table/GoalTable.h"
#include "GoalBoardSubsystem.generated.h"

struct FGameSaveData;
class UTableManagerSubsystem;

enum class EGoalState : uint8 { Locked, InProgress, ReadyToClaim, Completed };

// 보드/트래커 UI 용 미션 스냅샷
struct FGoalBoardEntry
{
	FName GoalID;
	FGoalTable Row;
	EGoalState State = EGoalState::InProgress;
	int64 ProgressCurrent = 0;
	int64 ProgressTarget = 0;
	bool bRecommendable = true;
	bool bPracticed = false;
	int32 ClaimCount = 0;
};

DECLARE_MULTICAST_DELEGATE(FOnGoalBoardChanged);

// 추적 중인 미션이 바뀔 때 (NAME_None = 해제). 트래커 행 강조 + 가이드 오버레이가 구독
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTrackedGoalChanged, FName);

/**
 * 튜토리얼 이후 성장 안내. 실제 행동 신호와 도달 조건을 판정해 완료한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGoalBoardSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsUnlocked() const { return bUnlocked; }
	// 체인 종료(CompleteActiveMission) / 치트 — 언락 + 전수 재평가 + 세이브
	void UnlockBoard();
	// 피날레 원자적 커밋용 2단계 unlock. Prepare는 상태/평가만 수행하고, Publish는 UI 알림만 발행한다.
	bool PrepareUnlockForAtomicSave();
	void RollbackPreparedUnlock();
	void PublishPreparedUnlock();

	// DT_Goal 행 순서 스냅샷 (보드 리스트)
	void GetBoardEntries(TArray<FGoalBoardEntry>& OutEntries) const;
	EGoalState GetGoalState(FName GoalID, const FGoalTable& Row) const;

	// 준비된 성장 목표 한 건. 안내 시작 여부와 별개라 임의로 스포트라이트를 켜지 않는다.
	FName GetRecommendedGoalID() const;
	bool SetActiveGoal(FName GoalID);
	bool ClaimGoal(FName GoalID, int32 ExpectedClaimCount);
	int32 GetGuideBuildingIndex(FName GoalID) const;
	// 직접 따라가는 부서 채용 안내의 대상 회사에서만 제한한다. Count는 자유 채용이다.
	EProductionDiscipline GetGuidedRecruitDiscipline(int32 BuildingIndex) const;

	// 이 세션에는 선택한 안내를 다시 자동 추천하지 않는다.
	void DismissGoal(FName GoalID);

	// ===== 미션판 표시 상태 (전부 transient — 세이브 안 함) =====

	// 안내(가이드 점등) 중인 미션. NAME_None = 없음
	FName GetTrackedGoalID() const { return TrackedGoalID; }
	// 안내 대상 지정 — NAME_None 이면 해제. 진행 중인 안내만 지정한다.
	// 한 번에 1건만: 다른 미션을 지정하면 기존 추적은 자동으로 풀린다
	void SetTrackedGoal(FName GoalID);

	// 미션판 접힘(미니 칩) 여부. 세션 한정 — 재시작 시 펼침이 기본
	bool IsTrackerFolded() const { return bTrackerFolded; }
	void SetTrackerFolded(bool bInFolded);

	// 목록 더보기 펼침 여부. 세션 한정 — 재시작 시 접힘이 기본.
	// ⚠ 트래커 위젯이 아니라 여기 두는 이유 = 트래커는 맵 전환마다 재생성된다(사무실 왕복에 리셋되면 안 됨)
	bool AreRowsExpanded() const { return bRowsExpanded; }
	void SetRowsExpanded(bool bInExpanded);

	bool HasAvailableGuide() const;

	FOnTrackedGoalChanged OnTrackedGoalChanged;

	// SaveLoadManager::SaveGameData 가 호출 — 보드 상태를 세이브 구조체로
	void CollectSaveData(FGameSaveData& OutData) const;

	// 치트 백엔드
	void DevCompleteGoal(FName GoalID);
	void DevResetGoals();

	FOnGoalBoardChanged OnGoalBoardChanged;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FGoalBoardEntryOrderTestAccessor;
	friend struct FGuideQuestTestAccessor;
#endif

	void HandleConditionSignal(EMissionConditionType Type, int64 Amount);
	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	void RefreshRecommendation();
	bool TickReadiness(float DeltaTime);
	bool IsGoalAvailable(const FGoalTable& Row) const;
	bool IsGoalReady(FName GoalID, const FGoalTable& Row) const;
	void HandleGameDataLoaded();
	void HandlePostLoadEvaluationSave();
	void BuildBoardEntriesFromTableManager(
		const UTableManagerSubsystem& TableMgr,
		TArray<FGoalBoardEntry>& OutEntries) const;

	// 도달형과 카운트 이벤트 조건의 현재/목표. 단발 이벤트형은 false를 반환한다.
	bool GetConditionProgress(FName GoalID, const FGoalTable& Row, int64& OutCurrent, int64& OutTarget) const;
	// 미충족 미션 1건 판정 — 이벤트형은 bFromEvent 일 때만, 도달형은 절대값. 충족 시 래치+브로드캐스트. 반환값=이번 호출에서 새로 래치했는지(호출부가 저장 여부 결정)
	bool EvaluateGoal(FName GoalID, const FGoalTable& Row, bool bFromEvent, bool bShouldBroadcast = true);
	// 전수 재평가 (언락/로드 직후 — 자유플레이 선충족 구제. 스펙 §5.1)
	bool EvaluateAllGoals(bool bShouldSave = true, bool bShouldBroadcast = true);

	bool bUnlocked = false;

	// 세이브 핸드오프 전 CollectSaveData 가 기본값을 내보내 진행도를 덮는 것을 막는다.
	bool bLoadedFromSave = false;
	bool bUnlockNotificationPending = false;
	bool bPreparedUnlockRollbackAvailable = false;
	bool bUnlockedBeforePreparedUnlock = false;
	TArray<FName> CompletedGoalIDsBeforePreparedUnlock;
	TMap<FName, int64> GoalEventProgressBeforePreparedUnlock;
	TMap<FName, int32> GoalSuccessCountsBeforePreparedUnlock;
	TArray<FName> ReadyGoalIDsBeforePreparedUnlock;
	FName ActiveGuideGoalBeforePreparedUnlock;
	TArray<FName> CompletedGoalIDs;
	TMap<FName, int64> GoalEventProgressByID;
	TMap<FName, int32> GoalSuccessCountsByID;
	TArray<FName> ReadyGoalIDs;
	FName ActiveGuideGoalID;
	bool bClaimInProgress = false;
	TSet<FName> DismissedGoalIDs;
	FTSTicker::FDelegateHandle ReadinessTickerHandle;

	FName TrackedGoalID = NAME_None;
	int32 TrackedRecruitBuildingIndex = INDEX_NONE;
	FName LastRecommendedGoalID = NAME_None;
	TArray<FName> LastAvailableGoalIDs;
	bool bTrackerFolded = false;
	bool bRowsExpanded = false;
};
