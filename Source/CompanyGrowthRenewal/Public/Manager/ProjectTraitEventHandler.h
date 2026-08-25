#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Enum/ProjectMode.h"
#include "Enum/ProjectTrait.h"
#include "Enum/ProjectEvent.h"
#include "Enum/CompanyType.h"
#include "Data/ProjectTraitData.h"
#include "Data/ProjectEventData.h"
#include "Data/EmployeeBuff.h"
#include "ProjectTraitEventHandler.generated.h"

struct FStageProgressData;

// 이벤트 트리거 시 UI에 팝업 요청
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectEventTriggered, const FProjectEventData&, EventData);

// 이벤트 일시정지 해제
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEventPauseEnded);

/**
 * 프로젝트 특성/이벤트 핸들러
 *
 * OfficeStageProgressManager가 소유하는 UObject
 * - 보드 생성 시 특성 부여 (AssignRandomTraits)
 * - 타이머 실행 중 이벤트 스케줄링/트리거 (PrepareEvents, CheckEventTrigger)
 * - 이벤트 선택지 효과 적용 (ApplyEventChoice)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProjectTraitEventHandler : public UObject
{
	GENERATED_BODY()

public:
	UProjectTraitEventHandler();

	// ===== 트레이트 API =====

	/**
	 * 랜덤 특성 부여
	 * 티어(ProjectIndex 기반) + 모드(수주/자체개발)에 따라 풀에서 추첨
	 */
	void AssignRandomTraits(ECompanyType CompanyType, int32 ProjectIndex, EProjectMode Mode);

	/** 현재 활성 트레이트 목록 */
	const TArray<FProjectTraitData>& GetActiveTraits() const { return ActiveTraits; }

	/** 트레이트 반영된 실효 타이머 시간 (기본 15초) */
	float GetEffectiveTimerDuration(float DefaultDuration = 15.0f) const;

	/** 요구 점수 배율 (모든 트레이트 곱셈) */
	float GetRequiredScoreMultiplier() const;

	/** 보상 배율 (모든 트레이트 곱셈) */
	float GetRewardMultiplier() const;

	/** 기본 점수 배율 (혁신 요구 등) */
	float GetBaseScoreMultiplier() const;

	/**
	 * 카테고리 가중치 (기본: 기획20/개발50/QA30)
	 * Focus 계열 트레이트가 있으면 오버라이드
	 */
	void GetCategoryWeights(float& OutPlanning, float& OutDev, float& OutQA) const;

	/** 랜덤 변동값 반환 (트레이트 적용, 안정지향이면 1.0 고정) */
	float GetRandomVariance() const;

	/** 크리티컬 배율 (기본 2.0, 혁신 요구 시 3.0) */
	float GetCriticalMultiplier(float DefaultMultiplier = 2.0f) const;

	/** 크리티컬 확률 배율 (기본 1.0, 트렌드 탑승 시 2.0) */
	float GetCriticalChanceMultiplier() const;

	// ===== 이벤트 API =====

	/**
	 * 이벤트 준비 (타이머 시작 전 호출)
	 * 모드/산업/티어에 따라 1~2개 이벤트 선택 + 트리거 시점 결정
	 */
	void PrepareEvents(ECompanyType CompanyType, int32 ProjectIndex, EProjectMode Mode);

	/**
	 * 이벤트 트리거 체크 (매 틱 호출)
	 * @param ElapsedRatio 타이머 경과 비율 (0.0~1.0)
	 * @return true면 이벤트 발동, UI에서 팝업 표시해야 함
	 */
	bool CheckEventTrigger(float ElapsedRatio);

	/** 현재 대기 중인 이벤트 데이터 */
	const FProjectEventData& GetPendingEvent() const { return CurrentPendingEvent; }

	/**
	 * 이벤트 선택지 효과 적용
	 * @param ChoiceIndex 선택한 선택지 (0 또는 1)
	 * @param ProgressData 현재 스테이지 진행 데이터 (직접 수정)
	 */
	void ApplyEventChoice(int32 ChoiceIndex, FStageProgressData& ProgressData);

	// ===== 일시정지 관리 =====

	bool IsEventPaused() const { return bEventPaused; }
	void SetEventPaused(bool bPaused);
	float GetRemainingPauseDuration() const { return PauseRemaining; }
	void TickPause(float DeltaTime);

	// ===== 초기화 =====

	/** 프로젝트 종료 시 모든 상태 초기화 */
	void ClearAll();

	// ===== 델리게이트 =====

	UPROPERTY(BlueprintAssignable, Category = "TraitEvent")
	FOnProjectEventTriggered OnProjectEventTriggered;

	UPROPERTY(BlueprintAssignable, Category = "TraitEvent")
	FOnEventPauseEnded OnEventPauseEnded;

private:
	// 활성 트레이트
	TArray<FProjectTraitData> ActiveTraits;

	// 스케줄된 이벤트
	TArray<FProjectEventData> ScheduledEvents;
	TArray<float> EventTriggerTimings;
	int32 NextEventIndex = 0;

	// 이벤트 일시정지
	bool bEventPaused = false;
	float PauseRemaining = 0.0f;
	FProjectEventData CurrentPendingEvent;

	// 티어 번호 산출 (ProjectIndex → 1~10)
	static int32 GetTierFromProjectIndex(int32 ProjectIndex);

	// 티어별 특성 개수 결정
	static void GetTraitCountRange(int32 Tier, int32& OutMin, int32& OutMax);

	// 티어별 이벤트 개수 결정
	static void GetEventCountRange(int32 Tier, int32& OutMin, int32& OutMax);

	// 모드별 특성 풀 구성
	static TArray<EProjectTrait> BuildTraitPool(EProjectMode Mode, int32 Tier);

	// 이벤트 풀 구성 (모드 + 산업 필터)
	static TArray<EProjectEvent> BuildEventPool(EProjectMode Mode, ECompanyType CompanyType, int32 Tier);

	// 이벤트 효과로 랜덤 N명 직원에게 일시 버프 부여
	void ApplyBuffToRandomWorkers(EBuffType Type, float Value, float Duration, int32 TargetCount);
};
