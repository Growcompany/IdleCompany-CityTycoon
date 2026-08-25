#pragma once

#include "CoreMinimal.h"
#include "Enum/ProjectEvent.h"
#include "Enum/ChoiceArchetype.h"
#include "Data/EmployeeBuff.h"
#include "ProjectEventData.generated.h"

/**
 * 이벤트 선택지
 * 각 이벤트에 보통 2개씩 제공
 */
USTRUCT(BlueprintType)
struct FProjectEventChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event|Choice")
	FText ChoiceText;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Choice")
	FText EffectDescription;

	// 카드 아이콘 결정용 행동 아키타입 (DT_EventChoiceIcon 조회 키)
	UPROPERTY(BlueprintReadOnly, Category = "Event|Choice")
	EChoiceArchetype Archetype = EChoiceArchetype::None;

	// 카테고리별 점수 보너스 (비율, 0.3 = +30%, -0.3 = -30%)
	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float PlanningScoreBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float DevScoreBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float QAScoreBonus = 0.0f;

	// 타이머 증감 (초, 양수 = 시간 추가)
	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float TimerAdjustment = 0.0f;

	// 일시 정지 시간 (초, 0 = 정지 없음)
	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float PauseDuration = 0.0f;

	// 랜덤 카테고리에 페널티 적용
	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	bool bRandomCategoryPenalty = false;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float RandomPenaltyAmount = 0.0f;

	// === 직원 일시 버프 (None이면 미적용) ===
	UPROPERTY(BlueprintReadOnly, Category = "Event|Buff")
	EBuffType BuffType = EBuffType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Buff")
	float BuffValue = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Buff")
	float BuffDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Buff")
	int32 BuffTargetCount = 3;

	// === 보상 배율 (1.0 = 변화 없음, 0.8 = 보상 -20%) ===
	// 정산 시 ProgressData.TraitRewardMultiplier 에 곱해져 수주 계약금/자체개발 수익에 반영
	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	float RewardMultiplier = 1.0f;

	// 이 선택지 선택 시 피버타임 발동 (전역 산출/크리율 일시 폭증 — dev-spectacle)
	UPROPERTY(BlueprintReadOnly, Category = "Event|Effect")
	bool bTriggerFever = false;

	FProjectEventChoice()
		: PlanningScoreBonus(0.0f)
		, DevScoreBonus(0.0f)
		, QAScoreBonus(0.0f)
		, TimerAdjustment(0.0f)
		, PauseDuration(0.0f)
		, bRandomCategoryPenalty(false)
		, RandomPenaltyAmount(0.0f)
	{}
};

/**
 * 프로젝트 중간 이벤트 데이터
 * MakeEventData() 팩토리로 enum에서 생성
 */
USTRUCT(BlueprintType)
struct FProjectEventData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	EProjectEvent EventType = EProjectEvent::None;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FText EventTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FText EventDescription;

	// 산업 필터 (None = 모든 산업)
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	ECompanyType RequiredCompanyType = ECompanyType::None;

	// 모드 필터 (None = 양쪽 모두)
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	EProjectMode RequiredMode = EProjectMode::None;

	// 선택지 (보통 2개)
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	TArray<FProjectEventChoice> Choices;

	// 트리거 가능 타이머 진행률 범위 (0.0~1.0)
	UPROPERTY(BlueprintReadOnly, Category = "Event|Trigger")
	float MinTimerProgress = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "Event|Trigger")
	float MaxTimerProgress = 0.9f;

	FProjectEventData()
		: EventType(EProjectEvent::None)
		, RequiredCompanyType(ECompanyType::None)
		, RequiredMode(EProjectMode::None)
		, MinTimerProgress(0.1f)
		, MaxTimerProgress(0.9f)
	{}

	static FProjectEventData MakeEventData(EProjectEvent Event)
	{
		FProjectEventData Data;
		Data.EventType = Event;
		Data.EventTitle = FText::FromString(ProjectEventToString(Event));
		Data.RequiredCompanyType = GetEventCompanyFilter(Event);
		Data.RequiredMode = GetEventModeFilter(Event);

		FProjectEventChoice ChoiceA, ChoiceB, ChoiceC;
		// 기본값: ChoiceC는 직원 버프형으로 채워짐 (각 case에서 설정)

		switch (Event)
		{
		case EProjectEvent::BugFound:
			Data.EventDescription = FText::FromString(TEXT("치명적 버그가 발견되었습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("즉시 수정"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("QA +30"));
			ChoiceA.QAScoreBonus = 30.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("무시하고 진행"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("시간 +3초"));
			ChoiceB.TimerAdjustment = 3.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("핫픽스 팀 가동"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("3명 작업속도↑ 8초"));
			ChoiceC.BuffType = EBuffType::WorkSpeed; ChoiceC.BuffValue = 0.6f; ChoiceC.BuffDuration = 8.0f; ChoiceC.BuffTargetCount = 3;
			break;

		case EProjectEvent::GoodIdea:
			Data.EventDescription = FText::FromString(TEXT("팀원이 좋은 아이디어를 냈습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("채택"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("기획 +50, 개발 -20"));
			ChoiceA.PlanningScoreBonus = 50.0f;
			ChoiceA.DevScoreBonus = -20.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("기각"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("변화 없음"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("팀에 영감"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("3명 점수×1.5 10초"));
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.5f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 3;
			break;

		case EProjectEvent::TeamConflict:
			Data.EventDescription = FText::FromString(TEXT("팀원 간 갈등이 발생했습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("중재"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("전체 -10, 2초 정지"));
			ChoiceA.PlanningScoreBonus = -10.0f;
			ChoiceA.DevScoreBonus = -10.0f;
			ChoiceA.QAScoreBonus = -10.0f;
			ChoiceA.PauseDuration = 2.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("방치"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("랜덤 카테고리 -30"));
			ChoiceB.bRandomCategoryPenalty = true;
			ChoiceB.RandomPenaltyAmount = -30.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("회식 약속"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("전체 -5, 2명 점수×1.8 12초"));
			ChoiceC.PlanningScoreBonus = -5.0f; ChoiceC.DevScoreBonus = -5.0f; ChoiceC.QAScoreBonus = -5.0f;
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.8f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 2;
			break;

		case EProjectEvent::OvertimeRequest:
			Data.EventDescription = FText::FromString(TEXT("야근을 요청할까요?"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("야근 수락"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("전체 +20"));
			ChoiceA.PlanningScoreBonus = 20.0f;
			ChoiceA.DevScoreBonus = 20.0f;
			ChoiceA.QAScoreBonus = 20.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("거절"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("변화 없음"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("자율 야근"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("4명 작업속도↑ 15초"));
			ChoiceC.BuffType = EBuffType::WorkSpeed; ChoiceC.BuffValue = 0.7f; ChoiceC.BuffDuration = 15.0f; ChoiceC.BuffTargetCount = 4;
			break;

		case EProjectEvent::TechDebt:
			Data.EventDescription = FText::FromString(TEXT("기술 부채가 발견되었습니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("지금 정리"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("개발 -30, 추가 이벤트 없음"));
			ChoiceA.DevScoreBonus = -30.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("나중에"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("변화 없음"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("페어 프로그래밍"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("개발 +15, 2명 크리율↑ 10초"));
			ChoiceC.DevScoreBonus = 15.0f;
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.4f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 2;
			break;

		case EProjectEvent::ClientRevision:
			Data.EventDescription = FText::FromString(TEXT("클라이언트가 수정을 요청했습니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("수용"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("기획 +40, 시간 -2초"));
			ChoiceA.PlanningScoreBonus = 40.0f;
			ChoiceA.TimerAdjustment = -2.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("거절"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("QA +20"));
			ChoiceB.QAScoreBonus = 20.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("협상"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("기획 +15, 3명 점수×1.4 8초"));
			ChoiceC.PlanningScoreBonus = 15.0f;
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.4f; ChoiceC.BuffDuration = 8.0f; ChoiceC.BuffTargetCount = 3;
			break;

		case EProjectEvent::ClientCancellation:
			Data.EventDescription = FText::FromString(TEXT("클라이언트가 계약 취소를 언급합니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("설득"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("전체 +15"));
			ChoiceA.PlanningScoreBonus = 15.0f;
			ChoiceA.DevScoreBonus = 15.0f;
			ChoiceA.QAScoreBonus = 15.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("양보"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("시간 +4초, 전체 -10"));
			ChoiceB.TimerAdjustment = 4.0f;
			ChoiceB.PlanningScoreBonus = -10.0f;
			ChoiceB.DevScoreBonus = -10.0f;
			ChoiceB.QAScoreBonus = -10.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("올스타 투입"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("5명 점수×1.6 10초"));
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.6f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 5;
			break;

		case EProjectEvent::MarketShift:
			Data.EventDescription = FText::FromString(TEXT("시장 트렌드가 변화하고 있습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("방향 전환"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("기획 -20, 개발 +40"));
			ChoiceA.PlanningScoreBonus = -20.0f;
			ChoiceA.DevScoreBonus = 40.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("유지"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("변화 없음"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("트렌드 학습"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("3명 크리율↑ 12초"));
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.3f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 3;
			break;

		case EProjectEvent::InvestorInterest:
			Data.EventDescription = FText::FromString(TEXT("투자자가 관심을 보입니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("프레젠테이션"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("전체 +25, 시간 -3초"));
			ChoiceA.PlanningScoreBonus = 25.0f;
			ChoiceA.DevScoreBonus = 25.0f;
			ChoiceA.QAScoreBonus = 25.0f;
			ChoiceA.TimerAdjustment = -3.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("무시"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("변화 없음"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("데모 시연"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("4명 점수×1.5 10초"));
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.5f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 4;
			break;

		case EProjectEvent::PublisherFeedback:
			Data.EventDescription = FText::FromString(TEXT("퍼블리셔의 피드백이 도착했습니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("반영"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("기획 +40"));
			ChoiceA.PlanningScoreBonus = 40.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("거절"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("QA +20"));
			ChoiceB.QAScoreBonus = 20.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("리뷰 워크샵"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("3명 작업속도↑ 10초"));
			ChoiceC.BuffType = EBuffType::WorkSpeed; ChoiceC.BuffValue = 0.65f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 3;
			break;

		case EProjectEvent::RegulationChange:
			Data.EventDescription = FText::FromString(TEXT("금융 규제가 변경되었습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("즉시 대응"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("QA +40, 시간 -2초"));
			ChoiceA.QAScoreBonus = 40.0f;
			ChoiceA.TimerAdjustment = -2.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("무시"));
			ChoiceB.EffectDescription = FText::FromString(TEXT("보상 -20%"));
			ChoiceB.RewardMultiplier = 0.8f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("법무팀 협업"));
			ChoiceC.EffectDescription = FText::FromString(TEXT("QA +15, 2명 크리율↑ 10초"));
			ChoiceC.QAScoreBonus = 15.0f;
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.4f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 2;
			break;

		case EProjectEvent::ServerIssue:
			Data.EventDescription = FText::FromString(TEXT("서버 장애가 발생했습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("즉시 수정"));
			ChoiceA.DevScoreBonus = 40.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("우회"));
			ChoiceB.TimerAdjustment = 2.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("DevOps 동원"));
			ChoiceC.DevScoreBonus = 10.0f;
			ChoiceC.BuffType = EBuffType::WorkSpeed; ChoiceC.BuffValue = 0.6f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 4;
			break;

		// === 신규 공통 이벤트 7개 ===

		case EProjectEvent::PowerOutage:
			Data.EventDescription = FText::FromString(TEXT("정전이 발생했습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("백업 발전기"));
			ChoiceA.PlanningScoreBonus = -10.f; ChoiceA.DevScoreBonus = -10.f; ChoiceA.QAScoreBonus = -10.f;
			ChoiceA.TimerAdjustment = 4.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("기다린다"));
			ChoiceB.PauseDuration = 3.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("디저트 회식"));
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.3f; ChoiceC.BuffDuration = 8.0f; ChoiceC.BuffTargetCount = 99;
			break;

		case EProjectEvent::NewRecruit:
			Data.EventDescription = FText::FromString(TEXT("신입이 합류했습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("교육에 시간 투자"));
			ChoiceA.TimerAdjustment = -3.0f;
			ChoiceA.BuffType = EBuffType::WorkSpeed; ChoiceA.BuffValue = 0.7f; ChoiceA.BuffDuration = 12.0f; ChoiceA.BuffTargetCount = 99;
			ChoiceB.ChoiceText = FText::FromString(TEXT("실전 투입"));
			ChoiceB.PlanningScoreBonus = 10.f; ChoiceB.DevScoreBonus = 10.f; ChoiceB.QAScoreBonus = 10.f;
			ChoiceB.bRandomCategoryPenalty = true; ChoiceB.RandomPenaltyAmount = -15.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("멘토 페어링"));
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.4f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 2;
			break;

		case EProjectEvent::CoffeeTime:
			Data.EventDescription = FText::FromString(TEXT("커피 한 잔의 여유"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("같이 한 잔"));
			ChoiceA.PlanningScoreBonus = 10.f; ChoiceA.DevScoreBonus = 10.f; ChoiceA.QAScoreBonus = 10.f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("계속 일"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("브레인스토밍"));
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.3f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 99;
			break;

		case EProjectEvent::Burnout:
			Data.EventDescription = FText::FromString(TEXT("팀이 번아웃 위기입니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("강행"));
			ChoiceA.PlanningScoreBonus = -15.f; ChoiceA.DevScoreBonus = -15.f; ChoiceA.QAScoreBonus = -15.f;
			ChoiceA.PauseDuration = 2.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("방치"));
			ChoiceB.bRandomCategoryPenalty = true; ChoiceB.RandomPenaltyAmount = -25.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("재충전"));
			ChoiceC.PauseDuration = 3.0f;
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.5f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 99;
			break;

		case EProjectEvent::SuddenSuccess:
			Data.EventDescription = FText::FromString(TEXT("깜짝 성과가 나왔습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("피버 타임!"));
			ChoiceA.EffectDescription = FText::FromString(TEXT("전원 폭주 — 산출·크리율 일시 폭증"));
			ChoiceA.PlanningScoreBonus = 30.f; ChoiceA.DevScoreBonus = 30.f; ChoiceA.QAScoreBonus = 30.f;
			ChoiceA.bTriggerFever = true;
			ChoiceB.ChoiceText = FText::FromString(TEXT("기록만"));
			ChoiceB.PlanningScoreBonus = 10.f; ChoiceB.DevScoreBonus = 10.f; ChoiceB.QAScoreBonus = 10.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("에이스 푸시"));
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 2.0f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 1;
			break;

		case EProjectEvent::MentorVisit:
			Data.EventDescription = FText::FromString(TEXT("외부 멘토가 방문했습니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("코드 리뷰"));
			ChoiceA.DevScoreBonus = 25.f; ChoiceA.QAScoreBonus = 15.f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("기획 컨설팅"));
			ChoiceB.PlanningScoreBonus = 30.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("팀 멘토링"));
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.5f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 99;
			break;

		case EProjectEvent::EquipmentUpgrade:
			Data.EventDescription = FText::FromString(TEXT("장비 업그레이드 기회"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("즉시 교체"));
			ChoiceA.DevScoreBonus = -20.f;
			ChoiceA.TimerAdjustment = -3.0f;
			ChoiceA.BuffType = EBuffType::WorkSpeed; ChoiceA.BuffValue = 0.65f; ChoiceA.BuffDuration = 15.0f; ChoiceA.BuffTargetCount = 99;
			ChoiceB.ChoiceText = FText::FromString(TEXT("나중에"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("일부만 교체"));
			ChoiceC.DevScoreBonus = -5.f;
			ChoiceC.BuffType = EBuffType::WorkSpeed; ChoiceC.BuffValue = 0.75f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 3;
			break;

		// === 신규 수주 전용 1개 ===

		case EProjectEvent::NegotiateExtension:
			Data.EventDescription = FText::FromString(TEXT("기한 협상이 가능합니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("기한 연장 요구"));
			ChoiceA.TimerAdjustment = 5.0f;
			ChoiceA.PlanningScoreBonus = -10.f; ChoiceA.DevScoreBonus = -10.f; ChoiceA.QAScoreBonus = -10.f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("그대로 진행"));
			ChoiceC.ChoiceText = FText::FromString(TEXT("기한 단축 + 보너스"));
			ChoiceC.TimerAdjustment = -3.0f;
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.6f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 99;
			break;

		// === 신규 자체개발 전용 1개 ===

		case EProjectEvent::Pivoting:
			Data.EventDescription = FText::FromString(TEXT("방향 전환을 검토합니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("과감한 피보팅"));
			ChoiceA.PlanningScoreBonus = -30.f; ChoiceA.DevScoreBonus = 50.f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("기존 방향 유지"));
			ChoiceB.QAScoreBonus = 15.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("프로토타입 병행"));
			ChoiceC.PlanningScoreBonus = 10.f; ChoiceC.DevScoreBonus = 10.f;
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.3f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 99;
			break;

		// === 신규 산업별 3개 ===

		case EProjectEvent::PlaytestFeedback:
			Data.EventDescription = FText::FromString(TEXT("플레이테스트 결과가 나왔습니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("재미 강화"));
			ChoiceA.PlanningScoreBonus = 25.f; ChoiceA.DevScoreBonus = 15.f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("버그 우선"));
			ChoiceB.QAScoreBonus = 35.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("긴급 패치 회의"));
			ChoiceC.PauseDuration = 2.0f;
			ChoiceC.BuffType = EBuffType::ScoreMultiplier; ChoiceC.BuffValue = 1.5f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 99;
			break;

		case EProjectEvent::SecurityAudit:
			Data.EventDescription = FText::FromString(TEXT("보안 감사가 진행됩니다"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("전면 협조"));
			ChoiceA.QAScoreBonus = 40.f;
			ChoiceA.TimerAdjustment = -2.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("회피"));
			ChoiceB.bRandomCategoryPenalty = true; ChoiceB.RandomPenaltyAmount = -30.f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("보안팀 강화"));
			ChoiceC.QAScoreBonus = 15.f;
			ChoiceC.BuffType = EBuffType::CritChance; ChoiceC.BuffValue = 0.4f; ChoiceC.BuffDuration = 10.0f; ChoiceC.BuffTargetCount = 3;
			break;

		case EProjectEvent::ScalingChallenge:
			Data.EventDescription = FText::FromString(TEXT("트래픽이 폭주했습니다!"));
			ChoiceA.ChoiceText = FText::FromString(TEXT("스케일 아웃"));
			ChoiceA.DevScoreBonus = 30.f;
			ChoiceA.TimerAdjustment = -2.0f;
			ChoiceB.ChoiceText = FText::FromString(TEXT("우회 처리"));
			ChoiceB.QAScoreBonus = 20.f;
			ChoiceB.TimerAdjustment = 2.0f;
			ChoiceC.ChoiceText = FText::FromString(TEXT("DevOps 풀파워"));
			ChoiceC.DevScoreBonus = 10.f;
			ChoiceC.BuffType = EBuffType::WorkSpeed; ChoiceC.BuffValue = 0.55f; ChoiceC.BuffDuration = 12.0f; ChoiceC.BuffTargetCount = 99;
			break;

		default:
			break;
		}

		// 선택지 행동 아키타입 (카드 아이콘 키 — DT_EventChoiceIcon). A/B/C 순서.
		// 규칙: 한 이벤트의 3선택지는 서로 다른 아키타입을 가져 카드 아이콘이 겹치지 않음.
		switch (Event)
		{
		case EProjectEvent::BugFound:           ChoiceA.Archetype = EChoiceArchetype::Fix;       ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::TeamRally; break;
		case EProjectEvent::GoodIdea:           ChoiceA.Archetype = EChoiceArchetype::Adopt;     ChoiceB.Archetype = EChoiceArchetype::Reject; ChoiceC.Archetype = EChoiceArchetype::Inspire;   break;
		case EProjectEvent::TeamConflict:       ChoiceA.Archetype = EChoiceArchetype::Negotiate; ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Party;     break;
		case EProjectEvent::OvertimeRequest:    ChoiceA.Archetype = EChoiceArchetype::Adopt;     ChoiceB.Archetype = EChoiceArchetype::Reject; ChoiceC.Archetype = EChoiceArchetype::Overtime;  break;
		case EProjectEvent::TechDebt:           ChoiceA.Archetype = EChoiceArchetype::Fix;       ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Train;     break;
		case EProjectEvent::ClientRevision:     ChoiceA.Archetype = EChoiceArchetype::Adopt;     ChoiceB.Archetype = EChoiceArchetype::Reject; ChoiceC.Archetype = EChoiceArchetype::Negotiate; break;
		case EProjectEvent::ClientCancellation: ChoiceA.Archetype = EChoiceArchetype::Negotiate; ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::TeamRally; break;
		case EProjectEvent::MarketShift:        ChoiceA.Archetype = EChoiceArchetype::Pivot;     ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Inspire;   break;
		case EProjectEvent::InvestorInterest:   ChoiceA.Archetype = EChoiceArchetype::Plan;      ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::TeamRally; break;
		case EProjectEvent::PublisherFeedback:  ChoiceA.Archetype = EChoiceArchetype::Adopt;     ChoiceB.Archetype = EChoiceArchetype::Reject; ChoiceC.Archetype = EChoiceArchetype::Train;     break;
		case EProjectEvent::RegulationChange:   ChoiceA.Archetype = EChoiceArchetype::Fix;       ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Shield;    break;
		case EProjectEvent::ServerIssue:        ChoiceA.Archetype = EChoiceArchetype::Fix;       ChoiceB.Archetype = EChoiceArchetype::Pivot;  ChoiceC.Archetype = EChoiceArchetype::TeamRally; break;
		case EProjectEvent::PowerOutage:        ChoiceA.Archetype = EChoiceArchetype::Infra;     ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Party;     break;
		case EProjectEvent::NewRecruit:         ChoiceA.Archetype = EChoiceArchetype::Train;     ChoiceB.Archetype = EChoiceArchetype::Push;   ChoiceC.Archetype = EChoiceArchetype::TeamRally; break;
		case EProjectEvent::CoffeeTime:         ChoiceA.Archetype = EChoiceArchetype::Coffee;    ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Inspire;   break;
		case EProjectEvent::Burnout:            ChoiceA.Archetype = EChoiceArchetype::Overtime;  ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Coffee;    break;
		case EProjectEvent::SuddenSuccess:      ChoiceA.Archetype = EChoiceArchetype::Fever;     ChoiceB.Archetype = EChoiceArchetype::Plan;   ChoiceC.Archetype = EChoiceArchetype::Push;      break;
		case EProjectEvent::MentorVisit:        ChoiceA.Archetype = EChoiceArchetype::QAReview;  ChoiceB.Archetype = EChoiceArchetype::Plan;   ChoiceC.Archetype = EChoiceArchetype::Train;     break;
		case EProjectEvent::EquipmentUpgrade:   ChoiceA.Archetype = EChoiceArchetype::Upgrade;   ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Patch;     break;
		case EProjectEvent::NegotiateExtension: ChoiceA.Archetype = EChoiceArchetype::Extend;    ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Push;      break;
		case EProjectEvent::Pivoting:           ChoiceA.Archetype = EChoiceArchetype::Pivot;     ChoiceB.Archetype = EChoiceArchetype::Keep;   ChoiceC.Archetype = EChoiceArchetype::Plan;      break;
		case EProjectEvent::PlaytestFeedback:   ChoiceA.Archetype = EChoiceArchetype::Inspire;   ChoiceB.Archetype = EChoiceArchetype::QAReview; ChoiceC.Archetype = EChoiceArchetype::Patch;   break;
		case EProjectEvent::SecurityAudit:      ChoiceA.Archetype = EChoiceArchetype::Adopt;     ChoiceB.Archetype = EChoiceArchetype::Reject; ChoiceC.Archetype = EChoiceArchetype::Shield;    break;
		case EProjectEvent::ScalingChallenge:   ChoiceA.Archetype = EChoiceArchetype::Infra;     ChoiceB.Archetype = EChoiceArchetype::Pivot;  ChoiceC.Archetype = EChoiceArchetype::TeamRally; break;
		default: break;
		}

		Data.Choices.Add(ChoiceA);
		Data.Choices.Add(ChoiceB);
		Data.Choices.Add(ChoiceC);
		return Data;
	}
};
