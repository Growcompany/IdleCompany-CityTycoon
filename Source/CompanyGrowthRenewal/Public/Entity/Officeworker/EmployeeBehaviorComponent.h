#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enum/EmployeeState.h"
#include "Data/EmployeeBuff.h"
#include "EmployeeBehaviorComponent.generated.h"

// 상태 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmployeeStateChanged, EEmployeeState, OldState, EEmployeeState, NewState);

// 인사 완료 델리게이트 (RecruitmentGameMode에서 바인딩)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGreetingFinished);

// 버프 변경 델리게이트 (Bubble/Overlay 갱신용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuffChanged, bool, bAnyActive);

class UFatigueConfig;

// 피로 슬랙 루프 단계 (내부 — Slacking 도달 → 텔레그래프(캐치 윈도우) → 늘어짐 → 드물게 번아웃 배회 → 복귀).
// 개발(Stage) 중에만 동작. Slumping 까지는 자리 유지, Bolting 은 자리 이탈해 배회(움직이는 캐치 타깃).
enum class EFatigueSlackPhase : uint8 { None, Telegraph, Slumping, Bolting };

/**
 * 직원 행동 시스템 컴포넌트
 * - 상태 관리 (Working, Rest, Wander 등)
 * - 피로도 시스템
 * - 수익 발생 및 축적
 * - 배회 시스템
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COMPANYGROWTHRENEWAL_API UEmployeeBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEmployeeBehaviorComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//========================================
	// ABP 연동용 변수 (BlueprintReadOnly)
	//========================================

	// 현재 행동 패턴 모드 (Idle/Stage/Operation)
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	EEmployeeBehaviorMode CurrentBehaviorMode = EEmployeeBehaviorMode::Idle;

	// 현재 직원 상태
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	EEmployeeState EmployeeState = EEmployeeState::Wander;

	// Dance 상태에서 재생할 댄스 클립 인덱스 (AnimInstance 경유 → AnimBP Blend Poses by Int)
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	int32 CurrentDanceIndex = 0;

	// 앉은 자세 변주 인덱스 (0=기본 1=태평(Lazy) 2=꾸벅(nod-off)) — 피로 밴드로 Sitting 진입 시 결정.
	// ABP 의 Sitting 상태에서 Blend Poses by Int(SeatedPoseIndex) 로 클립 선택. (Dance 패턴 미러)
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	int32 SeatedPoseIndex = 0;

	// 앉음/서있음 상태 (true: 앉음, false: 서있음)
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	bool bIsSeated = false;

	// 걷는 중 여부
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	bool Walking = false;

	// 이동 속도 (1~5: Walk, 5+: Run)
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Animation")
	float Speed = 0.0f;

	// 외부에서 초기화를 직접 제어할 때 true (RecruitmentMap 등)
	UPROPERTY()
	bool bSkipAutoInit = false;

	//========================================
	// 내부 관리용 변수
	//========================================

	// 축적된 수익
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Income")
	float AccumulatedIncome = 0.0f;

	// Stamina 기반 Work/Rest 사이클 타이머 (현재 상태에서 경과 시간)
	float CurrentStateTime = 0.0f;

	//========================================
	// 피로도 (Part A — 휘발성. 저장/리플리케이트 안 함, 맵 진입마다 fresh)
	//========================================

	// 0~100. 타이핑하면 누적, 휴식/배회 시 회복. 기존 모드와 직교한 레이어.
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Behavior|Fatigue")
	float Fatigue = 0.0f;

	// 현재 피로 밴드 (임계는 DA_FatigueConfig)
	UFUNCTION(BlueprintPure, Category = "Behavior|Fatigue")
	EFatigueBand GetFatigueBand() const;

	// 0~1 정규화 (머티리얼 FatigueFill 구동용)
	UFUNCTION(BlueprintPure, Category = "Behavior|Fatigue")
	float GetFatigue01() const { return Fatigue / 100.0f; }

	// 결과 모달(LaunchPending/ReportPending) 중 피로 동결 토글
	UFUNCTION(BlueprintCallable, Category = "Behavior|Fatigue")
	void SetFatigueFrozen(bool bFrozen) { bFatigueFrozenByLifecycle = bFrozen; }

	// 클릭 캐치 — 피로 부분 차감(DA TapRelief). 텔레그래프 중이면 임계 아래로 떨어져 다음 틱 복귀.
	UFUNCTION(BlueprintCallable, Category = "Behavior|Fatigue")
	void ApplyTapRelief();

	// 피로→출력 커플링 팩터 (1.0 ~ 1−MaxFatiguePenalty). Tired 이하 = 1.0, 노브는 DA_FatigueConfig.
	float GetFatigueOutputFactor() const;

	// 다운된 동안(꾸벅 텔레그래프 + 늘어짐) 내내 true — 손댈 수 있는 구간에는 항상 펄스 링이 떠 있어야 한다.
	UFUNCTION(BlueprintPure, Category = "Behavior|Fatigue")
	bool IsAwaitingCatch() const { return FatigueSlackPhase != EFatigueSlackPhase::None; }

	// 라이브 힌트 문구/키가 꾸벅(Telegraph·Slumping)과 이탈(Bolting)로 갈리므로 단계까지 노출한다.
	EFatigueSlackPhase GetFatigueSlackPhase() const { return FatigueSlackPhase; }

	//========================================
	// 일시 버프 (이벤트 효과)
	//========================================

	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Buff")
	TArray<FEmployeeBuff> ActiveBuffs;

	UPROPERTY(BlueprintAssignable, Category = "Behavior|Buff")
	FOnBuffChanged OnBuffChanged;

	/** 버프 추가 (이미 같은 Type이 있으면 RemainingTime을 더 긴 쪽으로 갱신) */
	UFUNCTION(BlueprintCallable, Category = "Behavior|Buff")
	void AddBuff(EBuffType Type, float Value, float Duration);

	/** 활성 버프 중 가장 큰 ScoreMultiplier 값 반환 (없으면 1.0) */
	UFUNCTION(BlueprintPure, Category = "Behavior|Buff")
	float GetTotalScoreMultiplier() const;

	/** 활성 버프 중 CritChance 보너스 합산 (없으면 0.0) */
	UFUNCTION(BlueprintPure, Category = "Behavior|Buff")
	float GetTotalCritChanceBonus() const;

	/** 활성 버프 중 최소 WorkSpeed 배율 반환 (없으면 1.0) */
	UFUNCTION(BlueprintPure, Category = "Behavior|Buff")
	float GetWorkSpeedMultiplier() const;

	/** 활성 버프가 하나라도 있는지 (UI 표시용) */
	UFUNCTION(BlueprintPure, Category = "Behavior|Buff")
	bool HasActiveBuff() const;

private:
	void TickBuffs(float DeltaTime);
public:

	//========================================
	// Work/Rest 사이클 설정값 (Stamina 기반)
	//========================================

	// 기본 업무 지속 시간 — 실제 WorkDuration = Base × (1 + Stamina × 0.02)
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Cycle Settings")
	float BaseWorkDuration = 30.0f;

	// 기본 휴식 지속 시간 — 실제 RestDuration = Base / (1 + Stamina × 0.01)
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Cycle Settings")
	float BaseRestDuration = 8.0f;

	//========================================
	// 수익 설정값 (EditDefaultsOnly)
	//========================================

	// Operation 모드 전용: 프로젝트 수익을 직원 수로 나눈 1인당 기본 수익
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Income")
	float OperationIncomePerSecond = 0.0f;

	//========================================
	// 배회 설정값 (EditDefaultsOnly)
	//========================================

	// 분당 배회 확률 (0~1)
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Wander Settings")
	float BaseWanderChancePerMinute = 0.3f;

	// 현재 배회 목적지
	UPROPERTY(BlueprintReadOnly, Category = "Behavior|Wander")
	EWanderDestination CurrentWanderDestination = EWanderDestination::RandomPoint;

	//========================================
	// 댄스 설정값 (EditDefaultsOnly)
	//========================================

	// 사용 가능한 댄스 클립 수 (AnimBP Blend Poses by Int 핀 수와 일치시킬 것). 랜덤 인덱스 범위.
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Dance Settings")
	int32 NumDanceAnimations = 10;

	// StartDance 에 Duration 미지정(<0) 시 기본 지속 시간(초). 루프 클립을 이 시간 동안 계속 춘 뒤 Wander 복귀.
	// 춤은 "특별한 상황"에서만 명시 호출 — 상시 idle 랜덤 댄스는 없음(상황/이벤트가 트리거).
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Dance Settings")
	float DanceDefaultDuration = 8.0f;

	//========================================
	// Operation 모드 설정값 (EditDefaultsOnly)
	//========================================

	// Operation 모드: 오피스 직접 관리 시 수익 배율. 1.0 = 직원 지급 합 ≈ 표시 수익/초 (2.0 이면 표시와 2배 괴리)
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Operation Settings")
	float OperationDirectManageMultiplier = 1.0f;

	//========================================
	// 이동 속도 설정값 (EditDefaultsOnly)
	//========================================

	// 평상시 MaxWalkSpeed(cm/s). 클립이 전부 in-place 라 BS_StickLocomotion 의 걷기 샘플과 같은 값이어야 발이 안 미끄러진다
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Move Speed")
	float MoveSpeedWalk = 125.0f;

	// 번아웃 배회(Bolting) 전용 속도 — 직원이 뛰는 상황은 이것뿐이다(Stage 한정).
	// BS_StickLocomotion 러닝 샘플(느린250/보통400/빠른550) 구간 안에 두어야 러닝 모션이 나온다.
	UPROPERTY(EditDefaultsOnly, Category = "Behavior|Move Speed")
	float MoveSpeedBolt = 450.0f;

	// 현재 슬랙 페이즈에 맞는 MaxWalkSpeed 적용 (Bolting = 뛰기, 그 외 = 걷기).
	// public 인 이유 = 치트(WorkerSpeed -1)가 원복에 사용. BP 노출은 불필요.
	void ApplyMoveSpeed();

	// 치트 전용: 피로 누적을 기다리지 않고 번아웃 배회를 즉시 발동 (러닝 모션 확인용)
	void ForceSlackBolt() { EnterSlackBolt(); }

	//========================================
	// 델리게이트
	//========================================

	// 상태 변경 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Behavior|Events")
	FOnEmployeeStateChanged OnStateChanged;

	// 인사 완료 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Behavior|Events")
	FOnGreetingFinished OnGreetingFinished;

	//========================================
	// 주요 함수
	//========================================

	// 상태 변경
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetState(EEmployeeState NewState);

	// 현재 상태 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Behavior")
	EEmployeeState GetState() const { return EmployeeState; }

	// Operation 모드 진입 시 1인당 수익 설정
	void SetOperationIncome(float IncomePerSecond);

	// 현재 모드/상태 기준 초당 수익 요율. GenerateIncome 의 적립·지급이 이 값을 쓰므로
	// 표시(수익 칩)가 여기를 읽으면 지급과 갈릴 수 없다. 앉아 쉬는 직원은 0.
	float GetCurrentIncomePerSecond() const;

	// 배회 시작
	UFUNCTION(BlueprintCallable, Category = "Behavior|Wander")
	void StartWander();

	// 업무공간으로 복귀
	UFUNCTION(BlueprintCallable, Category = "Behavior|Wander")
	void ReturnToWorkstation();

	// 행동 패턴 모드 변경
	UFUNCTION(BlueprintCallable, Category = "Behavior|Mode")
	void SetBehaviorMode(EEmployeeBehaviorMode NewMode);

	// 현재 행동 패턴 모드 반환
	UFUNCTION(BlueprintPure, Category = "Behavior|Mode")
	EEmployeeBehaviorMode GetBehaviorMode() const { return CurrentBehaviorMode; }

	// 앉아서 환호 (Step 성공 시)
	UFUNCTION(BlueprintCallable, Category = "Behavior|Cheer")
	void StartCheerSitting();

	// 일어서며 환호 (출시 성공 시)
	UFUNCTION(BlueprintCallable, Category = "Behavior|Cheer")
	void StartCheerStandUp();

	// 환호 애니메이션 완료 시 호출 (AnimNotify에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Behavior|Cheer")
	void OnCheerAnimationComplete();

	//========================================
	// 전환 애니메이션 완료 콜백 (AnimNotify에서 호출)
	//========================================

	UFUNCTION(BlueprintCallable, Category = "Behavior|Animation")
	void OnTypeToSitComplete();

	UFUNCTION(BlueprintCallable, Category = "Behavior|Animation")
	void OnSitToStandComplete();

	UFUNCTION(BlueprintCallable, Category = "Behavior|Animation")
	void OnStandToSitComplete();

	// 인사 시작 (RecruitmentGameMode에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Behavior|Greeting")
	void StartGreeting();

	// 인사 완료 콜백 (AnimNotify에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Behavior|Greeting")
	void OnGreetingComplete();

	// 춤 시작 — "특별한 상황"(채용 환영/이벤트 등)에서 명시 호출. DanceIndex<0 이면 랜덤 클립.
	// Duration<0 이면 DanceDefaultDuration. 루프 클립을 그 시간 동안 계속 춘 뒤 Wander 복귀(또는 StopDance).
	// AnimBP 에 Dance 상태 필요(없으면 멈춤) — 에셋 준비 전엔 자동 호출 금지.
	UFUNCTION(BlueprintCallable, Category = "Behavior|Dance")
	void StartDance(int32 DanceIndex = -1, float Duration = -1.0f);

	// 춤 즉시 중단 → Wander 복귀
	UFUNCTION(BlueprintCallable, Category = "Behavior|Dance")
	void StopDance();

	// 침착성 반영 실효 폭주 확률(초당) — 개발 중 매초 독립 굴림. 피로 사슬과 무관하다.
	// 치트(BoltStatus)가 공식을 재구현하지 않고 이 값을 그대로 읽으려 공개.
	float GetEffectiveBoltChance() const;

	// 간격/산출 공통 배수 — 유효업무속도(스탯+★) 와 WorkSpeed 버프가 모이는 단일 지점.
	// 초당 산출이 이 값에 비례하므로 피치 카드 추정기가 공식을 재구현하지 않고 그대로 읽으려 공개.
	float GetSpeedFactor() const;

	/** 직원 고유 속도에 책상 누적 속도율을 정확히 한 번 합성한다. */
	static float ComposeWorkstationSpeedFactor(float EmployeeSpeedFactor, float WorkstationRate);

	// 점수 정규화 기준 — **케이던스와 분리된 고정 상수**. 간격이 짧아진 만큼 초당 산출이 오르게 하는 축.
	// 케이던스(BaseWorkInterval)와 같은 값을 쓰면 점수가 그 값의 제곱에 비례해, 히트 수를 2배로 늘리는
	// 순간 DPS 가 절반이 된다. 분리해 두면 케이던스만 자유롭게 조정할 수 있다(현 DPS 유지 = 1.5).
	// 피치 카드 추정기(FStageProgressData::EstimateProjectOutlook)가 초당 산출을 역산하려 읽으므로 공개.
	static constexpr float ScoreNormInterval = 1.5f;

	// 이산 기여 간격 상수 (케이던스). Base 는 곧 ★0 의 간격이다.
	// 2026-07-31: 1.5 → 0.75. **출시 게이트 대응** — MeetsMinimumClearScore 는 활성 직능 스텝이
	// 각자 목표의 50% 이상이어야 하는 AND 조건인데, 15초 판의 기여 히트가 총 10.5회뿐이라
	// 5~6개 스텝 중 하나가 히트 0회일 확률이 56.5% 였다(= 점수 0 → 출시 영구 차단, 통과율 1.4%).
	// 히트 수는 크기 노브로 못 늘린다(OutputUnit 을 3배 올려도 통과율 30%에서 포화) → 케이던스로 푼다.
	// 2026-08-13: 0.75 → 2.0. 성장 여지를 넓히려 ★0 을 2초로 되돌렸다(★15 = 0.50초).
	// 초당 산출은 안 변한다 — Overflow = SF × WorkInterval 이 Base 로 수렴해 점수가 같이 커지고,
	// 바뀌는 건 리듬(1회 2오브·빈도 절반)과 히트 분산뿐이다. ⚠ 위 0히트 위험이 직원 1명 구간에서 되살아난다.
	// 스탯 표기(UEmployeeStatsHelper::GetStatFeltText)가 간격을 역산하려 읽으므로 공개.
	static constexpr float BaseWorkInterval = 2.0f;
	static constexpr float MinWorkInterval = 0.2f;

private:
	//========================================
	// 내부 로직 함수
	//========================================

	// 업무 상태 틱 처리
	void TickWorking(float DeltaTime);

	// 휴식 상태 틱 처리
	void TickRest(float DeltaTime);

	// 배회 상태 틱 처리
	void TickWander(float DeltaTime);

	// 상태별 수익 배율 반환
	float GetIncomeMultiplier() const;

	// 수익 발생 처리
	void GenerateIncome(float DeltaTime);

	// Stamina 기반 Work 지속 시간 (초) — 체력 높으면 오래 일함
	float GetWorkDuration() const;

	// Stamina 기반 Rest 지속 시간 (초) — 체력 높으면 짧게 쉼
	float GetRestDuration() const;

	// Owner 직원의 Stamina 스탯 조회 — EmployeeManager 경유, 실패 시 0
	int32 GetOwnerStamina() const;

	// Idle 모드로 전환 시 애니메이션 시퀀스 실행
	void StartIdleTransitionSequence();

	// Idle 배회 시작 (전환 시퀀스 완료 후 호출)
	void StartIdleWander();

	// Operation 착석 업무 시작 — 자리 텔레포트+착석+Typing+FloatingText. 상시 배회 타이머는 무장 안 함.
	// (자리이탈은 피로 슬랙 루프가 전담)
	void StartOperationSeatedWork();

	// 전환 애니메이션 폴백 타이머 (AnimNotify 실패 대비)
	FTimerHandle TransitionFallbackTimerHandle;
	void OnTransitionFallbackTimeout();

	// 댄스 지속 타이머 (루프 클립이라 노티파이 없음 → 시간 경과로 종료) + 완료 콜백
	FTimerHandle DanceTimerHandle;
	void OnDanceComplete();

	//========================================
	// FloatingText 관련 (Stage/Operation 모드 점수/수익 표시)
	//========================================

	// FloatingText 타이머 핸들
	FTimerHandle FloatingTextTimerHandle;

	// Stage 모드 첫 기여 타이머 여부 (스태거 오프셋용)
	bool bIsFirstStageTimer = false;

	// 1회 배출 orb 개수 상한 (성능). 초과분은 개당 점수로 흡수되므로 산출은 상한 없음
	static constexpr int32 MaxOrbPerEmission = 5;

	// 직원 기여 간격 계산 (스탯+버프 기반)
	float CalculateWorkInterval() const;

	// FloatingText 타이머 시작
	void StartFloatingTextTimer();

	// FloatingText 타이머 중지
	void StopFloatingTextTimer();

	// 점수 FloatingText 스폰
	void SpawnScoreFloatingText();

	// 기여 간격 반환 (Stage: 직원별 WorkInterval, Operation: 3~5초)
	float GetRandomTextInterval() const;

	//========================================
	// 피로도 내부
	//========================================

	// 전역 튜닝 (하드참조, 생성자에서 로드). 에셋 미존재 시 null → 모든 사용처 가드.
	UPROPERTY()
	UFatigueConfig* FatigueConfig = nullptr;

	// 라이프사이클 동결 플래그 (결과 모달 중 피로 정지)
	bool bFatigueFrozenByLifecycle = false;

	// 매프레임 피로 누적/회복/표시 (기존 모드 로직과 직교)
	void TickFatigue(float DeltaTime);

	// 피로 슬랙 루프 — Slacking → 꾸벅(텔레그래프=캐치 윈도우) → 미캐치 시 자리에서 늘어짐 → 업무 복귀.
	// 폭주(Bolting)는 이 사슬의 끝이 아니라 TickBoltRoll 이 독립으로 소유한다(체력=조는 것 / 침착성=뛰쳐나감).
	// 활성(Telegraph/Slumping/Bolting)이면 true 반환 → TickComponent 가 일반 사이클을 스킵(워커 점유).
	EFatigueSlackPhase FatigueSlackPhase = EFatigueSlackPhase::None;

	// 이번 다운의 사유 — 착석 자세와 레일 알림 문구가 이 하나를 같이 읽는다(진입 시 1회 결정).
	EWorkerDownReason SlackReason = EWorkerDownReason::Drowsy;

	float SlackTelegraphTimer = 0.0f;
	float SlackSlumpTimer = 0.0f;
	float SlackBoltTimer = 0.0f;
	bool TickFatigueSlack(float DeltaTime);
	void EnterSlackTelegraph();
	void EnterSlackSlump();
	void EnterSlackBolt();
	void ExitSlackToWork();

	// 폭주 굴림 — 피로 사슬과 독립. 개발 중 매초 1회 침착성 판정(1초 누산으로 프레임레이트 무관).
	float BoltRollTimer = 0.0f;
	void TickBoltRoll(float DeltaTime);

	// 레일 알림 발행/해제 — EmployeeManager 단일 채널 경유(N명이 이 한 채널로 모여 OfficeMain 이 1회만 구독).
	// 표시 시간은 DA 의 페이즈 길이에서 계산 — 튜닝하면 알림 수명도 같이 따라간다.
	void NotifyRailDown(EWorkerDownReason Reason);
	void NotifyRailRecovered();
};
