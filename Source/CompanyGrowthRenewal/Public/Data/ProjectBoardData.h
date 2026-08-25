#pragma once

#include "CoreMinimal.h"
#include "Table/ProjectDataTable.h"
#include "Data/TierLayout.h"
#include "ProjectBoardData.generated.h"

/**
 * 기획 보드 기획안 카드 한 장의 데이터
 * (구 수주/자체개발 보드 슬롯 — 보드 폐기 후 피치 카드 캐리어로만 사용)
 */
USTRUCT(BlueprintType)
struct FProjectBoardSlot
{
	GENERATED_BODY()

	// 규모 슬롯 번호 (DT_Project_* 참조 — 요구점수/기간의 근거)
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Board")
	int32 ProjectIndex = 0;

	// 프로젝트 이름 (이름풀 작명 or 조합 합성명)
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Board")
	FText ProjectName;

	// 스텝별 요구 점수 (Step1-3: 공통, Step4: variant 있을 때만 > 0)
	UPROPERTY(BlueprintReadOnly, Category = "Board|Score")
	int32 RequiredScore_Step1 = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Score")
	int32 RequiredScore_Step2 = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Score")
	int32 RequiredScore_Step3 = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Score")
	int32 RequiredScore_Step4 = 0;

	// ── 피치(자체개발 발견형) 표시값 ──
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	bool bPitchCard = false;

	// 프로젝트 대표 커버 (DT_Project_*.Icon)
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	TSoftObjectPtr<UTexture2D> Cover;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	FName Genre = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	FName Material = NAME_None;

	// 피치 시점 추정 단일값 (결산 헤드라인과 동일 척도) — 아래 ExpectedQuality 하나만을 입력으로 삼는다
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int64 EstimatedRevenue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 EstimatedOpTimeSec = 0;

	// 카드의 수익/운영시간/배지가 전부 이 값 하나에서 나온다.
	// 채우는 곳 = UOfficeStageProgressManager::EstimateProjectOutlook (현재 착석 인원 기준).
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	float ExpectedQuality = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	bool bGateRisk = false;

	// 이 프로젝트의 소재가 현재 트렌드인가 — EstimatedRevenue 에 이미 ×1.25 가 반영돼 있어 카드는 표기만 한다
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	bool bTrendMatched = false;

	// 현재 팀 전망의 직능 슬롯별 예상 점수. 카드 표시용 파생값이므로 저장하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	TArray<float> ExpectedDisciplineScores;

	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	bool bHasOutlookEstimate = false;

	// "?" 웰 장르색 틴트 (DT 파생, 미정의=중립 회색)
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	FLinearColor GenreColor = FLinearColor::Gray;

	// 제작 직능 요구 가중치 (0~5) — 피치 카드 "핵심 직능" 태그 표시용
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 Weight_Plan = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 Weight_Dev = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 Weight_Graphics = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 Weight_Sound = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 Weight_Server = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Board|Pitch")
	int32 Weight_QA = 0;

	FProjectBoardSlot()
		: ProjectIndex(0)
	{}
};

// 기본 레이아웃 상수 — 런타임 SOT 는 FTierLayout
namespace TierConstants
{
	constexpr int32 PROJECTS_PER_TIER = 10;
	constexpr int32 CLEAR_TO_UNLOCK = 7;
	constexpr int32 MAX_TIER = 10;
}

/**
 * 티어 그리드 셀 1개의 상태.
 * 착수는 전부 기획 보드가 담당하고, 이 상태는 도감 열람 표시(잠금/미발견/발견)에만 쓴다.
 */
UENUM()
enum class EProjectEntryState : uint8
{
	Locked,         // 현재 티어 이상 — 잠금
	Undiscovered,   // 지나온 티어 + 미개발 — 첫 개발(등급 미공개, 도감 +1)
	Discovered      // 지나온 티어 + 개발함 — 재개발(등급 공개, 도감 진척 없음)
};

/** 티어 그리드/리스트 한 줄 = 프로젝트 1개. 포트폴리오(열람)가 쓴다. */
USTRUCT()
struct FProjectTierEntry
{
	GENERATED_BODY()

	int32 ProjectIndex = 0;
	int32 Tier = 1;
	FName Genre;
	FName Material;
	FText ProjectName;
	TSoftObjectPtr<UTexture2D> Cover;
	float Duration = 0.0f;   // 개발 소요(초). 스텝은 동시 진행이라 1회분이 곧 개발 시간.
	bool bPitchReady = false;   // FProjectData::IsPitchReady — 착수 창구가 미저작 행을 거르는 기준
	EProjectEntryState State = EProjectEntryState::Locked;
};

/**
 * 티어 진행 상태
 * 10개 단위 Tier (1~10, 11~20, ... 91~100)
 * 7/10 첫 클리어 시 다음 Tier 해금
 */
USTRUCT(BlueprintType)
struct FProjectTierProgress
{
	GENERATED_BODY()

	// 현재 최고 해금 티어 (1~10)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Tier")
	int32 CurrentTier = 1;

	// 프로젝트 클리어 기록 (자체개발 + 제조 양산 공통 — 최소 점수 통과분만)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Proficiency")
	TArray<int32> ClearedProjects;

	// 도감 마일스톤 보상 지급된 티어(10/10 발견) — 중복 지급 방지
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Codex")
	TArray<int32> MilestoneRewardedTiers;

	// 전체 100개 발견 완성 보상 지급 여부
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Codex")
	bool bAllProjectsMilestoneRewarded = false;

	// 프로젝트 번호 → 티어 번호. 범위 밖 = 0 (loud) — 구 Clamp 는 101행을 T10 으로 조용히 삼켰다
	FORCEINLINE static int32 GetTierForProject(int32 ProjectIndex)
	{
		return FTierLayout::GetTierForProject(ProjectIndex);
	}

	// 해당 티어의 프로젝트 범위 (시작, 끝)
	FORCEINLINE static void GetTierProjectRange(int32 Tier, int32& OutStart, int32& OutEnd)
	{
		FTierLayout::GetRange(Tier, OutStart, OutEnd);
	}

	// 해당 티어에서 클리어한 프로젝트 수 (티어 해금 기준)
	// 자체개발 출시 / 제조 양산 출시 중 최소 점수를 통과한 것만 카운트
	int32 GetTierClearedCount(int32 Tier) const
	{
		int32 Start, End;
		GetTierProjectRange(Tier, Start, End);

		int32 Count = 0;
		for (int32 ProjIdx : ClearedProjects)
		{
			if (ProjIdx >= Start && ProjIdx <= End)
			{
				Count++;
			}
		}
		return Count;
	}

	// 해당 티어 해금 여부
	bool IsTierUnlocked(int32 Tier) const
	{
		if (Tier <= 1) return true;
		return GetTierClearedCount(Tier - 1) >= FTierLayout::ClearToUnlock(Tier - 1);
	}

	// 다음 티어 해금까지 남은 클리어 수
	int32 GetRemainingForNextTier() const
	{
		if (CurrentTier >= FTierLayout::MaxTier()) return 0;
		int32 Cleared = GetTierClearedCount(CurrentTier);
		return FMath::Max(0, FTierLayout::ClearToUnlock(CurrentTier) - Cleared);
	}

	// 프로젝트 클리어 기록 — 최소 점수를 통과한 경우에만 호출할 것
	// 첫 클리어 시 ClearedProjects에 추가되고 티어 승급을 판정한다
	// @return true면 티어 해금됨
	bool RecordProjectClear(int32 ProjectIndex)
	{
		if (ClearedProjects.Contains(ProjectIndex))
		{
			return false;
		}
		ClearedProjects.Add(ProjectIndex);
		return TryUnlockNextTier();
	}

	// 다음 티어 해금 시도. 조건: 다음 티어 존재 AND 현재 티어 7/10 클리어(IsTierUnlocked).
	// @return true면 CurrentTier가 올라감(해금됨)
	bool TryUnlockNextTier()
	{
		const int32 NextTier = CurrentTier + 1;
		if (NextTier <= FTierLayout::MaxTier() && IsTierUnlocked(NextTier))
		{
			CurrentTier = NextTier;
			return true;
		}
		return false;
	}

	FProjectTierProgress()
		: CurrentTier(1)
	{}
};
