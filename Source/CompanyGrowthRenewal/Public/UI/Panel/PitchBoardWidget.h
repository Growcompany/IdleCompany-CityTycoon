#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Engine/TimerHandle.h"
#include "Data/ProjectBoardData.h"
#include "Enum/CompanyType.h"
#include "Enum/CountryType.h"
#include "Enum/QualityGrade.h"
#include "Table/ProjectDataTable.h"
#include "UI/PitchBoardText.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "PitchBoardWidget.generated.h"

class UBorder;
class UButton;
class UModalFrameWidget;
class UCommonTextBlock;
class UTextBlock;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UImage;
class UOverlay;
class UPanelWidget;
class UProgressBar;
class UScrollBox;
class UOfficeRequestCardWidget;
class UPitchTileWidget;
class UPitchTierTabWidget;
class UUniformGridPanel;
class UOfficeStageProgressManager;
class UTableManagerSubsystem;
class UHorizontalBox;
class ULaunchLootOddsWidget;
class UButtonWidget;
class UWidget;
class UManufacturingDevelopmentManager;
class UProductionOrderManager;
class UWidgetSwitcher;
class UMaterialInstanceDynamic;
struct FPitchTileData;
struct FManufacturingResearchStatus;

/**
 * 착수 = 기획 보드 (UI_PitchBoardPanel). 현재 티어 프로젝트를 타일로 깔고, 선택한 프로젝트를 우측 카드로 보여준다.
 * 판정/수익은 열 때마다 현재 착석 로스터로 재계산하고 저장하지 않는다.
 * 스펙: docs/superpowers/specs/2026-09-19-pitch-board-polish-design.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPitchBoardWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 튜토리얼 M6 앵커 — 지금 카드가 실제로 보이는 CTA
	UWidget* GetRecommendCardCtaWidget() const;
	UWidget* GetProjectTileWidget(int32 ProjectIndex) const;
	UWidget* GetProjectCardCtaWidget(int32 ProjectIndex) const;
	bool IsProjectCardGateRisk(int32 ProjectIndex) const;
	UWidget* GetCloseButtonWidget() const;

	// 보드를 특정 티어로 연다 (범위 밖은 현재 티어로 클램프)
	void OpenTier(int32 Tier);

	// 완료 결과에서 이어 온 제품의 티어와 수량 확보 카드를 선택한다.
	void OpenProject(int32 ProjectIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

	// 최초 진입 코치마크 (DT_PanelIntro PanelKey="PitchBoard")
	void TryPlayPanelIntro();
	void HandlePanelIntroFinished();
	FTimerHandle PanelIntroTimer;
	TWeakObjectPtr<class UPanelIntroOverlayWidget> PanelIntroOverlay;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ── 헤더 ──
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// ── 좌측 세로 티어 레일 ──
	// 탭은 런타임 생성(UPitchTierTabWidget). 디자이너에선 비어 있는 게 정상.
	UPROPERTY(meta = (BindWidget))
	UPanelWidget* TierRailBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UScrollBox* TierRailScroll;

	// 그리드 상단 진행 안내와 다음 단계 해금 진행 바. C++ 주입.
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_GridSub;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HeaderProgressBar;

	// ── 본문 ──
	// 카드 뒤 판정 글로우 — RenderCard 가 표시 카드의 판정색(통과 그린/미달 레드)으로 색+알파를 준다(기본 알파 0).
	// Optional 이라 새 트리 붙여넣기 전 구 WBP 도 컴파일된다(전환 창).
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* VerdictGlow;

	// 카드 전환 애니메이션 대상 — 글로우+카드를 함께 감싼 오버레이. 페이드/슬라이드는 RenderTransform 만(레이아웃 무영향).
	UPROPERTY(meta = (BindWidgetOptional))
	UOverlay* CardOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	UOfficeRequestCardWidget* Card;

	// ── 제조업 제품 상세 ── 프로젝트 모드와 같은 WBP 라 전부 Optional. 제조 모드에서 빠진 것은 경고 로그로 드러낸다.
	// 순서: 그림 무대 → 상태 띠 → 수량 구역 | 부족 안내판 → 포인트(팀 능력) 행 → 개발 시간 → 행동 줄.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ManufacturingDetailRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ManufacturingProductName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ManufacturingCoverImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgStepText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgSubName;

	// 상태 띠 ― 0 미개발 / 1 개발 중 / 2 개발 완료 / 3 개발 완료·포인트 부족 / 4 수량 확보 완료.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> MfgStatusSwitcher;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgStatusNoneRight;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> MfgStatusProgressBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgStatusProgressText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgStatusSecuredText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgQuantitySection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgQuantityHeading;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgQuantityHint;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ManufacturingQuantityText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> ManufacturingMinusButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> ManufacturingPlusButton;
	// 묶음 버튼 25% · 50% · 최대 ― 버튼 아래 실제 개수, 지금 값과 같은 버튼만 선택 링.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> MfgPresetQuarterButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> MfgPresetHalfButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> ManufacturingMaxButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgPresetQuarterCount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgPresetHalfCount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgPresetMaxCount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgPresetQuarterRing;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgPresetHalfRing;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgPresetMaxRing;

	// 1개분도 확보할 수 없을 때 수량 구역 대신 보이는 빨간 안내판.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgShortageNotice;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgShortageTitle;

	// 분야 3행은 이름 규칙(MfgField{Row,Alert,Icon,Name,Bar,Value,ValueSmall,Sub,Check}{0..2})으로 찾는다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgPointsSection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgPointsHeading;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgPointsHint;

	// 개발 시간 칸 ― 0 = 예상 근무 시간, 1 = 진행 막대.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgTimeSection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgTimeHeading;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgTimeHint;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> MfgTimeSwitcher;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgTimeValue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> MfgTimeProgressBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgTimePercent;

	// 행동 줄 ― 주 버튼의 두 줄 라벨은 버튼 위에 얹은 글자(히트 통과). 확보 완료 상태만 공장 이동 버튼이 나란히 선다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> ManufacturingReserveButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgReserveLabel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgReserveSub;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgFactoryRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> MfgFactoryButton;

	// 목록 머리 ― 제조 모드는 「N단계 제품 · 다음 단계 눈금 · n / m」로 바꾸고 프로젝트 머리를 접는다.
	// 눈금 칸은 MfgUnlockSeg{0..9}(바탕) / MfgUnlockSegOn{0..9}(채움) 이름 규칙.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MfgListHeader;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgListTitle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgUnlockCaption;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> MfgUnlockCount;

	// 제조 모드 열 폭(WBP 단위) ― 값은 WBP 기본값이 소유한다. 둘 중 하나라도 0 이면 손대지 않는다.
	// 대상 = RailSizer / ProjectColumnWidth (SizeBox). 줄인 만큼 DetailColumnWidth 가 받고, 프로젝트 모드면 원래 폭으로 되돌린다.
	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingRailWidth = 0.f;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingListWidth = 0.f;

	// 제조 모드 목록 머리 높이(ProjectProgressHeight). 0 이면 손대지 않는다.
	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingListHeaderHeight = 0.f;

	// 제조 모드 안쪽 여백 ― 레일 테두리(RailBorder)·목록 트레이(ProjectSectionTray). bOverride 가 꺼져 있으면 손대지 않는다.
	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	bool bOverrideManufacturingInsets = false;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing", meta = (EditCondition = "bOverrideManufacturingInsets"))
	FMargin ManufacturingRailPadding;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing", meta = (EditCondition = "bOverrideManufacturingInsets"))
	FMargin ManufacturingTrayPadding;

	// 수량 확보 뒤 상세판 ― 상태 띠가 두 줄로 커지는 만큼 그림 무대와 분야 줄을 줄인다. 0 이면 손대지 않는다.
	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingSecuredHeroHeight = 0.f;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingSecuredCoverHeight = 0.f;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingSecuredFieldRowHeight = 0.f;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	float ManufacturingSecuredFieldRowGap = 0.f;

	// 분야 막대 채움 틴트 ― 보통 / 이 확보로 바닥나는 분야(최대를 정하는 분야) / 모자람.
	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	FLinearColor ManufacturingBarNormal = FLinearColor::FromSRGBColor(FColor(0x4C, 0xBC, 0x5C));

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	FLinearColor ManufacturingBarWarn = FLinearColor::FromSRGBColor(FColor(0xF4, 0xA4, 0x45));

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	FLinearColor ManufacturingBarShort = FLinearColor::FromSRGBColor(FColor(0xDD, 0x55, 0x48));

	// 분야 행 아래 줄 잉크 ― 보통(캡션 하한) / 바닥 / 부족.
	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	FLinearColor ManufacturingSubInk = FLinearColor::FromSRGBColor(FColor(0x5A, 0x6B, 0x7D));

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	FLinearColor ManufacturingSubWarnInk = FLinearColor::FromSRGBColor(FColor(0xA4, 0x52, 0x0B));

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Manufacturing")
	FLinearColor ManufacturingSubShortInk = FLinearColor::FromSRGBColor(FColor(0xB3, 0x26, 0x1E));

	// 현재 티어 프로젝트의 스크롤 그리드 — 타일은 런타임 생성
	UPROPERTY(meta = (BindWidget))
	UUniformGridPanel* TileGrid;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* RewardPreviewBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* RewardPreviewPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* Text_RewardTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> RewardOddsButton;

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Rewards")
	FVector2D RewardCardSize = FVector2D(112.f, 112.f);

	UPROPERTY(EditAnywhere, Category = "PitchBoard|Rewards")
	FVector2D RewardIconSize = FVector2D(96.f, 96.f);

private:
	// 보드 1칸의 계산 결과 묶음. 타일·카드·추천이 모두 이 한 벌에서 나온다.
	struct FBoardRow
	{
		FProjectData Project;
		FProjectBoardSlot BoardSlot;
		PitchBoardText::FPitchBoardEntry Entry;
		PitchBoardText::ETileVerdict Verdict = PitchBoardText::ETileVerdict::Short;
		bool bDeveloped = false;
		bool bResearchInProgress = false;
		bool bQuantitySecured = false;
		bool bHasShipped = false;
		EQualityGrade ReleaseGrade = EQualityGrade::Max;
		// 제조 전용 ― 이 건물의 확보 잔량 합 / 지금 확보 가능한 최대 / 최초 개발 진행률(0~1)
		int32 SecuredQuantity = 0;
		int32 MaxReservable = 0;
		float ResearchFraction = 0.f;
	};

	// 로스터 수집 → 행 계산 → 레일/타일/카드/헤더 렌더. 여는 순간에만 돈다(틱 금지).
	void Rebuild();
	void RenderTiles();
	void RenderCard();
	FPitchTileData BuildTileData(const FBoardRow& BoardRow) const;
	// 포인트 적립(1초) 때 타일의 「확보 가능 N개」만 다시 칠한다 ― 타일을 새로 만들지 않는다.
	void RefreshManufacturingTiles();
	void RenderManufacturingDetail();
	void RenderManufacturingResearch(const FManufacturingResearchStatus& Research, bool bLocked, bool bLabelsValid);
	void RenderManufacturingQuantity(const FBoardRow& BoardRow, bool bLocked, bool bLabelsValid);
	void SetManufacturingFieldRow(int32 FieldIndex, bool bVisible, const FText& Value, const FText& ValueSmall,
		const FText& Sub, float BarFraction, int32 Severity, bool bShowCheck);
	void SetManufacturingCta(const FText& Label, const FText& Sub, bool bEnabled);
	void CacheManufacturingFieldWidgets();
	void ApplyManufacturingLayout();
	void SetManufacturingQuantity(int32 NewQuantity);
	int32 GetManufacturingMaxQuantity() const;
	void HandleManufacturingReserve();
	void HandleManufacturingMinus();
	void HandleManufacturingPlus();
	void HandleManufacturingQuarter();
	void HandleManufacturingHalf();
	void HandleManufacturingMax();
	void HandleManufacturingFactoryMove();
	ECountryType FindManufacturingFactoryCountry(int32 ProjectIndex) const;
	void HandleManufacturingPointsChanged(int32 BuildingID);
	bool IsManufacturingBoard() const { return IsManufacturingType(Industry); }
	int32 ManufacturingQuantity = 1;
	// 최대를 따라가는 중 ― 포인트가 늘면 수량도 늘어난다. 사용자가 다른 값을 고르면 끈다.
	bool bManufacturingQuantityAtMax = true;
	int32 DisplayedManufacturingCoverProject = INDEX_NONE;
	int32 DisplayedResearchProjectIndex = INDEX_NONE;

	// − / + 길게 누르기 ― 0.35초 뒤부터 반복하고 오래 누를수록 한 번에 크게 움직인다.
	void HandleManufacturingMinusPressed();
	void HandleManufacturingPlusPressed();
	void HandleManufacturingStepReleased();
	void TickManufacturingRepeat();
	FTimerHandle ManufacturingRepeatTimer;
	int32 ManufacturingRepeatDirection = 0;
	int32 ManufacturingRepeatTicks = 0;
	// 길게 누르기로 이미 움직였으면 손을 뗄 때 오는 클릭 1회를 무시한다.
	bool bManufacturingRepeatFired = false;

	// 분야 행 위젯 캐시(이름 규칙 조회). 3행 × 9종.
	struct FManufacturingFieldWidgets
	{
		TWeakObjectPtr<UWidget> Row;
		TWeakObjectPtr<UWidget> Alert;
		TWeakObjectPtr<UImage> Icon;
		TWeakObjectPtr<UCommonTextBlock> Name;
		TWeakObjectPtr<UProgressBar> Bar;
		TWeakObjectPtr<UCommonTextBlock> Value;
		TWeakObjectPtr<UCommonTextBlock> ValueSmall;
		TWeakObjectPtr<UCommonTextBlock> Sub;
		TWeakObjectPtr<UWidget> Check;
	};
	FManufacturingFieldWidgets ManufacturingFieldWidgets[3];
	bool bManufacturingFieldWidgetsCached = false;

	// 프로젝트 모드로 재사용될 때 되돌릴 원래 열 폭(첫 적용 때 기록).
	TOptional<float> OriginalRailWidth;
	TOptional<float> OriginalListWidth;
	TOptional<float> OriginalDetailWidth;
	TOptional<float> OriginalListHeaderHeight;
	TOptional<FMargin> OriginalRailPadding;
	TOptional<FMargin> OriginalTrayPadding;
	// 확보 뒤 촘촘 배치에서 되돌릴 원래 치수 ― [0]=그림 무대 높이, [1]=그림 높이, [2]=분야 줄 높이, [3]=분야 줄 간격.
	TOptional<FVector4f> OriginalDetailMetrics;
	void ApplyManufacturingDetailDensity(bool bCompact);

	// 상세 그림 무대의 제품 키 재질 MID(흰 배경을 무대에 녹인다).
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ManufacturingCoverMID;

	// 좌측 레일 = MaxTier() 개 티어탭을 런타임 생성해 TierRailBox 에 채운다
	void RenderTierRail();
	// 그리드 상단의 다음 단계 해금 안내와 진행 바를 다시 채운다
	void RenderGridHeader();
	void RenderRewardPreview();
	void HandleRewardOddsClicked();
	void HideRewardOdds();

	TArray<FLaunchLootPreviewEntry> RewardPreviewEntries;

	UPROPERTY(Transient)
	TObjectPtr<ULaunchLootOddsWidget> RewardOddsPopup;

	// 선택 링만 다시 칠한다 — 타일 재생성 없이 선택을 옮기는 경로
	void UpdateTileSelection();
	// 레일 탭 선택만 다시 칠한다 (타일의 UpdateTileSelection 미러)
	void UpdateTierRailSelection();

	const FBoardRow* FindRow(int32 ProjectIndex) const;

	// 카드와 같은 식(ComputeDisciplineTarget)으로 최저 달성률 직능을 뽑는다
	static void ComputeWorstDiscipline(const FProjectBoardSlot& BoardSlot, float& OutRatio, int32& OutSlot);

	UFUNCTION()
	void HandleTileClicked(int32 ProjectIndex);

	UFUNCTION()
	void HandleCardAccepted(int32 SlotIndex);

	// 잠금 탭 클릭은 토스트로 안내하고, 열린 탭 전환은 그룹 선택 변경에서 처리한다.
	UFUNCTION()
	void HandleTierTabClicked(int32 Tier);

	UFUNCTION()
	void HandleTierSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	UTableManagerSubsystem* GetTableMgr() const;
	UOfficeStageProgressManager* GetStageMgr() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPitchTileWidget>> Tiles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPitchTierTabWidget>> TierTabs;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonGroupBase> TierButtonGroup;

	bool bUpdatingTierRail = false;
	bool bScrollSelectedTierPending = false;

	TArray<FBoardRow> Rows;

	ECompanyType Industry = ECompanyType::Game;
	int32 ViewTier = 0;          // 0 = 미결정(첫 Rebuild 에서 현재 티어로)
	int32 CurrentTier = 1;

	// 둘 다 ProjectIndex (배열 인덱스 아님) — 티어를 넘나들어도 같은 키로 남는다
	int32 RecommendedIndex = INDEX_NONE;
	int32 SelectedIndex = INDEX_NONE;

	// ── 카드 전환 애니메이션 ── 타일 선택 시 카드가 페이드아웃+슬라이드다운 → 새 내용 → 페이드인+슬라이드업.
	// NativeTick 이 RenderTransform(오버레이)만 굴린다. 나가는 페이즈 끝에서 RenderCard() 로 내용을 교체.
	void BeginCardChangeTransition();
	bool bCardTransitioning = false;
	bool bCardExiting = false;
	float CardTransElapsed = 0.0f;
};
