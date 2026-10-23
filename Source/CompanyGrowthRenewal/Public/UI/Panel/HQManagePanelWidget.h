// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Table/HQLevelData.h"
#include "HQManagePanelWidget.generated.h"

class UButtonWidget;
class UCostActionButtonWidget;
class UIconWithButtonWidget;
class UWidgetSwitcher;
class UScrollBox;
class UHorizontalBox;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UButton;
class UModalFrameWidget;
class UHQConditionCardWidget;
class UHQKpiCellWidget;
class UCommonTextBlock;
class UProgressBar;
class UImage;
class UTexture2D;
class UWrapBox;
class UUniformGridPanel;
struct FHQRewardSlot;
enum class EResourceType : uint8;

// 빌딩 목록 정렬 기준 (패널 내부 전용)
enum class EHQBuildingSortMode : uint8
{
	Level,
	Income,
	Employees
};

/**
 * 본사 관리 패널 — 성장 / 운영 현황 / 보유 빌딩 3탭.
 * 설계·목업 대응표: docs/superpowers/specs/2026-10-11-hq-panel-3tab-design.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQManagePanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UWidget* GetGrowthGuideTargetWidget(bool* bOutConditionsUnmet = nullptr) const;
	bool IsGrowthTabActive() const { return CurrentTabIndex == GrowthTabIndex; }

	// 튜토리얼 M29 — 자동 재생은 ShouldPlay 가 막으므로 명시적으로 부른다(본 적 있어도 재생). 정리는 기존 NativeOnDeactivated 경로가 맡는다
	bool PlayPanelIntro();
	// 튜토리얼 M30 — 레벨업 뒤 남은 본사 창을 닫고 건설로 가게 짚는다(헤더 X)
	UWidget* GetCloseButtonWidget() const;
	bool IsPanelIntroPlaying() const { return PanelIntroOverlay.IsValid(); }

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 최초 진입 코치마크 (DT_PanelIntro PanelKey="HQManage")
	void TryPlayPanelIntro();
	void HandlePanelIntroFinished();
	FTimerHandle PanelIntroTimer;
	TWeakObjectPtr<class UPanelIntroOverlayWidget> PanelIntroOverlay;

	// ===== 공용 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	// 공통 셸과 헤더. 본문 탭과 게임 동작은 이 패널이 소유한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

	// ===== 레일 탭 — 순서 = ContentSwitcher 인덱스 (성장 0 / 운영 현황 1 / 보유 빌딩 2) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* HQTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* OperationsTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* BuildingListTab;

	// 레벨업 가능한데 다른 탭을 보는 중이면 성장 탭에 알림 점
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GrowthTabAlert;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* ContentSwitcher;

	// ===== 성장 탭 — 회사 레벨 띠 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* CurLevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HeroTitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HeroSubText;

	// 종합 진행바 — 활성 조건들의 정규화 진행률 평균 (이산 충족 카운트 아님)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* OverallProgressBar;

	// "81%" / 최대 레벨 "MAX"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* OverallProgressText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HeroStatusText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> HeroMissingText;

	// 명판 광택 VFX — 저작에 있으면 동작
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LevelBadgeGloss;

	// ===== 성장 탭 — 일반 레벨 본문 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> GrowthBody;

	// DT 목표 0 = 조건 없음 → 카드 Collapsed. 보이는 카드만 2열로 채운다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHQConditionCardWidget* Row_Cond_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHQConditionCardWidget* Row_Cond_Buildings;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHQConditionCardWidget* Row_Cond_Tier;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHQConditionCardWidget* Row_Cond_Employees;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHQConditionCardWidget* Row_Cond_MarketCap;

	// 조건 카드 묶음 ― 패널 안내(DT_PanelIntro HQManage_1)가 카드 전체를 한 번에 비추는 앵커
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> ConditionGrid;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RewardTitleText;

	// 다음 레벨에서 실제로 받는 것 — UIE_HQRewardSlot 을 보상 수만큼 런타임 생성
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> RewardSlotBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* UnlockText;

	// 해금 표에 그림 열이 없는 보상의 아이콘 (저작이 넣는다)
	UPROPERTY(EditDefaultsOnly, Category = "HQ|Reward")
	TObjectPtr<UTexture2D> RandomRewardIcon;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Reward")
	TObjectPtr<UTexture2D> WorldMapRewardIcon;

	// 보상 칸 사이 (가로, 세로) — 2개일 때만 가로를 넓힌다(큰 칸 3개는 넓으면 한 줄에 안 들어간다)
	UPROPERTY(EditDefaultsOnly, Category = "HQ|Reward")
	FVector2D RewardSlotGap = FVector2D(26.f, 18.f);

	UPROPERTY(EditDefaultsOnly, Category = "HQ|Reward")
	FVector2D FewRewardSlotGap = FVector2D(44.f, 18.f);

	// 레벨업 버튼 — 비용과 승급 조건을 함께 표시한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCostActionButtonWidget* LevelUpButton;

	// 레벨업 가능 동안 CTA 위를 주기 횡단하는 샤인 (버튼 Overlay 안, ClipToBounds 전제)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CTAShineImage = nullptr;

	// ===== 성장 탭 — 최고 레벨 본문 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> MaxBody;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> LevelStepBox;

	// ===== 운영 현황 탭 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHQKpiCellWidget> Kpi_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHQKpiCellWidget> Kpi_MarketCap;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHQKpiCellWidget> Kpi_Buildings;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHQKpiCellWidget> Kpi_Employees;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> IndustryCountText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> IndustryCardBox;

	// ===== 보유 빌딩 탭 =====

	// CommonHierarchicalScrollBox는 UScrollBox 상속 — 부모 타입으로 바인딩
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* BuildingListScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ListHeaderText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* SortLevelBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* SortIncomeBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* SortEmployeeBtn;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FHQGrowthGuideTargetTestAccessor;
#endif
	static constexpr int32 GrowthTabIndex = 0;
	static constexpr int32 OperationsTabIndex = 1;
	static constexpr int32 BuildingsTabIndex = 2;
	// 애니메이티드 스위처는 전환 페이드가 끝나야 GetActiveWidgetIndex 가 바뀌므로 선택 탭을 따로 든다.
	int32 CurrentTabIndex = GrowthTabIndex;

	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	UPROPERTY()
	UCommonButtonGroupBase* SortButtonGroup = nullptr;

	// 기본 = 단계 내림차순. 같은 모드 재클릭 시 방향 토글 (ProductSellModal 컨벤션)
	EHQBuildingSortMode BuildingSortMode = EHQBuildingSortMode::Level;
	bool bBuildingSortAscending = false;

	int32 CachedHQLevel = 1;

	// 빌딩 관리 패널로 인계 중. 닫힘 애니메이션 탓에 이 패널의 비활성화는 후임 패널이 열린 뒤에 오므로,
	// 그때의 입력 모드/가림 고스트는 이미 후임 것이다 (되돌리면 후임 고스트가 죽는다)
	bool bHandingOffToManagePanel = false;

	// RefreshAffordability(경량 경로)용 스냅샷 — 돈 외 조건은 UpdateHQInfo 시점 값 재사용
	bool bNextLevelAvailable = false;
	FHQLevelData CachedNextData;
	int32 CachedBuildingCount = 0;
	int32 CachedTierBuildingCount = 0;
	int32 CachedTotalEmployees = 0;
	int64 CachedMarketCap = 0;
	bool bCanLevelUpNow = false;

	// 장식 VFX 상태 — 이벤트 구동 패널이라 틱은 VFX 전용 (이미지 없으면 즉시 탈출)
	bool bCTAShineActive = false;
	float VFXTimer = 0.f;
	static constexpr float ShinePeriod = 2.6f;     // 스윕 주기 (이동 + 휴지)
	static constexpr float ShineTravel = 0.6f;     // 횡단 시간
	static constexpr float ShineSweepHalf = 180.f; // 횡단 반폭 (버튼 폭 기준, 시각루프로 튜닝)

	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void HandleHQLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleStageComplete();

	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	void HandleEmployeeHireCompleted(const FString& EmployeeID);

	void OnLevelUpButtonClicked();

	void UpdateHQInfo();
	void UpdateGrowthBody(int32 NextLevel);
	void PopulateLevelSteps();
	void UpdateOperations(int64 CurrentMoney);

	// Money 변동 경량 경로 — OnResourceChanged는 고빈도라 전체 갱신 금지
	void RefreshAffordability(int64 CurrentMoney);

	// 보상 카드와 축하 화면이 같은 DataTable 해금 문구를 사용한다.
	FText BuildLevelRewardText(int32 Level) const;

	// 보상 칸 — 업종·채용권·랜덤 보상(지급 규칙) / 공장·세계지도(게이트) / 메뉴(DT_MenuUnlock + 메뉴 시트 타일) / 빌딩 종류(DT 크기별)
	void PopulateRewardSlots(int32 Level);
	void CollectRewardSlots(int32 Level, TArray<FHQRewardSlot>& OutSlots) const;

	void UpdateCountCondition(UHQConditionCardWidget* Card, const FText& Label, int32 Current, int32 Target, const FText& Unit);
	// 자금·시총 — int64 는 float 정밀도 한계라 축약 문자열 + 비율 직접 계산 (보유=Floor / 목표=Ceil)
	void UpdateCurrencyCondition(UHQConditionCardWidget* Card, const FText& Label, int64 Current, int64 Target);
	void UpdateOverallProgress(int64 CurrentMoney);
	float ComputeOverallProgress(int64 CurrentMoney) const;
	int32 ComputeUnmetCount(int64 CurrentMoney) const;
	FText BuildMissingConditionNames(int64 CurrentMoney) const;
	void UpdateLevelUpButtonState();
	void UpdateGrowthTabAlert();

	void PopulateBuildingList();
	void OnBuildingMoveClicked(int32 BuildingIndex);

	// 정렬 바 — 그룹은 배타 선택 비주얼만, 모드/방향은 개별 OnPressed 핸들러가 관리
	void SetupSortButtonGroup();
	void UpdateSortButtonTexts();
	void HandleSortLevelClicked();
	void HandleSortIncomeClicked();
	void HandleSortEmployeeClicked();
};
