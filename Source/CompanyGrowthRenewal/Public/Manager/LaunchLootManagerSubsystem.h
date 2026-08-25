#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Table/MissionTable.h"
#include "Enum/ResourceType.h"
#include "Enum/ItemType.h"
#include "LaunchLootManagerSubsystem.generated.h"

// 출시 보상 미리보기 1종 — 같은 보상은 테이블 전체에서 병합, 확률은 테이블별 슬롯 1회당 %
struct FLaunchLootPreviewEntry
{
	EResourceType ResourceType = EResourceType::None;
	EItemType ItemType = EItemType::None;
	// 인덱스 0=Low / 1=Mid / 2=High. 현재 티어 후보 기준 슬롯당 등장 확률(%), 미등장=0
	int32 ChancePercent[3] = { 0, 0, 0 };
	// 밴드별 1회 지급 수량 범위 — 뭉치 엔트리(x2~3) 표기용. "상위 등급인데 %가 낮다" 오독 방지
	int32 BandAmountMin[3] = { 0, 0, 0 };
	int32 BandAmountMax[3] = { 0, 0, 0 };
	int32 MinScore = 0;   // 등장 최저 평점 (0 = 모든 평점)
	int32 MaxTier = 0;    // 이 티어를 넘으면 퇴장 (0 = 무제한) — 미리보기 안내용
};

/**
 * 출시 전리품 드랍 — 평점 등급별 DT_LaunchLoot 가중 롤 + 즉시 지급.
 * 자체 세이브 상태 없음(즉시 지급). 훅: 출시 확정 / 회사 인수 / HQ 산업 해금.
 * 스펙 = docs/superpowers/specs/2026-08-02-launch-loot-drop-design.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULaunchLootManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 평점 등급 임계 SOT — ReviewScoreToTableKey 와 UI 안내 문구가 같은 값을 본다 (32 = 명예의 전당 임계 재사용)
	static constexpr int32 MidScoreThreshold = 24;
	static constexpr int32 HighScoreThreshold = 32;

	// 평점 총점(4~40) -> 드랍 테이블 키. 임계 SOT: <24 Low / 24~31 Mid / >=32 High(명예의 전당 임계와 동일)
	static FName ReviewScoreToTableKey(int32 ReviewScore);

	// 평점 구간 표시 라벨 (0=Low/1=Mid/2=High) — 임계 상수 파생, 확률표/툴팁 공용
	static FString GetScoreBandLabel(int32 BandIndex);

	// 티어 기준 등장 가능 보상 목록 (메인 보상 우선 정렬, 상태 없음) — 기획 보드 미리보기/확률표 UI 전용
	TArray<FLaunchLootPreviewEntry> GetLootPreview(int32 Tier) const;

	// 티어 구간 롤 보너스: T1~3=0 / T4~6=+1 / T7~10=+2
	static int32 GetTierRollBonus(int32 Tier);

	// TableKey 테이블을 (기본 롤 수 + ExtraRolls)회 롤해 지급하고 병합 결과 반환. bGrant=false 는 시뮬레이션(치트)
	// bSaveAfterGrant=false 는 호출부가 직후에 저장할 때 — 켜두면 전체 세이브가 한 액션에 2회 이상 돈다
	TArray<FMissionReward> RollAndGrant(FName TableKey, int32 Tier, int64 ScaleBasis, int32 ExtraRolls = 0, bool bGrant = true, bool bSaveAfterGrant = true);

	// 출시 확정 전용: 키 매핑 + 티어 보너스 + 결과 캐시 (보상 리빌 UI 가 GetLastLaunchLoot 로 동기 조회)
	void RollLaunchLoot(int32 ReviewScore, int32 ProjectTier);

	const TArray<FMissionReward>& GetLastLaunchLoot() const { return LastLaunchLoot; }

	// 롤을 돌지 않는 출시 경로(제조/최소점수 미달)가 이전 출시의 캐시를 보상 리빌 UI에 재노출하지 않게 하는 안전핀
	void ClearLastLaunchLoot() { LastLaunchLoot.Reset(); }

	// 인수/HQ 훅용 티어 컨텍스트 = 오피스 진행 티어 (미확보 시 1)
	int32 GetProgressionTierContext() const;

private:
	// 테이블별 기본 롤 수: Low/Mid 2, High 3
	static int32 GetBaseRolls(FName TableKey);

	void GrantRewards(const TArray<FMissionReward>& Rewards, bool bSave);

	TArray<FMissionReward> LastLaunchLoot;
};
