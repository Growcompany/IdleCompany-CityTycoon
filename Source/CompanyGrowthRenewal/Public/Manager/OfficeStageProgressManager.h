#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/StageProgressData.h"
#include "Data/EmployeeTypes.h"
#include "Table/ProjectDataTable.h"
#include "Enum/CompanyType.h"
#include "Enum/EmployeeState.h"
#include "Enum/ProjectMode.h"
#include "Enum/ProjectLifecycle.h"
#include "Enum/ProjectDirection.h"
#include "Enum/DevelopStartResult.h"
#include "Data/ProjectBoardData.h"
#include "Table/BoostGambleTable.h"
#include "Engine/DeveloperSettings.h"
#include "OfficeStageProgressManager.generated.h"

class UEmployeeManager;
class UResourceItemManager;
class UTableManagerSubsystem;
class UProjectTraitEventHandler;
struct FOperationData;
struct FProjectEventData;

/**
 * 프로젝트 한 판의 앞뒤 고정 대기 시간 (UDeveloperSettings).
 * Project Settings > Game > "CG 프로젝트 연출 타이밍" 에 노출되고 DefaultGame.ini 에 저장된다.
 * 환호/이펙트 잘림 회귀를 피하려 코드 상수를 임의로 줄이지 않고, PIE 실측 후 ini 로 조정한다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "CG 프로젝트 연출 타이밍"))
class COMPANYGROWTHRENEWAL_API UOfficeStageTimingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 착수 이펙트 재생 후 타이머 시작까지의 대기 (초)
	UPROPERTY(EditAnywhere, config, Category = "진행 연출", meta = (ClampMin = "0.0", ClampMax = "5.0",
		DisplayName = "착수 이펙트 지속 (초)"))
	float StepStartEffectDuration = 2.0f;

	// 종료 이펙트+환호 후 출시 확인 팝업까지의 대기 (초)
	UPROPERTY(EditAnywhere, config, Category = "진행 연출", meta = (ClampMin = "0.0", ClampMax = "6.0",
		DisplayName = "종료 후 출시 팝업 대기 (초)"))
	float LaunchPopupDelay = 3.0f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};

/**
 * 출시 실패 / 컨티뉴(추가 개발) 밸런스 노브 (UDeveloperSettings).
 * Project Settings > Game > "CG 출시 실패/컨티뉴" 에 노출되고 DefaultGame.ini 에 저장된다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "CG 출시 실패/컨티뉴"))
class COMPANYGROWTHRENEWAL_API UOfficeLaunchFailureSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 컨티뉴 1회 비용 (Diamond)
	UPROPERTY(EditAnywhere, config, Category = "컨티뉴", meta = (ClampMin = "0", ClampMax = "1000",
		DisplayName = "추가 개발 비용 (다이아)"))
	int32 RetryDiamondCost = 50;

	// 컨티뉴로 주어지는 추가 개발 시간 = 원 개발 시간 × 이 비율
	UPROPERTY(EditAnywhere, config, Category = "컨티뉴", meta = (ClampMin = "0.1", ClampMax = "1.0",
		DisplayName = "추가 개발 시간 비율"))
	float RetryDurationRatio = 0.5f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};

// ===== 델리게이트 선언 =====

// 시간 업데이트 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeUpdated, float, RemainingTime);

// 동시 진행 점수 업데이트 (매 틱) — 활성 직능 스텝별 배열. Scores[i]/Targets[i] = Steps 내 i번째 직능 스텝.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDisciplineScoresUpdated,
	const TArray<float>&, Scores, const TArray<float>&, Targets);

// 결과 이펙트 트리거 (타이머 종료 직후, 환호와 동시 재생)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStageResultEffect);

// 동시 진행 타이머 종료 후 팝업 트리거 (이펙트+환호 끝난 뒤)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimultaneousTimerEnd);

// Stage 완료 시 (출시 확정 → 운영 전환)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStageComplete);

// 직원 FloatingText 스폰 시 (UI 점수 업데이트 타이밍)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEmployeeFloatingTextSpawned);

// 진행 데이터 복원 시 (로드 후 이펙트 없이 UI 상태 갱신용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProgressDataRestored);

// Step 시작 이펙트 트리거 (StartSimultaneousTimer(bShowEffect=true) 시)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStepStartEffect);

// 프로젝트 선택 시 (타이머/직원 모드 변경 없이 데이터만 설정됨)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProjectSelected);

// 구슬 비행 요청 (직원 위치 → ProgressBar). OrbCount = 업무속도 Overflow 환산 배출 개수(정상 틱), 크리는 수신부에서 고정 버스트로 override.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnScoreOrbRequested,
	FVector, WorldPosition, int32, StepNumber, float, Score, bool, bIsCritical, int32, OrbCount);

// 티어 해금 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTierUnlocked, int32, NewTier);

// 프로젝트 중간 이벤트 트리거 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectEventTriggeredFromManager, const FProjectEventData&, EventData);

// 부스트 도박 요청 (개발 ~40% 지점 → 타임드 바이너리 모달 트리거)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBoostGambleRequested);

// 부스트 도박 결과 (연출용). bGambled=지름 여부, bSuccess=성공 여부(안전이면 둘 다 false)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoostGambleResolved, bool, bGambled, bool, bSuccess);

// 운영 모드 변경 시 (UI 좌측 슬롯 전환용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectModeChanged, EProjectMode, NewMode);

// 프로젝트 라이프사이클 전환 시 — UI 단일 진입점
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLifecycleChanged,
	EProjectLifecycle, OldState, EProjectLifecycle, NewState);

// 피버타임 상태 변경 시 (dev-spectacle: UI 불타는 오버레이/레전드 VFX 트리거)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFeverChanged, bool, bActive);

/**
 * 오피스 스테이지 진행 매니저
 *
 * 동시 진행 모드: 15초간 기획/개발/QA 3개 카테고리 점수 동시 누적
 * - OfficeMap에서만 생성됨
 * - 타이머 종료 → 결과 이펙트 + 환호 → 3초 후 자동 LaunchConfirmWidget 팝업
 * - 직능별 최소 점수 통과 = 출시/포기 선택 / 미달 = 출시 차단 후 추가 개발(컨티뉴 1회)/폐기 선택
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeStageProgressManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UOfficeStageProgressManager();

	// ===== WorldSubsystem 라이프사이클 =====
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	// TierProgress 는 메모리 멤버라서 OfficeMap 이탈 시 휘발됨. OfficeDataMap[BuildingIndex] 에 싱크.
	// 빌딩 진입 시 한 번 Load, RecordProjectClear 이후 Sync.
	void LoadTierProgressFromSave();

public:
	/**
	 * TierProgress 를 세이브(OfficeDataMap)에 반영.
	 * SetTierProgress 로 진척을 외부에서 바꿨으면 반드시 이걸 불러야 오피스 이탈 후에도 남는다
	 * (안 부르면 메모리 멤버만 바뀌고 재진입 시 옛 값으로 덮인다 — 치트 시드가 사라지는 원인이었다).
	 */
	void SyncTierProgressToSave();


	// ===== 스테이지 시작/종료 =====

	/**
	 * 프로젝트 선택 (데이터 설정만, 타이머/직원 모드 변경 없음)
	 * OfficeStagePanelWidget에서 카드 "선택" 시 호출
	 * @param ProjectData 프로젝트 데이터
	 * @param ProjectNumber 몇 번째 프로젝트 (1, 2, 3...)
	 * @param StageNumber 프로젝트 내 스테이지 번호 (기본값 1)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress")
	void SelectProject(const FProjectData& ProjectData, int32 ProjectNumber, int32 StageNumber = 1);

	/**
	 * 새 스테이지 시작 (SelectProject + 타이머/직원 모드 변경)
	 * @param ProjectData 프로젝트 데이터
	 * @param ProjectNumber 몇 번째 프로젝트 (1, 2, 3...)
	 * @param StageNumber 프로젝트 내 스테이지 번호 (기본값 1)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress")
	void StartNewStage(const FProjectData& ProjectData, int32 ProjectNumber, int32 StageNumber = 1);

	/**
	 * 현재 스테이지 종료 (강제 중단)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress")
	void EndCurrentStage();

	// ===== 동시 진행 타이머 =====

	/**
	 * 동시 진행 타이머 시작 (15초, 3개 카테고리 동시)
	 * @param bShowEffect true: 이펙트 표시 후 타이머 시작, false: 즉시 타이머 시작
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Timer")
	void StartSimultaneousTimer(bool bShowEffect = false);

	/**
	 * 타이머 정지
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Timer")
	void StopTimer();

	// ===== 점수 계산 =====

	/**
	 * 직원 직능 기여 점수 추가 — 특정 직능 스텝에 누적 + 배열 브로드캐스트.
	 * @param DisciplineSlot EProductionDiscipline 슬롯(그 직원이 기여한 직능)
	 * @param Score 기여 점수(ProjectYield 배율은 내부에서 곱함)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Score")
	void AddDisciplineContribution(int32 DisciplineSlot, float Score);

	/**
	 * 점수 추가 (Active 스킬 보너스 등 외부 점수) — 활성 직능 스텝에 균등 배분
	 * @param BonusScore 추가할 점수
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Score")
	void AddBonusScore(float BonusScore);

	/** 직능 포인트 → 특화 배율(튜닝 노브). 0=약(0.5), 높을수록 강. */
	// 성장 축 단일화 `[확정 2026-08-13]` — 레벨 직접 배수를 없앤 대신 이 곡선 하나가 성장을 전담한다.
	// 복리인 이유: 요구곡선이 티어당 등비(×3.1→×1.6)라 단리 공급(구 0.5+P*0.1, 만렙 9.2배)으로는
	// 후반에 구조적으로 못 따라간다. 1.06^P 는 SP 10점까지는 단리보다 낮고 30점부터 벌어진다.
	static float DisciplineAffinity(int32 Points)
	{
		return 0.5f + FMath::Pow(1.06f, static_cast<float>(Points)) - 1.0f;
	}

	/** 활성 직능 스텝의 현재 점수/목표를 배열로 브로드캐스트 (5개 누적 지점 공용) */
	void BroadcastDisciplineScores();

	// 티어 기준선 — 그 티어 행들의 RequiredScore_Step2 중앙값. 이벤트/도전이 "티어 상대 요구치"를 만들 때 쓰는 자리(스펙 §14.2-3)
	static float ComputeTierBaselineFromScores(const TArray<int32>& Step2Scores);
	static float ComputeTierBaseline(ECompanyType Industry, int32 Tier, const UTableManagerSubsystem* TableMgr);

	// ===== 상태 조회 =====

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	const FStageProgressData& GetProgressData() const { return ProgressData; }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	int32 GetCurrentStep() const { return ProgressData.CurrentStep; }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	float GetRemainingTime() const { return ProgressData.RemainingTime; }

	// Trait/티어 override까지 반영된 현재 Step 의 총 길이 — UI ProgressBar 정규화 용
	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	float GetTotalDuration() const { return CachedTotalDuration; }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	bool IsTimerRunning() const { return ProgressData.bIsTimerRunning; }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	EQualityGrade GetCurrentQualityGrade() const { return ProgressData.CalculateQualityGrade(); }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	float GetCurrentQualityScore() const { return ProgressData.CalculateQualityScore(); }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|State")
	bool IsStageInProgress() const { return bStageInProgress; }

	// 치트 전용 — 활성 직능 스텝 점수를 목표 대비 비율로 일괄 세팅 (리뷰/실패 화면 반복 검증용)
	void Debug_FillScores(float Ratio);

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Launch")
	bool IsManufacturing() const { return ProgressData.bIsManufacturing; }

	// ===== 델리게이트 =====

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnTimeUpdated OnTimeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnDisciplineScoresUpdated OnDisciplineScoresUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnStageResultEffect OnStageResultEffect;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnSimultaneousTimerEnd OnSimultaneousTimerEnd;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnStageComplete OnStageComplete;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnEmployeeFloatingTextSpawned OnEmployeeFloatingTextSpawned;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnProgressDataRestored OnProgressDataRestored;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnStepStartEffect OnStepStartEffect;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnProjectSelected OnProjectSelected;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Events")
	FOnScoreOrbRequested OnScoreOrbRequested;

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Score")
	void RequestScoreOrb(FVector WorldPos, int32 StepNumber, float Score, bool bIsCritical, int32 OrbCount);

	// ===== GDS 착수/티어 시스템 (프로젝트형 산업군 전용) =====

	/**
	 * GDS 착수(프로젝트 idx 기반) — 픽칭/도감이 고른 실제 프로젝트 행 하나로 자체개발 시작.
	 * 그 행의 장르/소재/커버/요구점수/규모를 그대로 근거로 사용(규모 슬롯 재추첨 없음) → 카드 정체성과 착수가 한 행으로 일치.
	 * @param ProjectIndex 착수할 프로젝트 행 인덱스(DT_Project_* 의 ProjectIndex)
	 * @param Direction    방향성 결단(T2 해금, 초반엔 Standard)
	 * @param CustomName   플레이어가 수정한 이름. 비면 행 기본명 유지.
	 * @return 착수 결과. 실패 사유는 호출자가 알림 문구로 옮긴다 — 조용한 실패 방지.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Board")
	EDevelopStartResult StartSelfDevelopFromProject(int32 ProjectIndex, EProjectDirection Direction,
		const FText& CustomName);

	/**
	 * 착수 실패 사유를 안내 알림으로 옮긴다. Success 면 아무것도 하지 않는다.
	 * 착수 진입점은 기획 보드 하나지만, 실패 문구가 호출부마다 갈리지 않게 여기 모아 둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Board")
	static void NotifyDevelopStartFailure(EDevelopStartResult Result);

	/**
	 * 피치 카드 추정 — 총 예상수익(피크 rps × 감쇠 적분) + 운영시간(초).
	 * 방향=표준(중립). 미지수였던 실행 품질을 호출자가 현재 팀 기준으로 확정해 넘기므로 범위가 아니라 값 하나가 나온다.
	 * @param ExpectedQuality 현재 로스터 기준 예상 품질 [0.5, 2.0] (FProjectOutlookEstimate::ExpectedQuality)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Board")
	void EstimatePitchEconomy(ECompanyType Industry, float ExpectedQuality, bool bTrend,
		int64& OutRevenue, int32& OutOpTimeSec, int32 ProjectNumberOverride = -1);

	/**
	 * 개발 점수에 실제로 기여할 직원만 추정기 입력으로 모은다 — 기여 조건 = 책상 배정.
	 * 배정된 책상이 없으면 Stage 전환의 TeleportToWorkstationAndSit 이 실패해 Idle 로 되돌아가고,
	 * 기여 루프(SpawnScoreFloatingText)도 GetAssignedWorkstation 을 요구한다. 로스터 전원을 넣으면
	 * 카드가 미착석 인원수만큼 과대평가한다.
	 */
	void BuildContributorRoster(TArray<FEstimateWorkerInput>& OutRoster) const;

	/**
	 * 로스터 1벌로 프로젝트 1건의 직능별 예상 점수/품질/게이트 위험을 뽑는다.
	 * 가중치·요구점수·기간(빈 Duration 폴백 포함) 조립을 여기 한 곳에 두어, 카드와 착수가 같은 입력을 쓴다.
	 * 로스터를 인자로 받는 이유 = 카드 3장이 같은 팀을 공유하므로 수집은 호출자가 한 번만 한다.
	 */
	FProjectOutlookEstimate EstimateProjectOutlook(
		const TArray<FEstimateWorkerInput>& Roster, const FProjectData& ProjectRow) const;

	/**
	 * 티어 1개의 프로젝트 10개를 상태(잠금/미개발/개발함)와 함께 조회.
	 * 포트폴리오는 오피스 밖(MainMap)에서도 열리므로 매니저 인스턴스에 의존하지 않는 static —
	 * 개발 이력은 호출자가 넘긴다(오피스=TierProgress / 그 외=세이브 OfficeDataMap).
	 */
	static void BuildTierProjectEntries(ECompanyType Industry, int32 Tier, int32 CurrentTier,
		const TArray<int32>& DevelopedProjects, TArray<FProjectTierEntry>& OutEntries);

	/** 이벤트 선택지 처리 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Board")
	void HandleEventChoice(int32 ChoiceIndex);

	/** 부스트 도박 해결 — bGamble=지른다(true)/안전(false). 모달 타임아웃 시 false로 호출. */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Board")
	void ResolveBoostGamble(bool bGamble);

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Board")
	bool IsBoostGamblePending() const { return bBoostGamblePending; }

	// 현재 트리거된 부스트 도박 시나리오 (모달 위젯이 제목/메시지/버튼 라벨 주입에 사용)
	UFUNCTION(BlueprintPure, Category = "Stage Progress|Board")
	const FBoostGambleRow& GetCurrentBoostGamble() const { return CurrentBoostGamble; }

	// 현재 이벤트의 확정 비용(GoCost)을 감당 가능한지 — 위젯이 지른다 버튼 게이트에 사용
	bool CanAffordCurrentBoostGamble() const;

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Board")
	const FProjectTierProgress& GetTierProgress() const { return TierProgress; }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Board")
	bool IsEventPaused() const;

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Board")
	UProjectTraitEventHandler* GetTraitEventHandler() const { return TraitEventHandler; }

	/** 티어 진행 데이터 설정 (SaveLoad용) */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Board")
	void SetTierProgress(const FProjectTierProgress& InProgress) { TierProgress = InProgress; }

	// ===== 이벤트/티어 델리게이트 =====

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Board")
	FOnTierUnlocked OnTierUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Board")
	FOnProjectEventTriggeredFromManager OnProjectEventTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Board")
	FOnProjectModeChanged OnProjectModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Board")
	FOnBoostGambleRequested OnBoostGambleRequested;

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Board")
	FOnBoostGambleResolved OnBoostGambleResolved;

	// ===== 라이프사이클 FSM (UI 단일 진입점) =====

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Lifecycle")
	FOnLifecycleChanged OnLifecycleChanged;

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Lifecycle")
	EProjectLifecycle GetLifecycle() const { return Lifecycle; }

	// 개발중 집중 모드 술어 — 월드 상호작용/도크/트래커/뒤로가기·포트폴리오가 전부 이 한 줄을 본다.
	// 가드가 여러 곳으로 흩어져 구간이 갈라지는 것을 막으려고 매니저가 소유한다.
	UFUNCTION(BlueprintPure, Category = "Stage Progress|Lifecycle")
	bool IsFocusLocked() const
	{
		return Lifecycle == EProjectLifecycle::Developing || Lifecycle == EProjectLifecycle::LaunchPending;
	}

	/**
	 * 라이프사이클 전환 — 유효성 검증 + 브로드캐스트
	 * 외부에서 직접 호출하지 않고 Request/Notify 인텐트 메서드 사용 권장
	 */
	void TransitionTo(EProjectLifecycle NewState);

	/** 모드까지 고려한 전환 유효성 검증 */
	static bool IsValidTransition(EProjectLifecycle From, EProjectLifecycle To, EProjectMode Mode);

	// ----- 인텐트 API (외부 호출 진입점) -----

	/** LaunchPending에서 출시(자체) / 납품(수주) 확정 → 모드별 분기 후 Operating 또는 Idle로 전환 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Lifecycle")
	void RequestLaunchConfirm();

	/** LaunchPending에서 취소 → Idle로 복귀 (점수/모드 리셋) */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Lifecycle")
	void RequestLaunchCancel();

	/** Operating → ReportPending (자체개발 운영 종료, OperationManager에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Lifecycle")
	void NotifyOperationCompleted();

	/** ReportPending → Idle (결산서 닫기/다음 클릭) */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Lifecycle")
	void NotifyReportClosed();

	// ===== 저장/로드 =====

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|SaveLoad")
	void SetProgressData(const FStageProgressData& InData);

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|SaveLoad")
	void SetStageInProgress(bool bInProgress) { bStageInProgress = bInProgress; }

	// ===== 출시 시스템 =====

	/**
	 * 출시/양산 확정 시작 (LaunchConfirmWidget에서 호출)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Launch")
	void StartLaunch();

	/**
	 * 결과 포기 (LaunchConfirmWidget X 닫기)
	 * 점수 리셋 + idle 복귀
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Launch")
	void HandleLaunchDismissed();

	// ===== 출시 실패 / 컨티뉴 =====

	/**
	 * 수준 미달로 출시가 차단된 상태인가.
	 * 튜토리얼 M10(첫 자체개발 출시) 진행 중에는 항상 false — 실패가 미션 체인을 교착시키는 걸 막는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Stage Progress|Launch")
	bool IsLaunchBlocked() const;

	/** 컨티뉴 가능한가 — 미달 && 프로젝트당 1회 미사용. 다이아 보유 여부는 별도(GetRetryDiamondCost) */
	UFUNCTION(BlueprintPure, Category = "Stage Progress|Launch")
	bool CanRetryDevelopment() const;

	/** 컨티뉴 1회 비용 (Diamond) */
	UFUNCTION(BlueprintPure, Category = "Stage Progress|Launch")
	int32 GetRetryDiamondCost() const;

	/**
	 * 컨티뉴 실행 — 다이아 차감 후 누적 점수를 유지한 채 개발 타이머 재개 (LaunchPending → Developing).
	 * 결제 수단은 알지 않는다("재도전 1회 승인"만) — 광고 리워드가 붙어도 이 함수는 그대로다.
	 * @return 게이트/잔액 미충족이면 false (상태 변경 없음)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Launch")
	bool TryRetryDevelopment();

	// 실패 프로젝트 [폐기]는 별도 API 없이 기존 취소 경로(HandleLaunchDismissed)를 그대로 탄다 — 동작이 동일하다

	// ===== 직원 행동 모드 관리 =====

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Employee")
	void NotifyEmployeesBehaviorMode(EEmployeeBehaviorMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Employee")
	void NotifyEmployeesCheerSitting();

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Employee")
	void NotifyEmployeesCheerStandUp();

	// 결과 모달(LaunchPending/ReportPending) 중 전 직원 피로 동결/해제
	void NotifyEmployeesFatigueFrozen(bool bFrozen);

	// ===== 크리티컬 =====

	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Critical")
	void SetCriticalChance(float Chance) { CriticalChance = FMath::Clamp(Chance, 0.0f, 1.0f); }

	// [치트] 크리티컬 확률 강제 오버라이드(0~1). Chance01>=0 이면 GetCriticalChance 가 기본/특성/피버 무시하고 이 값 반환,
	// Chance01<0 이면 해제. 스테이지 리셋(CriticalChance=0.003)에 영향받지 않아 트레일러 캡처 내내 유지된다.
	void SetCritChanceOverride(float Chance01) { CritChanceOverride = (Chance01 >= 0.0f) ? FMath::Clamp(Chance01, 0.0f, 1.0f) : -1.0f; }
	float GetCritChanceOverride() const { return CritChanceOverride; }

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Critical")
	float GetCriticalChance() const;

	// ===== 피버타임 (dev-spectacle) =====

	UPROPERTY(BlueprintAssignable, Category = "Stage Progress|Fever")
	FOnFeverChanged OnFeverChanged;

	/** 피버타임 발동 — 일정 시간 전역 산출/크리율 폭증 (이벤트 "피버" 선택지에서 호출). 이미 활성 시 시간 갱신. */
	UFUNCTION(BlueprintCallable, Category = "Stage Progress|Fever")
	void StartFever();

	UFUNCTION(BlueprintPure, Category = "Stage Progress|Fever")
	bool IsFeverActive() const { return bFeverActive; }

	// 피버 중 점수 배수(비활성 1.0) — 직원 기여 계산의 전역 배수 훅에서 곱함
	UFUNCTION(BlueprintPure, Category = "Stage Progress|Fever")
	float GetFeverScoreMultiplier() const { return bFeverActive ? FeverOutputMult : 1.0f; }

	// 이번 판에 직원 1인이 받은 경험치 총량(3스텝 합, 큐브 배율 전) — 게이트 판정보다 먼저 지급되므로 실패 화면도 이걸 쓴다
	UFUNCTION(BlueprintPure, Category = "Stage Progress")
	float GetLastRoundEmployeeExp() const { return LastRoundEmployeeExp; }

protected:
	// ===== 내부 처리 함수 =====

	/** 동시 진행 타이머 틱 (3개 카테고리 DPS 동시 계산) */
	void SimultaneousTimerTick();

	/** 동시 진행 타이머 종료 (이펙트+환호 동시 재생 → 3초 후 팝업) */
	void OnSimultaneousTimerEnded();

	/** 이펙트+환호 끝난 뒤 LaunchConfirmWidget 팝업 트리거 */
	void ShowLaunchPopupAfterEffect();

	/** Stage 전체 완료 처리 (출시 확정 후) */
	void OnStageCompleted();

	/** Operation 완료 시 직원 BehaviorMode를 Idle로 전환 */
	UFUNCTION()
	void OnOperationCompletedHandler(int32 BuildingID, const FOperationData& Data);

	/** Step 완료 시 경험치 분배 */
	void DistributeStepExperience(int32 CompletedStep);

	/** 랜덤 변동 계수 계산 (0.8 ~ 1.2) */
	float CalculateRandomFactor() const;

	/** 크리티컬 체크 및 배율 반환 */
	float CheckCritical() const;

private:
	// 스테이지 진행 데이터
	UPROPERTY()
	FStageProgressData ProgressData;

	// 스테이지 진행 중 여부
	UPROPERTY()
	bool bStageInProgress = false;

	// 이번 판 직원 경험치 누적 (타이머 종료 시 0 으로 리셋 후 3스텝 합산)
	UPROPERTY()
	float LastRoundEmployeeExp = 0.0f;

	// 동시 진행 타이머 핸들
	FTimerHandle SimultaneousTimerHandle;

	// 이펙트 딜레이 타이머 핸들
	FTimerHandle EffectDelayTimerHandle;

	// 환호 후 LaunchConfirmWidget 팝업 타이머
	FTimerHandle LaunchPopupTimerHandle;

	// 이펙트 후 실제 타이머 시작 내부 함수 (SetTimer 콜백이라 무인자 유지)
	void StartSimultaneousTimerInternal();

	// 타이머 구동 공통부. bResetScores=false 면 누적 점수를 보존한다(컨티뉴 재개)
	void BeginTimerRun(float Duration, bool bResetScores);

	// 캐시된 매니저 참조
	UPROPERTY()
	mutable UEmployeeManager* CachedEmployeeManager = nullptr;

	UPROPERTY()
	mutable UResourceItemManager* CachedResourceItemManager = nullptr;

	UPROPERTY()
	mutable UTableManagerSubsystem* CachedTableManager = nullptr;

	// 매니저 캐시 초기화
	UEmployeeManager* GetEmployeeManager() const;
	UResourceItemManager* GetResourceItemManager() const;
	UTableManagerSubsystem* GetTableManager() const;

	// ===== 라이프사이클 FSM 상태 =====
	UPROPERTY(SaveGame)
	EProjectLifecycle Lifecycle = EProjectLifecycle::Idle;

	// 모드 무관 종료 처리 — ProgressData/TraitEventHandler 전체 리셋
	void ResetProjectState();

	// 출시 시 빌딩 EXP 부여 (품질 등급 비례)

	// DT_Project 의 Duration 이 비었을 때만 쓰는 fallback (정상 경로는 ResolveStepDuration)
	static constexpr float DefaultStepDuration = 15.0f;

	// 현재 ProgressData(CompanyType + ProjectID) 기준 Step 타이머 시간 해석.
	// 우선순위: ProgressData.StepDuration(>0, 합성 프로젝트라 테이블 조회가 무의미한 경우) → FProjectData.Duration → DefaultStepDuration
	float ResolveStepDuration() const;

	// 착수 공통 시퀀스 — 완성된 Synth 행 + 규모 idx 로 SelectProject→ProgressData 주입→FSM 점화.
	EDevelopStartResult LaunchDevelopFromRow(const FProjectData& Synth, int32 ScaleIndex,
		EProjectDirection Direction, ECompanyType Industry, int32 Tier);

	// 도감 마일스톤 보상(다이아) — 튜닝 노브. 티어 10/10 발견 / 전체 100개 발견.
	static constexpr int32 CodexTierMilestoneDiamond = 20;
	static constexpr int32 CodexFullMilestoneDiamond = 200;

	// 자체개발 클리어 직후 도감 마일스톤(티어 10/10, 전체 100%) 체크 + 보상 지급(중복 방지, 세이브).
	void CheckAndGrantCodexMilestone(int32 ClearedProjectIndex);

	// 리뷰 /40 계산 + (장르,소재) 발견 기록 — 개발 종료 시, InHouse 프로젝트형만
	void ComputeReviewAndDiscovery();

	// 타이머 틱 간격 (초)
	static constexpr float TimerTickInterval = 0.1f;

	// 부스트 도박 — 개발 중 1회, ~40% 지점. 시나리오/오즈는 DT_BoostGamble(산업별) 주도.
	bool bBoostGamblePending = false;   // 모달 표시 중 개발 타이머/기여 일시정지
	bool bBoostGambleDone = false;      // 프로젝트당 1회
	static constexpr float BoostTriggerRatio = 0.4f;
	// 티어1(온보딩 구간)은 제외 — 개발 루프를 처음 배우는 동안 도박 모달로 끊지 않는다.
	static constexpr int32 BoostMinTier = 2;
	FBoostGambleRow CurrentBoostGamble; // 트리거 시 DT 풀에서 뽑은 현재 시나리오 (모달/해결 공용)

	// 크리티컬 확률 — 값 SOT = EmployeeStatTuning::BaseCritChance (생성자가 거기서 초기화한다)
	float CriticalChance = 0.013f;

	// [치트] 크리율 강제 오버라이드(-1=미사용). SetCritChanceOverride 로 설정 → GetCriticalChance 우선 반환.
	float CritChanceOverride = -1.0f;

	// ===== 피버타임 (dev-spectacle, 튜너블 — 추후 DA_DevSpectacleConfig 승격 가능) =====
	bool bFeverActive = false;
	FTimerHandle FeverTimerHandle;
	float FeverDuration = 6.0f;     // 발동 후 지속(초)
	float FeverOutputMult = 2.0f;   // 피버 중 점수 배수
	float FeverCritBonus = 0.3f;    // 피버 중 크리 확률 가산

	// 피버 종료(타이머 콜백) — 상태/타이머 정리 + 브로드캐스트 (영구 폭증 누수 방지)
	void EndFever();

	// ===== 프로젝트 진행/티어 =====

	// 특성/이벤트 핸들러
	UPROPERTY()
	UProjectTraitEventHandler* TraitEventHandler = nullptr;

	// 티어 진행 상태
	UPROPERTY()
	FProjectTierProgress TierProgress;

	// 이벤트 트리거용 총 타이머 시간 캐시
	float CachedTotalDuration = 15.0f;
};
