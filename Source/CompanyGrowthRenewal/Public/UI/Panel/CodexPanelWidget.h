#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/CompanyType.h"
#include "CodexPanelWidget.generated.h"

class UCommonTextBlock;
class UButtonWidget;
class UCloseButtonWidget;
class UButton;
class UImage;
class UWidgetSwitcher;
class UScrollBox;
class UHorizontalBox;
class UVerticalBox;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UFont;
class UTexture2D;

// 출시작 정렬 기준
UENUM()
enum class EShippedSort : uint8
{
	ByReview,    // 평점순
	ByRevenue    // 누적매출순
};

/**
 * 포트폴리오 패널 (UI_CodexPanel) — 건물(회사) 개별 뷰. 2탭: [조합 발견]=10티어 진척 지도 / [출시작].
 * 표시명만 "포트폴리오", 내부명 Codex 유지. 산업 시그니처색을 입음(런타임 DT AccentColor 주입).
 * ★ 열람 전용 — 착수는 기획 보드(UI_PitchBoardPanel)가 단일 창구다.
 *   셀에 개발비/수익/소요가 없어 정보 없이 돈을 쓰게 되므로 2026-07-26 착수 배선을 제거했다.
 * 로드맵 노드(UIE_RoadmapNode)/조합 셀(UIE_PortfolioCell)은 재사용 컴포넌트를 CreateWidget+Configure 로 채움
 * (컨테이너 RoadmapBox/DexGroupsBox 만 BindWidget). 데이터 = DiscoveredCombos/장르×소재 DT + 티어 진척.
 * 설계 SOT = memory project_portfolio_panel_design.md / 목업 docs/05_UI/Codex_Improved_MOCKUP.html.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCodexPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- 탭 (세그먼트 트랙) ----
	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Tab_Discovery;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Tab_Shipped;

	// 0=조합 발견 / 1=출시작
	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* CodexSwitcher;

	// ---- 페이지 0: 조합 발견 ----
	// 가로 로드맵 컨테이너 — T1~T10 노드는 C++ 생성
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* RoadmapBox;

	// 도감 그룹 컨테이너(장르별) — 장르 그룹 + 소재 셀은 C++ 생성
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* DexGroupsBox;

	// ---- 페이지 1: 출시작 (행은 C++ 생성) ----
	UPROPERTY(meta = (BindWidget))
	UScrollBox* ShippedList;

	// 공용 닫기 X — 타입은 반드시 UCloseButtonWidget (UButtonWidget 아님)
	UPROPERTY(meta = (BindWidget))
	UCloseButtonWidget* CloseButton;

	// ---- 헤더: 회사 정체성 ----
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* CompanyNameText;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* CompanyIndustryText;

	// ---- 랭크 요약칩 (탭 트랙 우측) ----
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_RankTier;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_RankTitle;

	// ---- 선택 티어 상세 헤더 ----
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_TierRank;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_TierProgress;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_TierReward;

	// ---- 산업색 주입 대상 (비주얼, 선택) ----
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* AccentBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* CompanyPlate;

	// 로드맵 진척 채움 라인 — C++ 가 폭(RenderScale.X)/산업색 주입
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* RoadmapFillLine;

	// 딤 배경 클릭 = 닫기 (선택)
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// ---- 출시작 정렬 토글 [평점순 | 매출순] (선택) ----
	UPROPERTY(meta = (BindWidgetOptional))
	UButtonWidget* SortByReviewBtn;

	UPROPERTY(meta = (BindWidgetOptional))
	UButtonWidget* SortByRevenueBtn;

private:
	void ApplyCompanyIdentity();   // 산업색/이름/칩 — 정적 1회
	void RefreshRankSummary();     // 랭크 요약칩
	void PopulateRoadmap();        // 티어 노드 (스텁)
	void RefreshTierDetail();      // 선택 티어 헤더 + 그리드
	void PopulateTierGrid();       // 도감: 장르 그룹 × 소재 셀 (DiscoveredCombos 발견 판정 + 발견 카운트)
	void PopulateShipped();        // 출시작 리스트

	// 이 회사의 자체개발 이력 — 오피스면 매니저, 그 외(MainMap 등)는 세이브 OfficeDataMap 에서.
	void ResolveDevelopedProjects(TArray<int32>& OutDeveloped) const;

	UCommonTextBlock* MakeText(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold) const;  // ShippedList 행 전용

	UFUNCTION()
	void OnTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseDelegate();

	UFUNCTION()
	void OnBackgroundClicked();

	UFUNCTION()
	void OnShippedSortChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	ECompanyType Industry = ECompanyType::Game;
	int32 CurrentTier = 1;
	int32 SelectedTier = 1;   // 로드맵에서 선택된 티어(기본 = 현재)
	EShippedSort ShippedSort = EShippedSort::ByReview;
	FLinearColor IndustryColor = FLinearColor(0.391f, 0.091f, 0.930f, 1.0f);  // #A855F7 폴백

	UPROPERTY()
	UCommonButtonGroupBase* TabGroup = nullptr;

	UPROPERTY()
	UCommonButtonGroupBase* SortGroup = nullptr;

	// 코드 생성 텍스트용 폰트 (WBP 미경유 — Roboto 폴백이면 한글 깨져서 직접 로드)
	UPROPERTY()
	UFont* BoldFont = nullptr;

	UPROPERTY()
	UFont* RegularFont = nullptr;
};
