#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Table/GoalTable.h"
#include "GoalBoardSubsystem.generated.h"

struct FGameSaveData;
class UTableManagerSubsystem;

enum class EGoalState : uint8 { Locked, InProgress, Claimable, Claimed };

// 보드/트래커 UI 용 미션 스냅샷
struct FGoalBoardEntry
{
	FName GoalID;
	FGoalTable Row;
	EGoalState State = EGoalState::InProgress;
	int64 ProgressCurrent = 0;
	int64 ProgressTarget = 0;
};

DECLARE_MULTICAST_DELEGATE(FOnGoalBoardChanged);

// 추적 중인 미션이 바뀔 때 (NAME_None = 해제). 트래커 행 강조 + 가이드 오버레이가 구독
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTrackedGoalChanged, FName);

/**
 * 미션판 — 체인 종료 후 동시 노출·자유 순서 미션 (스펙 2026-08-02 §5).
 * MissionManager 의 OnConditionSignal 구독으로 판정. 이벤트형=래치 / 도달형=절대값 평가.
 * 수령은 전건 수동: 조건 달성 시 Claimable 로 래치하고 트래커의 수령 동작에서만 보상을 지급한다.
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

	// [수령] — Claimable(충족+선행 수령)에서만 지급. 성공 시 세이브 + 밴드 토스트 + 브로드캐스트.
	bool ClaimGoal(FName GoalID);

	// ===== 미션판 표시 상태 (전부 transient — 세이브 안 함) =====

	// 안내(가이드 점등) 중인 미션. NAME_None = 없음
	FName GetTrackedGoalID() const { return TrackedGoalID; }
	// 안내 대상 지정 — NAME_None 이면 해제. Locked/Claimed 미션은 거부(무시)한다.
	// 한 번에 1건만: 다른 미션을 지정하면 기존 추적은 자동으로 풀린다
	void SetTrackedGoal(FName GoalID);

	// 미션판 접힘(미니 칩) 여부. 세션 한정 — 재시작 시 펼침이 기본
	bool IsTrackerFolded() const { return bTrackerFolded; }
	void SetTrackerFolded(bool bInFolded);

	// 목록 더보기 펼침 여부. 세션 한정 — 재시작 시 접힘이 기본.
	// ⚠ 트래커 위젯이 아니라 여기 두는 이유 = 트래커는 맵 전환마다 재생성된다(사무실 왕복에 리셋되면 안 됨)
	bool AreRowsExpanded() const { return bRowsExpanded; }
	void SetRowsExpanded(bool bInExpanded);

	// 미션판 첫 등장 연출을 이번 세션에 아직 안 했으면 true 를 돌려주고 소진한다.
	// transient 라 재접속마다 1회 재생된다 — 세이브 필드를 늘릴 값어치가 없는 미약한 연출이라 의도된 선택
	bool ConsumeEntranceOnce();

	// 트래커 표시 여부 — Claimed 가 아닌 항목이 하나라도 있으면 true.
	// Locked 도 세는 것이 의도: 잠금 행도 리스트에 노출된다(대표 미션 기준과 다름)
	bool HasAnyUnclaimedGoal() const;

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
#endif

	void HandleConditionSignal(EMissionConditionType Type);
	void HandleGameDataLoaded();
	void HandlePostLoadEvaluationSave();
	void BuildBoardEntriesFromTableManager(
		const UTableManagerSubsystem& TableMgr,
		TArray<FGoalBoardEntry>& OutEntries) const;

	// 도달형과 카운트 이벤트 조건의 현재/목표. 단발 이벤트형은 false를 반환한다.
	bool GetConditionProgress(FName GoalID, const FGoalTable& Row, int64& OutCurrent, int64& OutTarget) const;
	EGoalState GetGoalState(FName GoalID, const FGoalTable& Row) const;
	// 미충족 미션 1건 판정 — 이벤트형은 bFromEvent 일 때만, 도달형은 절대값. 충족 시 래치+브로드캐스트. 반환값=이번 호출에서 새로 래치했는지(호출부가 저장 여부 결정)
	bool EvaluateGoal(FName GoalID, const FGoalTable& Row, bool bFromEvent, bool bShouldBroadcast = true);
	// 전수 재평가 (언락/로드 직후 — 자유플레이 선충족 구제. 스펙 §5.1)
	bool EvaluateAllGoals(bool bShouldSave = true, bool bShouldBroadcast = true);

	bool bUnlocked = false;
	bool bUnlockNotificationPending = false;
	bool bPreparedUnlockRollbackAvailable = false;
	bool bUnlockedBeforePreparedUnlock = false;
	TArray<FName> CompletedGoalIDsBeforePreparedUnlock;
	TMap<FName, int64> GoalEventProgressBeforePreparedUnlock;
	TArray<FName> ClaimedGoalIDsBeforePreparedUnlock;
	TArray<FName> CompletedGoalIDs;
	TMap<FName, int64> GoalEventProgressByID;
	TArray<FName> ClaimedGoalIDs;

	FName TrackedGoalID = NAME_None;
	bool bTrackerFolded = false;
	bool bRowsExpanded = false;
	bool bEntrancePlayed = false;
};
