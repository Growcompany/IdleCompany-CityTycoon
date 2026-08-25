#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Table/MissionTable.h"
#include "RewardClaimSplashWidget.generated.h"

class UVerticalBox;

/**
 * 미션 보상 클레임 스플래시 — 화면 중앙에 "보상 획득!" + [아이콘+수치] 를 큼지막하게 팝.
 * 순수 코드 트리(WBP 불필요). 매니저가 ClaimActiveMission 에서 생성 → AddToViewport(9500).
 * 팝인(오버슈트) → 홀드 → 페이드아웃 후 스스로 RemoveFromParent (자기 수명 관리).
 * HitTestInvisible — 입력 차단 없음.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URewardClaimSplashWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 매니저가 CreateWidget 직후 1회 호출 — 보상 스냅샷으로 트리 구성 (지급 전 Row 기준)
	void InitSplash(const FMissionTable& Mission);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentBox = nullptr;

	float Elapsed = 0.f;

	static constexpr float PopDuration = 0.3f;   // 팝인 (ease-out-back 오버슈트)
	static constexpr float HoldDuration = 2.2f;  // 정지 표시 — 보상 읽을 시간 확보
	static constexpr float FadeDuration = 0.45f; // 떠오르며 페이드아웃
};
