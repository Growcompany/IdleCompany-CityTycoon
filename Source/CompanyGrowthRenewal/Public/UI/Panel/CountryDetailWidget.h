// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/CountryType.h"
#include "Enum/CompanyType.h"
#include "Enum/WidgetType.h"
#include "Data/ProductionOrderData.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/MineManager.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "CountryDetailWidget.generated.h"

class UButton;
class UPanelWidget;
class UWidgetSwitcher;
class UUniformGridPanel;
class UScrollBox;
class UCommonTextBlock;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UTabButtonWidget;
class UPanelHeaderWidget;
class UWorldFactoryLineWidget;
class UMineLineWidget;
class UDrawerHeroRowWidget;
class UDrawerInfoRowWidget;
class UDrawerPageToggleWidget;
class UProductionStartPopupWidget;
class UMinePickerWidget;
class UProductionOrderManager;
class UTradeOrderManager;
class UWorldMapManager;
class UResourceItemManager;
class UWidget;
class UConfirmCancelWidget;
class UInstantFinishConfirmWidget;
class UTexture2D;
struct FCountryInfoTable;

/**
 * UCountryDetailWidget
 * WorldMap에서 국가 클릭 시 여는 오른쪽 서랍.
 *
 * 공장·광산 = 헤더 아래 생산/강화 토글 + 판 안 페이지 스위처(0 = 요약 줄 고정 + 라인 목록 스크롤, 1 = 강화 목록).
 * 무역항 = 토글 없이 요약 줄 + 주문 줄 + 완제품 4열 칸 격자.
 * 시설 탭(공장/광산(유전)/항구)은 시설이 2개 이상일 때만 보인다. 시설 = DT_CountryInfo(bSupportsFactory/MinableResources/bIsTradeHub).
 * 탭 번호는 시설에 고정(TAB_INDEX_*) — OnTabChanged/GetCurrentTabIndex 로 월드맵이 무대 모듈을 고른다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UCountryDetailWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCountryDetailCloseRequested);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountryDetailTabChanged, int32, TabIndex);

	UPROPERTY(BlueprintAssignable, Category = "CountryDetail|Events")
	FOnCountryDetailCloseRequested OnCloseRequested;

	// 인자 = 시설 탭 번호(TAB_INDEX_FACTORY/MINE/PORT). 탭 줄이 숨은 나라에서도 시설이 바뀌면 방송한다.
	UPROPERTY(BlueprintAssignable, Category = "CountryDetail|Events")
	FOnCountryDetailTabChanged OnTabChanged;

	// 표시할 국가 지정 → DT 조회 → 시설 탭 구성 + 모든 페이지 갱신
	UFUNCTION(BlueprintCallable, Category = "CountryDetail")
	void SetCurrentCountry(ECountryType Country);

	// 외부 호출자 호환 별칭
	UFUNCTION(BlueprintCallable, Category = "CountryDetail")
	void SetCountry(ECountryType Country) { SetCurrentCountry(Country); }

	UFUNCTION(BlueprintPure, Category = "CountryDetail")
	ECountryType GetCurrentCountry() const { return CurrentCountry; }

	// TAB_INDEX_INFO(0) 또는 그 나라에 없는 시설이면 첫 시설로 간다.
	UFUNCTION(BlueprintCallable, Category = "CountryDetail")
	void SwitchToTab(int32 TabIndex);

	UFUNCTION(BlueprintPure, Category = "CountryDetail")
	int32 GetCurrentTabIndex() const { return CurrentTabIndex; }

	// 지금 보이는 시설. 시설이 하나도 없는 나라면 HasFacility() == false 이고 값은 Factory.
	EDioramaModule GetCurrentFacility() const { return CurrentFacility; }
	bool HasFacility() const { return bHasFacility; }
	bool IsProductionStartPopupOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

	// ── 헤더 / 배경 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn;

	// ── 시설 탭 (UIE_TabButton 합성 래퍼). InfoTab 은 한 장 서랍에서 항상 접힌다 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> TabBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTabButtonWidget> InfoTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTabButtonWidget> FactoryTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTabButtonWidget> MineTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTabButtonWidget> PortTab;

	// 자식 순서 = 정보(0)·공장(1)·광산(2)·항구(3). 페이지가 바인딩돼 있으면 순서 대신 위젯으로 고른다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> MainContentSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> FactoryPlate;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> MinePlate;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> PortPlate;

	// ── 생산/강화 토글(무역항에서는 접힌다) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UDrawerPageToggleWidget> PageToggle;

	// 0 = 생산, 1 = 강화
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> FactoryPageSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> MinePageSwitcher;

	// ── 공장 페이지 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UDrawerHeroRowWidget> FactoryHero;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> FactoryLineList;

	// 라인 목록 스크롤(요약 줄 아래). 막대가 보이면 판 안쪽 여백으로 밀어 줄 폭을 지킨다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> FactoryLineScroll;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> FactoryEnhanceList;

	// ── 광산 페이지 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UDrawerHeroRowWidget> MineHero;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> MineLineList;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> MineLineScroll;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> MineEnhanceList;

	// ── 항구 페이지 ──
	// 한 장 스크롤(요약 줄 포함). 라인 스크롤과 같은 규칙으로 막대가 보여도 줄 폭을 지킨다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> PortPageScroll;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UDrawerHeroRowWidget> PortHero;

	// 「주문」 소제목 ― 받아 둔 주문이 0 이면 접는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> PortOrderHeader;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> PortOrderList;

	// 「보관 중인 완제품」 칸 격자(PortGoodsColumns 열)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> PortGoodsList;

	// 창고가 비었을 때만 보이는 안내 한 줄(문구는 WBP)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> PortEmptyText;

	// 목록 행은 런타임 생성이라 WBP 에 슬롯이 없다 — 행 간격은 여기서만 조절 가능(spec 줄 사이 16)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CountryDetail")
	float ListRowGap = 16.f;

	// 요약 줄 시설 칸 색 — 공장은 산업 대표색(DT_CompanyInfo.AccentColor)을 쓰므로 광산·무역항만 둔다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CountryDetail|Hero")
	FLinearColor MineFacilityTileColor = FLinearColor(FColor(0x8F, 0x60, 0x40));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CountryDetail|Hero")
	FLinearColor PortFacilityTileColor = FLinearColor(FColor(0x17, 0x84, 0x78));

	// 무역항 「보관 중인 완제품」 격자 열 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CountryDetail")
	int32 PortGoodsColumns = 4;

public:
	// 외부(CountryRouterPanelWidget·월드맵)에서 매직넘버 없이 참조하기 위한 시설 탭 번호.
	static constexpr int32 TAB_INDEX_INFO = 0;
	static constexpr int32 TAB_INDEX_FACTORY = 1;
	static constexpr int32 TAB_INDEX_MINE = 2;
	static constexpr int32 TAB_INDEX_PORT = 3;

private:
	ECountryType CurrentCountry = ECountryType::Korea;
	int32 CurrentTabIndex = TAB_INDEX_INFO;
	EDioramaModule CurrentFacility = EDioramaModule::Factory;
	bool bHasFacility = false;
	TArray<EDioramaModule> Facilities;
	CountryDrawer::EDrawerPage CurrentPage = CountryDrawer::EDrawerPage::Production;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> MainTabGroup;

	UPROPERTY()
	TObjectPtr<UWorldFactoryManager> FactoryMgr;

	UPROPERTY()
	TObjectPtr<UProductionOrderManager> OrderMgr;

	UPROPERTY()
	TObjectPtr<UResourceItemManager> ResourceMgr;

	UPROPERTY()
	TObjectPtr<UTradeOrderManager> TradeOrderMgr;

	UPROPERTY()
	TObjectPtr<UWorldMapManager> WarehouseMgr;

	UPROPERTY()
	TArray<TObjectPtr<UWorldFactoryLineWidget>> SpawnedFactoryLines;

	UPROPERTY()
	TArray<TObjectPtr<UDrawerInfoRowWidget>> SpawnedEmptyLineRows;

	UPROPERTY()
	TObjectPtr<UProductionStartPopupWidget> ActiveProductionPopup;

	UPROPERTY()
	TObjectPtr<UMinePickerWidget> ActiveMinePicker;

	// DT 행이 없을 때만 쓰는 옛 범용 확인 창
	TWeakObjectPtr<UConfirmCancelWidget> InstantFinishConfirm;
	TWeakObjectPtr<UInstantFinishConfirmWidget> InstantFinishWindow;
	// 확인 창이 가리키는 라인 ― 그 라인이 저절로 끝나면 창을 닫는다
	int32 InstantFinishLineId = INDEX_NONE;
	EResourceType InstantFinishResource = EResourceType::None;
	bool bWarnedInstantFinishFallback = false;
	bool IsInstantFinishConfirmOpen() const;
	void CloseInstantFinishConfirm();
	// 창을 건드리지 않고 기억만 지운다(창이 스스로 닫히는 결과 이벤트 안에서 부른다)
	void ForgetInstantFinishConfirm();
	void OpenInstantFinishConfirm(UTexture2D* Icon, const FText& Name, int64 Current, int64 Target, double Unit01,
		const FTimespan& Remaining, int64 Cost, bool bMine, TFunction<void()> OnConfirm);

	void CloseProductionPopup();
	void CloseMinePicker();

	UPROPERTY()
	TArray<TObjectPtr<UDrawerInfoRowWidget>> SpawnedEmptyMineRows;

	UPROPERTY()
	TArray<TObjectPtr<UDrawerInfoRowWidget>> SpawnedPortRows;

	// 강화 행 캐시 (UpgradeType -> 런타임 생성된 UpgradeSlot)
	UPROPERTY()
	TMap<EWorldFactoryUpgradeType, class UUpgradeSlot*> FactoryEnhanceSlots;

	// DT 시설 캐시
	bool bSupportsFactory = false;
	bool bSupportsMine = false;
	bool bSupportsPort = false;
	TSet<EWidgetType> WarnedRowFallbacks;

	// Lifecycle
	void InitializeMainTabs();
	void BindManagerEvents();
	void UnbindManagerEvents();

	// Country / 시설 탭
	void ApplyCountryBasics();
	bool GetCountryRow(FCountryInfoTable& OutRow) const;
	void ApplyFacilityTabs();
	UTabButtonWidget* TabButtonFor(EDioramaModule Facility) const;
	UWidget* PageFor(EDioramaModule Facility) const;
	void SelectFacility(EDioramaModule Facility);
	void ApplyFacilitySelection(EDioramaModule Facility);
	void RebuildAllPages();

	// 생산/강화 페이지
	void SelectPage(CountryDrawer::EDrawerPage Page, bool bFromUser);
	void UpdatePageToggleVisibility();
	void RefreshProductionDot();
	// 라인이 저절로 끝나거나(공장) 가득 차는(광산) 순간엔 매니저 방송이 없다 ― 그 시각에 요약 줄·빨간 점을 다시 잰다
	void ScheduleLineEdgeRefresh();
	void HandleLineEdge();
	FTimerHandle LineEdgeTimerHandle;
	// 월드 타이머는 앱이 백그라운드인 동안 멈추고 매니저는 벽시계로 간다 ― 돌아오면 바로 다시 잰다
	void HandleAppEnteredForeground();
	// 요약 줄 「모두 받기」 수량과 빨간 점이 같은 값을 쓴다
	void CountFactoryLines(int32& OutRunning, int32& OutReady, int64& OutReadyQty) const;
	int64 PendingMineQuantity() const;
	bool HasClaimableFullMineLine() const;

	// 새 한 줄 행 WBP 가 아직 등록 전이면 옛 카드형으로 그린다(경고 1회).
	TSubclassOf<UUserWidget> ResolveRowClass(EWidgetType Preferred, EWidgetType Fallback, bool bUseFallback);
	UDrawerInfoRowWidget* SpawnInfoRow(UPanelWidget* Container);
	void AddListRow(UPanelWidget* List, UWidget* Row) const;
	// 라인 증설이 최대 미만이면 다음 1줄을 잠긴 채 붙인다.
	void AddLockedLineRow(UPanelWidget* List, int32 CurrentSlots, int32 MaxSlots);
	int32 FactoryMaxSlots() const;
	// 라인 증설 최대와 나라 자원 종류 수 중 작은 값
	int32 MineMaxSlots() const;
	// 완제품 칸 하나. DrawerGoodsTile 행이 없으면 false(경고 1회)
	bool AddPortGoodsTile(int32 Index, class UTexture2D* Icon, const FText& GoodsName, int64 Amount);

	// ── 공장 페이지 ──
	void RebuildFactoryPage();
	void RebuildFactoryLineList();
	void RefreshEmptyFactoryRows();
	void ScheduleEmptyFactoryRowsRefresh();
	void FlushEmptyFactoryRowsRefresh();
	bool ResolveQuickStartOrder(FProductionOrder& OutOrder, int32& OutMaxProducible) const;
	void RefreshFactoryHero();
	void ClearFactoryLineWidgets();
	UWorldFactoryLineWidget* SpawnFactoryLineWidget(const FWorldFactoryLineState& State, int32 Number);
	UWorldFactoryLineWidget* FindFactoryLineWidgetById(int32 LineId) const;
	static FTimespan CalcRemainingTime(const FWorldFactoryLineState& State);
	void HandleFactoryHeroAction();
	void HandleEmptyLineStart(UDrawerInfoRowWidget* Row);

	// Factory 강화 행 — 동적 슬롯 생성/갱신 (BuildingManagePanelWidget 패턴 이식)
	void RebuildFactoryEnhancementSlots();
	void UpdateFactoryEnhancementSlot(EWorldFactoryUpgradeType UpgradeType, class UUpgradeSlot* InSlot);
	void ApplyFactorySlotDefinition(class UUpgradeSlot* InSlot, const struct FWorldFactoryUpgradeDefinition& Def);
	void BindFactorySlotButton(class UUpgradeSlot* InSlot, EWorldFactoryUpgradeType Type);
	void OnFactoryUpgradeClicked(EWorldFactoryUpgradeType Type);

	// 홀드 연속 강화 (LineExpansion 제외 — 마일스톤형이라 1회만)
	void StartFactoryHold(EWorldFactoryUpgradeType Type);
	void StopFactoryHold();
	void OnFactoryHoldTick();

	// 업그레이드 거부 안내 (공장/채광 공용). 홀드는 10Hz 라 알림 쿨다운이 스팸을 막는다.
	void NotifyUpgradeBlocked(EResourceType CostType, int64 Cost, bool bMaxLevel) const;

	FTimerHandle FactoryHoldTimerHandle;
	FTimerHandle EmptyFactoryRowsRefreshTimerHandle;
	EWorldFactoryUpgradeType CurrentFactoryHoldType = EWorldFactoryUpgradeType::None;
	float FactoryHoldInitialDelay = 0.3f;
	float FactoryHoldRepeatInterval = 0.1f;

	// 국가가 지원하는 산업군 필터 (DT 기반, fallback all true)
	bool IsCompanyTypeSupportedByCountry(ECompanyType Company) const;

	// ── 항구 페이지 ──
	void RefreshPortPage();
	void ClearPortRows();
	void HandlePortHeroAction();

	// ── 클릭 / 탭 핸들러 ──
	UFUNCTION()
	void HandleBackgroundClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void OnMainTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void HandleLineInstantFinish(UWorldFactoryLineWidget* Line);

	UFUNCTION()
	void HandleLineClaim(UWorldFactoryLineWidget* Line);

	UFUNCTION()
	void HandleFactoryLineCompleted(UWorldFactoryLineWidget* Line);

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
	void HandleMgrLineRemoved(ECountryType Country, int32 LineId);

	UFUNCTION()
	void HandleMgrUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel);

	UFUNCTION()
	void HandleWarehouseChanged(FIntPoint ProductKey, int64 NewAmount);

	UFUNCTION()
	void HandleProductionOrderCreated(int32 BuildingID, const FProductionOrder& Order);

	UFUNCTION()
	void HandleProductionOrderConsumed(int32 OrderID, int32 ConsumedAmount, int32 RemainingQuantity);

	UFUNCTION()
	void HandleProductionOrderDepleted(int32 OrderID);

	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	UFUNCTION()
	void HandleTradeOrderChanged(int32 OrderId);

	UFUNCTION()
	void HandleTradeOrderPartial(int32 OrderId, int64 RemainingQty);

	UFUNCTION()
	void HandleTradeOrderDismissed(int32 OrderId, bool bWasAccepted);

	UFUNCTION()
	void HandleFactoryLineScrollBar(ESlateVisibility NewVisibility);

	UFUNCTION()
	void HandleMineLineScrollBar(ESlateVisibility NewVisibility);

	UFUNCTION()
	void HandlePortPageScrollBar(ESlateVisibility NewVisibility);

	static void ApplyLineScrollBarInset(UScrollBox* Scroll, ESlateVisibility BarVisibility);

	// 최대(회색 판) 강화 버튼 라벨 = 차콜, 그 외 = 버튼 인스턴스 저작 잉크(spec §2.3 회색 = 못 함)
	void ApplyUpgradeLabelInk(class UUpgradeSlot* InSlot, bool bMaxed) const;
	// 최대 줄은 다음 값이 없다 ― 슬롯은 최대여도 「현재 〉 다음」을 그려서 서랍 변형(UIE_DrawerUpgradeSlot)의 다음 값·화살표를 접는다
	void ApplyUpgradeMaxedValue(class UUpgradeSlot* InSlot, bool bMaxed) const;

	UPROPERTY(EditAnywhere, Category = "CountryDetail")
	FSlateColor MaxedUpgradeLabelInk = FSlateColor(FLinearColor(FColor(0x1B, 0x23, 0x30)));

	// ===== 광산 페이지 (Mine) — Factory 패턴 미러링 =====

	UPROPERTY()
	TObjectPtr<UMineManager> MineMgr;

	UPROPERTY()
	TArray<TObjectPtr<UMineLineWidget>> SpawnedMineLines;

	// 강화 행 캐시 (UpgradeType -> 런타임 생성 UpgradeSlot)
	UPROPERTY()
	TMap<EMineUpgradeType, class UUpgradeSlot*> MineEnhanceSlots;

	void RebuildMinePage();
	void RebuildMineLineList();
	void HandleEmptyMineStart(UDrawerInfoRowWidget* Row);
	void RefreshMineHero();
	void ClearMineLineWidgets();
	UMineLineWidget* SpawnMineLineWidget(const FMineLineState& State);
	UMineLineWidget* FindMineLineWidgetByResource(EResourceType Resource) const;
	void HandleMineHeroAction();

	// 채광 강화 행 — 동적 슬롯 생성/갱신 (Factory 동일 패턴)
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

	UFUNCTION()
	void HandleMineLineQuantityChanged(UMineLineWidget* Line);

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
