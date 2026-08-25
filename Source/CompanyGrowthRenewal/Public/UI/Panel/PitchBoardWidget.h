#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/ProjectBoardData.h"
#include "Enum/CompanyType.h"
#include "Table/ProjectDataTable.h"
#include "UI/PitchBoardText.h"
#include "PitchBoardWidget.generated.h"

class UButton;
class UButtonWidget;
class UCloseButtonWidget;
class UCommonTextBlock;
class UHorizontalBox;
class UOfficeRequestCardWidget;
class UPitchTileWidget;
class UUniformGridPanel;
class UOfficeStageProgressManager;
class UTableManagerSubsystem;
class UTrendManagerSubsystem;

/**
 * 착수 = 기획 보드 (UI_PitchBoardPanel). 현재 티어 10개 전부를 타일로 깔고, 팀 기준 추천 1건을 좌측 카드로 올린다.
 * 판정/수익은 열 때마다 현재 착석 로스터로 재계산하고 저장하지 않는다.
 * 스펙: docs/superpowers/specs/2026-08-22-pitch-board-design.md §4, §6, §7
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPitchBoardWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 튜토리얼 M6 앵커 — 지금 카드가 실제로 보이는 CTA
	UWidget* GetRecommendCardCtaWidget() const;

	// 보드를 특정 티어로 연다 (범위 밖은 현재 티어로 클램프)
	void OpenTier(int32 Tier);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ── 헤더 ──
	UPROPERTY(meta = (BindWidget))
	UCloseButtonWidget* CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// ── 티어 줄 ──
	UPROPERTY(meta = (BindWidget))
	UButtonWidget* PrevTierButton;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* NextTierButton;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_TierName;

	// 클리어 미터 — 자식 Image 10개는 WBP 저작, C++ 는 색만 토글
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* MeterBox;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_TierCount;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Trend;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* TrendRerollButton;

	// ── 본문 ──
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Kicker;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* BackToRecommendButton;

	UPROPERTY(meta = (BindWidget))
	UOfficeRequestCardWidget* Card;

	// 4열 × 3행 — 타일은 런타임 생성
	UPROPERTY(meta = (BindWidget))
	UUniformGridPanel* TileGrid;

private:
	// 보드 1칸의 계산 결과 묶음. 타일·카드·추천이 모두 이 한 벌에서 나온다.
	struct FBoardRow
	{
		FProjectData Project;
		FProjectBoardSlot BoardSlot;
		PitchBoardText::FPitchBoardEntry Entry;
		PitchBoardText::ETileVerdict Verdict = PitchBoardText::ETileVerdict::Short;
		bool bDeveloped = false;
		FString DevelopedGrade;
		bool bTrend = false;
		// 잠금 문구 분기용 — 선착수 개시 여부와 무관하게 "CurrentTier+1 의 첫 2칸인가"만 본다
		bool bFirstTwoOfNext = false;
	};

	// 로스터 수집 → 행 계산 → 타일/카드/티어줄 렌더. 여는 순간에만 돈다(틱 금지).
	void Rebuild();
	void RenderTiles();
	void RenderCard();
	void RenderTierLine();

	// 선택 링만 다시 칠한다 — 타일 재생성 없이 선택을 옮기는 경로
	void UpdateTileSelection();

	const FBoardRow* FindRow(int32 ProjectIndex) const;

	// 카드와 같은 식(ComputeDisciplineTarget)으로 최저 달성률 직능을 뽑는다
	static void ComputeWorstDiscipline(const FProjectBoardSlot& BoardSlot, float& OutRatio, int32& OutSlot);

	FText MakeVerdictWord(const FBoardRow& BoardRow) const;

	UFUNCTION()
	void HandleTileClicked(int32 ProjectIndex);

	UFUNCTION()
	void HandleCardAccepted(int32 SlotIndex);

	UFUNCTION()
	void HandleViewEmployees(int32 WorstSlot);

	void HandleBackToRecommend();
	void HandlePrevTier();
	void HandleNextTier();
	void HandleTrendReroll();

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	UTableManagerSubsystem* GetTableMgr() const;
	UOfficeStageProgressManager* GetStageMgr() const;
	UTrendManagerSubsystem* GetTrendMgr() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPitchTileWidget>> Tiles;

	TArray<FBoardRow> Rows;

	ECompanyType Industry = ECompanyType::Game;
	int32 ViewTier = 0;          // 0 = 미결정(첫 Rebuild 에서 현재 티어로)
	int32 CurrentTier = 1;

	// 둘 다 ProjectIndex (배열 인덱스 아님) — 티어를 넘나들어도 같은 키로 남는다
	int32 RecommendedIndex = INDEX_NONE;
	int32 SelectedIndex = INDEX_NONE;

	// 「자세히」 = 세션 기억(세이브 아님)
	bool bCardExpanded = false;
};
