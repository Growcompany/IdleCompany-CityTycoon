#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Enum/MissionTypes.h"
#include "Enum/ItemType.h"
#include "Enum/ProductionDiscipline.h"
#include "Enum/CompanyType.h"
#include "MissionTable.generated.h"

// 미션 완료 보상 1건 (자원 지급)
USTRUCT(BlueprintType)
struct FMissionReward
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly)
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly)
	int64 Amount = 0;

	// 아이템 보상 (ItemType != None && ItemAmount > 0 이면 ItemInventoryManager 로 지급). 자원 보상과 동시 허용.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly)
	EItemType ItemType = EItemType::None;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly)
	int32 ItemAmount = 0;
};

/**
 * 미션 체인 정의 (DT_Mission 행 = 미션 1개, RowName = MissionID).
 * 오프닝 튜토리얼 = 이 체인의 첫 미션들 (GDD_PROGRESSION Phase 4 미션 시스템의 토대).
 */
USTRUCT(BlueprintType)
struct FMissionTable : public FTableRowBase
{
	GENERATED_BODY()

	// 트래커 카드 제목 (예: "첫 회사를 건설하자")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	// 페이즈별 멘토 대사 1줄 (인덱스 = 가이드 페이즈)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> MentorLines;

	// 대상 옆 말풍선 설명. 빈 항목은 해당 페이즈에서 말풍선을 표시하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> GuideLines;

	// 순차 설명 페이즈의 스텝별 한 문장 설명.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> ExplainLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionConditionType ConditionType = EMissionConditionType::None;

	// 조건 목표 수치 (CollectBricks = 목표 보유 벽돌)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 ConditionAmount = 0;

	// Count면 부서 제한 없음. 채용 안내 대상과 도달 인원 판정에 함께 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EProductionDiscipline RequiredRecruitDiscipline = EProductionDiscipline::Count;

	// 0이면 자유 선택. 첫 개발 안내에서 지정한 DT 프로젝트 행만 표시·착수한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredProjectIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMissionReward> Rewards;

	// 빈 값 = 체인 끝
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NextMissionID;

	// 하드 게이트 여부: true = 현재 타겟 외 입력 잠금, false = 지연 힌트만
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHardGate = false;

	// 조건 달성 후 트래커에서 보상을 직접 수령할 미션만 true.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bManualClaim = false;

	// 걸음을 저장된 페이즈가 아니라 화면 상태에서 도출하는 미션 (재접속·이탈에도 맞는 걸음이 나온다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bDerivedGuide = false;

	// 재화 부족으로 대상 버튼이 비활성일 때의 공용 안내. 빈 값이면 걸음 문구를 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText ShortfallLine;

	// 안내 대상 회사 = 지은 순서로 몇 번째 회사인가(1 = 첫 회사). 두 번째 회사 직원 채우기처럼 첫 회사가 아닌 미션만 바꾼다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 TutorialCompanyOrder = 1;

	// 건설 안내가 짚을 산업. None 이면 첫 회사의 산업
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECompanyType RequiredIndustry = ECompanyType::None;

	// 인수 체험 미션의 회수율(%) 고정값. 0 = 회사 표대로 굴린다 ― 강제 체험의 첫인상이 손해·대박으로 갈리지 않게
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 FixedAcquireYieldPct = 0;
};
