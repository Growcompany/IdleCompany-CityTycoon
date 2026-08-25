#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Enum/MissionTypes.h"
#include "Enum/ItemType.h"
#include "MissionTable.generated.h"

// 미션 완료 보상 1건 (자원 지급)
USTRUCT(BlueprintType)
struct FMissionReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EResourceType ResourceType = EResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 Amount = 0;

	// 아이템 보상 (ItemType != None && ItemAmount > 0 이면 ItemInventoryManager 로 지급). 자원 보상과 동시 허용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType ItemType = EItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
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

	// 가이드 페이즈별 멘토 1줄 (BuildFirstBuilding: [0]=건설 버튼 [1]=게임 타일/카드 [2]=배치 확정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> MentorLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMissionConditionType ConditionType = EMissionConditionType::None;

	// 조건 목표 수치 (CollectBricks = 목표 보유 벽돌)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 ConditionAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMissionReward> Rewards;

	// 빈 값 = 체인 끝
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NextMissionID;

	// 하드 입력 게이트 구간 여부 — true 면 현행 게이트(현재 타겟 외 잠금), false 면 소프트 존(지연 힌트만, 입력 안 막음).
	// 하드 = M1~M10b(코어 루프 1회전) / 소프트 = M11·M13 (스펙 2026-08-02 §2)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHardGate = false;
};
