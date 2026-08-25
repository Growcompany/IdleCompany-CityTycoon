// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Element/Trade/WorldFactoryLineWidget.h"
#include "UI/Element/Trade/MineLineWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Trade/CountrySellRowWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Table/CompanyInfoTable.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Data/MineUpgradeData.h"
#include "UI/Panel/ProductionStartPopupWidget.h"
#include "UI/Panel/MinePickerWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CountryInfoTable.h"
#include "Enum/WidgetType.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"

void UCountryDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
		OrderMgr = GI->GetSubsystem<UProductionOrderManager>();
		MineMgr = GI->GetSubsystem<UMineManager>();
	}

	// 디자이너 프리뷰 라인들 제거
	if (FactoryLineScrollBox)
	{
		FactoryLineScrollBox->ClearChildren();
	}
	if (MineLineScrollBox)
	{
		MineLineScrollBox->ClearChildren();
	}

	if (CreateButton)
	{
		CreateButton->OnClicked().AddUObject(this, &UCountryDetailWidget::HandleCreateClicked);
	}
	if (MineCreateButton)
	{
		MineCreateButton->OnClicked().AddUObject(this, &UCountryDetailWidget::HandleMineCreateClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UCountryDetailWidget::HandleBackgroundClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UCountryDetailWidget::HandleCloseButtonClicked);
	}

	InitializeMainTabs();
	InitializeFactorySubTabs();
	InitializeMineSubTabs();
	BindManagerEvents();

	// Switcher 내부 콘텐츠가 탭 버튼 히트테스트를 차단하지 않도록
	if (MainContentSwitcher)    MainContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
	if (FactoryContentSwitcher) FactoryContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
	if (MineContentSwitcher)    MineContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);

	// 초기 국가 기준 UI 구성
	ApplyCountryBasics();
	ApplyTabVisibility();
	RebuildAllTabContent();
}

void UCountryDetailWidget::NativeDestruct()
{
	if (CreateButton)
	{
		CreateButton->OnClicked().RemoveAll(this);
	}
	if (MineCreateButton)
	{
		MineCreateButton->OnClicked().RemoveAll(this);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UCountryDetailWidget::HandleBackgroundClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UCountryDetailWidget::HandleCloseButtonClicked);
	}
	if (MainTabGroup)
	{
		MainTabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (FactorySubTabGroup)
	{
		FactorySubTabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (MineSubTabGroup)
	{
		MineSubTabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	UnbindManagerEvents();

	for (UWorldFactoryLineWidget* Line : SpawnedFactoryLines)
	{
		if (Line)
		{
			Line->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineInstantFinish);
			Line->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineClaim);
		}
	}

	for (UMineLineWidget* MineLine : SpawnedMineLines)
	{
		if (MineLine)
		{
			MineLine->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineClaim);
			MineLine->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineInstantFinish);
		}
	}

	// 강화 홀드 타이머 정리 (위젯 닫힐 때 dangling 타이머 방지)
	StopFactoryHold();
	StopMineHold();

	// 동적 생성된 UpgradeSlot 버튼 콜백 명시 cleanup (BuildingManagePanelWidget 동일 패턴)
	// AddWeakLambda 자체는 weak this 라 안전하지만, RemoveAll(this) 로 일관성 + 만약 강참조 콜백 추가 시에도 안전
	for (const TPair<EWorldFactoryUpgradeType, UUpgradeSlot*>& Pair : FactoryEnhanceSlots)
	{
		if (UUpgradeSlot* SlotItem = Pair.Value)
		{
			if (UCostActionButtonWidget* Btn = SlotItem->GetUpgradeButton())
			{
				Btn->OnClicked().RemoveAll(this);
				Btn->OnPressed().RemoveAll(this);
				Btn->OnReleased().RemoveAll(this);
			}
		}
	}

	for (const TPair<EMineUpgradeType, UUpgradeSlot*>& Pair : MineEnhanceSlots)
	{
		if (UUpgradeSlot* SlotItem = Pair.Value)
		{
			if (UCostActionButtonWidget* Btn = SlotItem->GetUpgradeButton())
			{
				Btn->OnClicked().RemoveAll(this);
				Btn->OnPressed().RemoveAll(this);
				Btn->OnReleased().RemoveAll(this);
			}
		}
	}

	Super::NativeDestruct();
}

void UCountryDetailWidget::InitializeMainTabs()
{
	if (!InfoTab && !FactoryTab && !MineTab) return;

	MainTabGroup = NewObject<UCommonButtonGroupBase>(this);
	MainTabGroup->SetSelectionRequired(true);

	// 등록 순서 = 탭 인덱스. UIE_TabButton 합성 래퍼라 GetButton() 으로 내부 핸들 꺼냄.
	auto AddTab = [this](UTabButtonWidget* Tab)
	{
		if (!Tab) return;
		if (UCommonButtonBase* Btn = Tab->GetButton())
		{
			MainTabGroup->AddWidget(Btn);
			Btn->SetIsSelectable(true);
		}
	};
	AddTab(InfoTab);
	AddTab(FactoryTab);
	AddTab(MineTab);

	MainTabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCountryDetailWidget::OnMainTabSelectionChanged);

	CurrentTabIndex = TAB_INDEX_INFO;
	MainTabGroup->SelectButtonAtIndex(TAB_INDEX_INFO);
	if (MainContentSwitcher)
	{
		MainContentSwitcher->SetActiveWidgetIndex(TAB_INDEX_INFO);
	}
}

void UCountryDetailWidget::InitializeFactorySubTabs()
{
	if (!FactoryProduceTab || !FactoryEnhanceTab) return;

	FactorySubTabGroup = NewObject<UCommonButtonGroupBase>(this);
	FactorySubTabGroup->SetSelectionRequired(true);

	// UButtonWidget 은 UCommonButtonBase 파생이라 래퍼 경유 없이 그룹에 직접 등록한다.
	auto AddSubTab = [this](UButtonWidget* Tab)
	{
		if (!Tab) return;
		FactorySubTabGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
	};
	AddSubTab(FactoryProduceTab);
	AddSubTab(FactoryEnhanceTab);

	FactorySubTabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCountryDetailWidget::OnFactorySubTabSelectionChanged);

	FactorySubTabGroup->SelectButtonAtIndex(SUBTAB_INDEX_PRODUCE);
	if (FactoryContentSwitcher)
	{
		FactoryContentSwitcher->SetActiveWidgetIndex(SUBTAB_INDEX_PRODUCE);
	}
}

void UCountryDetailWidget::InitializeMineSubTabs()
{
	if (!MineOperateTab || !MineEnhanceTab) return;

	MineSubTabGroup = NewObject<UCommonButtonGroupBase>(this);
	MineSubTabGroup->SetSelectionRequired(true);

	auto AddMineSubTab = [this](UButtonWidget* Tab)
	{
		if (!Tab) return;
		MineSubTabGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
	};
	AddMineSubTab(MineOperateTab);
	AddMineSubTab(MineEnhanceTab);

	MineSubTabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCountryDetailWidget::OnMineSubTabSelectionChanged);

	MineSubTabGroup->SelectButtonAtIndex(SUBTAB_INDEX_PRODUCE);
	if (MineContentSwitcher)
	{
		MineContentSwitcher->SetActiveWidgetIndex(SUBTAB_INDEX_PRODUCE);
	}
}

void UCountryDetailWidget::BindManagerEvents()
{
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineStarted);
		FactoryMgr->OnLineProgressed.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineProgressed);
		FactoryMgr->OnLineCompleted.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineCompleted);
		FactoryMgr->OnLineClaimed.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineClaimed);
		FactoryMgr->OnUpgradeChanged.AddDynamic(this, &UCountryDetailWidget::HandleMgrUpgradeChanged);
	}
	if (MineMgr)
	{
		MineMgr->OnLineCreated.AddDynamic(this, &UCountryDetailWidget::HandleMineMgrLineCreated);
		MineMgr->OnLineStorageChanged.AddDynamic(this, &UCountryDetailWidget::HandleMineMgrLineStorageChanged);
		MineMgr->OnLineSaturated.AddDynamic(this, &UCountryDetailWidget::HandleMineMgrLineSaturated);
		MineMgr->OnResourceClaimed.AddDynamic(this, &UCountryDetailWidget::HandleMineMgrResourceClaimed);
		MineMgr->OnLineRemoved.AddDynamic(this, &UCountryDetailWidget::HandleMineMgrLineRemoved);
		MineMgr->OnUpgradeChanged.AddDynamic(this, &UCountryDetailWidget::HandleMineMgrUpgradeChanged);
	}
	// 주문 관련 델리게이트는 정보 탭에서 안 씀 — 추후 공장 탭이 필요해지면 여기서 바인딩
}

void UCountryDetailWidget::UnbindManagerEvents()
{
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineStarted);
		FactoryMgr->OnLineProgressed.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineProgressed);
		FactoryMgr->OnLineCompleted.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineCompleted);
		FactoryMgr->OnLineClaimed.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineClaimed);
		FactoryMgr->OnUpgradeChanged.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrUpgradeChanged);
	}
	if (MineMgr)
	{
		MineMgr->OnLineCreated.RemoveDynamic(this, &UCountryDetailWidget::HandleMineMgrLineCreated);
		MineMgr->OnLineStorageChanged.RemoveDynamic(this, &UCountryDetailWidget::HandleMineMgrLineStorageChanged);
		MineMgr->OnLineSaturated.RemoveDynamic(this, &UCountryDetailWidget::HandleMineMgrLineSaturated);
		MineMgr->OnResourceClaimed.RemoveDynamic(this, &UCountryDetailWidget::HandleMineMgrResourceClaimed);
		MineMgr->OnLineRemoved.RemoveDynamic(this, &UCountryDetailWidget::HandleMineMgrLineRemoved);
		MineMgr->OnUpgradeChanged.RemoveDynamic(this, &UCountryDetailWidget::HandleMineMgrUpgradeChanged);
	}
	// OrderMgr 델리게이트 미사용
}

void UCountryDetailWidget::SetCurrentCountry(ECountryType Country)
{
	CurrentCountry = Country;
	ApplyCountryBasics();
	ApplyTabVisibility();

	// 채광 지원 국가일 경우 라인 보장 (DT_CountryInfo.MinableResources 기반 동기화)
	if (bSupportsMine && MineMgr)
	{
		MineMgr->EnsureCountryLines(Country);
	}

	EnsureCurrentTabValid();
	RebuildAllTabContent();
}

namespace
{
	// % 가중치 텍스트 변환 (1.0 → "기본", 1.3 → "+30%", 0.9 → "-10%")
	FString FormatBiasPercent(float Bias)
	{
		const int32 Pct = FMath::RoundToInt((Bias - 1.0f) * 100.0f);
		if (Pct == 0) return TEXT("기본");
		return FString::Printf(TEXT("%s%d%%"), Pct > 0 ? TEXT("+") : TEXT(""), Pct);
	}
}

void UCountryDetailWidget::ApplyCountryBasics()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		bSupportsFactory = false;
		bSupportsMine = false;
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	bool bSuccess = false;
	FCountryInfoTable Info = TableMgr->GetCountryInfo(CurrentCountry, bSuccess);

	if (!bSuccess)
	{
		// DT 미등록 국가 — 이름은 비우고 경고로 드러낸다(코드 폴백 금지 원칙).
		// 구 enum 폴백은 UMETA 가 에디터 전용이라 패키징 빌드에서 "Korea"/"Japan" 으로 나왔고, DT 누락도 가렸다.
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] DT_CountryInfo 미등록 국가 (Country=%d)"), static_cast<int32>(CurrentCountry));
		if (CountryNameText)
		{
			CountryNameText->SetText(FText::GetEmpty());
		}
		bSupportsFactory = false;
		bSupportsMine = false;
		return;
	}

	if (CountryFlagImage && !Info.FlagIcon.IsNull())
	{
		if (UTexture2D* Flag = Info.FlagIcon.LoadSynchronous())
		{
			CountryFlagImage->SetBrushFromTexture(Flag);
		}
	}
	if (CountryNameText)
	{
		CountryNameText->SetText(Info.DisplayName);
	}
	// ── 좌측 미니카드 ── 탭과 무관하게 상시 노출되므로 국가 고정 속성(성향/허브/교역거점)은 여기에만 둔다.
	if (HubBadgeBox)
	{
		HubBadgeBox->SetVisibility(Info.bIsTradeHub ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (MoneyBiasText)
	{
		MoneyBiasText->SetText(FText::FromString(FormatBiasPercent(Info.MoneyBias)));
	}
	if (MarketCapBiasText)
	{
		MarketCapBiasText->SetText(FText::FromString(FormatBiasPercent(Info.MarketCapBias)));
	}
	if (PortDescText)
	{
		PortDescText->SetText(Info.PortDescription);
	}

	bSupportsFactory = Info.bSupportsFactory;
	bSupportsMine = Info.bSupportsMine;
}

void UCountryDetailWidget::ApplyTabVisibility()
{
	// 미지원 탭 = 완전히 숨김 (Collapsed - 레이아웃에서도 제외, 나머지 탭이 Fill 로 자리 흡수).
	// SetIsSelectable(false) 도 같이 — CommonButtonGroupBase 가 selection 후보에서 제외.
	// 래퍼 자체에 SetVisibility, 내부 버튼에 SetIsSelectable 분리 적용
	auto Apply = [](UTabButtonWidget* Tab, bool bSupported)
	{
		if (!Tab) return;
		Tab->SetVisibility(bSupported ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (UCommonButtonBase* Btn = Tab->GetButton())
		{
			Btn->SetIsSelectable(bSupported);
		}
	};

	Apply(InfoTab,    true);
	Apply(FactoryTab, bSupportsFactory);
	Apply(MineTab,    bSupportsMine);
}

void UCountryDetailWidget::EnsureCurrentTabValid()
{
	bool bValid = true;
	switch (CurrentTabIndex)
	{
	case TAB_INDEX_FACTORY: bValid = bSupportsFactory; break;
	case TAB_INDEX_MINE:    bValid = bSupportsMine; break;
	default: bValid = true; break;
	}
	if (!bValid)
	{
		SwitchToTab(TAB_INDEX_INFO);
	}
}

void UCountryDetailWidget::RebuildAllTabContent()
{
	RebuildInfoTab();
	RebuildFactoryTab();
	RebuildMineTab();
}

void UCountryDetailWidget::SwitchToTab(int32 TabIndex)
{
	if (MainTabGroup)
	{
		MainTabGroup->SelectButtonAtIndex(TabIndex);
	}
	else if (MainContentSwitcher)
	{
		CurrentTabIndex = TabIndex;
		MainContentSwitcher->SetActiveWidgetIndex(TabIndex);
	}
}

// ── 정보 탭 ──

void UCountryDetailWidget::RebuildInfoTab()
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bSuccess = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(CurrentCountry, bSuccess);
	if (!bSuccess) return;

	// ── 시장 머리말 밴드 ──
	// 성향/능력 칩은 넣지 않는다 — 좌측 미니카드(성향)·탭바(공장/채광)와 같은 값이 두 번 나오면
	// 이 리디자인이 걷어낸 결함(MarketCapBias 중복, 정보량 0 인 "지원/미지원" 행)의 재현이다.
	if (CountryBandSpecialtyText)
	{
		CountryBandSpecialtyText->SetText(Info.SpecialtyName);
	}
	if (CountryFlavorText)
	{
		CountryFlavorText->SetText(Info.FactoryDescription);
	}

	// ── 시장 수요 카드 (DT_CountryDemand 가 정의한 제조 3종) ──
	if (MarketDemandContainer)
	{
		MarketDemandContainer->ClearChildren();

		TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::CountryDemandCard);
		if (CardClass)
		{
			static const ECompanyType DemandIndustries[] = {
				ECompanyType::Semiconductor, ECompanyType::Electronics, ECompanyType::Automobile };

			for (ECompanyType Industry : DemandIndustries)
			{
				UCountrySellRowWidget* Card = CreateWidget<UCountrySellRowWidget>(this, CardClass);
				if (!Card) continue;
				// SetData 가 UCountryMarketManager::OnDemandChanged 를 구독하므로 별도 갱신 배선 불필요
				Card->SetData(CurrentCountry, Industry);

				// 런타임 생성이라 카드 간격은 여기서만 줄 수 있다 (WBP 에 슬롯이 없음)
				if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(MarketDemandContainer->AddChild(Card)))
				{
					CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MarketDemandCardGap));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryDetail] CountryDemandCard 위젯 클래스 없음 - DT_WidgetClass 에 EWidgetType::CountryDemandCard 행 필요"));
		}
	}

	RebuildInfoChips(Info);
}

void UCountryDetailWidget::RebuildInfoChips(const FCountryInfoTable& Info)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	// 값이 비면 라벨까지 함께 숨긴다 — 라벨만 남은 빈 줄 방지
	auto ApplyStrip = [](UWidget* Row, UCommonTextBlock* Text, const TArray<FString>& Names)
	{
		const bool bHas = Names.Num() > 0;
		if (Text && bHas)
		{
			// 가운뎃점 U+00B7 — NEXON cmap 실측 보유 글리프
			Text->SetText(FText::FromString(FString::Join(Names, TEXT(" · "))));
		}
		if (Row)
		{
			Row->SetVisibility(bHas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	TArray<FString> IndustryNames;
	for (ECompanyType Type : Info.SupportedIndustries)
	{
		bool bOk = false;
		const FCompanyInfoTable Company = TableMgr->GetCompanyInfo(Type, bOk);
		if (bOk && !Company.DisplayName.IsEmpty())
		{
			IndustryNames.Add(Company.DisplayName.ToString());
		}
	}
	ApplyStrip(SupportedIndustryRow, SupportedIndustryText, IndustryNames);

	TArray<FString> ResourceNames;
	for (EResourceType Res : Info.MinableResources)
	{
		bool bOk = false;
		const FResourceInfo ResInfo = TableMgr->GetResourceInfo(Res, bOk);
		if (bOk && !ResInfo.DisplayName.IsEmpty())
		{
			ResourceNames.Add(ResInfo.DisplayName.ToString());
		}
	}
	ApplyStrip(MinableResourceRow, MinableResourceText, ResourceNames);
}

bool UCountryDetailWidget::IsCompanyTypeSupportedByCountry(ECompanyType Company) const
{
	if (Company == ECompanyType::None) return false;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return true;

	bool bSuccess = false;
	FCountryInfoTable Info = TableMgr->GetCountryInfo(CurrentCountry, bSuccess);
	if (!bSuccess || Info.SupportedIndustries.Num() == 0)
	{
		return true;  // DT 미지정 시 전체 허용
	}
	return Info.SupportedIndustries.Contains(Company);
}

// ── 공장 탭 ──

void UCountryDetailWidget::RebuildFactoryTab()
{
	// 기존 라인 위젯 정리
	for (UWorldFactoryLineWidget* Line : SpawnedFactoryLines)
	{
		if (Line)
		{
			Line->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineInstantFinish);
			Line->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineClaim);
		}
	}
	SpawnedFactoryLines.Reset();
	if (FactoryLineScrollBox)
	{
		FactoryLineScrollBox->ClearChildren();
	}

	if (FactoryMgr)
	{
		for (const FWorldFactoryLineState& State : FactoryMgr->GetActiveLines(CurrentCountry))
		{
			SpawnFactoryLineWidget(State);
		}
	}

	RefreshFactoryLockState();
	RebuildFactoryEnhancementSlots();
}

void UCountryDetailWidget::RefreshFactoryLockState()
{
	const bool bSlotFull = FactoryMgr
		&& FactoryMgr->GetActiveLineCount(CurrentCountry) >= FactoryMgr->GetMaxLines(CurrentCountry);
	const bool bLocked = bExternalLocked || bSlotFull;

	if (LockedBorder)
	{
		LockedBorder->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LockConditionTextBlock && bLocked)
	{
		const FText& Reason = bExternalLocked ? ExternalLockReason : SlotFullLockReason;
		LockConditionTextBlock->SetText(Reason);
	}
}

UWorldFactoryLineWidget* UCountryDetailWidget::SpawnFactoryLineWidget(const FWorldFactoryLineState& State)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return nullptr;
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return nullptr;

	TSubclassOf<UUserWidget> LineClass = TableMgr->GetWidgetClass(EWidgetType::WorldFactoryLine);
	if (!LineClass) return nullptr;

	UWorldFactoryLineWidget* Line = CreateWidget<UWorldFactoryLineWidget>(this, LineClass);
	if (!Line) return nullptr;

	UTexture2D* IconTex = State.IconPath.LoadSynchronous();
	const FTimespan Remaining = CalcRemainingTime(State);

	// 진단 로그 — 패널 Close/Open 시 매니저 LineElapsedSec 가 살아있는지 확인용.
	UE_LOG(LogTemp, Log, TEXT("[CountryDetail] SpawnFactoryLine LineId=%d Cur=%lld/Tgt=%lld Elapsed=%.2fs Rate=%.3f Completed=%d"),
		State.LineId, State.CurrentQty, State.TargetQty, State.LineElapsedSec, State.RatePerSecond, State.bCompleted ? 1 : 0);

	Line->SetLineId(State.LineId);
	Line->SetCountry(CurrentCountry);  // NativeTick polling sync 용
	Line->SetProductionData(IconTex, State.ProductName, State.RatePerSecond,
		State.CurrentQty, State.TargetQty, Remaining, State.LineElapsedSec);
	if (State.bCompleted)
	{
		Line->SetCompleted(State.CurrentQty);
	}

	Line->OnInstantFinishRequested.AddDynamic(this, &UCountryDetailWidget::HandleLineInstantFinish);
	Line->OnClaimRequested.AddDynamic(this, &UCountryDetailWidget::HandleLineClaim);

	if (FactoryLineScrollBox)
	{
		FactoryLineScrollBox->AddChild(Line);
	}
	SpawnedFactoryLines.Add(Line);
	return Line;
}

UWorldFactoryLineWidget* UCountryDetailWidget::FindFactoryLineWidgetById(int32 LineId) const
{
	for (UWorldFactoryLineWidget* Line : SpawnedFactoryLines)
	{
		if (Line && Line->GetLineId() == LineId) return Line;
	}
	return nullptr;
}

FTimespan UCountryDetailWidget::CalcRemainingTime(const FWorldFactoryLineState& State)
{
	if (State.RatePerSecond <= 0.0f) return FTimespan::Zero();
	const int64 Remaining = State.TargetQty - State.CurrentQty;
	if (Remaining <= 0) return FTimespan::Zero();
	return FTimespan::FromSeconds(static_cast<double>(Remaining) / State.RatePerSecond);
}

// ── 버튼 클릭 ──

void UCountryDetailWidget::HandleCreateClicked()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ProductionStartPopup);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] ProductionStartPopup widget class not found"));
		return;
	}

	// PromptStack push 하면 CountryDetail 이 deactivate 됨 (stack 단일 활성).
	// 직접 viewport 에 띄워 CountryDetail 위에 오버레이 — Mine 의 MinePicker 와 동일 패턴.
	UProductionStartPopupWidget* Popup = CreateWidget<UProductionStartPopupWidget>(GetWorld(), Cls);
	if (Popup)
	{
		Popup->InitializeForCountry(CurrentCountry);
		Popup->AddToViewport(100);  // ZOrder 100 = PromptStack 위
	}
}

void UCountryDetailWidget::HandleBackgroundClicked()
{
	OnCloseRequested.Broadcast();
	DeactivateWidget();
}

void UCountryDetailWidget::HandleCloseButtonClicked()
{
	OnCloseRequested.Broadcast();
	DeactivateWidget();
}

// ── 탭 전환 ──

void UCountryDetailWidget::OnMainTabSelectionChanged(UCommonButtonBase* /*Btn*/, int32 ButtonIndex)
{
	CurrentTabIndex = ButtonIndex;
	if (MainContentSwitcher)
	{
		MainContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}

	// 정보 탭 재빌드 안 함 — 이 탭 콘텐츠는 DT 정적값(특산/플레이버/칩) + 수요 카드뿐이고,
	// 카드는 UCountryMarketManager::OnDemandChanged 를 자체 구독해 스스로 갱신한다.
	// 여기서 rebuild 하면 탭 누를 때마다 구독 위젯 3개를 파괴/재생성해 깜빡임만 생긴다.
	// 국가가 바뀌는 경로는 SetCurrentCountry → RebuildAllTabContent 가 담당.

	OnTabChanged.Broadcast(ButtonIndex);
}

void UCountryDetailWidget::OnFactorySubTabSelectionChanged(UCommonButtonBase* /*Btn*/, int32 ButtonIndex)
{
	if (FactoryContentSwitcher)
	{
		FactoryContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}
}

void UCountryDetailWidget::OnMineSubTabSelectionChanged(UCommonButtonBase* /*Btn*/, int32 ButtonIndex)
{
	if (MineContentSwitcher)
	{
		MineContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}
}

// ── 라인 위젯 이벤트 ──

void UCountryDetailWidget::HandleLineInstantFinish(UWorldFactoryLineWidget* Line)
{
	if (!Line || !FactoryMgr) return;
	FactoryMgr->InstantFinishLine(CurrentCountry, Line->GetLineId());
}

void UCountryDetailWidget::HandleLineClaim(UWorldFactoryLineWidget* Line)
{
	if (!Line || !FactoryMgr) return;
	FactoryMgr->ClaimLine(CurrentCountry, Line->GetLineId());
}

// ── Manager 이벤트 핸들러 ──

void UCountryDetailWidget::HandleMgrLineStarted(ECountryType Country, FWorldFactoryLineState LineState)
{
	if (Country != CurrentCountry) return;
	SpawnFactoryLineWidget(LineState);
	RefreshFactoryLockState();
}

void UCountryDetailWidget::HandleMgrLineProgressed(ECountryType Country, int32 LineId, int64 CurrentQty)
{
	if (Country != CurrentCountry || !FactoryMgr) return;
	UWorldFactoryLineWidget* Line = FindFactoryLineWidgetById(LineId);
	if (!Line) return;
	FWorldFactoryLineState State;
	if (FactoryMgr->GetLineState(Country, LineId, State))
	{
		// State.LineElapsedSec 도 함께 넘겨야 catchup 큰 점프 동기화됨 (위젯 자체 시계 보정)
		Line->UpdateProgress(CurrentQty, CalcRemainingTime(State), State.LineElapsedSec);
	}
}

void UCountryDetailWidget::HandleMgrLineCompleted(ECountryType Country, int32 LineId)
{
	if (Country != CurrentCountry || !FactoryMgr) return;
	UWorldFactoryLineWidget* Line = FindFactoryLineWidgetById(LineId);
	if (!Line) return;
	FWorldFactoryLineState State;
	if (FactoryMgr->GetLineState(Country, LineId, State))
	{
		Line->SetCompleted(State.CurrentQty);
	}
}

void UCountryDetailWidget::HandleMgrLineClaimed(ECountryType Country, int32 LineId, int64 /*FinalQty*/)
{
	if (Country != CurrentCountry) return;
	UWorldFactoryLineWidget* Line = FindFactoryLineWidgetById(LineId);
	if (!Line) return;

	Line->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineInstantFinish);
	Line->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineClaim);

	if (FactoryLineScrollBox)
	{
		FactoryLineScrollBox->RemoveChild(Line);
	}
	SpawnedFactoryLines.Remove(Line);
	Line->RemoveFromParent();  // belt-and-suspenders — Mine 측과 일관 (parent reseat 보장)
	RefreshFactoryLockState();
}

void UCountryDetailWidget::HandleMgrUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 /*NewLevel*/)
{
	if (Country != CurrentCountry) return;
	RefreshFactoryLockState();

	// 정보 탭 재빌드 안 함 — 리디자인 후 그 탭에 강화/라인 의존 표시가 없다(구 "공장 라인 N/M" 행 폐기).

	// 변경된 enum 의 슬롯만 부분 갱신 (전체 rebuild 회피)
	// 변수명 'Slot' 은 UWidget::Slot 멤버를 가리므로 TargetSlot 사용 (BuildingManagePanelWidget 동일 패턴)
	if (UUpgradeSlot* TargetSlot = FactoryEnhanceSlots.FindRef(UpgradeType))
	{
		UpdateFactoryEnhancementSlot(UpgradeType, TargetSlot);
		TargetSlot->PlayUpgradeEffect();
	}
}

// ===== Factory 강화 탭 동적 슬롯 (BuildingManagePanelWidget 패턴 이식) =====

void UCountryDetailWidget::RebuildFactoryEnhancementSlots()
{
	if (!FactoryEnhanceSlotContainer)
	{
		// 컨테이너가 WBP 에 없으면 강화 페이지 자체가 disabled — 조용히 패스
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetailWidget] TableManagerSubsystem 없음 - 강화 슬롯 생성 불가"));
		return;
	}

	TSubclassOf<UUserWidget> SlotClass = TableMgr->GetWidgetClass(EWidgetType::UpgradeSlot);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CountryDetailWidget] UpgradeSlot 위젯 클래스 없음 - DT_Widget 에 EWidgetType::UpgradeSlot 행 필요"));
		return;
	}

	// 기존 슬롯 전체 제거 + 캐시 초기화
	FactoryEnhanceSlotContainer->ClearChildren();
	FactoryEnhanceSlots.Reset();

	// DT 단일 진실 - SortOrder 정렬된 5개 행
	const TArray<FWorldFactoryUpgradeDefinition> Defs = TableMgr->GetAllWorldFactoryUpgradeDefinitions();
	for (const FWorldFactoryUpgradeDefinition& Def : Defs)
	{
		UUpgradeSlot* NewSlot = CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!NewSlot) continue;

		ApplyFactorySlotDefinition(NewSlot, Def);
		BindFactorySlotButton(NewSlot, Def.UpgradeType);

		FactoryEnhanceSlotContainer->AddChild(NewSlot);
		FactoryEnhanceSlots.Add(Def.UpgradeType, NewSlot);

		UpdateFactoryEnhancementSlot(Def.UpgradeType, NewSlot);
	}

	UE_LOG(LogTemp, Log, TEXT("[CountryDetailWidget] Factory enhancement slots rebuilt: %d slots (Country=%d)"),
		FactoryEnhanceSlots.Num(), (int32)CurrentCountry);
}

void UCountryDetailWidget::ApplyFactorySlotDefinition(UUpgradeSlot* InSlot, const FWorldFactoryUpgradeDefinition& Def)
{
	if (!InSlot) return;

	InSlot->Description = Def.DisplayName;
	InSlot->SubDescription = Def.SubDescription;
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->CostResourceType = Def.CostResourceType;

	if (!Def.Icon.IsNull())
	{
		InSlot->Icon = Def.Icon;
	}
}

void UCountryDetailWidget::BindFactorySlotButton(UUpgradeSlot* InSlot, EWorldFactoryUpgradeType Type)
{
	if (!InSlot) return;
	UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton();
	if (!Btn) return;

	// 안전하게 기존 바인딩 제거
	Btn->OnClicked().RemoveAll(this);
	Btn->OnPressed().RemoveAll(this);
	Btn->OnReleased().RemoveAll(this);

	if (Type == EWorldFactoryUpgradeType::LineExpansion)
	{
		// 마일스톤형 — 클릭 1회만 (MaxLevel=3, 큰 분기점이라 hold 부적합. BuildingFloor 와 동일 패턴)
		Btn->OnClicked().AddWeakLambda(this, [this, Type]()
		{
			OnFactoryUpgradeClicked(Type);
		});
	}
	else
	{
		// idle clicker — Pressed 시 1회 + 홀드 타이머 시작 / Released 시 타이머 중지
		Btn->OnPressed().AddWeakLambda(this, [this, Type]()
		{
			OnFactoryUpgradeClicked(Type);
			StartFactoryHold(Type);
		});
		Btn->OnReleased().AddUObject(this, &UCountryDetailWidget::StopFactoryHold);
	}
}

void UCountryDetailWidget::UpdateFactoryEnhancementSlot(EWorldFactoryUpgradeType UpgradeType, UUpgradeSlot* InSlot)
{
	if (!InSlot || !FactoryMgr) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FWorldFactoryUpgradeDefinition Def;
	if (!TableMgr || !TableMgr->GetWorldFactoryUpgradeDefinition(UpgradeType, Def)) return;

	const int32 CurLv = FactoryMgr->GetUpgradeLevel(CurrentCountry, UpgradeType);
	const int64 Cost = FactoryMgr->GetUpgradeCost(CurrentCountry, UpgradeType);

	// 표시값 — ValueUnit 이 "%" 면 (PerLevel * Lv * 100), 그 외엔 BaseValue + PerLevel * Lv (절대값)
	const bool bPercent = (Def.ValueUnit == TEXT("%"));
	const float CurDisplay = bPercent
		? (Def.PerLevel * CurLv * 100.0f)
		: (Def.BaseValue + Def.PerLevel * CurLv);
	const float NextDisplay = bPercent
		? (Def.PerLevel * (CurLv + 1) * 100.0f)
		: (Def.BaseValue + Def.PerLevel * (CurLv + 1));

	const bool bMaxed = (Def.MaxLevel > 0 && CurLv >= Def.MaxLevel);
	InSlot->UpdateInfo(CurLv, CurDisplay, NextDisplay, Cost, bMaxed, Def.MaxLevel);
}

void UCountryDetailWidget::OnFactoryUpgradeClicked(EWorldFactoryUpgradeType Type)
{
	if (!FactoryMgr) return;

	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* ResMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!ResMgr || !TableMgr) return;

	FWorldFactoryUpgradeDefinition Def;
	if (!TableMgr->GetWorldFactoryUpgradeDefinition(Type, Def)) return;

	const int64 Cost = FactoryMgr->GetUpgradeCost(CurrentCountry, Type);

	// 자원 부족 — 홀드 중이면 자동 멈춤. MaxLevel 가드는 매니저 측.
	if (!ResMgr->HasResource(Def.CostResourceType, Cost))
	{
		UE_LOG(LogTemp, Log, TEXT("[CountryDetailWidget] Factory upgrade 자원 부족: Country=%d Type=%d Need=%lld"),
			(int32)CurrentCountry, (int32)Type, Cost);
		if (CurrentFactoryHoldType == Type)
		{
			StopFactoryHold();
		}
		NotifyUpgradeBlocked(Def.CostResourceType, Cost, /*bMaxLevel=*/false);
		return;
	}

	const bool bOk = FactoryMgr->UpgradeLevelUp(CurrentCountry, Type);
	if (!bOk)
	{
		UE_LOG(LogTemp, Log, TEXT("[CountryDetailWidget] Upgrade rejected: Country=%d Type=%d (MaxLevel?)"),
			(int32)CurrentCountry, (int32)Type);

		if (CurrentFactoryHoldType == Type)
		{
			StopFactoryHold();
		}
		NotifyUpgradeBlocked(Def.CostResourceType, Cost, /*bMaxLevel=*/true);
		return;
	}

	// 홀드(10Hz) 경로 — 즉시 저장 대신 지연 저장 (즉시 저장=틱마다 전체 세이브 히칭)
	ResMgr->SpendResource(Def.CostResourceType, Cost, /*bShouldSave=*/false);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->RequestDeferredSave();
	}
}

// ===== 홀드 연속 강화 (BuildingManagePanelWidget 동일 패턴) =====

void UCountryDetailWidget::StartFactoryHold(EWorldFactoryUpgradeType Type)
{
	CurrentFactoryHoldType = Type;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FactoryHoldTimerHandle,
			this,
			&UCountryDetailWidget::OnFactoryHoldTick,
			FactoryHoldRepeatInterval,
			true,                       // bLoop
			FactoryHoldInitialDelay     // 첫 발동까지 대기 (싱글 클릭과 구분)
		);
	}
}

void UCountryDetailWidget::StopFactoryHold()
{
	CurrentFactoryHoldType = EWorldFactoryUpgradeType::None;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FactoryHoldTimerHandle);
	}
}

void UCountryDetailWidget::OnFactoryHoldTick()
{
	if (!FactoryMgr || CurrentFactoryHoldType == EWorldFactoryUpgradeType::None)
	{
		StopFactoryHold();
		return;
	}
	OnFactoryUpgradeClicked(CurrentFactoryHoldType);
}

// ===== 채광 탭 (Mine) =====

void UCountryDetailWidget::RebuildMineTab()
{
	// 기존 라인 위젯 정리
	for (UMineLineWidget* Line : SpawnedMineLines)
	{
		if (Line)
		{
			Line->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineClaim);
			Line->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineInstantFinish);
		}
	}
	SpawnedMineLines.Reset();
	if (MineLineScrollBox)
	{
		MineLineScrollBox->ClearChildren();
	}

	if (MineMgr && bSupportsMine)
	{
		for (const FMineLineState& State : MineMgr->GetMineLines(CurrentCountry))
		{
			SpawnMineLineWidget(State);
		}
	}

	RefreshMineLockState();
	RebuildMineEnhancementSlots();
}

void UCountryDetailWidget::RefreshMineLockState()
{
	// 옵션 B 슬롯 모델: 활성 라인 = 최대 슬롯 도달 시 잠금 표시 ("슬롯 가득").
	const bool bSlotFull = MineMgr
		&& MineMgr->GetActiveLineCount(CurrentCountry) >= MineMgr->GetMaxLines(CurrentCountry);
	const bool bLocked = bExternalLocked || bSlotFull;

	if (MineLockedBorder)
	{
		MineLockedBorder->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (MineLockConditionText && bLocked)
	{
		const FText& Reason = bExternalLocked ? ExternalLockReason : SlotFullLockReason;
		MineLockConditionText->SetText(Reason);
	}
}

UMineLineWidget* UCountryDetailWidget::SpawnMineLineWidget(const FMineLineState& State)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return nullptr;
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr || !MineMgr) return nullptr;

	TSubclassOf<UUserWidget> LineClass = TableMgr->GetWidgetClass(EWidgetType::MineLine);
	if (!LineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] MineLine widget class not registered in DT_Widget"));
		return nullptr;
	}

	UMineLineWidget* Line = CreateWidget<UMineLineWidget>(this, LineClass);
	if (!Line) return nullptr;

	// 라인별 한도 — Override 가 있으면 그 값, 아니면 Country MaxStorage.
	const int64 LineMax = MineMgr->GetEffectiveMaxStorage(CurrentCountry, State.Resource);
	const int32 RateLevel = MineMgr->GetUpgradeLevel(CurrentCountry, EMineUpgradeType::MiningRate);
	const float EffRate = MineMgr->GetEffectiveRatePerMinute(CurrentCountry, State.Resource);

	Line->SetLineState(CurrentCountry, State, LineMax, RateLevel, EffRate);
	Line->OnClaimRequested.AddDynamic(this, &UCountryDetailWidget::HandleMineLineClaim);
	Line->OnInstantFinishRequested.AddDynamic(this, &UCountryDetailWidget::HandleMineLineInstantFinish);

	if (MineLineScrollBox)
	{
		MineLineScrollBox->AddChild(Line);
	}
	SpawnedMineLines.Add(Line);
	return Line;
}

UMineLineWidget* UCountryDetailWidget::FindMineLineWidgetByResource(EResourceType Resource) const
{
	for (UMineLineWidget* Line : SpawnedMineLines)
	{
		if (Line && Line->GetResource() == Resource) return Line;
	}
	return nullptr;
}

// ===== Mine 강화 탭 동적 슬롯 (Factory 동일 패턴) =====

void UCountryDetailWidget::RebuildMineEnhancementSlots()
{
	if (!MineUpgradeSlotScrollBox) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> SlotClass = TableMgr->GetWidgetClass(EWidgetType::UpgradeSlot);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CountryDetailWidget] UpgradeSlot 위젯 클래스 없음 - 채광 강화 슬롯 생성 불가"));
		return;
	}

	MineUpgradeSlotScrollBox->ClearChildren();
	MineEnhanceSlots.Reset();

	const TArray<FMineUpgradeDefinition> Defs = TableMgr->GetAllMineUpgradeDefinitions();
	for (const FMineUpgradeDefinition& Def : Defs)
	{
		UUpgradeSlot* NewSlot = CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!NewSlot) continue;

		ApplyMineSlotDefinition(NewSlot, Def);
		BindMineSlotButton(NewSlot, Def.UpgradeType);

		MineUpgradeSlotScrollBox->AddChild(NewSlot);
		MineEnhanceSlots.Add(Def.UpgradeType, NewSlot);

		UpdateMineEnhancementSlot(Def.UpgradeType, NewSlot);
	}

	UE_LOG(LogTemp, Log, TEXT("[CountryDetailWidget] Mine enhancement slots rebuilt: %d slots (Country=%d)"),
		MineEnhanceSlots.Num(), (int32)CurrentCountry);
}

void UCountryDetailWidget::ApplyMineSlotDefinition(UUpgradeSlot* InSlot, const FMineUpgradeDefinition& Def)
{
	if (!InSlot) return;

	InSlot->Description = Def.DisplayName;
	InSlot->SubDescription = Def.SubDescription;
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->CostResourceType = Def.CostResourceType;

	if (!Def.Icon.IsNull())
	{
		InSlot->Icon = Def.Icon;
	}
}

void UCountryDetailWidget::BindMineSlotButton(UUpgradeSlot* InSlot, EMineUpgradeType Type)
{
	if (!InSlot) return;
	UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton();
	if (!Btn) return;

	Btn->OnClicked().RemoveAll(this);
	Btn->OnPressed().RemoveAll(this);
	Btn->OnReleased().RemoveAll(this);

	// 채광 강화 2종 모두 idle clicker 형 — 홀드 연속 강화
	Btn->OnPressed().AddWeakLambda(this, [this, Type]()
	{
		OnMineUpgradeClicked(Type);
		StartMineHold(Type);
	});
	Btn->OnReleased().AddUObject(this, &UCountryDetailWidget::StopMineHold);
}

void UCountryDetailWidget::UpdateMineEnhancementSlot(EMineUpgradeType UpgradeType, UUpgradeSlot* InSlot)
{
	if (!InSlot || !MineMgr) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FMineUpgradeDefinition Def;
	if (!TableMgr || !TableMgr->GetMineUpgradeDefinition(UpgradeType, Def)) return;

	const int32 CurLv = MineMgr->GetUpgradeLevel(CurrentCountry, UpgradeType);
	const int64 Cost = MineMgr->GetUpgradeCost(CurrentCountry, UpgradeType);

	// MiningRate (%)는 PerLevel * Lv * 100, MiningStorage (개)는 BaseValue + PerLevel * Lv 절대값
	const bool bPercent = (Def.ValueUnit == TEXT("%"));
	const float CurDisplay = bPercent
		? (Def.PerLevel * CurLv * 100.0f)
		: (Def.BaseValue + Def.PerLevel * CurLv);
	const float NextDisplay = bPercent
		? (Def.PerLevel * (CurLv + 1) * 100.0f)
		: (Def.BaseValue + Def.PerLevel * (CurLv + 1));

	const bool bMaxed = (Def.MaxLevel > 0 && CurLv >= Def.MaxLevel);
	InSlot->UpdateInfo(CurLv, CurDisplay, NextDisplay, Cost, bMaxed, Def.MaxLevel);
}

void UCountryDetailWidget::OnMineUpgradeClicked(EMineUpgradeType Type)
{
	if (!MineMgr) return;

	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* ResMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!ResMgr || !TableMgr) return;

	FMineUpgradeDefinition Def;
	if (!TableMgr->GetMineUpgradeDefinition(Type, Def)) return;

	const int64 Cost = MineMgr->GetUpgradeCost(CurrentCountry, Type);

	if (!ResMgr->HasResource(Def.CostResourceType, Cost))
	{
		UE_LOG(LogTemp, Log, TEXT("[CountryDetailWidget] Mine upgrade 자원 부족: Country=%d Type=%d Need=%lld"),
			(int32)CurrentCountry, (int32)Type, Cost);
		if (CurrentMineHoldType == Type)
		{
			StopMineHold();
		}
		NotifyUpgradeBlocked(Def.CostResourceType, Cost, /*bMaxLevel=*/false);
		return;
	}

	const bool bOk = MineMgr->UpgradeLevelUp(CurrentCountry, Type);
	if (!bOk)
	{
		UE_LOG(LogTemp, Log, TEXT("[CountryDetailWidget] Mine upgrade rejected: Country=%d Type=%d (MaxLevel?)"),
			(int32)CurrentCountry, (int32)Type);

		if (CurrentMineHoldType == Type)
		{
			StopMineHold();
		}
		NotifyUpgradeBlocked(Def.CostResourceType, Cost, /*bMaxLevel=*/true);
		return;
	}

	// 홀드(10Hz) 경로 — 즉시 저장 대신 지연 저장 (즉시 저장=틱마다 전체 세이브 히칭)
	ResMgr->SpendResource(Def.CostResourceType, Cost, /*bShouldSave=*/false);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->RequestDeferredSave();
	}
}

// ===== 채광 홀드 연속 강화 =====

void UCountryDetailWidget::StartMineHold(EMineUpgradeType Type)
{
	CurrentMineHoldType = Type;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MineHoldTimerHandle,
			this,
			&UCountryDetailWidget::OnMineHoldTick,
			FactoryHoldRepeatInterval,  // Factory와 동일 호흡
			true,
			FactoryHoldInitialDelay
		);
	}
}

void UCountryDetailWidget::NotifyUpgradeBlocked(EResourceType CostType, int64 Cost, bool bMaxLevel) const
{
	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!UIMgr)
	{
		return;
	}

	if (bMaxLevel)
	{
		// 거부가 아니라 상태 안내라 Warning
		UIMgr->ShowRejectNotification(
			NSLOCTEXT("CountryDetail", "UpgradeMaxLevel", "이미 최대 레벨입니다"), 0.4f, ENotificationType::Warning);
		return;
	}

	UIMgr->NotifyInsufficientResource(CostType, Cost);
}

void UCountryDetailWidget::StopMineHold()
{
	CurrentMineHoldType = EMineUpgradeType::None;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MineHoldTimerHandle);
	}
}

void UCountryDetailWidget::OnMineHoldTick()
{
	if (!MineMgr || CurrentMineHoldType == EMineUpgradeType::None)
	{
		StopMineHold();
		return;
	}
	OnMineUpgradeClicked(CurrentMineHoldType);
}

// ===== Mine Create 모달 =====

void UCountryDetailWidget::HandleMineCreateClicked()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::MinePicker);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] MinePicker widget class not found"));
		return;
	}

	// PromptStack 에 push 하면 CountryDetail 이 deactivate (한 stack 단일 활성).
	// 직접 viewport 에 추가 → CountryDetail 그대로 유지하고 위에 모달 오버레이.
	UMinePickerWidget* Picker = CreateWidget<UMinePickerWidget>(GetWorld(), Cls);
	if (Picker)
	{
		Picker->InitializeForCountry(CurrentCountry);
		Picker->AddToViewport(100);  // ZOrder 100 = PromptStack(보통 0~10) 위
	}
}

// ===== Mine 라인 위젯 → 매니저 =====

void UCountryDetailWidget::HandleMineLineClaim(UMineLineWidget* Line)
{
	if (!Line || !MineMgr) return;
	MineMgr->ClaimResource(CurrentCountry, Line->GetResource());
}

void UCountryDetailWidget::HandleMineLineInstantFinish(UMineLineWidget* Line)
{
	if (!Line || !MineMgr) return;
	// TODO: 다이아 비용 차감 (ResourceItemManager 통합 시점에 추가).
	// 지금은 비용 무시하고 한도 즉시 채움 (밸런싱 단계).
	MineMgr->InstantFinishLine(CurrentCountry, Line->GetResource());
}

// ===== Mine Manager 이벤트 핸들러 =====

void UCountryDetailWidget::HandleMineMgrLineCreated(ECountryType Country, FMineLineState LineState)
{
	if (Country != CurrentCountry) return;
	SpawnMineLineWidget(LineState);
	RefreshMineLockState();
}

void UCountryDetailWidget::HandleMineMgrLineStorageChanged(ECountryType Country, EResourceType Resource, int64 NewQty)
{
	if (Country != CurrentCountry || !MineMgr) return;
	UMineLineWidget* Line = FindMineLineWidgetByResource(Resource);
	if (!Line) return;

	// 라인별 effective max — Override 가 있으면 그 값 우선.
	const int64 LineMax = MineMgr->GetEffectiveMaxStorage(Country, Resource);
	FMineLineState State;
	const bool bSat = MineMgr->GetLineState(Country, Resource, State) ? State.bSaturated : false;
	Line->UpdateStorage(NewQty, LineMax, bSat);
}

void UCountryDetailWidget::HandleMineMgrLineSaturated(ECountryType Country, EResourceType Resource, bool bSaturated)
{
	if (Country != CurrentCountry || !MineMgr) return;
	UMineLineWidget* Line = FindMineLineWidgetByResource(Resource);
	if (!Line) return;

	FMineLineState State;
	const int64 CurQty = MineMgr->GetLineState(Country, Resource, State) ? State.CurrentQty : 0;
	Line->UpdateStorage(CurQty, MineMgr->GetEffectiveMaxStorage(Country, Resource), bSaturated);
}

void UCountryDetailWidget::HandleMineMgrLineRemoved(ECountryType Country, EResourceType Resource)
{
	if (Country != CurrentCountry) return;

	UMineLineWidget* Target = FindMineLineWidgetByResource(Resource);
	if (!Target) return;

	// 델리게이트 해제 + 컬렉션/스크롤박스 제거 + 위젯 destroy.
	Target->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineClaim);
	Target->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineInstantFinish);

	SpawnedMineLines.RemoveSingle(Target);
	if (MineLineScrollBox)
	{
		MineLineScrollBox->RemoveChild(Target);
	}
	Target->RemoveFromParent();

	// 슬롯 잠금 상태 갱신 (라인 1개 빠졌으니 추가 가능 여부 변경)
	RefreshMineLockState();
}

void UCountryDetailWidget::HandleMineMgrResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty)
{
	if (Country != CurrentCountry || !MineMgr) return;
	// LineStorageChanged 가 0 으로 후속 호출되어 UI 갱신은 거기서 처리. 여기서는 향후 토스트/이펙트 훅용.
}

void UCountryDetailWidget::HandleMineMgrUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 /*NewLevel*/)
{
	if (Country != CurrentCountry) return;
	RefreshMineLockState();

	// 정보 탭 재빌드 안 함 (HandleMgrUpgradeChanged 와 동일 사유 — 강화 의존 표시가 그 탭에 없다).

	// 강화 슬롯 부분 갱신 + 강화 이펙트
	if (UUpgradeSlot* TargetSlot = MineEnhanceSlots.FindRef(UpgradeType))
	{
		UpdateMineEnhancementSlot(UpgradeType, TargetSlot);
		TargetSlot->PlayUpgradeEffect();
	}

	// 채광 속도/한도 변경은 모든 라인의 표시값에 영향 — 라인 위젯 갱신
	if (!MineMgr) return;
	const int32 NewRateLv = MineMgr->GetUpgradeLevel(Country, EMineUpgradeType::MiningRate);

	for (UMineLineWidget* Line : SpawnedMineLines)
	{
		if (!Line) continue;
		const EResourceType Res = Line->GetResource();
		const float EffRate = MineMgr->GetEffectiveRatePerMinute(Country, Res);
		// 라인별 effective max — Override 라인은 강화 ceiling 까지만 영향 받음.
		const int64 LineMax = MineMgr->GetEffectiveMaxStorage(Country, Res);

		FMineLineState State;
		const bool bHasState = MineMgr->GetLineState(Country, Res, State);
		const int64 CurQty = bHasState ? State.CurrentQty : 0;
		const bool bSat = bHasState ? State.bSaturated : false;

		Line->UpdateStorage(CurQty, LineMax, bSat);
		Line->UpdateRate(NewRateLv, EffRate);
	}
}

