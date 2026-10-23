// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "UI/Panel/ProductionStartPopupWidget.h"
#include "UI/Panel/MinePickerWidget.h"
#include "UI/Panel/InstantFinishConfirmWidget.h"
#include "UI/UIBase.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Trade/WorldFactoryLineWidget.h"
#include "UI/Element/Trade/MineLineWidget.h"
#include "UI/Element/Trade/DrawerHeroRowWidget.h"
#include "UI/Element/Trade/DrawerInfoRowWidget.h"
#include "UI/Element/Trade/DrawerPageToggleWidget.h"
#include "UI/Element/Trade/DrawerGoodsTileWidget.h"
#include "UI/UISoundTags.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Table/CompanyInfoTable.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ResourceInfo.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Data/MineUpgradeData.h"
#include "Data/WorldMapTypes.h"
#include "Enum/QualityGrade.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/TradeOrderManager.h"
#include "Manager/TradePort.h"
#include "Manager/WorldMapManager.h"
#include "Manager/OverseasActions.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreDelegates.h"

void UCountryDetailWidget::NativeConstruct()
{
	bIsBackHandler = true;
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
		OrderMgr = GI->GetSubsystem<UProductionOrderManager>();
		ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
		MineMgr = GI->GetSubsystem<UMineManager>();
		TradeOrderMgr = GI->GetSubsystem<UTradeOrderManager>();
		WarehouseMgr = GI->GetSubsystem<UWorldMapManager>();
	}

	// 디자이너 프리뷰 행들 제거
	for (UPanelWidget* List : TArray<UPanelWidget*>{ FactoryLineList.Get(), MineLineList.Get(), PortOrderList.Get(), PortGoodsList.Get() })
	{
		if (List)
		{
			List->ClearChildren();
		}
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UCountryDetailWidget::HandleBackgroundClicked);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UCountryDetailWidget::HandleCloseButtonClicked);
	}
	if (FactoryHero)
	{
		FactoryHero->OnActionClicked().AddUObject(this, &UCountryDetailWidget::HandleFactoryHeroAction);
	}
	if (MineHero)
	{
		MineHero->OnActionClicked().AddUObject(this, &UCountryDetailWidget::HandleMineHeroAction);
	}
	if (PortHero)
	{
		PortHero->OnActionClicked().AddUObject(this, &UCountryDetailWidget::HandlePortHeroAction);
	}
	PageToggle->OnPageChanged().AddUObject(this, &UCountryDetailWidget::SelectPage, /*bFromUser=*/true);
	FactoryLineScroll->OnScrollBarVisibilityChanged.AddDynamic(this, &UCountryDetailWidget::HandleFactoryLineScrollBar);
	MineLineScroll->OnScrollBarVisibilityChanged.AddDynamic(this, &UCountryDetailWidget::HandleMineLineScrollBar);
	PortPageScroll->OnScrollBarVisibilityChanged.AddDynamic(this, &UCountryDetailWidget::HandlePortPageScrollBar);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UCountryDetailWidget::HandleAppEnteredForeground);

	InitializeMainTabs();
	BindManagerEvents();

	// Switcher 내부 콘텐츠가 탭 버튼 히트테스트를 차단하지 않도록
	if (MainContentSwitcher)
	{
		MainContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
	}
	// 애니 전환이면 서랍이 열릴 때마다 접어 둔 정보 페이지(0번)가 비친 뒤 시설 페이지로 넘어간다
	if (UCommonAnimatedSwitcher* AnimatedPages = Cast<UCommonAnimatedSwitcher>(MainContentSwitcher))
	{
		AnimatedPages->SetDisableTransitionAnimation(true);
	}

	// 초기 국가 기준 UI 구성 — 첫 시설 선택도 여기서(탭 그룹 초기 선택은 NativeConstruct 규칙)
	ApplyCountryBasics();
	ApplyFacilityTabs();
	EDioramaModule Initial = EDioramaModule::Factory;
	if (CountryDrawer::FacilityForTab(CurrentTabIndex, Facilities, Initial))
	{
		SelectFacility(Initial);
	}
	UpdatePageToggleVisibility();
	SelectPage(CurrentPage, false);
	RebuildAllPages();
}

void UCountryDetailWidget::NativeDestruct()
{
	CloseInstantFinishConfirm();
	CloseProductionPopup();
	CloseMinePicker();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EmptyFactoryRowsRefreshTimerHandle);
	}
	EmptyFactoryRowsRefreshTimerHandle.Invalidate();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineEdgeTimerHandle);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UCountryDetailWidget::HandleBackgroundClicked);
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UCountryDetailWidget::HandleCloseButtonClicked);
	}
	for (UDrawerHeroRowWidget* Hero : { FactoryHero.Get(), MineHero.Get(), PortHero.Get() })
	{
		if (Hero)
		{
			Hero->OnActionClicked().RemoveAll(this);
		}
	}
	PageToggle->OnPageChanged().RemoveAll(this);
	FactoryLineScroll->OnScrollBarVisibilityChanged.RemoveDynamic(this, &UCountryDetailWidget::HandleFactoryLineScrollBar);
	MineLineScroll->OnScrollBarVisibilityChanged.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineScrollBar);
	PortPageScroll->OnScrollBarVisibilityChanged.RemoveDynamic(this, &UCountryDetailWidget::HandlePortPageScrollBar);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	if (MainTabGroup)
	{
		MainTabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		// 선택 필수인 채로 비우면 엔진이 남은 버튼을 재선택한다 — 재구성 때 새 그룹이 같은 버튼을 받으므로 옛 그룹과 끊는다
		MainTabGroup->SetSelectionRequired(false);
		MainTabGroup->RemoveAll();
		MainTabGroup = nullptr;
	}

	UnbindManagerEvents();

	ClearFactoryLineWidgets();
	ClearMineLineWidgets();
	ClearPortRows();

	// 스택이 인스턴스를 재사용하므로 닫았다 다시 열면 생산부터 — 다음 NativeConstruct 의 SelectPage 가 토글을 맞춘다
	CurrentPage = CountryDrawer::EDrawerPage::Production;

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

void UCountryDetailWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	// 닫혀 있는 동안 끝난 라인·가득 찬 광산은 방송이 없다 ― 열릴 때 요약 줄·빨간 점·경계 타이머를 다시 잡는다
	HandleLineEdge();
}

void UCountryDetailWidget::HandleAppEnteredForeground()
{
	if (IsActivated())
	{
		HandleLineEdge();
	}
}

void UCountryDetailWidget::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineEdgeTimerHandle);
	}
	CloseInstantFinishConfirm();
	CloseProductionPopup();
	CloseMinePicker();
	Super::NativeOnDeactivated();
}

bool UCountryDetailWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this)) return false;
	if (ActiveProductionPopup && ActiveProductionPopup->IsInViewport())
	{
		ActiveProductionPopup->CloseFromBack();
		ActiveProductionPopup = nullptr;
		return true;
	}
	if (ActiveMinePicker && ActiveMinePicker->IsInViewport())
	{
		CloseMinePicker();
		return true;
	}
	HandleCloseButtonClicked();
	return true;
}

void UCountryDetailWidget::CloseProductionPopup()
{
	if (ActiveProductionPopup)
	{
		ActiveProductionPopup->RemoveFromParent();
		ActiveProductionPopup = nullptr;
	}
}

bool UCountryDetailWidget::IsProductionStartPopupOpen() const
{
	return ActiveProductionPopup && ActiveProductionPopup->IsInViewport();
}

void UCountryDetailWidget::CloseMinePicker()
{
	if (ActiveMinePicker)
	{
		ActiveMinePicker->RemoveFromParent();
		ActiveMinePicker = nullptr;
	}
}

bool UCountryDetailWidget::IsInstantFinishConfirmOpen() const
{
	return (InstantFinishWindow.IsValid() && InstantFinishWindow->IsInViewport())
		|| (InstantFinishConfirm.IsValid() && InstantFinishConfirm->IsInViewport());
}

void UCountryDetailWidget::CloseInstantFinishConfirm()
{
	if (UInstantFinishConfirmWidget* Window = InstantFinishWindow.Get())
	{
		Window->OnConfirmed().RemoveAll(this);
		Window->OnCancelled().RemoveAll(this);
		Window->Dismiss();
	}
	if (UConfirmCancelWidget* Confirm = InstantFinishConfirm.Get())
	{
		Confirm->OnConfirm.RemoveAll(this);
		Confirm->OnCancel.RemoveAll(this);
		Confirm->DeactivateWidget();
		Confirm->RemoveFromParent();
	}
	ForgetInstantFinishConfirm();
}

void UCountryDetailWidget::ForgetInstantFinishConfirm()
{
	InstantFinishWindow.Reset();
	InstantFinishConfirm.Reset();
	InstantFinishLineId = INDEX_NONE;
	InstantFinishResource = EResourceType::None;
}

void UCountryDetailWidget::OpenInstantFinishConfirm(UTexture2D* Icon, const FText& Name, int64 Current, int64 Target,
	double Unit01, const FTimespan& Remaining, int64 Cost, bool bMine, TFunction<void()> OnConfirm)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> WindowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::InstantFinishConfirm) : nullptr;
	// 범용 확인 창과 같은 층 ― 컨트롤러 소유 + Z 10000 이라 뒤로가기가 이 창의 취소를 먼저 누른다
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UInstantFinishConfirmWidget* Window = (WindowClass && PC) ? CreateWidget<UInstantFinishConfirmWidget>(PC, WindowClass) : nullptr;
	if (Window)
	{
		InstantFinishWindow = Window;
		Window->AddToViewport(10000);
		Window->Configure(Icon, Name, Current, Target, Unit01, Remaining,
			static_cast<int32>(FMath::Min<int64>(Cost, MAX_int32)), bMine);
		Window->PlayOverlayAppear();
		// 창이 스스로 닫힌다 ― 여기서 RemoveAll 하면 방송 중인 이 람다가 풀린다
		Window->OnCancelled().AddWeakLambda(this, [this]() { ForgetInstantFinishConfirm(); });
		Window->OnConfirmed().AddWeakLambda(this, [this, OnConfirm]()
		{
			ForgetInstantFinishConfirm();
			OnConfirm();
		});
		return;
	}

	if (!bWarnedInstantFinishFallback)
	{
		bWarnedInstantFinishFallback = true;
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] EWidgetType::InstantFinishConfirm 미등록 ― 범용 확인 창으로 대신 띄운다"));
	}
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!UIMgr) return;
	const FText Title = bMine
		? NSLOCTEXT("CountryDetail", "MineFinishTitle", "채광 즉시 완료")
		: NSLOCTEXT("CountryDetail", "FactoryFinishTitle", "생산 즉시 완료");
	const FText Message = bMine
		? FText::Format(NSLOCTEXT("CountryDetail", "MineFinishMessage", "다이아 {0}개로 이 채광을 즉시 완료합니다."), FText::AsNumber(Cost))
		: FText::Format(NSLOCTEXT("CountryDetail", "FactoryFinishMessage", "다이아 {0}개로 이 생산을 즉시 완료합니다."), FText::AsNumber(Cost));
	UConfirmCancelWidget* Confirm = UIMgr->CreateConfirmDialog(Title, Message);
	if (!Confirm) return;
	Confirm->bAutoRemove = false;
	Confirm->RestoreDefaultTexts();
	Confirm->SetConfirmOnly(false);
	Confirm->SetCancelOnly(false);
	Confirm->SetTitle(Title);
	Confirm->SetMessage(Message);
	Confirm->SetConfirmButtonText(NSLOCTEXT("CountryDetail", "InstantFinishConfirm", "즉시 완료"));
	Confirm->SetCancelButtonText(NSLOCTEXT("CountryDetail", "InstantFinishCancel", "취소"));
	InstantFinishConfirm = Confirm;
	Confirm->OnCancel.AddWeakLambda(this, [this]() { ForgetInstantFinishConfirm(); });
	Confirm->OnConfirm.AddWeakLambda(this, [this, OnConfirm]()
	{
		ForgetInstantFinishConfirm();
		OnConfirm();
	});
}

void UCountryDetailWidget::InitializeMainTabs()
{
	if (!FactoryTab && !MineTab && !PortTab) return;

	MainTabGroup = NewObject<UCommonButtonGroupBase>(this);
	MainTabGroup->SetSelectionRequired(true);

	// 그룹 인덱스는 쓰지 않는다 — 선택된 버튼으로 시설을 찾으므로 접힌 탭이 섞여도 어긋나지 않는다.
	for (UTabButtonWidget* Tab : { FactoryTab.Get(), MineTab.Get(), PortTab.Get() })
	{
		if (UCommonButtonBase* Btn = Tab ? Tab->GetButton() : nullptr)
		{
			MainTabGroup->AddWidget(Btn);
			Btn->SetIsSelectable(true);
		}
	}

	MainTabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCountryDetailWidget::OnMainTabSelectionChanged);
}

void UCountryDetailWidget::BindManagerEvents()
{
	if (OrderMgr)
	{
		OrderMgr->OnOrderCreated.AddDynamic(this, &UCountryDetailWidget::HandleProductionOrderCreated);
		OrderMgr->OnOrderConsumed.AddDynamic(this, &UCountryDetailWidget::HandleProductionOrderConsumed);
		OrderMgr->OnOrderDepleted.AddDynamic(this, &UCountryDetailWidget::HandleProductionOrderDepleted);
	}
	if (ResourceMgr)
	{
		ResourceMgr->OnResourceChanged.AddUObject(this, &UCountryDetailWidget::HandleResourceChanged);
	}
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineStarted);
		FactoryMgr->OnLineProgressed.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineProgressed);
		FactoryMgr->OnLineCompleted.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineCompleted);
		FactoryMgr->OnLineClaimed.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineClaimed);
		FactoryMgr->OnLineRemoved.AddDynamic(this, &UCountryDetailWidget::HandleMgrLineRemoved);
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
	if (WarehouseMgr)
	{
		WarehouseMgr->OnProductAdded.AddDynamic(this, &UCountryDetailWidget::HandleWarehouseChanged);
		WarehouseMgr->OnProductConsumed.AddDynamic(this, &UCountryDetailWidget::HandleWarehouseChanged);
	}
	if (TradeOrderMgr)
	{
		TradeOrderMgr->OnOrderAccepted.AddDynamic(this, &UCountryDetailWidget::HandleTradeOrderChanged);
		TradeOrderMgr->OnOrderCompleted.AddDynamic(this, &UCountryDetailWidget::HandleTradeOrderChanged);
		TradeOrderMgr->OnOrderExpired.AddDynamic(this, &UCountryDetailWidget::HandleTradeOrderChanged);
		TradeOrderMgr->OnOrderFulfilledPartial.AddDynamic(this, &UCountryDetailWidget::HandleTradeOrderPartial);
		TradeOrderMgr->OnOrderDismissed.AddDynamic(this, &UCountryDetailWidget::HandleTradeOrderDismissed);
	}
}

void UCountryDetailWidget::UnbindManagerEvents()
{
	if (OrderMgr)
	{
		OrderMgr->OnOrderCreated.RemoveDynamic(this, &UCountryDetailWidget::HandleProductionOrderCreated);
		OrderMgr->OnOrderConsumed.RemoveDynamic(this, &UCountryDetailWidget::HandleProductionOrderConsumed);
		OrderMgr->OnOrderDepleted.RemoveDynamic(this, &UCountryDetailWidget::HandleProductionOrderDepleted);
	}
	if (ResourceMgr)
	{
		ResourceMgr->OnResourceChanged.RemoveAll(this);
	}
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineStarted);
		FactoryMgr->OnLineProgressed.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineProgressed);
		FactoryMgr->OnLineCompleted.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineCompleted);
		FactoryMgr->OnLineClaimed.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineClaimed);
		FactoryMgr->OnLineRemoved.RemoveDynamic(this, &UCountryDetailWidget::HandleMgrLineRemoved);
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
	if (WarehouseMgr)
	{
		WarehouseMgr->OnProductAdded.RemoveDynamic(this, &UCountryDetailWidget::HandleWarehouseChanged);
		WarehouseMgr->OnProductConsumed.RemoveDynamic(this, &UCountryDetailWidget::HandleWarehouseChanged);
	}
	if (TradeOrderMgr)
	{
		TradeOrderMgr->OnOrderAccepted.RemoveDynamic(this, &UCountryDetailWidget::HandleTradeOrderChanged);
		TradeOrderMgr->OnOrderCompleted.RemoveDynamic(this, &UCountryDetailWidget::HandleTradeOrderChanged);
		TradeOrderMgr->OnOrderExpired.RemoveDynamic(this, &UCountryDetailWidget::HandleTradeOrderChanged);
		TradeOrderMgr->OnOrderFulfilledPartial.RemoveDynamic(this, &UCountryDetailWidget::HandleTradeOrderPartial);
		TradeOrderMgr->OnOrderDismissed.RemoveDynamic(this, &UCountryDetailWidget::HandleTradeOrderDismissed);
	}
}

void UCountryDetailWidget::SetCurrentCountry(ECountryType Country)
{
	const bool bCountryChanged = Country != CurrentCountry;
	if (bCountryChanged)
	{
		CloseProductionPopup();
		CloseMinePicker();
		CloseInstantFinishConfirm();
	}
	CurrentCountry = Country;
	ApplyCountryBasics();

	// 채광 지원 국가일 경우 라인 보장 (DT_CountryInfo.MinableResources 기반 동기화)
	if (bSupportsMine && MineMgr)
	{
		MineMgr->EnsureCountryLines(Country);
	}

	ApplyFacilityTabs();
	// 같은 시설이 있으면 그 탭을 지킨다(같은 나라 재클릭·같은 산업 나라 이동)
	EDioramaModule Target = EDioramaModule::Factory;
	if (CountryDrawer::FacilityForTab(CurrentTabIndex, Facilities, Target))
	{
		SelectFacility(Target);
	}
	else
	{
		bHasFacility = false;
		CurrentTabIndex = TAB_INDEX_INFO;
		if (MainContentSwitcher)
		{
			MainContentSwitcher->SetActiveWidgetIndex(TAB_INDEX_INFO);
		}
		UpdatePageToggleVisibility();
	}
	// 같은 나라 재클릭은 보던 페이지를 지킨다
	if (bCountryChanged)
	{
		SelectPage(CountryDrawer::EDrawerPage::Production, false);
	}
	RebuildAllPages();
}

bool UCountryDetailWidget::GetCountryRow(FCountryInfoTable& OutRow) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return false;
	}
	bool bSuccess = false;
	OutRow = TableMgr->GetCountryInfo(CurrentCountry, bSuccess);
	return bSuccess;
}

void UCountryDetailWidget::ApplyCountryBasics()
{
	FCountryInfoTable Info;
	if (!GetCountryRow(Info))
	{
		// DT 미등록 국가 — 이름은 비우고 경고로 드러낸다(코드 폴백 금지 원칙).
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] DT_CountryInfo 미등록 국가 (Country=%d)"), static_cast<int32>(CurrentCountry));
		if (PanelHeader)
		{
			PanelHeader->SetTitle(FText::GetEmpty());
		}
		bSupportsFactory = false;
		bSupportsMine = false;
		bSupportsPort = false;
		Facilities.Reset();
		return;
	}

	if (PanelHeader)
	{
		PanelHeader->SetTitle(Info.DisplayName);
	}

	Facilities = CountryDrawer::FacilitiesOf(Info);
	bSupportsFactory = Facilities.Contains(EDioramaModule::Factory);
	bSupportsMine = Facilities.Contains(EDioramaModule::Mine);
	bSupportsPort = Facilities.Contains(EDioramaModule::Port);
}

void UCountryDetailWidget::ApplyFacilityTabs()
{
	// 미지원 탭 = 완전히 숨김 (Collapsed + SetIsSelectable(false) — 그룹 선택 후보에서 제외).
	auto Apply = [](UTabButtonWidget* Tab, bool bSupported)
	{
		if (!Tab) return;
		Tab->SetVisibility(bSupported ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (UCommonButtonBase* Btn = Tab->GetButton())
		{
			Btn->SetIsSelectable(bSupported);
		}
	};

	Apply(InfoTab, false);
	Apply(FactoryTab, bSupportsFactory);
	Apply(MineTab, bSupportsMine);
	Apply(PortTab, bSupportsPort);

	if (MineTab && bSupportsMine)
	{
		FCountryInfoTable Info;
		const bool bAllOil = GetCountryRow(Info) && CountryDrawer::IsAllOil(Info.MinableResources);
		MineTab->SetTabText(CountryDrawer::MineTabLabel(bAllOil));
	}

	if (TabBar)
	{
		TabBar->SetVisibility(CountryDrawer::ShouldShowTabs(Facilities)
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UTabButtonWidget* UCountryDetailWidget::TabButtonFor(EDioramaModule Facility) const
{
	switch (Facility)
	{
	case EDioramaModule::Factory: return FactoryTab;
	case EDioramaModule::Mine: return MineTab;
	case EDioramaModule::Port: return PortTab;
	default: return nullptr;
	}
}

UWidget* UCountryDetailWidget::PageFor(EDioramaModule Facility) const
{
	switch (Facility)
	{
	case EDioramaModule::Factory: return FactoryPlate;
	case EDioramaModule::Mine: return MinePlate;
	case EDioramaModule::Port: return PortPlate;
	default: return nullptr;
	}
}

void UCountryDetailWidget::SelectFacility(EDioramaModule Facility)
{
	UTabButtonWidget* Tab = TabButtonFor(Facility);
	UCommonButtonBase* Btn = Tab ? Tab->GetButton() : nullptr;
	if (MainTabGroup && Btn)
	{
		const int32 GroupIndex = MainTabGroup->FindButtonIndex(Btn);
		if (GroupIndex != INDEX_NONE && MainTabGroup->GetSelectedButtonIndex() != GroupIndex)
		{
			// 선택 콜백(OnMainTabSelectionChanged)이 내용 전환·방송을 맡는다
			MainTabGroup->SelectButtonAtIndex(GroupIndex);
			if (bHasFacility && CurrentFacility == Facility)
			{
				return;
			}
		}
	}
	ApplyFacilitySelection(Facility);
}

void UCountryDetailWidget::ApplyFacilitySelection(EDioramaModule Facility)
{
	const bool bChanged = !bHasFacility || CurrentFacility != Facility;
	CurrentFacility = Facility;
	bHasFacility = true;
	CurrentTabIndex = CountryDrawer::TabIndexOf(Facility);

	if (MainContentSwitcher)
	{
		UWidget* Page = PageFor(Facility);
		if (Page && MainContentSwitcher->GetChildIndex(Page) != INDEX_NONE)
		{
			MainContentSwitcher->SetActiveWidget(Page);
		}
		else
		{
			MainContentSwitcher->SetActiveWidgetIndex(CurrentTabIndex);
		}
	}

	if (bChanged)
	{
		OnTabChanged.Broadcast(CurrentTabIndex);
	}
	UpdatePageToggleVisibility();
	RefreshProductionDot();
}

void UCountryDetailWidget::RebuildAllPages()
{
	RebuildFactoryPage();
	RebuildMinePage();
	RefreshPortPage();
}

// ── 생산/강화 페이지 ──

void UCountryDetailWidget::SelectPage(CountryDrawer::EDrawerPage Page, bool bFromUser)
{
	const bool bChanged = CurrentPage != Page;
	CurrentPage = Page;
	const int32 PageIndex = Page == CountryDrawer::EDrawerPage::Upgrade ? 1 : 0;
	FactoryPageSwitcher->SetActiveWidgetIndex(PageIndex);
	MinePageSwitcher->SetActiveWidgetIndex(PageIndex);
	PageToggle->SetPage(Page, false);
	// 시설 탭 버튼과 같은 클릭음 — 토글 칸은 자체 클릭음을 끄고 여기서만 낸다
	if (bFromUser && bChanged)
	{
		UGameInstance* GI = GetGameInstance();
		if (USoundManagerSubsystem* SoundMgr = GI ? GI->GetSubsystem<USoundManagerSubsystem>() : nullptr)
		{
			SoundMgr->PlayUISound(CGUISoundTags::ButtonClick);
		}
	}
	RefreshProductionDot();
}

void UCountryDetailWidget::UpdatePageToggleVisibility()
{
	const bool bShow = bHasFacility && CountryDrawer::ShowsPageToggle(CurrentFacility);
	PageToggle->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UCountryDetailWidget::RefreshProductionDot()
{
	int64 Pending = 0;
	if (bHasFacility && CurrentFacility == EDioramaModule::Factory)
	{
		int32 Running = 0;
		int32 Ready = 0;
		CountFactoryLines(Running, Ready, Pending);
	}
	else if (bHasFacility && CurrentFacility == EDioramaModule::Mine)
	{
		Pending = HasClaimableFullMineLine() ? 1 : 0;
	}
	PageToggle->SetProductionDot(CountryDrawer::ShowsProductionDot(CurrentPage, Pending));
	ScheduleLineEdgeRefresh();
}

void UCountryDetailWidget::ScheduleLineEdgeRefresh()
{
	UWorld* TimerWorld = GetWorld();
	if (!TimerWorld)
	{
		return;
	}
	FTimerManager& Timers = TimerWorld->GetTimerManager();
	Timers.ClearTimer(LineEdgeTimerHandle);
	if (!bHasFacility || !IsActivated())
	{
		return;
	}
	double Soonest = TNumericLimits<double>::Max();
	if (CurrentFacility == EDioramaModule::Factory && FactoryMgr)
	{
		for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(CurrentCountry))
		{
			FWorldFactoryLineState Live;
			if (Line.RatePerSecond > 0.0f && Line.TargetQty > 0 && FactoryMgr->GetLineState(CurrentCountry, Line.LineId, Live) && !Live.bCompleted)
			{
				Soonest = FMath::Min(Soonest, static_cast<double>(Live.TargetQty) / Live.RatePerSecond - Live.LineElapsedSec);
			}
		}
	}
	else if (CurrentFacility == EDioramaModule::Mine && MineMgr)
	{
		const double RateMul = MineMgr->GetRateMultiplier(CurrentCountry);
		for (const FMineLineState& Line : MineMgr->GetMineLines(CurrentCountry))
		{
			FMineLineState Live;
			const double Rate = static_cast<double>(Line.BaseRatePerSecond) * RateMul;
			const int64 LineMax = MineMgr->GetEffectiveMaxStorage(CurrentCountry, Line.Resource);
			if (Rate > 0.0 && LineMax > 0 && MineMgr->GetLineState(CurrentCountry, Line.Resource, Live) && !Live.bSaturated)
			{
				const double Left = static_cast<double>(LineMax - Live.CurrentQty) - Live.PendingFraction;
				Soonest = FMath::Min(Soonest, Left / Rate);
			}
		}
	}
	if (Soonest < static_cast<double>(MAX_int32))
	{
		// 하한 0.1초 = 경계 직전 연타 방지, 상한 10초 = 월드 시간이 벽시계보다 늦게 간 만큼(히치·백그라운드)을 몇 초 안에 따라잡는다
		Timers.SetTimer(LineEdgeTimerHandle, this, &UCountryDetailWidget::HandleLineEdge,
			static_cast<float>(FMath::Clamp(Soonest + 0.05, 0.1, 10.0)), false);
	}
}

void UCountryDetailWidget::HandleLineEdge()
{
	// 보이는 시설의 요약 줄만 ― 무역항을 보고 있으면 숨은 공장 요약을 다시 그릴 이유가 없다
	if (!bHasFacility)
	{
		return;
	}
	if (CurrentFacility == EDioramaModule::Mine)
	{
		RefreshMineHero();
	}
	else if (CurrentFacility == EDioramaModule::Factory)
	{
		RefreshFactoryHero();
	}
}

bool UCountryDetailWidget::HasClaimableFullMineLine() const
{
	// 자동 수령 나라는 가득 차면 틱이 바로 받으므로 점을 띄울 이유가 없다
	if (!MineMgr || !bSupportsMine || MineMgr->IsAutoClaimEnabled(CurrentCountry))
	{
		return false;
	}
	for (const FMineLineState& Line : MineMgr->GetMineLines(CurrentCountry))
	{
		FMineLineState Live;
		if (MineMgr->GetLineState(CurrentCountry, Line.Resource, Live) && Live.bSaturated)
		{
			return true;
		}
	}
	return false;
}

void UCountryDetailWidget::CountFactoryLines(int32& OutRunning, int32& OutReady, int64& OutReadyQty) const
{
	OutRunning = 0;
	OutReady = 0;
	OutReadyQty = 0;
	if (!FactoryMgr || !bSupportsFactory)
	{
		return;
	}
	for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(CurrentCountry))
	{
		FWorldFactoryLineState Live;
		if (!FactoryMgr->GetLineState(CurrentCountry, Line.LineId, Live))
		{
			continue;
		}
		// 수령 판정은 OverseasActions 와 같다(완료 + 수량 > 0)
		if (Live.bCompleted && Live.CurrentQty > 0)
		{
			++OutReady;
			OutReadyQty += Live.CurrentQty;
		}
		else if (!Live.bCompleted)
		{
			++OutRunning;
		}
	}
}

int64 UCountryDetailWidget::PendingMineQuantity() const
{
	if (!MineMgr || !bSupportsMine)
	{
		return 0;
	}
	int64 Total = 0;
	for (const FMineLineState& Line : MineMgr->GetMineLines(CurrentCountry))
	{
		FMineLineState Live;
		if (MineMgr->GetLineState(CurrentCountry, Line.Resource, Live))
		{
			Total += FMath::Max<int64>(Live.CurrentQty, 0);
		}
	}
	return Total;
}

void UCountryDetailWidget::SwitchToTab(int32 TabIndex)
{
	EDioramaModule Target = EDioramaModule::Factory;
	if (CountryDrawer::FacilityForTab(TabIndex, Facilities, Target))
	{
		SelectFacility(Target);
	}
}

TSubclassOf<UUserWidget> UCountryDetailWidget::ResolveRowClass(EWidgetType Preferred, EWidgetType Fallback, bool bUseFallback)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return nullptr;
	}
	if (TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(Preferred))
	{
		return Cls;
	}
	if (!WarnedRowFallbacks.Contains(Preferred))
	{
		WarnedRowFallbacks.Add(Preferred);
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetail] DT_WidgetClass 에 EWidgetType %d 행 없음 — %s"),
			static_cast<int32>(Preferred), bUseFallback ? TEXT("옛 카드형으로 그린다") : TEXT("행을 만들지 않는다"));
	}
	return bUseFallback ? TableMgr->GetWidgetClass(Fallback) : nullptr;
}

UDrawerInfoRowWidget* UCountryDetailWidget::SpawnInfoRow(UPanelWidget* Container)
{
	if (!Container)
	{
		return nullptr;
	}
	TSubclassOf<UUserWidget> RowClass = ResolveRowClass(EWidgetType::DrawerInfoRow, EWidgetType::DrawerInfoRow, false);
	UDrawerInfoRowWidget* Row = RowClass ? CreateWidget<UDrawerInfoRowWidget>(this, RowClass) : nullptr;
	if (Row)
	{
		AddListRow(Container, Row);
	}
	return Row;
}

void UCountryDetailWidget::AddListRow(UPanelWidget* List, UWidget* Row) const
{
	if (!List || !Row)
	{
		return;
	}
	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(List->AddChild(Row)))
	{
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, ListRowGap));
	}
}

void UCountryDetailWidget::AddLockedLineRow(UPanelWidget* List, int32 CurrentSlots, int32 MaxSlots)
{
	if (!CountryDrawer::ShowsLockedLine(CurrentSlots, MaxSlots))
	{
		return;
	}
	if (UDrawerInfoRowWidget* Row = SpawnInfoRow(List))
	{
		// 자물쇠 그림은 WBP 의 LockIcon 이 그린다
		Row->SetRow(nullptr, FText::GetEmpty(), CountryDrawer::LockedLineName(CurrentSlots + 1), /*bDimName=*/true,
			CountryDrawer::LockedLineDetail(), FText::GetEmpty(), EDrawerRowTone::Normal);
		Row->SetAction(FText::GetEmpty());
		Row->SetEmptyPlate(false);
		Row->SetLocked(true);
	}
}

int32 UCountryDetailWidget::FactoryMaxSlots() const
{
	const int32 Current = FactoryMgr ? FactoryMgr->GetMaxLines(CurrentCountry) : 0;
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FWorldFactoryUpgradeDefinition Def;
	if (!TableMgr || !TableMgr->GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType::LineExpansion, Def))
	{
		return Current;
	}
	// MaxLevel 0 = 증설 무제한 — 다음 1줄은 늘 열 수 있다
	if (Def.MaxLevel <= 0)
	{
		return Current + 1;
	}
	return FMath::Max(Current, FMath::FloorToInt32(Def.BaseValue + Def.PerLevel * Def.MaxLevel));
}

int32 UCountryDetailWidget::MineMaxSlots() const
{
	if (!MineMgr)
	{
		return 0;
	}
	const int32 ResourceCap = MineMgr->GetResourceLineCap(CurrentCountry);
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FMineUpgradeDefinition Def;
	if (!TableMgr || !TableMgr->GetMineUpgradeDefinition(EMineUpgradeType::LineExpansion, Def))
	{
		return MineMgr->GetMaxLines(CurrentCountry);
	}
	if (Def.MaxLevel <= 0)
	{
		return ResourceCap;
	}
	// MineManager::GetMaxLines 와 같은 식을 최대 레벨에 넣는다
	return FMath::Min(ResourceCap, FMath::Max(1, FMath::FloorToInt32(Def.BaseValue + Def.PerLevel * Def.MaxLevel)));
}

bool UCountryDetailWidget::IsCompanyTypeSupportedByCountry(ECompanyType Company) const
{
	if (Company == ECompanyType::None) return false;

	FCountryInfoTable Info;
	if (!GetCountryRow(Info) || Info.SupportedIndustries.Num() == 0)
	{
		return true;  // DT 미지정 시 전체 허용
	}
	return Info.SupportedIndustries.Contains(Company);
}

// ── 공장 페이지 ──

void UCountryDetailWidget::RebuildFactoryPage()
{
	RebuildFactoryLineList();
	RefreshFactoryHero();
	RebuildFactoryEnhancementSlots();
}

void UCountryDetailWidget::ClearFactoryLineWidgets()
{
	for (UWorldFactoryLineWidget* Line : SpawnedFactoryLines)
	{
		if (Line)
		{
			Line->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineInstantFinish);
			Line->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleLineClaim);
			Line->OnLineCompleted.RemoveDynamic(this, &UCountryDetailWidget::HandleFactoryLineCompleted);
		}
	}
	SpawnedFactoryLines.Reset();
	for (UDrawerInfoRowWidget* Row : SpawnedEmptyLineRows)
	{
		if (Row)
		{
			Row->OnActionClicked().RemoveAll(this);
		}
	}
	SpawnedEmptyLineRows.Reset();
	if (FactoryLineList)
	{
		FactoryLineList->ClearChildren();
	}
}

void UCountryDetailWidget::RebuildFactoryLineList()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EmptyFactoryRowsRefreshTimerHandle);
	}
	EmptyFactoryRowsRefreshTimerHandle.Invalidate();
	ClearFactoryLineWidgets();
	if (!bSupportsFactory || !FactoryMgr)
	{
		return;
	}

	// 가동 라인 다음에 빈 슬롯 — 빈 슬롯 「생산 시작」이 새 라인을 맨 뒤에 붙이므로 번호가 유지된다
	const TArray<FWorldFactoryLineState> Lines = FactoryMgr->GetActiveLines(CurrentCountry);
	int32 Number = 0;
	for (const FWorldFactoryLineState& State : Lines)
	{
		SpawnFactoryLineWidget(State, ++Number);
	}

	const int32 MaxLines = FactoryMgr->GetMaxLines(CurrentCountry);
	const int32 EmptySlots = FMath::Max(0, MaxLines - Lines.Num());
	for (int32 Index = 0; Index < EmptySlots; ++Index)
	{
		UDrawerInfoRowWidget* Row = SpawnInfoRow(FactoryLineList);
		if (!Row)
		{
			break;
		}
		Row->OnActionClicked().AddUObject(this, &UCountryDetailWidget::HandleEmptyLineStart);
		SpawnedEmptyLineRows.Add(Row);
	}
	RefreshEmptyFactoryRows();
	AddLockedLineRow(FactoryLineList, MaxLines, FactoryMaxSlots());
}

bool UCountryDetailWidget::ResolveQuickStartOrder(FProductionOrder& OutOrder, int32& OutMaxProducible) const
{
	UGameInstance* GI = GetGameInstance();
	if (!GI || !OrderMgr || !FactoryMgr)
	{
		return false;
	}

	const UTradePort* Port = GI->GetSubsystem<UTradePort>();
	TArray<CountryDrawer::FQuickStartCandidate> Candidates;
	for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
	{
		if (Order.IsConsumed() || !IsCompanyTypeSupportedByCountry(Order.CompanyType))
		{
			continue;
		}
		EResourceType Bottleneck = EResourceType::None;
		bool bSupplyBound = false;
		CountryDrawer::FQuickStartCandidate Candidate;
		Candidate.OrderID = Order.OrderID;
		Candidate.Company = Order.CompanyType;
		Candidate.ProjectIndex = Order.ProjectIndex;
		Candidate.MaxProducible = OrderMgr->GetMaxProducible(Order, Bottleneck, bSupplyBound);
		Candidate.UnitValue = Port
			? static_cast<int64>(static_cast<double>(Port->GetBasePrice(Order.CompanyType, Order.ProjectIndex))
				* GetQualityGradeRevenueMultiplier(Order.Grade))
			: 0;
		Candidates.Add(Candidate);
	}

	ECompanyType LastCompany = ECompanyType::None;
	int32 LastProject = 0;
	int32 LastLineId = 0;
	for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(CurrentCountry))
	{
		if (Line.LineId > LastLineId && Line.CompanyType != ECompanyType::None)
		{
			LastLineId = Line.LineId;
			LastCompany = Line.CompanyType;
			LastProject = Line.ProductIndex;
		}
	}

	const int32 Pick = CountryDrawer::PickQuickStart(Candidates, LastCompany, LastProject);
	if (Pick == INDEX_NONE)
	{
		return false;
	}
	const CountryDrawer::FQuickStartCandidate& Chosen = Candidates[Pick];
	if (const FProductionOrder* Order = OrderMgr->GetOrderByID(Chosen.OrderID))
	{
		OutOrder = *Order;
		OutMaxProducible = Chosen.MaxProducible;
		return true;
	}
	return false;
}

void UCountryDetailWidget::RefreshEmptyFactoryRows()
{
	// 빈 줄은 무엇을 만들지 미리 말하지 않는다 ― 주문 고르기는 생산 설정 창 몫(ResolveQuickStartOrder = 창의 사전 선택)
	for (int32 Index = 0; Index < SpawnedEmptyLineRows.Num(); ++Index)
	{
		if (UDrawerInfoRowWidget* Row = SpawnedEmptyLineRows[Index])
		{
			Row->SetRow(nullptr, FText::AsNumber(SpawnedFactoryLines.Num() + Index + 1), CountryDrawer::EmptyLineName(),
				/*bDimName=*/false, CountryDrawer::EmptyLineDetail(), FText::GetEmpty(), EDrawerRowTone::Normal);
			Row->SetAction(CountryDrawer::StartLineLabel());
			Row->SetEmptyPlate(true);
			Row->SetLocked(false);
		}
	}
}

void UCountryDetailWidget::ScheduleEmptyFactoryRowsRefresh()
{
	if (SpawnedEmptyLineRows.IsEmpty() || EmptyFactoryRowsRefreshTimerHandle.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		EmptyFactoryRowsRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UCountryDetailWidget::FlushEmptyFactoryRowsRefresh));
	}
}

void UCountryDetailWidget::FlushEmptyFactoryRowsRefresh()
{
	EmptyFactoryRowsRefreshTimerHandle.Invalidate();
	RefreshEmptyFactoryRows();
}

void UCountryDetailWidget::RefreshFactoryHero()
{
	if (!FactoryHero || !bSupportsFactory || !FactoryMgr)
	{
		return;
	}

	int32 Running = 0;
	int32 Ready = 0;
	int64 ReadyQty = 0;
	CountFactoryLines(Running, Ready, ReadyQty);

	FText IndustryName;
	UTexture2D* Glyph = nullptr;
	FLinearColor TileColor = FLinearColor::White;
	FCountryInfoTable Info;
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (TableMgr && GetCountryRow(Info))
	{
		ECompanyType Industry = Info.RepresentativeIndustry;
		if (Industry == ECompanyType::None && Info.SupportedIndustries.Num() > 0)
		{
			Industry = Info.SupportedIndustries[0];
		}
		bool bOk = false;
		const FCompanyInfoTable Company = TableMgr->GetCompanyInfo(Industry, bOk);
		if (bOk)
		{
			IndustryName = Company.DisplayName;
			Glyph = Company.GlyphIcon.IsNull() ? nullptr : Company.GlyphIcon.LoadSynchronous();
			TileColor = Company.AccentColor;
		}
	}

	FactoryHero->SetFacilityTile(Glyph, TileColor);
	FactoryHero->SetSummary(CountryDrawer::FactoryTitle(IndustryName),
		CountryDrawer::FactorySummary(FactoryMgr->GetMaxLines(CurrentCountry), Running, Ready));
	FactoryHero->SetAction(CountryDrawer::ClaimFactoryLabel(), EResourceType::Box, ReadyQty);
	RefreshProductionDot();
}

UWorldFactoryLineWidget* UCountryDetailWidget::SpawnFactoryLineWidget(const FWorldFactoryLineState& State, int32 Number)
{
	if (!FactoryLineList) return nullptr;

	TSubclassOf<UUserWidget> LineClass = ResolveRowClass(EWidgetType::WorldFactoryLineRow, EWidgetType::WorldFactoryLine, true);
	if (!LineClass) return nullptr;

	UWorldFactoryLineWidget* Line = CreateWidget<UWorldFactoryLineWidget>(this, LineClass);
	if (!Line) return nullptr;

	UTexture2D* IconTex = State.IconPath.LoadSynchronous();
	const FTimespan Remaining = CalcRemainingTime(State);

	Line->SetLineId(State.LineId);
	Line->SetCountry(CurrentCountry);  // NativeTick polling sync 용
	Line->SetLineNumber(Number);
	Line->SetProductionData(IconTex, State.ProductName, State.RatePerSecond,
		State.CurrentQty, State.TargetQty, Remaining, State.LineElapsedSec);
	if (State.bCompleted)
	{
		Line->SetCompleted(State.CurrentQty);
	}

	Line->OnInstantFinishRequested.AddDynamic(this, &UCountryDetailWidget::HandleLineInstantFinish);
	Line->OnClaimRequested.AddDynamic(this, &UCountryDetailWidget::HandleLineClaim);
	Line->OnLineCompleted.AddDynamic(this, &UCountryDetailWidget::HandleFactoryLineCompleted);

	AddListRow(FactoryLineList, Line);
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

void UCountryDetailWidget::HandleFactoryHeroAction()
{
	// 라인 행·창고·지도 말풍선은 매니저 델리게이트로 따라온다
	OverseasActions::ClaimCountryFactory(GetGameInstance(), CurrentCountry);
	RefreshFactoryHero();
}

void UCountryDetailWidget::HandleEmptyLineStart(UDrawerInfoRowWidget* /*Row*/)
{
	if (ActiveProductionPopup && ActiveProductionPopup->IsInViewport()) return;
	ActiveProductionPopup = nullptr;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;
	TSubclassOf<UUserWidget> PopupClass = TableMgr->GetWidgetClass(EWidgetType::ProductionStartPopup);
	if (!PopupClass) return;

	FProductionOrder Order;
	int32 MaxProducible = 0;
	const int32 PreferredOrderID = ResolveQuickStartOrder(Order, MaxProducible) ? Order.OrderID : 0;
	if (UProductionStartPopupWidget* Popup = CreateWidget<UProductionStartPopupWidget>(this, PopupClass))
	{
		ActiveProductionPopup = Popup;
		Popup->InitializeForCountry(CurrentCountry, PreferredOrderID);
		Popup->AddToViewport(100);
		Popup->PlayOverlayAppear();
	}
}

// ── 버튼 클릭 ──

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

void UCountryDetailWidget::OnMainTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 /*ButtonIndex*/)
{
	for (const EDioramaModule Facility : { EDioramaModule::Factory, EDioramaModule::Mine, EDioramaModule::Port })
	{
		UTabButtonWidget* Tab = TabButtonFor(Facility);
		if (Tab && Tab->GetButton() == SelectedButton)
		{
			ApplyFacilitySelection(Facility);
			return;
		}
	}
}

// ── 라인 위젯 이벤트 ──

void UCountryDetailWidget::HandleLineInstantFinish(UWorldFactoryLineWidget* Line)
{
	if (!Line || !FactoryMgr) return;
	if (IsInstantFinishConfirmOpen()) return;
	CloseInstantFinishConfirm();
	const ECountryType Country = CurrentCountry;
	const int32 LineId = Line->GetLineId();
	const int64 Cost = FactoryMgr->GetInstantFinishCost(Country, LineId);
	if (Cost <= 0 || FactoryMgr->IsInstantFinishPending(Country, LineId)) return;
	FWorldFactoryLineState State;
	if (!FactoryMgr->GetLineState(Country, LineId, State)) return;
	const double Made = State.LineElapsedSec * static_cast<double>(State.RatePerSecond);
	const double Unit01 = State.RatePerSecond > 0.0f ? Made - FMath::FloorToDouble(Made) : 0.0;

	InstantFinishLineId = LineId;
	OpenInstantFinishConfirm(State.IconPath.LoadSynchronous(), State.ProductName, State.CurrentQty, State.TargetQty,
		Unit01, CalcRemainingTime(State), Cost, /*bMine=*/false, [this, Country, LineId, Cost]()
	{
		if (!FactoryMgr || FactoryMgr->IsInstantFinishPending(Country, LineId)) return;
		if (FactoryMgr->GetInstantFinishCost(Country, LineId) <= 0) return;
		FactoryMgr->RequestInstantFinish(Country, LineId, Cost,
			FOnCloudEconomyOpDone::CreateWeakLambda(this, [this, Cost](bool bOk, FName Code)
		{
			if (!bOk)
			{
				if (UUIManagerSubsystem* ResultUI = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
				{
					ResultUI->NotifyCloudOpFailure(Code, EResourceType::Diamond, Cost);
				}
			}
		}));
	});
}

void UCountryDetailWidget::HandleLineClaim(UWorldFactoryLineWidget* Line)
{
	if (!Line || !FactoryMgr) return;
	FactoryMgr->ClaimLine(CurrentCountry, Line->GetLineId());
}

void UCountryDetailWidget::HandleFactoryLineCompleted(UWorldFactoryLineWidget* Line)
{
	if (Line && FactoryMgr && FindFactoryLineWidgetById(Line->GetLineId()) == Line)
	{
		RefreshFactoryHero();
	}
}

// ── Manager 이벤트 핸들러 ──

void UCountryDetailWidget::HandleMgrLineStarted(ECountryType Country, FWorldFactoryLineState /*LineState*/)
{
	if (Country != CurrentCountry) return;
	// 빈 슬롯 행이 새 라인 행으로 바뀐다 — 번호·순서를 지키려고 목록을 다시 쌓는다
	RebuildFactoryLineList();
	RefreshFactoryHero();
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
	// 창을 연 채 저절로 끝나면 살 것이 없다 ― 다이아 0 소비로 닫는다
	if (LineId == InstantFinishLineId && IsInstantFinishConfirmOpen())
	{
		CloseInstantFinishConfirm();
	}
	if (UWorldFactoryLineWidget* Line = FindFactoryLineWidgetById(LineId))
	{
		FWorldFactoryLineState State;
		if (FactoryMgr->GetLineState(Country, LineId, State))
		{
			Line->SetCompleted(State.CurrentQty);
		}
	}
	RefreshFactoryHero();
}

void UCountryDetailWidget::HandleMgrLineClaimed(ECountryType Country, int32 LineId, int64 /*FinalQty*/)
{
	if (Country != CurrentCountry || !FactoryMgr) return;
	RefreshFactoryHero();
	UWorldFactoryLineWidget* Line = FindFactoryLineWidgetById(LineId);
	if (!Line) return;

	// 라인은 남아 처음부터 다시 돈다 — 위젯 자체 시계가 앞으로만 따라잡으므로 새 상태로 다시 심어야 0 으로 돌아간다.
	FWorldFactoryLineState State;
	if (FactoryMgr->GetLineState(Country, LineId, State))
	{
		// 같은 제품이라 아이콘은 그대로 둔다(nullptr = 브러시 유지).
		Line->SetProductionData(nullptr, State.ProductName, State.RatePerSecond,
			State.CurrentQty, State.TargetQty, CalcRemainingTime(State), State.LineElapsedSec);
	}
}

void UCountryDetailWidget::HandleMgrLineRemoved(ECountryType Country, int32 /*LineId*/)
{
	if (Country != CurrentCountry) return;
	RebuildFactoryLineList();
	RefreshFactoryHero();
}

void UCountryDetailWidget::HandleMgrUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 /*NewLevel*/)
{
	if (Country != CurrentCountry) return;

	// 라인 증설은 빈 슬롯 행 수와 「라인 N개」를 바꾼다
	if (UpgradeType == EWorldFactoryUpgradeType::LineExpansion)
	{
		RebuildFactoryLineList();
		RefreshFactoryHero();
	}

	// 변경된 enum 의 슬롯만 부분 갱신 (전체 rebuild 회피)
	// 변수명 'Slot' 은 UWidget::Slot 멤버를 가리므로 TargetSlot 사용 (BuildingManagePanelWidget 동일 패턴)
	if (UUpgradeSlot* TargetSlot = FactoryEnhanceSlots.FindRef(UpgradeType))
	{
		UpdateFactoryEnhancementSlot(UpgradeType, TargetSlot);
		TargetSlot->PlayUpgradeEffect();
	}
}

// ===== Factory 강화 행 동적 슬롯 (BuildingManagePanelWidget 패턴 이식) =====

void UCountryDetailWidget::RebuildFactoryEnhancementSlots()
{
	// 기존 슬롯 전체 제거 + 캐시 초기화
	FactoryEnhanceList->ClearChildren();
	FactoryEnhanceSlots.Reset();
	if (!bSupportsFactory)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CountryDetailWidget] TableManagerSubsystem 없음 - 강화 슬롯 생성 불가"));
		return;
	}

	TSubclassOf<UUserWidget> SlotClass = ResolveRowClass(EWidgetType::UpgradeRow, EWidgetType::UpgradeSlot, true);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CountryDetailWidget] UpgradeSlot 위젯 클래스 없음 - DT_Widget 에 EWidgetType::UpgradeSlot 행 필요"));
		return;
	}

	// DT 단일 진실 - SortOrder 정렬된 행
	const TArray<FWorldFactoryUpgradeDefinition> Defs = TableMgr->GetAllWorldFactoryUpgradeDefinitions();
	for (const FWorldFactoryUpgradeDefinition& Def : Defs)
	{
		UUpgradeSlot* NewSlot = CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!NewSlot) continue;

		ApplyFactorySlotDefinition(NewSlot, Def);
		BindFactorySlotButton(NewSlot, Def.UpgradeType);

		AddListRow(FactoryEnhanceList, NewSlot);
		FactoryEnhanceSlots.Add(Def.UpgradeType, NewSlot);

		UpdateFactoryEnhancementSlot(Def.UpgradeType, NewSlot);
	}
}

void UCountryDetailWidget::ApplyFactorySlotDefinition(UUpgradeSlot* InSlot, const FWorldFactoryUpgradeDefinition& Def)
{
	if (!InSlot) return;

	InSlot->Description = Def.DisplayName;
	InSlot->SubDescription = Def.SubDescription;
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->CostResourceType = Def.CostResourceType;

	InSlot->Icon = Def.Icon;
	InSlot->SetIconTexture(Def.Icon.IsNull() ? nullptr : Def.Icon.LoadSynchronous());
	// 서랍 인스턴스에만 「최대」 ― 공용 버튼 CDO 의 MAX 는 그대로 둔다
	if (UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton())
	{
		Btn->MaxLevelText = CountryDrawer::MaxLevelLabel();
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
	// 최대는 UpdateInfo 가 MaxLevel 로 스스로 판정해 회색 「최대」 버튼으로 바꾼다
	InSlot->UpdateInfo(CurLv, CurDisplay, NextDisplay, Cost, CountryDrawer::UpgradeSlotLocked(bMaxed), Def.MaxLevel);
	ApplyUpgradeLabelInk(InSlot, bMaxed);
	ApplyUpgradeMaxedValue(InSlot, bMaxed);
}

void UCountryDetailWidget::ApplyUpgradeMaxedValue(UUpgradeSlot* InSlot, bool bMaxed) const
{
	for (const TCHAR* PartName : { TEXT("NextValueText"), TEXT("NextValueArrow") })
	{
		if (UWidget* Part = InSlot ? InSlot->GetWidgetFromName(PartName) : nullptr)
		{
			Part->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

void UCountryDetailWidget::ApplyUpgradeLabelInk(UUpgradeSlot* InSlot, bool bMaxed) const
{
	UCostActionButtonWidget* Button = InSlot ? InSlot->GetUpgradeButton() : nullptr;
	UTextBlock* Label = Button && Button->WidgetTree ? Cast<UTextBlock>(Button->WidgetTree->FindWidget(TEXT("BtnText"))) : nullptr;
	if (Label && Button->bOverrideTextStyle)
	{
		Label->SetColorAndOpacity(bMaxed ? MaxedUpgradeLabelInk : Button->BtnTextColor);
	}
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

// ===== 항구 페이지 =====

void UCountryDetailWidget::ClearPortRows()
{
	SpawnedPortRows.Reset();
	PortOrderList->ClearChildren();
	PortGoodsList->ClearChildren();
}

void UCountryDetailWidget::HandleFactoryLineScrollBar(ESlateVisibility NewVisibility)
{
	ApplyLineScrollBarInset(FactoryLineScroll, NewVisibility);
}

void UCountryDetailWidget::HandleMineLineScrollBar(ESlateVisibility NewVisibility)
{
	ApplyLineScrollBarInset(MineLineScroll, NewVisibility);
}

void UCountryDetailWidget::HandlePortPageScrollBar(ESlateVisibility NewVisibility)
{
	ApplyLineScrollBarInset(PortPageScroll, NewVisibility);
}

void UCountryDetailWidget::ApplyLineScrollBarInset(UScrollBox* Scroll, ESlateVisibility BarVisibility)
{
	// 막대 칸만큼 스크롤 상자를 판 오른쪽 안쪽 여백(24)으로 내밀어, 4번째 줄이 붙는 순간 줄 폭이 줄지 않게 한다
	UVerticalBoxSlot* ScrollSlot = Scroll ? Cast<UVerticalBoxSlot>(Scroll->Slot) : nullptr;
	if (!ScrollSlot)
	{
		return;
	}
	const bool bBarShown = BarVisibility == ESlateVisibility::Visible || BarVisibility == ESlateVisibility::HitTestInvisible
		|| BarVisibility == ESlateVisibility::SelfHitTestInvisible;
	// 막대 폭 = 엄지 두께와 트랙 위·아래 칸 그림 중 넓은 것(SScrollBarTrack::ComputeDesiredSize) + 막대 슬롯 여백
	const FMargin BarPadding = Scroll->GetScrollbarPadding();
	const FScrollBarStyle& BarStyle = Scroll->GetWidgetBarStyle();
	const float BarWidth = FMath::Max3(static_cast<float>(Scroll->GetScrollbarThickness().X),
		static_cast<float>(BarStyle.VerticalTopSlotImage.ImageSize.X), static_cast<float>(BarStyle.VerticalBottomSlotImage.ImageSize.X));
	const float BarSpace = BarWidth + BarPadding.Left + BarPadding.Right;
	ScrollSlot->SetPadding(FMargin(0.f, 0.f, bBarShown ? -BarSpace : 0.f, 0.f));
}

void UCountryDetailWidget::RefreshPortPage()
{
	ClearPortRows();
	if (!bSupportsPort)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const UTradePort* Port = GI ? GI->GetSubsystem<UTradePort>() : nullptr;
	FCountryInfoTable Info;
	if (!TableMgr || !GetCountryRow(Info))
	{
		return;
	}

	// 판매는 받아 둔 주문부터 채운다(TradePort 매칭 규칙) — 받지 않은 주문은 판매와 무관해 보이지 않는다
	const TArray<FTradeOrder> Orders = TradeOrderMgr ? TradeOrderMgr->GetAcceptedOrders() : TArray<FTradeOrder>();

	if (PortHero)
	{
		bool bBoxOk = false;
		const FResourceInfo BoxInfo = TableMgr->GetResourceInfo(EResourceType::Box, bBoxOk);
		PortHero->SetFacilityTile(bBoxOk && !BoxInfo.Icon.IsNull() ? BoxInfo.Icon.LoadSynchronous() : nullptr,
			PortFacilityTileColor);
		PortHero->SetSummary(CountryDrawer::PortTitle(Info.DisplayName),
			CountryDrawer::PortSummary(Port ? Port->GetPortPriceMultiplier(CurrentCountry) : 1.0f, Orders.Num()));
		PortHero->SetAction(CountryDrawer::SellAllLabel(), EResourceType::Money,
			OverseasActions::PreviewSellAll(GI, CurrentCountry));
	}

	for (const FTradeOrder& Order : Orders)
	{
		UDrawerInfoRowWidget* Row = SpawnInfoRow(PortOrderList);
		if (!Row)
		{
			UE_LOG(LogTemp, Error, TEXT("[CountryDetail] 주문 줄을 만들지 못했다(DT_WidgetClass DrawerInfoRow) ― 주문 %d건이 「주문」 소제목 아래에서 빠진다"),
				Orders.Num());
			break;
		}
		FText TierLabel;
		FLinearColor TierColor;
		TableMgr->GetTradeOrderTierDisplay(Order.Tier, TierLabel, TierColor);
		bool bProjOk = false;
		const FProjectData Proj = TableMgr->GetProjectData(Order.CompanyType, Order.ProjectIndex, bProjOk);
		const int64 Have = WarehouseMgr ? WarehouseMgr->GetProductAmount(MakeProductKey(Order.CompanyType, Order.ProjectIndex)) : 0;
		const bool bDeliverable = Order.RemainingQuantity > 0 && Have >= Order.RemainingQuantity;
		UTexture2D* OrderIcon = bProjOk && !Proj.Icon.IsNull() ? Proj.Icon.LoadSynchronous() : nullptr;
		Row->SetRow(OrderIcon, FText::GetEmpty(), CountryDrawer::OrderName(TierLabel, bProjOk ? Proj.ProjectName : FText::GetEmpty()),
			/*bDimName=*/false, CountryDrawer::OrderDetail(Order.RemainingQuantity, Order.RewardMultiplier),
			CountryDrawer::OrderStatus(bDeliverable), bDeliverable ? EDrawerRowTone::Good : EDrawerRowTone::Muted);
		Row->SetAction(FText::GetEmpty());
		SpawnedPortRows.Add(Row);
	}
	PortOrderHeader->SetVisibility(Orders.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	TArray<FIntPoint> Keys = WarehouseMgr ? WarehouseMgr->GetAllProductKeys() : TArray<FIntPoint>();
	Keys.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	// 빈 창고 안내는 「칸을 몇 개 그렸나」가 아니라 「창고에 물건이 있나」를 따른다
	int32 GoodsKinds = 0;
	int32 GoodsTiles = 0;
	for (const FIntPoint& Key : Keys)
	{
		const int64 Amount = WarehouseMgr->GetProductAmount(Key);
		if (Amount <= 0)
		{
			continue;
		}
		++GoodsKinds;
		bool bProjOk = false;
		const FProjectData Proj = TableMgr->GetProjectData(static_cast<ECompanyType>(Key.X), Key.Y, bProjOk);
		UTexture2D* Icon = (bProjOk && !Proj.Icon.IsNull()) ? Proj.Icon.LoadSynchronous() : nullptr;
		const FText GoodsName = bProjOk ? Proj.ProjectName : FText::GetEmpty();
		if (AddPortGoodsTile(GoodsTiles, Icon, GoodsName, Amount))
		{
			++GoodsTiles;
		}
	}
	// 균일 격자는 채워진 열 수로 칸 폭을 나눈다 ― 첫 줄이 덜 차면 빈 칸으로 열 수를 지킨다
	const int32 Columns = FMath::Max(1, PortGoodsColumns);
	for (int32 Column = GoodsTiles; GoodsTiles > 0 && Column < Columns; ++Column)
	{
		PortGoodsList->AddChildToUniformGrid(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()), 0, Column);
	}
	// 빈 격자의 음수 슬롯 여백이 안내 줄을 소제목 위로 끌어올리지 않게 칸이 없으면 접는다
	PortGoodsList->SetVisibility(GoodsTiles > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	PortEmptyText->SetVisibility(GoodsKinds == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UCountryDetailWidget::AddPortGoodsTile(int32 Index, UTexture2D* Icon, const FText& GoodsName, int64 Amount)
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> TileClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::DrawerGoodsTile) : nullptr;
	if (!TileClass)
	{
		if (!WarnedRowFallbacks.Contains(EWidgetType::DrawerGoodsTile))
		{
			WarnedRowFallbacks.Add(EWidgetType::DrawerGoodsTile);
			UE_LOG(LogTemp, Error, TEXT("[CountryDetail] DT_WidgetClass 에 DrawerGoodsTile 행 없음 ― 완제품 칸을 그리지 않는다"));
		}
		return false;
	}

	UDrawerGoodsTileWidget* Tile = CreateWidget<UDrawerGoodsTileWidget>(this, TileClass);
	if (!Tile)
	{
		return false;
	}
	const int32 Columns = FMath::Max(1, PortGoodsColumns);
	if (UUniformGridSlot* GridSlot = PortGoodsList->AddChildToUniformGrid(Tile, Index / Columns, Index % Columns))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	Tile->SetGoods(Icon, GoodsName, Amount);
	return true;
}

void UCountryDetailWidget::HandlePortHeroAction()
{
	// 창고·주문 델리게이트가 이 페이지를 다시 그린다
	OverseasActions::SellAllAt(GetGameInstance(), CurrentCountry);
	RefreshPortPage();
}

void UCountryDetailWidget::HandleWarehouseChanged(FIntPoint /*ProductKey*/, int64 /*NewAmount*/)
{
	RefreshPortPage();
	ScheduleEmptyFactoryRowsRefresh();
}

void UCountryDetailWidget::HandleProductionOrderCreated(int32 /*BuildingID*/, const FProductionOrder& /*Order*/)
{
	ScheduleEmptyFactoryRowsRefresh();
}

void UCountryDetailWidget::HandleProductionOrderConsumed(int32 /*OrderID*/, int32 /*ConsumedAmount*/, int32 /*RemainingQuantity*/)
{
	ScheduleEmptyFactoryRowsRefresh();
}

void UCountryDetailWidget::HandleProductionOrderDepleted(int32 /*OrderID*/)
{
	ScheduleEmptyFactoryRowsRefresh();
}

void UCountryDetailWidget::HandleResourceChanged(EResourceType /*Type*/, int64 /*NewValue*/)
{
	ScheduleEmptyFactoryRowsRefresh();
}

void UCountryDetailWidget::HandleTradeOrderChanged(int32 /*OrderId*/)
{
	RefreshPortPage();
}

void UCountryDetailWidget::HandleTradeOrderPartial(int32 /*OrderId*/, int64 /*RemainingQty*/)
{
	RefreshPortPage();
}

void UCountryDetailWidget::HandleTradeOrderDismissed(int32 /*OrderId*/, bool /*bWasAccepted*/)
{
	RefreshPortPage();
}

// ===== 광산 페이지 (Mine) =====

void UCountryDetailWidget::ClearMineLineWidgets()
{
	for (UMineLineWidget* Line : SpawnedMineLines)
	{
		if (Line)
		{
			Line->OnClaimRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineClaim);
			Line->OnInstantFinishRequested.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineInstantFinish);
			Line->OnQuantityChanged.RemoveDynamic(this, &UCountryDetailWidget::HandleMineLineQuantityChanged);
		}
	}
	SpawnedMineLines.Reset();
	for (UDrawerInfoRowWidget* Row : SpawnedEmptyMineRows)
	{
		if (Row)
		{
			Row->OnActionClicked().RemoveAll(this);
		}
	}
	SpawnedEmptyMineRows.Reset();
	if (MineLineList)
	{
		MineLineList->ClearChildren();
	}
}

void UCountryDetailWidget::RebuildMinePage()
{
	RebuildMineLineList();
	RefreshMineHero();
	RebuildMineEnhancementSlots();
}

void UCountryDetailWidget::RebuildMineLineList()
{
	ClearMineLineWidgets();
	if (!MineMgr || !bSupportsMine)
	{
		return;
	}

	// 광산은 슬롯 모델이라 라인이 저절로 생기지 않는다.
	const TArray<FMineLineState> Lines = MineMgr->GetMineLines(CurrentCountry);
	for (const FMineLineState& State : Lines)
	{
		SpawnMineLineWidget(State);
	}

	const int32 MaxLines = MineMgr->GetMaxLines(CurrentCountry);
	const int32 EmptySlots = FMath::Max(0, MaxLines - Lines.Num());
	const TArray<EResourceType> Available = MineMgr->GetAvailableResources(CurrentCountry);
	for (int32 Index = 0; Index < FMath::Min(EmptySlots, Available.Num()); ++Index)
	{
		UDrawerInfoRowWidget* Row = SpawnInfoRow(MineLineList);
		if (!Row)
		{
			break;
		}
		Row->SetRow(nullptr, FText::AsNumber(Lines.Num() + Index + 1), CountryDrawer::EmptyLineName(), /*bDimName=*/false,
			CountryDrawer::EmptyMineDetail(), FText::GetEmpty(), EDrawerRowTone::Normal);
		Row->SetAction(CountryDrawer::StartMineLabel());
		Row->SetEmptyPlate(true);
		Row->SetLocked(false);
		Row->OnActionClicked().AddUObject(this, &UCountryDetailWidget::HandleEmptyMineStart);
		SpawnedEmptyMineRows.Add(Row);
	}
	AddLockedLineRow(MineLineList, MaxLines, MineMaxSlots());
}

void UCountryDetailWidget::HandleEmptyMineStart(UDrawerInfoRowWidget* /*Row*/)
{
	if (ActiveMinePicker && ActiveMinePicker->IsInViewport()) return;
	ActiveMinePicker = nullptr;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;
	TSubclassOf<UUserWidget> PickerClass = TableMgr->GetWidgetClass(EWidgetType::MinePicker);
	if (!PickerClass) return;

	if (UMinePickerWidget* Picker = CreateWidget<UMinePickerWidget>(this, PickerClass))
	{
		ActiveMinePicker = Picker;
		Picker->InitializeForCountry(CurrentCountry);
		Picker->AddToViewport(100);
	}
}

void UCountryDetailWidget::RefreshMineHero()
{
	if (!MineHero || !bSupportsMine || !MineMgr)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	const TArray<FMineLineState> Lines = MineMgr->GetMineLines(CurrentCountry);
	TArray<EResourceType> Resources;
	int32 FullLines = 0;
	const int64 Total = PendingMineQuantity();
	for (const FMineLineState& Line : Lines)
	{
		Resources.Add(Line.Resource);
		FMineLineState Live;
		if (MineMgr->GetLineState(CurrentCountry, Line.Resource, Live) && Live.bSaturated)
		{
			++FullLines;
		}
	}

	// 시설 칸 그림 = 첫 라인 자원, 라인이 없으면 나라의 첫 채광 자원
	EResourceType TileResource = Resources.Num() > 0 ? Resources[0] : EResourceType::None;
	FCountryInfoTable Info;
	if (TileResource == EResourceType::None && GetCountryRow(Info) && Info.MinableResources.Num() > 0)
	{
		TileResource = Info.MinableResources[0];
	}
	bool bResOk = false;
	const FResourceInfo TileInfo = TableMgr && TileResource != EResourceType::None
		? TableMgr->GetResourceInfo(TileResource, bResOk) : FResourceInfo();
	MineHero->SetFacilityTile(bResOk && !TileInfo.Icon.IsNull() ? TileInfo.Icon.LoadSynchronous() : nullptr,
		MineFacilityTileColor);

	const bool bAllOil = CountryDrawer::IsAllOil(Resources);
	MineHero->SetSummary(CountryDrawer::MineTitle(bAllOil, Lines.Num()),
		CountryDrawer::MineHeroSummary(Lines.Num(), FullLines));
	// 칩 아이콘 = 받을 자원. 섞여 있으면 한 자원 그림이 합계를 대표하지 않게 상자
	const bool bOneResource = Resources.Num() > 0 && !Resources.ContainsByPredicate(
		[&Resources](EResourceType R) { return R != Resources[0]; });
	MineHero->SetAction(CountryDrawer::ClaimMineLabel(), bOneResource ? Resources[0] : EResourceType::Box, Total);
	RefreshProductionDot();
}

void UCountryDetailWidget::HandleMineHeroAction()
{
	OverseasActions::ClaimCountryMine(GetGameInstance(), CurrentCountry);
	RefreshMineHero();
}

UMineLineWidget* UCountryDetailWidget::SpawnMineLineWidget(const FMineLineState& State)
{
	if (!MineMgr || !MineLineList) return nullptr;

	TSubclassOf<UUserWidget> LineClass = ResolveRowClass(EWidgetType::MineLineRow, EWidgetType::MineLine, true);
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

	FMineLineState LiveState;
	Line->SetLineState(CurrentCountry,
		MineMgr->GetLineState(CurrentCountry, State.Resource, LiveState) ? LiveState : State,
		LineMax, RateLevel, EffRate);
	Line->OnClaimRequested.AddDynamic(this, &UCountryDetailWidget::HandleMineLineClaim);
	Line->OnInstantFinishRequested.AddDynamic(this, &UCountryDetailWidget::HandleMineLineInstantFinish);
	Line->OnQuantityChanged.AddDynamic(this, &UCountryDetailWidget::HandleMineLineQuantityChanged);

	AddListRow(MineLineList, Line);
	SpawnedMineLines.Add(Line);
	Line->SetLineNumber(SpawnedMineLines.Num());
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

// ===== Mine 강화 행 동적 슬롯 (Factory 동일 패턴) =====

void UCountryDetailWidget::RebuildMineEnhancementSlots()
{
	if (!MineEnhanceList) return;

	MineEnhanceList->ClearChildren();
	MineEnhanceSlots.Reset();
	if (!bSupportsMine)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> SlotClass = ResolveRowClass(EWidgetType::UpgradeRow, EWidgetType::UpgradeSlot, true);
	if (!SlotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[CountryDetailWidget] UpgradeSlot 위젯 클래스 없음 - 채광 강화 슬롯 생성 불가"));
		return;
	}

	const TArray<FMineUpgradeDefinition> Defs = TableMgr->GetAllMineUpgradeDefinitions();
	for (const FMineUpgradeDefinition& Def : Defs)
	{
		UUpgradeSlot* NewSlot = CreateWidget<UUpgradeSlot>(this, SlotClass);
		if (!NewSlot) continue;

		ApplyMineSlotDefinition(NewSlot, Def);
		BindMineSlotButton(NewSlot, Def.UpgradeType);

		AddListRow(MineEnhanceList, NewSlot);
		MineEnhanceSlots.Add(Def.UpgradeType, NewSlot);

		UpdateMineEnhancementSlot(Def.UpgradeType, NewSlot);
	}
}

void UCountryDetailWidget::ApplyMineSlotDefinition(UUpgradeSlot* InSlot, const FMineUpgradeDefinition& Def)
{
	if (!InSlot) return;

	InSlot->Description = Def.DisplayName;
	InSlot->SubDescription = Def.SubDescription;
	InSlot->ValueUnit = Def.ValueUnit;
	InSlot->bIsInteger = Def.bIsInteger;
	InSlot->CostResourceType = Def.CostResourceType;

	InSlot->Icon = Def.Icon;
	InSlot->SetIconTexture(Def.Icon.IsNull() ? nullptr : Def.Icon.LoadSynchronous());
	if (UCostActionButtonWidget* Btn = InSlot->GetUpgradeButton())
	{
		Btn->MaxLevelText = CountryDrawer::MaxLevelLabel();
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

	// 채광 강화는 모두 idle clicker 형 — 홀드 연속 강화
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

	// 라인이 없는 저장 한도는 국가 값, 설치된 라인은 실제 라인 한도를 표시한다.
	const bool bPercent = (Def.ValueUnit == TEXT("%"));
	float CurDisplay = bPercent
		? (Def.PerLevel * CurLv * 100.0f)
		: (Def.BaseValue + Def.PerLevel * CurLv);
	float NextDisplay = bPercent
		? (Def.PerLevel * (CurLv + 1) * 100.0f)
		: (Def.BaseValue + Def.PerLevel * (CurLv + 1));
	FText SlotDetail = Def.SubDescription;
	if (UpgradeType == EMineUpgradeType::MiningStorage)
	{
		const TArray<FMineLineState> Lines = MineMgr->GetMineLines(CurrentCountry);
		if (!Lines.IsEmpty())
		{
			const FMineLineState& Example = Lines[0];
			const int64 CurrentCap = MineMgr->GetEffectiveMaxStorage(CurrentCountry, Example.Resource);
			const int64 CountryCap = MineMgr->GetMaxStorage(CurrentCountry);
			const int64 NextCountryCap = static_cast<int64>(NextDisplay);
			const int64 NextCap = FMath::Min<int64>(CurrentCap + FMath::Max<int64>(0, NextCountryCap - CountryCap), NextCountryCap);
			CurDisplay = static_cast<float>(CurrentCap);
			NextDisplay = static_cast<float>(NextCap);
			bool bResourceFound = false;
			const FResourceInfo ResourceInfo = TableMgr->GetResourceInfo(Example.Resource, bResourceFound);
			if (bResourceFound && !ResourceInfo.DisplayName.IsEmpty())
			{
				SlotDetail = FText::Format(NSLOCTEXT("CountryDetail", "MineStorageLineBenefit",
					"설치한 모든 라인에 적용 · {0} {1}→{2}개"),
					ResourceInfo.DisplayName, FText::AsNumber(CurrentCap), FText::AsNumber(NextCap));
			}
		}
	}
	int32 DisplayMaxLevel = Def.MaxLevel;
	if (UpgradeType == EMineUpgradeType::LineExpansion)
	{
		const int32 ResourceCap = MineMgr->GetResourceLineCap(CurrentCountry);
		const int32 CountryMaxLevel = Def.PerLevel > 0.0f
			? FMath::Max(0, FMath::CeilToInt((ResourceCap - Def.BaseValue) / Def.PerLevel))
			: 0;
		DisplayMaxLevel = Def.MaxLevel > 0 ? FMath::Min(Def.MaxLevel, CountryMaxLevel) : CountryMaxLevel;
		CurDisplay = static_cast<float>(MineMgr->GetMaxLines(CurrentCountry));
		NextDisplay = static_cast<float>(FMath::Min(ResourceCap,
			FMath::Max(1, FMath::FloorToInt32(Def.BaseValue + Def.PerLevel * (CurLv + 1)))));
		// 공용 슬롯에서 MaxLevel=0은 무제한이므로 레벨 0에서 끝나는 국가는 배지를 직접 표시한다.
		InSlot->SetLevelBadgeOverride(DisplayMaxLevel == 0
			? FText::Format(NSLOCTEXT("CountryDetail", "MineLineCapLevel", "Lv.{0} / 0"), CurLv)
			: FText::GetEmpty());
	}
	InSlot->SetRuntimePresentation(Def.DisplayName, SlotDetail, Def.CostResourceType);

	const bool bMaxed = (UpgradeType == EMineUpgradeType::LineExpansion
			? CurLv >= DisplayMaxLevel : Def.MaxLevel > 0 && CurLv >= Def.MaxLevel)
		|| (UpgradeType == EMineUpgradeType::LineExpansion && NextDisplay <= CurDisplay);
	InSlot->UpdateInfo(CurLv, CurDisplay, NextDisplay, Cost, CountryDrawer::UpgradeSlotLocked(bMaxed),
		UpgradeType == EMineUpgradeType::LineExpansion ? FMath::Max(1, DisplayMaxLevel) : Def.MaxLevel);
	// 라인 증설은 자원 종류 상한으로도 끝난다 — 슬롯의 레벨 판정이 못 잡는 최대도 같은 회색 「최대」로 맞춘다
	if (bMaxed)
	{
		if (UCostActionButtonWidget* Button = InSlot->GetUpgradeButton())
		{
			Button->SetMaxLevelState(true);
			Button->SetLockedState(true);
		}
	}
	ApplyUpgradeLabelInk(InSlot, bMaxed);
	ApplyUpgradeMaxedValue(InSlot, bMaxed);
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

// ===== Mine 라인 위젯 → 매니저 =====

void UCountryDetailWidget::HandleMineLineClaim(UMineLineWidget* Line)
{
	if (!Line || !MineMgr) return;
	MineMgr->ClaimResource(CurrentCountry, Line->GetResource());
}

void UCountryDetailWidget::HandleMineLineInstantFinish(UMineLineWidget* Line)
{
	if (!Line || !MineMgr) return;
	if (IsInstantFinishConfirmOpen()) return;
	CloseInstantFinishConfirm();
	const ECountryType Country = CurrentCountry;
	const EResourceType Resource = Line->GetResource();
	FMineLineState QuotedState;
	if (!MineMgr->GetLineState(Country, Resource, QuotedState)) return;
	const FDateTime QuotedStartWallUtc = QuotedState.StartWallUtc;
	const int64 Cost = MineMgr->GetInstantFinishCost(Country, Resource);
	if (Cost <= 0 || MineMgr->IsInstantFinishPending(Country, Resource)) return;

	const int64 Target = MineMgr->GetEffectiveMaxStorage(Country, Resource);
	const double RatePerSec = static_cast<double>(MineMgr->GetEffectiveRatePerMinute(Country, Resource)) / 60.0;
	const double Unit01 = static_cast<double>(QuotedState.PendingFraction);
	const double UntilFullSec = RatePerSec > 0.0
		? (static_cast<double>(Target - QuotedState.CurrentQty) - Unit01) / RatePerSec : 0.0;
	UTexture2D* Icon = nullptr;
	FText Name;
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bFound = false;
		const FResourceInfo Info = TableMgr->GetResourceInfo(Resource, bFound);
		if (bFound)
		{
			Name = Info.DisplayName;
			Icon = Info.Icon.IsNull() ? nullptr : Info.Icon.LoadSynchronous();
		}
	}

	InstantFinishResource = Resource;
	OpenInstantFinishConfirm(Icon, Name, QuotedState.CurrentQty, Target, Unit01,
		FTimespan::FromSeconds(FMath::Max(FMath::FloorToDouble(UntilFullSec), 0.0)), Cost, /*bMine=*/true,
		[this, Country, Resource, Cost, QuotedStartWallUtc]()
	{
		if (!MineMgr || MineMgr->IsInstantFinishPending(Country, Resource)) return;
		FMineLineState LiveState;
		if (!MineMgr->GetLineState(Country, Resource, LiveState)
			|| LiveState.StartWallUtc != QuotedStartWallUtc
			|| MineMgr->GetInstantFinishCost(Country, Resource) <= 0) return;
		MineMgr->RequestInstantFinish(Country, Resource, Cost,
			FOnCloudEconomyOpDone::CreateWeakLambda(this, [this, Cost](bool bOk, FName Code)
		{
			if (!bOk)
			{
				if (UUIManagerSubsystem* ResultUI = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
				{
					ResultUI->NotifyCloudOpFailure(Code, EResourceType::Diamond, Cost);
				}
			}
		}));
	});
}

void UCountryDetailWidget::HandleMineLineQuantityChanged(UMineLineWidget* /*Line*/)
{
	RefreshMineHero();
}

// ===== Mine Manager 이벤트 핸들러 =====

void UCountryDetailWidget::HandleMineMgrLineCreated(ECountryType Country, FMineLineState /*LineState*/)
{
	if (Country != CurrentCountry) return;
	// 새 라인은 빈 칸 행 앞에 와야 하므로 목록을 다시 쌓는다
	RebuildMineLineList();
	RefreshMineHero();
}

void UCountryDetailWidget::HandleMineMgrLineStorageChanged(ECountryType Country, EResourceType Resource, int64 NewQty)
{
	if (Country != CurrentCountry || !MineMgr) return;
	RefreshMineHero();
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
	if (bSaturated && Resource == InstantFinishResource && IsInstantFinishConfirmOpen())
	{
		CloseInstantFinishConfirm();
	}
	RefreshMineHero();
	UMineLineWidget* Line = FindMineLineWidgetByResource(Resource);
	if (!Line) return;

	FMineLineState State;
	const int64 CurQty = MineMgr->GetLineState(Country, Resource, State) ? State.CurrentQty : 0;
	Line->UpdateStorage(CurQty, MineMgr->GetEffectiveMaxStorage(Country, Resource), bSaturated);
}

void UCountryDetailWidget::HandleMineMgrLineRemoved(ECountryType Country, EResourceType Resource)
{
	if (Country != CurrentCountry || !FindMineLineWidgetByResource(Resource)) return;
	// 지운 라인 자리가 빈 칸 「채광 시작」으로 돌아와야 하므로 목록을 다시 쌓는다
	RebuildMineLineList();
	RefreshMineHero();
}

void UCountryDetailWidget::HandleMineMgrResourceClaimed(ECountryType Country, EResourceType /*Resource*/, int64 /*ClaimedQty*/)
{
	if (Country != CurrentCountry || !MineMgr) return;
	// LineStorageChanged 가 0 으로 후속 호출되어 행 갱신은 거기서 처리. 여기서는 향후 토스트/이펙트 훅용.
}

void UCountryDetailWidget::HandleMineMgrUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 /*NewLevel*/)
{
	if (Country != CurrentCountry) return;

	// 강화 슬롯 부분 갱신 + 강화 이펙트
	if (UUpgradeSlot* TargetSlot = MineEnhanceSlots.FindRef(UpgradeType))
	{
		UpdateMineEnhancementSlot(UpgradeType, TargetSlot);
		TargetSlot->PlayUpgradeEffect();
	}

	// 채광 속도/한도/자동 수령 변경은 모든 라인과 큰 버튼 줄의 표시값에 영향
	if (!MineMgr) return;
	if (UpgradeType == EMineUpgradeType::LineExpansion)
	{
		RebuildMineLineList();
	}
	RefreshMineHero();
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

		if (bHasState)
		{
			Line->SetLineState(Country, State, LineMax, NewRateLv, EffRate);
		}
	}
}
