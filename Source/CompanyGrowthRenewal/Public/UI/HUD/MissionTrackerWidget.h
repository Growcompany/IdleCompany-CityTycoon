#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Table/MissionTable.h"
#include "MissionTrackerWidget.generated.h"

class UMissionManagerSubsystem;
class UCommonTextBlock;
class UHorizontalBox;
class UWidgetAnimation;
class UImage;

/**
 * 미션 트래커 카드 (HUD 상주, 목표 깔때기 — 항상 현재 미션 1개만 노출).
 * 매니저가 CreateWidget 직후 InitTracker 호출 → AddToViewport(8000).
 * 델리게이트(활성/완료/가이드 페이즈) 구독으로 자가 갱신. 비인터랙티브(HitTestInvisible).
 * 체인이 끝나면 Collapsed — 그 뒤의 미션판은 별도 위젯(UGoalTrackerWidget)이 그린다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMissionTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 매니저가 CreateWidget 직후 1회 호출 — 매니저 저장 + 델리게이트 구독 + 현재 미션 렌더
	void InitTracker(UMissionManagerSubsystem* InManager);

	// 오피스 개발중 "집중 모드" — 좌측으로 슬라이드아웃(+페이드)해 화면을 비운다.
	// OfficeMainWidget::RefreshOfficeUI 가 라이프사이클 전환마다 호출. 복귀 시 미션 상태로 재settle.
	void SetFocusHidden(bool bHidden);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// 클레임 대기 중에만 카드 탭 = 보상 수령 (평소엔 HitTestInvisible 라 도달 자체가 안 됨)
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 미션 제목 (FMissionTable.Title)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MissionTitleText = nullptr;

	// 현 가이드 페이즈의 멘토 1줄 (Manager->GetCurrentMentorLine)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MentorLineText = nullptr;

	// 보상 요약 1줄 (자원 표시명 DT + 수치 축약)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RewardText = nullptr;

	// 피날레 보상 행. 중간 미션은 부모 행 전체를 접어 라벨과 아이콘이 함께 사라진다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> RewardSection = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RewardLabel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> RewardBox = nullptr;

	// 누적형 진행도 "현재 / 목표" (CollectBricks 등). 진행도 없는 미션이면 Collapsed
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProgressText = nullptr;

	// 완료 연출 (없어도 동작)
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	TObjectPtr<UWidgetAnimation> CompleteAnim = nullptr;

private:
	// BindWidgetOptional 4개가 전부 null(빈 WBP — 헤드리스 자동화 생성)일 때만 코드로 트리 구성.
	// UE5.4 Python 이 WBP 위젯트리를 못 만드는 한계 우회. WBP 에 동명 위젯이 있으면 그쪽 우선.
	void BuildFallbackTree();

	void HandleMissionActivated(const FMissionTable& Mission);
	void HandleMissionCompleted(FName CompletedID, const FMissionTable& Mission);
	void HandleGuidePhaseChanged();
	void HandleMissionProgressChanged();
	// 조건 달성 → "완료! 탭해서 보상 받기" 상태 전환 (그린 + 펄스 + 클릭 가능)
	void HandleMissionReadyToClaim(const FMissionTable& Mission);

	// FMissionTable 1건을 카드에 그림
	void RenderMission(const FMissionTable& Mission);
	// 매니저 현재 미션 재조회 → 있으면 렌더, 없으면 숨김
	void RefreshFromManager();
	void UpdateMentorLine();
	// 매니저 GetMissionProgress 재조회 → 누적형이면 "현재 / 목표", 아니면 Collapsed
	void UpdateProgress();
	FText BuildRewardSummary(const TArray<FMissionReward>& Rewards) const;
	// 완료 글로우/클레임 강조로 바뀐 카드 시각을 기본값으로 되돌림 (미션 렌더 진입)
	void ResetCardVisuals();

	// 완료 연출 후 다음 미션 전환 (타이머 콜백)
	void OnCompleteDelayElapsed();

	// 완료 글로우 엔벨로프 — CompleteAnim(디자이너 WBP 애니)이 없을 때만 코드가 직접 구동.
	// CardGlint/CardOutline 두 UImage 알파를 NativeTick 에서 시간 기반 보간.
	void StartCompleteGlow();
	// 두 위젯 알파를 기본값(Glint 0 / Outline 0.15)으로 복원하고 엔벨로프 비활성
	void StopCompleteGlow();

	TWeakObjectPtr<UMissionManagerSubsystem> Manager;
	FTimerHandle CompleteTimerHandle;

	// 순수 WBP 위젯(코드에 BindWidget 멤버 없음) — 이름 계약으로만 조회.
	// 디자이너 카드 트리: CardGlint(글린트 머티리얼, 평소 알파 0) / CardOutline(흰 외곽선, 평소 알파 0.15).
	// 이름이 바뀌면 캐시는 null 이 되고 글로우 연출만 조용히 생략된다(크래시 금지).
	TWeakObjectPtr<UImage> CardGlint;
	TWeakObjectPtr<UImage> CardOutline;

	// 완료 스파클 (이름 계약: SparkleA/B/C — WBP에 넣은 것만 동작, 미존재 시 조용히 생략)
	TWeakObjectPtr<UImage> SparkleA;
	TWeakObjectPtr<UImage> SparkleB;
	TWeakObjectPtr<UImage> SparkleC;

	// 클레임 상태 시각용 (이름 계약: AccentBar/MissionIcon — 미존재 시 해당 연출만 생략)
	TWeakObjectPtr<UImage> AccentBarImage;
	TWeakObjectPtr<UWidget> MissionIconWidget;
	// 클레임 대기 강조용 (이름 계약: CardBg — 미존재 시 해당 연출만 생략)
	TWeakObjectPtr<UImage> CardBgImage;

	// 클레임 대기 상태 — 카드 클릭 가능 + 그린 펄스. 클릭 또는 다음 미션 렌더에서 해제
	bool bAwaitingClaim = false;
	float ClaimPulseTime = 0.f;
	// 클레임 진입 시 액센트 바 원색 백업 (디자이너 소유 색을 코드가 기억해 복원)
	FSlateColor SavedAccentTint;
	bool bAccentTintSaved = false;
	// 클레임 진입 시 카드 배경 틴트 백업 (완료 시 초록/골드로 전환, 복원용)
	FLinearColor SavedCardBgTint = FLinearColor::White;
	bool bCardBgTintSaved = false;

	// 코드 구동 완료 글로우 엔벨로프 상태
	bool bGlowActive = false;
	float GlowElapsed = 0.f;

	static constexpr float CompleteDelay = 0.5f;

	// 글로우 엔벨로프 키프레임 (초 / 알파)
	static constexpr float GlowDuration = 1.3f;          // 전체 길이
	static constexpr float GlintRiseEnd = 0.1f;          // Glint 0->1 선형 상승 끝
	static constexpr float GlintPeak = 1.f;              // Glint 피크 알파
	static constexpr float OutlineRiseEnd = 0.15f;       // Outline 0.15->0.7 선형 상승 끝
	static constexpr float OutlineBase = 0.15f;          // Outline 기본 알파
	static constexpr float OutlinePeak = 0.7f;           // Outline 피크 알파

	// 완료 스파클 팝 타이밍 (글로우 엔벨로프에 피기백)
	static constexpr float SparkleStagger = 0.12f;       // A→B→C 시차
	static constexpr float SparkleLife = 0.6f;           // 개당 팝 수명

	// 클레임 대기 강조 튜닝
	static constexpr float ClaimGlintPeriod = 1.4f;      // 글린트 스윕 1주기(초)

	// ===== 집중 모드 좌측 슬라이드 =====
	bool bFocusHidden = false;      // true=개발중 집중 모드(좌측 밖으로)
	float FocusSlideAlpha = 0.f;    // 0=제자리 / 1=완전 슬라이드아웃(투명)
	// 미션 카드 폭(560)+여유. 알파1에서 opacity 0 이라 오프스크린 여부와 무관하게 안 보임.
	static constexpr float FocusSlideDistance = 720.f;
	static constexpr float FocusSlideSpeed = 11.f;       // FInterpTo 속도(≈0.2s)
};
