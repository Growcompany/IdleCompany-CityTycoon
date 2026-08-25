#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PanelIntroSubsystem.generated.h"

struct FGameSaveData;

/**
 * 패널 최초 진입 코치마크 — "이 패널을 본 적 있는가" 만 소유한다.
 * 재생/드로잉은 UPanelIntroOverlayWidget, 문구/타겟은 DT_PanelIntro(TableManager).
 * 설계 = docs/superpowers/specs/2026-08-12-panel-intro-coachmark-design.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPanelIntroSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 스텝이 1개 이상 정의돼 있고 아직 안 본 패널이면 true
	bool ShouldPlay(FName PanelKey) const;

	bool HasSeen(FName PanelKey) const { return SeenPanelKeys.Contains(PanelKey); }

	// 마지막 스텝 종료 시 1회만 호출 — 스텝마다 부르면 풀세이브가 스텝 수만큼 돈다
	void MarkSeen(FName PanelKey);

	// 치트 ResetPanelIntro
	void ResetAll();

	// 제스처 힌트 졸업 — "몇 번 해냈는가" (캐치 성공 등). Threshold 이상이면 힌트를 더 띄우지 않는다
	int32 GetHintCount(FName Key) const;
	void IncrementHint(FName Key);
	bool IsHintGraduated(FName Key, int32 Threshold) const { return GetHintCount(Key) >= Threshold; }

	void CollectSaveData(FGameSaveData& OutData) const;

private:
	void HandleGameDataLoaded();

	TArray<FName> SeenPanelKeys;
	TMap<FName, int32> HintCounts;
};
