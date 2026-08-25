// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "Data/ProductionOrderData.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/MineManager.h"
#include "CountryDetailWidget.generated.h"

class UImage;
class UButton;
class UVerticalBox;
class UScrollBox;
class UWidgetSwitcher;
class UCommonTextBlock;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UButtonWidget;
class UTabButtonWidget;
class UCloseButtonWidget;
class UWorldFactoryLineWidget;
class UMineLineWidget;
class UProductionOrderManager;
class UWidget;

/**
 * UCountryDetailWidget
 * WorldMap에서 국가 클릭 시 여는 허브 패널. 단일 클래스가 모든 탭 로직을 관리.
 *
 * 탭 구성:
 *  - 정보 : 국가 특산 + 해금 제품 리스트 (항상 표시)
 *  - 공장 : 생산(라인) / 강화(6슬롯) 서브탭 - 지원 국가만 표시
 *  - 채광 : 조업 / 강화 서브탭 - 지원 국가만 표시 (Mine Manager 추후)
 *
 * 탭 가시성은 DT_CountryInfo의 bSupports* 필드로 제어.
 * 미지원 탭 선택 중이면 정보 탭으로 자동 이동.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountryDetailWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCountryDetailCloseRequested);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountryDetailTabChanged, int32, TabIndex);

	UPROPERTY(BlueprintAssignable, Category = "CountryDetail|Events")
	FOnCountryDetailCloseRequested OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "CountryDetail|Events")
	FOnCountryDetailTabChanged OnTabChanged;

	// 표시할 국가 지정 → DT 조회 → 탭 가시성 설정 + 모든 탭 내용 갱신
	UFUNCTION(BlueprintCallable, Category = "CountryDetail")
	void SetCurrentCountry(ECountryType Country);

	// 외부 호출자 호환 별칭
	UFUNCTION(BlueprintCallable, Category = "CountryDetail")
	void SetCountry(ECountryType Country) { SetCurrentCountry(Country); }

	UFUNCTION(BlueprintPure, Category = "CountryDetail")
	ECountryType GetCurrentCountry() const { return CurrentCountry; }

	UFUNCTION(BlueprintCallable, Category = "CountryDetail")
	void SwitchToTab(int32 TabIndex);

	UFUNCTION(BlueprintPure, Category = "CountryDetail")
	int32 GetCurrentTabIndex() const { return CurrentTabIndex; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── 좌측 국가 정보 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CountryFlagImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CountryNameText;

	// 무역 허브 배지 (레퍼런스 좌측 카드의 LevelText 자리) — 허브 아니면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HubBadgeBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MoneyBiasText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MarketCapBiasText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PortDescText;

	// ── 헤더 / 배경 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn;

	// ── 상위 탭 버튼 (UIE_TabButton 합성 래퍼) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTabButtonWidget> InfoTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTabButtonWidget> FactoryTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTabButtonWidget> MineTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> MainContentSwitcher;

	// ── 정보 탭: 시장 머리말 밴드 (레퍼런스 특성 탭 EquippedBandBorder 대응) ──
	// 성향/능력 칩은 넣지 않는다 — 좌측 미니카드·탭바와 중복되면 이 리디자인이 고치려는 결함의 재현이다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CountryBandSpecialtyText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CountryFlavorText;

	// ── 정보 탭: 음각 well (TraitWellBorder 대응) — 수요 카드 런타임 생성 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<class UPanelWidget> MarketDemandContainer;

	// ── 정보 탭: 스트립 2줄 — 값이 비면 라벨까지 숨기려고 Row/Text 쌍으로 바인딩 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SupportedIndustryRow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SupportedIndustryText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MinableResourceRow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MinableResourceText;

	// ── 공장 탭: 서브탭 (UI_Element_Button + CUI_Style2_TapButton_Blue 토글) ──
	// 레퍼런스 스킨 탭의 ExteriorBtn/LightingBtn 과 같은 부품. UButtonWidget 은 UCommonButtonBase
	// 파생이라 ButtonGroup 에 직접 등록한다 (UTabButtonWidget 처럼 GetButton() 경유 불필요).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> FactoryProduceTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> FactoryEnhanceTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> FactoryContentSwitcher;

	// ── 공장 탭: 생산 페이지 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> FactoryLineScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> CreateButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockedBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LockConditionTextBlock;

	// ── 공장 탭: 강화 페이지 — 동적 생성 컨테이너 ──
	// WBP 에서 ScrollBox/VerticalBox/Panel 어떤 컨테이너든 BindWidgetOptional 매칭됨.
	// 자식 슬롯들은 RebuildFactoryEnhancementSlots() 가 DT 기반으로 런타임에 채움.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<class UPanelWidget> FactoryEnhanceSlotContainer;

	// ── 채광 탭: 서브탭 (공장과 동일 토글 문법) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MineOperateTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MineEnhanceTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> MineContentSwitcher;

	// ── 채광 탭: 조업 페이지 (Mine Manager 추후, 바인딩만 확보) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MineLineScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> MineCreateButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MineLockedBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MineLockConditionText;

	// ── 채광 탭: 강화 페이지 — 동적 생성 컨테이너 (FactoryEnhanceSlotContainer 패턴) ──
	// RebuildMineEnhancementSlots() 가 DT 기반으로 ClearChildren + 5개 슬롯 동적 생성.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<class UPanelWidget> MineUpgradeSlotScrollBox;

	// ── 디자이너 조절 ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CountryDetail")
	FText SlotFullLockReason = NSLOCTEXT("CountryDetail", "SlotFull", "슬롯 가득참");

	// 수요 카드는 런타임 생성이라 WBP 에 슬롯이 없다 — 간격은 여기서만 조절 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CountryDetail")
	float MarketDemandCardGap = 14.f;

public:
	// 외부(CountryRouterPanelWidget 등)에서 매직넘버 없이 참조하기 위한 탭 인덱스 상수.
	static constexpr int32 TAB_INDEX_INFO = 0;
	static constexpr int32 TAB_INDEX_FACTORY = 1;
	static constexpr int32 TAB_INDEX_MINE = 2;

	static constexpr int32 SUBTAB_INDEX_PRODUCE = 0;
	static constexpr int32 SUBTAB_INDEX_ENHANCE = 1;

private:
	ECountryType CurrentCountry = ECountryType::Korea;
	int32 CurrentTabIndex = TAB_INDEX_INFO;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> MainTabGroup;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> FactorySubTabGroup;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> MineSubTabGroup;

	UPROPERTY()
	TObjectPtr<UWorldFactoryManager> FactoryMgr;

	UPROPERTY()
	TObjectPtr<UProductionOrderManager> OrderMgr;

	UPROPERTY()
	TArray<TObjectPtr<UWorldFactoryLineWidget>> SpawnedFactoryLines;

	// 강화 탭의 동적 생성 슬롯 캐시 (UpgradeType -> 런타임 생성된 UpgradeSlot)
	UPROPERTY()
	TMap<EWorldFactoryUpgradeType, class UUpgradeSlot*> FactoryEnhanceSlots;

	// DT 탭 지원 캐시
	bool bSupportsFactory = false;
	bool bSupportsMine = false;
	bool bExternalLocked = false;
	FText ExternalLockReason;

	// Lifecycle
	void InitializeMainTabs();
	void InitializeFactorySubTabs();
	void InitializeMineSubTabs();
	void BindManagerEvents();
	void UnbindManagerEvents();

	// Country 전환 시 업데이트 루틴
	void ApplyCountryBasics();
	void ApplyTabVisibility();
	void EnsureCurrentTabValid();
	void RebuildAllTabContent();
	void RebuildInfoTab();
	// 생산가능 산업 / 채광 자원 스트립 — 값 비면 Row 째 Collapsed
	void RebuildInfoChips(const struct FCountryInfoTable& Info);
	void RebuildFactoryTab();

	// Factory 내부
	void RefreshFactoryLockState();
	UWorldFactoryLineWidget* SpawnFactoryLineWidget(const FWorldFactoryLineState& State);
	UWorldFactoryLineWidget* FindFactoryLineWidgetById(int32 LineId) const;
	static FTimespan CalcRemainingTime(const FWorldFactoryLineState& State);

	// Factory 강화 탭 — 동적 슬롯 생성/갱신 (BuildingManagePanelWidget 패턴 이식)
	// CompanyType 필터는 없음 (5종 전부 모든 국가 노출)
	void RebuildFactoryEnhancementSlots();
	// 단일 슬롯 UI 갱신 (현재 레벨, 표시값, 비용)
	void UpdateFactoryEnhancementSlot(EWorldFactoryUpgradeType UpgradeType, class UUpgradeSlot* InSlot);
	// DT 정의를 슬롯 멤버에 주입 (Description, SubDescription, ValueUnit, bIsInteger, Icon, CostResourceType)
	void ApplyFactorySlotDefinition(class UUpgradeSlot* InSlot, const struct FWorldFactoryUpgradeDefinition& Def);
	// 업그레이드 버튼 동적 바인딩 (슬롯 생성 시 1회)
	void BindFactorySlotButton(class UUpgradeSlot* InSlot, EWorldFactoryUpgradeType Type);
	// 슬롯별 업그레이드 클릭 핸들러
	void OnFactoryUpgradeClicked(EWorldFactoryUpgradeType Type);

	// 홀드 연속 강화 (LineExpansion 제외 — 마일스톤형이라 1회만)
	void StartFactoryHold(EWorldFactoryUpgradeType Type);
	void StopFactoryHold();
	void OnFactoryHoldTick();

	// 업그레이드 거부 안내 (공장/채광 공용). 홀드는 10Hz 라 알림 쿨다운이 스팸을 막는다.
	void NotifyUpgradeBlocked(EResourceType CostType, int64 Cost, bool bMaxLevel) const;

	FTimerHandle FactoryHoldTimerHandle;
	EWorldFactoryUpgradeType CurrentFactoryHoldType = EWorldFactoryUpgradeType::None;
	float FactoryHoldInitialDelay = 0.3f;
	float FactoryHoldRepeatInterval = 0.1f;

	// 국가가 지원하는 산업군 필터 (DT 기반, fallback all true)
	bool IsCompanyTypeSupportedByCountry(ECompanyType Company) const;

	// ── 클릭 / 탭 핸들러 ──
	void HandleCreateClicked();

	UFUNCTION()
	void HandleBackgroundClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void OnMainTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnFactorySubTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnMineSubTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void HandleLineInstantFinish(UWorldFactoryLineWidget* Line);

	UFUNCTION()
	void HandleLineClaim(UWorldFactoryLineWidget* Line);

	// ── Manager 이벤트 핸들러 ──
	UFUNCTION()
	void HandleMgrLineStarted(ECountryType Country, FWorldFactoryLineState LineState);

	UFUNCTION()
	void HandleMgrLineProgressed(ECountryType Country, int32 LineId, int64 CurrentQty);

	UFUNCTION()
	void HandleMgrLineCompleted(ECountryType Country, int32 LineId);

	UFUNCTION()
	void HandleMgrLineClaimed(ECountryType Country, int32 LineId, int64 FinalQty);

	UFUNCTION()
	void HandleMgrUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel);

	// ===== 채광 탭 (Mine) — Factory 패턴 미러링 =====

	UPROPERTY()
	TObjectPtr<UMineManager> MineMgr;

	UPROPERTY()
	TArray<TObjectPtr<UMineLineWidget>> SpawnedMineLines;

	// 강화 탭 동적 슬롯 캐시 (UpgradeType -> 런타임 생성 UpgradeSlot)
	UPROPERTY()
	TMap<EMineUpgradeType, class UUpgradeSlot*> MineEnhanceSlots;

	// 채광 탭 빌드/갱신
	void RebuildMineTab();
	void RefreshMineLockState();
	void HandleMineCreateClicked();
	UMineLineWidget* SpawnMineLineWidget(const FMineLineState& State);
	UMineLineWidget* FindMineLineWidgetByResource(EResourceType Resource) const;

	// 채광 강화 탭 — 동적 슬롯 생성/갱신 (Factory 동일 패턴)
	void RebuildMineEnhancementSlots();
	void UpdateMineEnhancementSlot(EMineUpgradeType UpgradeType, class UUpgradeSlot* InSlot);
	void ApplyMineSlotDefinition(class UUpgradeSlot* InSlot, const struct FMineUpgradeDefinition& Def);
	void BindMineSlotButton(class UUpgradeSlot* InSlot, EMineUpgradeType Type);
	void OnMineUpgradeClicked(EMineUpgradeType Type);

	// 홀드 연속 강화 (Factory 와 동일 메커니즘 — 별도 상태 머신 유지)
	void StartMineHold(EMineUpgradeType Type);
	void StopMineHold();
	void OnMineHoldTick();

	FTimerHandle MineHoldTimerHandle;
	EMineUpgradeType CurrentMineHoldType = EMineUpgradeType::None;

	// 채광 라인 위젯 → 매니저 연결
	UFUNCTION()
	void HandleMineLineClaim(UMineLineWidget* Line);

	UFUNCTION()
	void HandleMineLineInstantFinish(UMineLineWidget* Line);

	// MineMgr 델리게이트 핸들러
	UFUNCTION()
	void HandleMineMgrLineCreated(ECountryType Country, FMineLineState LineState);

	UFUNCTION()
	void HandleMineMgrLineStorageChanged(ECountryType Country, EResourceType Resource, int64 NewQty);

	UFUNCTION()
	void HandleMineMgrLineSaturated(ECountryType Country, EResourceType Resource, bool bSaturated);

	UFUNCTION()
	void HandleMineMgrResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty);

	UFUNCTION()
	void HandleMineMgrLineRemoved(ECountryType Country, EResourceType Resource);

	UFUNCTION()
	void HandleMineMgrUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 NewLevel);

};
