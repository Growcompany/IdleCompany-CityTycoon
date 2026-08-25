#pragma once
#include "CoreMinimal.h"

/**
 * 기획 보드·판정 밴드 카드의 순수 문구/판정 로직. 위젯·매니저 의존 0 — 테스트가 곧 문구 계약이다.
 * 문구 규칙(스펙 §2-5): 처방에 수량을 쓰지 않는다. 안내톤 "~습니다".
 */
namespace PitchBoardText
{
	// 마지막 글자가 한글 완성형이고 받침이 있으면 true. 라틴/숫자 = false("QA가").
	inline bool HasBatchim(const FString& Word)
	{
		if (Word.IsEmpty()) { return false; }
		const TCHAR C = Word[Word.Len() - 1];
		if (C < 0xAC00 || C > 0xD7A3) { return false; }
		return ((C - 0xAC00) % 28) != 0;
	}

	inline FText MakeShortageSentence(const FText& DisciplineName)
	{
		const FString N = DisciplineName.ToString();
		return FText::FromString(N + (HasBatchim(N) ? TEXT("이") : TEXT("가")) + TEXT(" 부족합니다"));
	}

	inline FText MakeShortageWord(const FText& DisciplineName)
	{
		return FText::FromString(DisciplineName.ToString() + TEXT(" 부족"));
	}

	inline FString NextGradeLetter(const FString& Grade)
	{
		if (Grade == TEXT("C")) { return TEXT("B"); }
		if (Grade == TEXT("B")) { return TEXT("A"); }
		if (Grade == TEXT("A")) { return TEXT("S"); }
		return FString();
	}

	inline FText MakeNextGradeHint(const FText& WorstDisciplineName, const FString& NextGrade)
	{
		if (NextGrade.IsEmpty()) { return FText::FromString(TEXT("현재 팀으로 최고 등급입니다")); }
		return FText::FromString(WorstDisciplineName.ToString() + TEXT(" 투자 시 예상 ") + NextGrade);
	}

	/**
	 * 6칸 중 최저 달성률(Expected/Target) 직능 1개. 카드 밴드 처방과 보드 타일이 같은 답을 내도록 결정만 모은다.
	 * 목표 0 이하(비활성)·비유한 값 칸은 건너뛰고, 크기가 6이 아니거나 고를 칸이 없으면 false + INDEX_NONE.
	 */
	inline bool FindWorstDiscipline(const TArray<float>& Scores, const TArray<float>& Targets, int32& OutSlot, float& OutRatio)
	{
		OutSlot = INDEX_NONE;
		OutRatio = 0.f;
		if (Scores.Num() != 6 || Targets.Num() != 6) { return false; }

		float Worst = TNumericLimits<float>::Max();
		for (int32 SlotNo = 0; SlotNo < 6; ++SlotNo)
		{
			const float Target = Targets[SlotNo];
			const float Score = Scores[SlotNo];
			if (!FMath::IsFinite(Target) || !FMath::IsFinite(Score) || Target <= 0.f) { continue; }
			const float Ratio = Score / Target;
			if (Ratio < Worst) { Worst = Ratio; OutSlot = SlotNo; }
		}
		if (OutSlot == INDEX_NONE) { return false; }
		OutRatio = Worst;
		return true;
	}

	enum class ETileVerdict : uint8 { Pass, Redevelop, Short, Wait, Locked, Preview };

	struct FPitchBoardEntry
	{
		int32 ProjectIndex = 0;
		bool bHasOutlook = false;     // 로스터 0 이면 false — 추천 후보에서 제외
		bool bGateRisk = true;
		bool bDeveloped = false;
		int64 EstimatedRevenue = 0;
		float WorstRatio = 0.f;       // 최저 직능 달성률 (미달 "가까운 순" 정렬 키)
		int32 WorstSlot = INDEX_NONE; // EProductionDiscipline 슬롯
		FString Grade;                // 예상 등급 글자 (개발함은 기록 등급)
	};

	inline ETileVerdict ClassifyTile(const FPitchBoardEntry& E, bool bLockedTier, bool bPreviewTier)
	{
		if (bLockedTier) { return ETileVerdict::Locked; }
		if (bPreviewTier) { return ETileVerdict::Preview; }
		// 로스터 0 = 판정 자체가 없다. 미달(레드)로 칠하면 내리지도 않은 "이 팀으론 안 된다"를 내민다
		if (!E.bHasOutlook) { return ETileVerdict::Wait; }
		if (!E.bGateRisk) { return E.bDeveloped ? ETileVerdict::Redevelop : ETileVerdict::Pass; }
		return ETileVerdict::Short;
	}

	// §6.2: ① 통과·미개발 수익 최대 ② 통과·개발함 수익 최대 ③ 없음. 동률 = ProjectIndex 작은 쪽.
	inline int32 ComputeRecommendedIndex(const TArray<FPitchBoardEntry>& Entries)
	{
		auto Best = [&Entries](bool bWantDeveloped) -> int32
		{
			int32 BestIdx = INDEX_NONE; int64 BestRev = -1;
			for (const FPitchBoardEntry& E : Entries)
			{
				if (!E.bHasOutlook || E.bGateRisk || E.bDeveloped != bWantDeveloped) { continue; }
				// "아직 못 골랐다" 판정은 BestIdx 로만 한다 — BestRev 초기값을 센티널로 쓰면 음수 수익 후보가 통째로 탈락한다
				if (BestIdx == INDEX_NONE || E.EstimatedRevenue > BestRev || (E.EstimatedRevenue == BestRev && E.ProjectIndex < BestIdx))
				{
					BestRev = E.EstimatedRevenue; BestIdx = E.ProjectIndex;
				}
			}
			return BestIdx;
		};
		const int32 Fresh = Best(false);
		return Fresh != INDEX_NONE ? Fresh : Best(true);
	}

	// §6.4: 실패 화면 "Lv{N}까지 M판". PerRound 0 이하 = 지급이 없어 영영 안 오르므로 0(문구 생략).
	// 이미 만렙치를 넘겼어도 1 — 다음 판에 레벨업한다.
	inline int32 ComputeRoundsToLevelUp(float Experience, float MaxExperience, float PerRound)
	{
		if (PerRound <= 0.f) { return 0; }
		return FMath::Max(1, FMath::CeilToInt((MaxExperience - Experience) / PerRound));
	}

	inline int32 ComputeClosestShortIndex(const TArray<FPitchBoardEntry>& Entries)
	{
		int32 BestIdx = INDEX_NONE; float BestRatio = -1.f;
		for (const FPitchBoardEntry& E : Entries)
		{
			if (!E.bHasOutlook || !E.bGateRisk) { continue; }
			if (BestIdx == INDEX_NONE || E.WorstRatio > BestRatio || (E.WorstRatio == BestRatio && E.ProjectIndex < BestIdx)) { BestRatio = E.WorstRatio; BestIdx = E.ProjectIndex; }
		}
		return BestIdx;
	}
}
