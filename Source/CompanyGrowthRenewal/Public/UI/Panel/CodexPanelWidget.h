#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/CompanyType.h"
#include "CodexPanelWidget.generated.h"

class UCommonTextBlock;
class UButtonWidget;
class UModalFrameWidget;
class UIconWithButtonWidget;
class UButton;
class UImage;
class UWidget;
class UPanelWidget;
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
 * 도감 패널 (UI_CodexPanel) — 좌측 산업 레일로 6산업 전체를 넘나드는 뷰. 2탭: [프로젝트]=10티어 진척 지도 / [출시작].
 * 표시 산업의 권위는 레일 선택이다 — GameInstance 의 현재 건물 산업은 초기 선택으로만 쓴다.
 * 표시명 "도감"(업적 A_Codex_1~5 가 쓰는 말), 내부명 Codex 유지. 산업 시그니처색은 레일 탭 웰에 싣는다(런타임 DT AccentColor).
 * ★ 열람 전용 — 착수는 기획 보드(UI_PitchBoardPanel)가 단일 창구다.
 *   셀에 개발비/수익/소요가 없어 정보 없이 돈을 쓰게 되므로 2026-07-26 착수 배선을 제거했다.
 * 로드맵 노드(UIE_RoadmapNode)/조합 셀(UIE_PortfolioCell)은 재사용 컴포넌트를 CreateWidget+Configure 로 채움
 * (컨테이너 RoadmapBox/DexGroupsBox 만 BindWidget). 데이터 = DiscoveredCombos/장르×소재 DT + 티어 진척.
 * 설계 SOT = memory project_portfolio_panel_design.md / 목업 docs/05_UI/Codex_Improved_MOCKUP.html.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCodexPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;

	// ---- 좌측 산업 레일 ----
	// 탭은 런타임 생성(UIE_IndustryRailTab). 디자이너에선 비어 있는 게 정상.
	UPROPERTY(meta = (BindWidget))
	UPanelWidget* IndustryRailBox;

	// ---- 탭 (세그먼트 트랙) ----
	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Tab_Discovery;

	UPROPERTY(meta = (BindWidget))
	UButtonWidget* Tab_Shipped;

	// 0=조합 발견 / 1=출시작
	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* CodexSwitcher;

	// ---- 페이지 0: 조합 발견 ----
	// 대표작 3 ― 기록이 없으면 섹션째 Collapsed (빈 판 금지)
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* TopHitsSection;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* TopHitsBox;

	// 가로 로드맵 컨테이너 — T1~T10 노드는 C++ 생성
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* RoadmapBox;

	// 도감 그룹 컨테이너(장르별) — 장르 그룹 + 소재 셀은 C++ 생성
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* DexGroupsBox;

	// ---- 페이지 1: 출시작 (행은 C++ 생성) ----
	UPROPERTY(meta = (BindWidget))
	UScrollBox* ShippedList;

	// 공통 셸·제목·도움말·닫기 영역
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

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
	// 랭크 칩 뒤판 — 선택 산업의 AccentColor 를 곱해 칩 채움/테두리를 물들인다
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
	void PopulateIndustryRail();   // 레일 탭 생성(가용 업종만) + 요약 주입
	void SelectIndustry(ECompanyType InIndustry);   // 산업 전환 + 전 구획 재빌드
	void ApplyCompanyIdentity();   // 산업색/업종 강조 — 산업이 바뀔 때마다
	void RefreshIndustryRecord();  // 기록 요약 4항 — 그리드가 카운트를 채운 뒤에 불러야 한다
	void PopulateRoadmap();        // 진척 한 줄 (R2: 10칸 사다리 폐지)
	void PopulateTopHits();        // 누적매출 상위 3
	void PopulateComboGrid();      // 도감: 전 티어를 한 판에, 장르 그룹 × 소재 셀
	void PopulateShipped();        // 출시작 리스트

	// 선택 산업의 자체개발 이력 — 그 산업 건물들의 합집합. 건물별로 갈리면 같은 작품이 한쪽에서 '?'가 된다.
	void ResolveDevelopedProjects(TArray<int32>& OutDeveloped) const;

	UCommonTextBlock* MakeText(const FText& InText, int32 Size, const FLinearColor& Color, bool bBold) const;  // ShippedList 행 전용

	UFUNCTION()
	void OnIndustryTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseDelegate();

	UFUNCTION()
	void OnBackgroundClicked();

	UFUNCTION()
	void OnShippedSortChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	ECompanyType Industry = ECompanyType::Game;
	// 미진출은 0 이다. 1 로 두면 진출한 적 없는 산업이 "TIER 1"으로 표시된다(R2 이전 버그).
	int32 CurrentTier = 0;

	// ===== 산업 총괄 기록 (R2) — 표시 직전에 채워진다 =====
	bool bIndustryOwned = false;
	int64 IndustryRevenue = 0;
	int32 IndustryShippedCount = 0;
	int32 DiscoveredCount = 0;   // 그리드 순회가 채운다
	int32 TotalComboCount = 0;   // 〃 (분모)
	EShippedSort ShippedSort = EShippedSort::ByReview;
	FLinearColor IndustryColor = FLinearColor(0.391f, 0.091f, 0.930f, 1.0f);  // #A855F7 폴백

	UPROPERTY()
	UCommonButtonGroupBase* RailGroup = nullptr;

	// 레일 탭 인덱스 → 산업 (표시 순서 = ECompanyType 선언 순서)
	TArray<ECompanyType> RailIndustries;

	TSet<ECompanyType> OwnedIndustries;

	// SelectIndustry 가 한 번이라도 돌았는가 ― 델리게이트가 이미 적용한 뒤 폴백이 또 리빌드하는 걸 막는다
	bool bIndustryApplied = false;

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
