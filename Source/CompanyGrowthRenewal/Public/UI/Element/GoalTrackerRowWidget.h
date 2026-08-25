#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Manager/GoalBoardSubsystem.h"
#include "GoalTrackerRowWidget.generated.h"

class UCommonButtonStyle;
class UCommonTextBlock;
class UBorder;
class UButton;
class UButtonWidget;
class UImage;
class UProgressBar;
class UHorizontalBox;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoalRowIntent, FName);

/**
 * 미션 행 1건 — 접힘(배지+제목+진행) ↔ 펼침(설명/진행바/보상/버튼).
 * 트래커가 재사용하며 매 Configure 마다 상태를 통째로 다시 주입한다(부분 갱신 아님).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGoalTrackerRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 트래커가 행 상태를 통째로 주입. InPrereqTitle = 잠금 행에 보여줄 선행 미션 제목(비잠금이면 빈 FText)
	// InPlaceHint = 추적 중인데 이 맵에서 못 하는 경우의 갈 곳("사무실에서"), 여기서 되면 빈 FText
	void Configure(const FGoalBoardEntry& InEntry, bool bInTracked, bool bInExpanded, const FText& InPrereqTitle, const FText& InPlaceHint);

	// 미션판 첫 등장 — OrderIndex 만큼 늦게 시작하는 페이드+슬라이드.
	// 상수/이징은 URankingEntryCardWidget::PlayIntro 와 동일(두 리스트가 같은 리듬으로 읽혀야 한다)
	void PlayIntro(int32 OrderIndex);

	FName GetGoalID() const { return Entry.GoalID; }

	FOnGoalRowIntent OnRowTapped;
	FOnGoalRowIntent OnStartRequested;
	FOnGoalRowIntent OnStopRequested;
	FOnGoalRowIntent OnClaimRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 접힘 행 전체가 히트 영역 (행 높이가 터치 하한보다 낮아도 폭 620 으로 면적 보상)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RowButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText = nullptr;

	// 도달형 "n/N" — 이벤트형이면 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProgressShortText = nullptr;

	// "안내 중" / "완료" — 없으면 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StateTagText = nullptr;

	// 태그 칩 배경 — 텍스트만 숨기면 빈 칩이 남으므로 토글 대상은 이 플레이트다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> StateTagPlate = nullptr;

	// 펼침 영역 루트 — 접힘 시 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ExpandRoot = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DescText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProgressLongText = nullptr;

	// 보상 칩이 들어갈 자리 — 트래커가 아니라 이 위젯이 채운다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> RewardBox = nullptr;

	// [안내하기] / [안내 끄기] 겸용 (라벨·스타일만 전환)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> ActionButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimButton = nullptr;

	// 상태를 말하는 유일한 상시 채널 — 태그 칩은 두 상태에만 뜬다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> AccentBar = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> StateDot = nullptr;

	// ⚠ required — 접힌 잠금 행에서 이게 유일한 잠금 신호다(진행 텍스트·태그·사유 문구가 전부 Collapsed)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> LockIcon = nullptr;

	// [안내하기](컬러 CTA) ↔ [안내 끄기](고스트) 스타일 — 색은 스타일 에셋이 소유, 전환만 코드
	UPROPERTY(EditAnywhere, Category = "GoalRow|Style")
	TSoftClassPtr<UCommonButtonStyle> ActionStyle_Start = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Blue_Sq.CUI_Style2_Btn_Blue_Sq_C")));

	UPROPERTY(EditAnywhere, Category = "GoalRow|Style")
	TSoftClassPtr<UCommonButtonStyle> ActionStyle_Stop = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Ghost_Sq.CUI_Style2_Btn_Ghost_Sq_C")));

private:
	// RowButton 은 UButton(UMG) 이라 DYNAMIC — UFUNCTION 필요
	UFUNCTION()
	void HandleRowClicked();

	// ActionButton/ClaimButton 은 UButtonWidget(=UCommonButtonBase 파생) 이라 네이티브 델리게이트 — UFUNCTION 금지
	void HandleActionClicked();
	void HandleClaimClicked();

	void RebuildRewardChips();

	// ===== 완료 연출 (UIE_MissionTracker 와 같은 세트) =====
	// 순수 WBP 위젯 — BindWidget 이 아니라 **이름 계약**으로만 조회한다. 이름이 바뀌면 캐시가 null 이 되고
	// 그 연출만 조용히 생략된다(크래시 금지). CardGlint/CardLine 은 이미 트리에 있고, SparkleA~C 는 넣으면 동작.
	void CacheFxWidgets();
	void StartCompleteGlow();
	void StopCompleteGlow();

	TWeakObjectPtr<UImage> CardGlint;
	TWeakObjectPtr<UImage> CardLine;   // 미션 카드의 CardOutline 과 같은 역할 (평소 알파 0.15)
	TWeakObjectPtr<UImage> CardBgImage;
	TWeakObjectPtr<UImage> SparkleA;
	TWeakObjectPtr<UImage> SparkleB;
	TWeakObjectPtr<UImage> SparkleC;

	// ⚠ 전이 감지는 **GoalID 기준** — 행은 풀에서 재사용되므로 인스턴스 기준으로 보면
	//   다른 미션이 들어온 것을 "완료됨"으로 오인해 엉뚱한 행이 번쩍인다
	FName FxGoalID;
	EGoalState FxLastState = EGoalState::Locked;

	bool bAwaitingClaim = false;
	float ClaimPulseTime = 0.f;
	bool bGlowActive = false;
	float GlowElapsed = 0.f;

	float IntroElapsed = 0.f;
	float IntroDelay = 0.f;
	bool bIntroPlaying = false;

	static constexpr float IntroDuration = 0.28f;
	static constexpr float IntroSlideX = 60.f;
	static constexpr float IntroStagger = 0.06f;
	static constexpr float IntroMaxDelay = 0.48f;   // 아래쪽 행까지 끝없이 늦어지지 않게 캡

	// 엔벨로프 키프레임 — UIE_MissionTracker 와 같은 값(두 연출이 같은 리듬으로 읽혀야 한다)
	static constexpr float GlowDuration = 1.3f;
	static constexpr float GlintRiseEnd = 0.1f;
	static constexpr float GlintPeak = 1.f;
	static constexpr float OutlineRiseEnd = 0.15f;
	static constexpr float OutlineBase = 0.15f;
	static constexpr float OutlinePeak = 0.7f;
	static constexpr float SparkleStagger = 0.12f;
	static constexpr float SparkleLife = 0.6f;
	static constexpr float ClaimGlintPeriod = 1.4f;

	// 잠금 배지는 지름 12 (다른 상태는 18) — 브러시 크기를 못 바꾸므로 렌더 스케일로 줄인다
	static constexpr float LockedDotScale = 12.f / 18.f;

	FGoalBoardEntry Entry;
	bool bTracked = false;
	bool bExpanded = false;
};
