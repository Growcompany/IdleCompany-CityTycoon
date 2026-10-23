#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"   // FPanelIntroAnchorResolver 기본 인자용
#include "PanelIntroSubsystem.generated.h"

struct FGameSaveData;
class UUserWidget;

// 마지막 스텝까지 본 안내 — 이미 본 패널을 다시보기로 끝낸 경우에도 나간다(HasSeen 만으로는 "이번에 읽었는가"를 모른다)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPanelIntroCompleted, FName /*PanelKey*/);

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

	/**
	 * 코치마크 재생 단일 경로. 최초 진입(bIgnoreSeen=false)과 "?" 다시보기(true)가 같이 쓴다.
	 * 미션 가이드 오버레이 숨김/복구까지 여기서 소유한다 ― 딤 2겹 방지 로직을 호출부마다 복제하지 않는다.
	 * @param AnchorResolver 런타임 생성 위젯이 앵커인 호출부만 바인드(미션 트래커). 미바인드면 GetWidgetFromName 경로.
	 * @return 재생이 시작됐으면 오버레이. 스텝 0개 / 이미 본 패널 / 위젯 미등록이면 nullptr
	 */
	UPanelIntroOverlayWidget* PlayIntro(
		UUserWidget* OwnerPanel,
		FName PanelKey,
		bool bIgnoreSeen,
		const FPanelIntroAnchorResolver& AnchorResolver = FPanelIntroAnchorResolver());

	// 스텝이 1개 이상 정의돼 있고 아직 안 본 패널이면 true
	bool ShouldPlay(FName PanelKey) const;

	// 튜토리얼 후 첫 게임 후속 프로젝트에서 기획 인원이 부족할 때 채용/자리 준비 안내 키를 고른다.
	FName GetRecruitDepartmentIntroKey(int32 BuildingIndex) const;

	bool HasSeen(FName PanelKey) const { return SeenPanelKeys.Contains(PanelKey); }

	// 마지막 스텝 종료 시 1회만 호출 — 스텝마다 부르면 풀세이브가 스텝 수만큼 돈다
	void MarkSeen(FName PanelKey);

	// 코치마크 오버레이가 지금 화면에 떠 있는가 — 미션 안내는 그동안 대상을 내려 딤 2겹을 피한다
	bool IsIntroPlaying() const;

	FOnPanelIntroCompleted OnIntroCompleted;

	// 치트 ResetPanelIntro
	void ResetAll();

	// 제스처 힌트 졸업 — "몇 번 해냈는가" (캐치 성공 등). Threshold 이상이면 힌트를 더 띄우지 않는다
	int32 GetHintCount(FName Key) const;
	void IncrementHint(FName Key);
	bool IsHintGraduated(FName Key, int32 Threshold) const { return GetHintCount(Key) >= Threshold; }

	void CollectSaveData(FGameSaveData& OutData) const;

private:
	void HandleGameDataLoaded();

	void HandleIntroFinished();

	// 재생 동안 숨겨 둔 미션 가이드 오버레이 ― 종료 시 원래 가시성으로 되돌린다
	TWeakObjectPtr<UUserWidget> HiddenMissionOverlay;

	TWeakObjectPtr<UPanelIntroOverlayWidget> PlayingOverlay;

	TArray<FName> SeenPanelKeys;
	TMap<FName, int32> HintCounts;
};
