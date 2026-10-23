#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/SideContentSaveData.h"
#include "Data/SideSkylineTypes.h"
#include "Data/SideTrainingTypes.h"
#include "Data/SideFusionTypes.h"
#include "Table/MissionTable.h"
#include "SideContentSubsystem.generated.h"

/** 출발을 막는 이유. UI 가 문구를 고르는 단일 기준 — 화면마다 다른 문장을 쓰지 않는다. */
UENUM(BlueprintType)
enum class EDispatchBlockReason : uint8
{
	None = 0,
	NoCard,
	SlotBusy,
	CompanyLocked,
	CrewCountMismatch,
	CrewUnavailable,
	DisciplineShort
};

/**
 * 사이드 컨텐츠 4모듈(기술 연구소·스카이라인·산업 융합·출장)의 상태를 소유하는 단일 서브시스템(설계 §1.2).
 * 새 매니저·새 재화·새 상점을 만들지 않는다. 시간은 UGameClockSubsystem, 지급은 FRewardGrant 가 권위.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USideContentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnSideContentChanged);
	FOnSideContentChanged OnSideContentChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── SaveLoadManager 핸드오프 (평구조체) ──
	const FSideContentSaveData& GetSideContentData() const { return Save; }
	void SetSideContentData(const FSideContentSaveData& InData);

	// ── 건물 축 공통 ──
	const FSideBuildingState* FindBuilding(int32 BuildingIndex) const;

	/** 현재 보유 회사 건물의 레벨과 DT 요구치를 비교한다. 행 누락 시 잠금 상태와 빈 표시명을 반환한다. */
	bool GetCompanyFeatureUnlock(int32 BuildingIndex, FName FeatureKey, int32& OutRequiredLevel, FText& OutDisplayName) const;

	/** 사라진 건물의 상태를 털어낸다. 건물 목록이 바뀌는 지점에서 부른다(설계 §1.2). */
	void PruneMissingBuildings(const TArray<int32>& ExistingBuildingIndices);

	// ── 출장 ──

	/** 오늘 카드 3장. 날이 바뀌었으면 여기서 다시 뽑는다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Trip")
	TArray<FName> GetDispatchCards(int32 BuildingIndex);

	UFUNCTION(BlueprintPure, Category = "SideContent|Trip")
	bool IsDispatchActive(int32 BuildingIndex) const;

	/** 남은 초. 진행 중이 아니면 0. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Trip")
	int32 GetDispatchRemainingSeconds(int32 BuildingIndex) const;

	/** 도착했고 서버 시각이 유효할 때만 true. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Trip")
	bool IsDispatchClaimable(int32 BuildingIndex) const;

	/** [자동 선발] 기본값 — 벤치 우선. 플레이어가 그대로 쓰거나 직접 바꾼다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Trip")
	TArray<int32> AutoPickDispatchCrew(int32 BuildingIndex, FName CardKey) const;

	/** 고른 직원들의 요구 직능 점수 합. 판정에 보이는 유일한 수치. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Trip")
	int32 GetDispatchCrewScore(FName CardKey, const TArray<int32>& CrewIDs) const;

	EDispatchBlockReason GetDispatchBlockReason(int32 BuildingIndex, FName CardKey, const TArray<int32>& CrewIDs) const;

	UFUNCTION(BlueprintCallable, Category = "SideContent|Trip")
	bool Depart(int32 BuildingIndex, FName CardKey, const TArray<int32>& CrewIDs);

	/** 도착한 슬롯을 정산한다. 드랍은 OutRewards 로 돌려줘 UI 가 결과 카드에 그린다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Trip")
	bool ClaimDispatch(int32 BuildingIndex, TArray<FMissionReward>& OutRewards);

	// ── 기술 연구소 ──
	// 판정이 즉시·결정적이라 UI 는 누르기 전에 아래 조회만으로 화면을 다 그린다.

	/** 과정 k 요구치 = Base x Mul^(k-1). 밴드가 없으면 0. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Training")
	float GetCourseRequirement(int32 Course) const;

	/** 지금 도전 중인 과정 = CourseBest + 1. 최종 과정을 넘지 않는다. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Training")
	int32 GetCurrentCourse(int32 BuildingIndex) const;

	/** 이미 깬 최고 과정 = 소탕 보상의 기준. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Training")
	int32 GetCourseBest(int32 BuildingIndex) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Training")
	int32 GetTrainingEntries(int32 BuildingIndex) const;

	/** 그 건물 배치 직원 전원의 파워 합. 기술 연구소는 선발이 없다 — 출장 중인 직원도 포함한다. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Training")
	float GetTrainingTeamPower(int32 BuildingIndex) const;

	/** 요구치를 넘는가. 판정 밴드 색·CTA 노출·[전체 시작] 분기가 전부 이 하나를 본다. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Training")
	bool CanAdvance(int32 BuildingIndex) const;

	ETrainingBlockReason GetTrainingBlockReason(int32 BuildingIndex) const;

	/** 도전. 성공해야 과정 +1 하고 입장권 1 차감 — 미달이면 아무것도 안 바뀐다(설계 §0-4). */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Training")
	bool Challenge(int32 BuildingIndex, TArray<FMissionReward>& OutRewards);

	/** 소탕. 판정 없이 최고 과정 보상. 입장권 1 은 무조건 차감된다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Training")
	bool SweepTraining(int32 BuildingIndex, TArray<FMissionReward>& OutRewards);


	/** 검수 전용 — 과정·입장권을 직접 세운다(음수 = 그대로). 최종 과정 화면은 정상 플레이로 못 만든다.
	 *  BlueprintCallable 이 아니다: 치트만 부르는 디버그 통로를 게임 로직에 노출하지 않는다. */
	void DebugSetTraining(int32 BuildingIndex, int32 CourseBest, int32 Entries);

	// ── 콜라보 개발 ──
	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	int32 GetFusionLevel(FName FusionKey) const;

	// 패널에 올릴 콜라보 키. DT 순서에서 미가용 업종을 요구하는 행만 뺀다(DT_CompanyInfo.bAvailableAtLaunch).
	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	TArray<FName> GetAvailableFusionKeys() const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	float GetFusionRequirement(FName FusionKey, int32 LevelOverride = -1) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	TArray<FMissionReward> GetFusionRewardPreview(FName FusionKey, int32 LevelOverride = -1) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	TArray<int32> GetFusionCandidateBuildings(ECompanyType Industry) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	TArray<int32> AutoPickFusionBuildings(FName FusionKey) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	ECompanyType GetFusionMissingIndustry(FName FusionKey) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	EFusionBlockReason GetFusionBlockReason(FName FusionKey, const TArray<int32>& BuildingIndices) const;

	UFUNCTION(BlueprintCallable, Category = "SideContent|Fusion")
	bool StartFusion(FName FusionKey, const TArray<int32>& BuildingIndices);

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	bool IsFusionActive() const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	bool IsFusionClaimable() const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	int32 GetFusionRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Fusion")
	float GetFusionProgress() const;

	UFUNCTION(BlueprintCallable, Category = "SideContent|Fusion")
	bool ClaimFusion(FFusionClaimResult& OutResult);

	/** 음수는 해당 값을 유지한다. RemainingSeconds=0 은 수령 가능한 타이머를 만든다. */
	void DebugSetFusion(FName FusionKey, int32 Level, int32 RemainingSeconds);

	// ── 스카이라인 ──
	// 판정이 즉시·결정적이라 UI 는 누르기 전에 아래 조회만으로 로비를 다 그린다.
	// **계산은 ComputeSkylineReach 하나뿐이다** — 사무실 씬은 Climb 이 돌려준 결과를 재생만 하고 다시 판정하지 않는다.

	/** 층 f 요구치 = Base x Mul^(f-1) x (보스면 BossMul). 밴드가 없으면 0. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	float GetFloorRequirement(int32 Floor) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	bool IsBossFloor(int32 Floor) const;

	/** 가장 가까운 미돌파 보스 층 = 로비의 `다음 관문` 태그가 붙는 자리. 없으면 0. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 GetNextBossFloor() const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 GetSkylineMaxFloor() const;

	/** 층 카드에 붙는 입주 기업 플레이버. 밴드가 범위라 같은 밴드는 같은 이름이다. 밴드가 없으면 빈 FText. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	FText GetFloorTenantName(int32 Floor) const;

	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 GetBestFloor() const { return Save.BestFloor; }

	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 GetSkylineEntries() const { return Save.SkylineEntries; }

	/** 입장권 상한. UI 가 "n/5" 의 5 를 따로 들고 있으면 수치를 고칠 때 화면만 옛말을 한다. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	static int32 GetSkylineEntryCap() { return SkylineEntryCap; }

	/** 최고 층 10층당 +1% — 전 건물 운영 수익에 곱해진다. 1.0 = 보너스 없음. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	float GetSkylineRevenueMultiplier() const;

	/** 출전 회사. 저장값이 유효하지 않으면 **팀 파워 최대 건물**을 돌려준다(진입 자동 선택). 없으면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 GetSkylineBuildingIndex() const;

	UFUNCTION(BlueprintCallable, Category = "SideContent|Skyline")
	void SetSkylineBuildingIndex(int32 BuildingIndex);

	/** 고를 수 있는 회사(건물) — 팀 파워 내림차순. 목록 첫 항목이 곧 자동 선택값이다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Skyline")
	TArray<int32> GetSkylineCandidateBuildings() const;

	/** 그 건물 배치 팀의 파워 합. 기술 연구소와 같은 계산 — 두 화면이 다른 숫자를 보이면 안 된다. */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	float GetSkylineTeamPower(int32 BuildingIndex) const;

	/**
	 * 이 팀으로 닿는 층. **로비의 예고와 사무실의 결과가 같은 함수의 같은 출력이다.**
	 * 최고 층부터 한 층씩 요구치를 넘는 동안 올라가고, 처음 막히는 층에서 멈춘다.
	 */
	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 ComputeSkylineReach(int32 BuildingIndex) const;

	ESkylineBlockReason GetSkylineBlockReason(int32 BuildingIndex) const;

	/**
	 * 등반. 닿는 층까지 한 번에 판정하고 층별 1회성 보상을 전부 지급한다.
	 * **한 층도 못 오르면 입장권이 줄지 않는다**(설계 §0-4) — 그때도 결과 구조체는 채워 돌려준다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Skyline")
	bool Climb(int32 BuildingIndex, FSkylineRunResult& OutResult);

	/** 선택한 회사의 실제 오피스 입장 의도. 로딩이 끝나기 전에는 입장권을 쓰지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Skyline")
	bool PrepareSkylineOfficeRun(int32 BuildingIndex);

	/** 직원 복원 완료 후 한 번만 판정한다. 재호출은 같은 결과를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "SideContent|Skyline")
	bool StartSkylineOfficeRun(FSkylineRunResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "SideContent|Skyline")
	int32 GetSkylineOfficeBuildingIndex() const { return SkylineOfficeBuildingIndex; }

	void FinishSkylineOfficeRun();
	bool ConsumeSkylineLobbyReturn();

	/** 검수 전용 — 최고 층·입장권을 직접 세운다(음수 = 그대로). 100층 화면은 정상 플레이로 못 만든다. */
	void DebugSetSkyline(int32 BestFloor, int32 Entries);

private:
	// 리필 수치는 여기가 단일 진실이다 (DECISION_RECORDS §13.2 ― 등반 1회 기준 2/일 · 최대 5)
	static constexpr int32 SkylineEntriesPerDay = 2;
	static constexpr int32 SkylineEntryCap = 5;

	FSideBuildingState& FindOrAddBuilding(int32 BuildingIndex);
	TArray<FMissionReward> RollTrainingLoot(int32 BuildingIndex, int32 Course) const;
	TArray<FMissionReward> RollSkylineLoot(int32 Floor) const;
	TArray<FFusionBuildingCandidate> GetFusionOwnedBuildings() const;
	void RefreshDispatchCardsIfNewDay(FSideBuildingState& State);
	void HandleGameDataLoaded();
	void HandleDayKeyChanged(const FString& NewDayKey);

	// 하루 키가 바뀌었으면 입장권을 리필한다. 로드 직후 따라잡기는 bShouldSave=false 로 부른다.
	void RefillEntriesForDay(const FString& NewDayKey, bool bShouldSave);
	void RequestSave();

	int64 NowUnix() const;
	bool IsServerTimeValid() const;
	int32 GetHQLevel() const;

	FSideContentSaveData Save;

	// 레벨 이동 동안만 유지한다. 이미 지급한 결과를 위젯 재생 때문에 다시 계산하지 않는다.
	UPROPERTY(Transient)
	FSkylineRunResult SkylineOfficeRun;
	int32 SkylineOfficeBuildingIndex = INDEX_NONE;
	bool bSkylineOfficeRunStarted = false;
	bool bReturnToSkylineLobby = false;

	// 서버 시각이 세이브 로드보다 먼저 도착할 수 있다 — 기본값 Save 로 정산하면 실제 세이브를 덮는다
	bool bLoaded = false;
};
