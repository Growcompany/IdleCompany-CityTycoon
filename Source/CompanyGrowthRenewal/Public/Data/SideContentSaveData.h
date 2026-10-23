#pragma once

#include "CoreMinimal.h"
#include "SideContentSaveData.generated.h"

/** 출장 슬롯 1건 (건물당 1개). EndUnix == 0 = 빈 슬롯. */
USTRUCT(BlueprintType)
struct FSideDispatchSlot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName CardKey;

	UPROPERTY(SaveGame)
	int64 StartUnix = 0;

	UPROPERTY(SaveGame)
	int64 EndUnix = 0;

	UPROPERTY(SaveGame)
	TArray<int32> EmployeeIDs;

	bool IsActive() const { return EndUnix > 0; }
};

/** 건물 축(연수원·출장) 상태. 건물이 사라지면 항목째 제거한다. */
USTRUCT(BlueprintType)
struct FSideBuildingState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 BuildingIndex = INDEX_NONE;

	UPROPERTY(SaveGame)
	int32 CourseBest = 0;

	UPROPERTY(SaveGame)
	int32 CourseEntries = 0;

	UPROPERTY(SaveGame)
	FSideDispatchSlot DispatchSlot;

	/** 카드는 자정에 갱신 — 이 키가 오늘과 다르면 DispatchCardKeys 를 다시 뽑는다 */
	UPROPERTY(SaveGame)
	FString DispatchCardDayKey;

	UPROPERTY(SaveGame)
	TArray<FName> DispatchCardKeys;
};

/**
 * 사이드 컨텐츠 전체 상태 (공통 축 + 건물 축).
 * 평구조체 — SaveLoadManager 가 통째로 넘긴다(설계 §1.2).
 */
USTRUCT(BlueprintType)
struct FSideContentSaveData
{
	GENERATED_BODY()

	// ── 공통 축: 스카이라인 ──
	UPROPERTY(SaveGame)
	int32 BestFloor = 0;

	/** 입장권 1장 = 등반 1회(닿는 층까지). 층 단위가 아니다 — 2/일·최대 5 (DECISION_RECORDS §13.2). */
	UPROPERTY(SaveGame)
	int32 SkylineEntries = 0;

	/** 마지막 출전 회사. INDEX_NONE = 아직 안 골랐다 → 진입 시 팀 파워 최대 건물이 자동 선택된다. */
	UPROPERTY(SaveGame)
	int32 SkylineBuildingIndex = INDEX_NONE;

	/** 1회성 층 보상을 이미 받은 층. BestFloor 와 별도인 이유 = 지급이 실패한 층을 다시 주기 위해서다. */
	UPROPERTY(SaveGame)
	TArray<int32> ClaimedFloorRewards;

	// ── 공통 축: 콜라보 개발 (진행 중 1건) ──
	UPROPERTY(SaveGame)
	FName FusionActiveKey;

	UPROPERTY(SaveGame)
	int64 FusionEndUnix = 0;

	UPROPERTY(SaveGame)
	TArray<int32> FusionBuildingIndices;

	// 콜라보 키 -> 성공 횟수. 완료 여부는 값 >= 1 로 파생한다(완료 집합 필드 없음)
	UPROPERTY(SaveGame)
	TMap<FName, int32> FusionLevels;

	// ── 건물 축 ──
	UPROPERTY(SaveGame)
	TArray<FSideBuildingState> Buildings;

	/** 리필이 마지막으로 돈 날. 하루에 두 번 리필되지 않게 하는 유일한 가드. */
	UPROPERTY(SaveGame)
	FString LastRefillDayKey;
};
