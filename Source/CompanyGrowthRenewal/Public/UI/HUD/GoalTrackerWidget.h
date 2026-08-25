#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GoalTrackerWidget.generated.h"

class UGoalBoardSubsystem;
class UGoalTrackerRowWidget;
class UCommonTextBlock;
class UButton;
class UPanelIntroOverlayWidget;
class UVerticalBox;
class UScrollBox;
class UWidget;

/**
 * 미션 트래커 (HUD 상주, 좌측). 체인 종료 후 미수령 미션 전부를 리스트로 보여준다.
 * 행 탭 = 펼침(아코디언, 한 번에 1행) / [안내하기] = 안내 점등 지정 / [수령] = ClaimGoal.
 * 헤더 [∧] 로 전체를 미니 칩으로 접는다(세션 한정).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGoalTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 매니저가 CreateWidget 직후 1회 호출 — 구독 + 최초 렌더
	void InitTracker();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ===== 펼침 상태 =====
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ExpandedRoot = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HeaderCountText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FoldButton = nullptr;

	// 세로 오버플로 안전망 — 행이 늘거나 카드가 펼쳐져도 화면 밖으로 나가지 않는다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> RowScrollBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> RowBox = nullptr;

	// 접힘 시 나머지 미션을 여는 행. 숨긴 게 없고 펼침도 아니면 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MoreButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MoreLabel = nullptr;

	// ===== 접힘 상태 (미니 칩) =====
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> FoldedRoot = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> UnfoldButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> FoldedCountText = nullptr;

	// 수령 가능 개수 배지 — 0 이면 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ClaimBadgeRoot = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ClaimBadgeText = nullptr;

private:
	void RefreshAll();

	UFUNCTION()
	void HandleFoldClicked();

	UFUNCTION()
	void HandleUnfoldClicked();

	UFUNCTION()
	void HandleMoreClicked();

	void HandleBoardChanged();
	void HandleTrackedChanged(FName TrackedID);

	void HandleRowTapped(FName GoalID);
	void HandleStartRequested(FName GoalID);
	void HandleStopRequested(FName GoalID);
	void HandleClaimRequested(FName GoalID);

	UGoalBoardSubsystem* GetBoard() const;

	// ===== 최초 노출 코치마크 (튜토리얼 체인 종료 직후 = 이 위젯이 처음 뜨는 순간) =====
	void TryScheduleIntro();
	void TryPlayIntro();
	UWidget* ResolveIntroAnchor(FName AnchorName) const;
	void HandleIntroFinished();

	// 표시 중인 첫 행 — 코치마크 앵커 겸 자동 펼침 대상
	UGoalTrackerRowWidget* GetFirstVisibleRow() const;

	FTimerHandle IntroTimer;
	TWeakObjectPtr<UPanelIntroOverlayWidget> IntroOverlay;
	TWeakObjectPtr<UWidget> HiddenMissionOverlay;

	// 행 등장 스태거(IntroMaxDelay 0.48 + IntroDuration 0.28)가 끝난 뒤 딤을 덮는다
	static constexpr float IntroPlayDelay = 0.8f;

	// 현재 펼쳐진 행 (NAME_None = 전부 접힘). 아코디언 — 한 번에 1행
	FName ExpandedGoalID = NAME_None;

	// 행 클래스 미등록 에러는 이벤트마다 반복되므로 1회만 남긴다
	bool bLoggedMissingRowClass = false;

	// 재사용 풀 — 행 수가 줄면 초과분만 Collapsed
	UPROPERTY()
	TArray<TObjectPtr<UGoalTrackerRowWidget>> RowPool;
};
