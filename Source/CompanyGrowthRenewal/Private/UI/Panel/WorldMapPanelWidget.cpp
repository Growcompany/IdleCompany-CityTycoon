// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorldMapPanelWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Element/WorldMap/TradeRouteWidget.h"
#include "UI/Element/WorldMap/MapIndustryTagWidget.h"
#include "UI/Element/WorldMap/MapActionBubbleWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/OverseasActions.h"
#include "Manager/WorldMapManager.h"
#include "Table/ResourceInfo.h"
#include "Enum/NotificationType.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "TimerManager.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/TradeOrderManager.h"
#include "Manager/CountryMarketManager.h"
#include "Manager/OverseasDioramaSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Global/GlobalUtilFunctions.h"
#include "Player/MainMapPlayerController.h"
#include "Table/CountryInfoTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/CountryDemandTable.h"
#include "Data/WorldMapProjection.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Utils/FWidgetAnimationUtils.h"

UWorldMapPanelWidget::UWorldMapPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 닫기 버튼 BindWidget 이 없는 패널이라 키 경로가 없으면 열고 못 닫는다.
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UWorldMapPanelWidget::GetDesiredInputConfig() const
{
	// 커서는 잡지 않는다 ― 메달리온·도크·드로어가 모두 마우스/터치로 눌려야 한다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UWorldMapPanelWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	// 헤더 X 와 같은 규칙으로 닫는다 ― 드로어를 띄운 채 패널이 먼저 닫히면 드로어만 허공에 남는다.
	HandleCloseClicked();
	// 처리했다고 알려야 같은 백 입력이 아래 위젯까지 내려가 연쇄로 닫지 않는다.
	return true;
}

FVector2D UWorldMapPanelWidget::AnchorToCanvasPosition(const FVector2D& Anchor, const FVector2D& BoardSize)
{
	return FVector2D(Anchor.X * BoardSize.X, Anchor.Y * BoardSize.Y);
}

bool UWorldMapPanelWidget::IsCountryUnlocked(int64 CurrentMarketCap, int64 RequiredMarketCap)
{
	return CurrentMarketCap >= RequiredMarketCap;
}

ECountryOwnershipRule UWorldMapPanelWidget::ResolveOwnershipRule(const FCountryInfoTable& Row)
{
	// 판정은 "지을 수 있는가" 로 한다. bIsTradeHub 는 미국에도 켜져 있어서 그걸로 가르면
	// 시설을 지을 수 있는 미국까지 해금 즉시 진출이 되고 진출 가능 단계가 통째로 사라진다.
	return (Row.bSupportsFactory || Row.bSupportsMine)
		? ECountryOwnershipRule::Facility
		: ECountryOwnershipRule::TradeHub;
}

ECountryMapState UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule Rule, int32 FacilityCount,
	int64 CurrentMarketCap, int64 RequiredMarketCap)
{
	// 허브는 시설을 못 지으므로 "진출 가능" 이라는 중간 상태가 존재할 수 없다 ― 해금되는 순간이 곧 진출이다.
	if (Rule == ECountryOwnershipRule::TradeHub)
	{
		return IsCountryUnlocked(CurrentMarketCap, RequiredMarketCap)
			? ECountryMapState::Owned
			: ECountryMapState::Locked;
	}

	if (FacilityCount >= 1)
	{
		return ECountryMapState::Owned;
	}
	return IsCountryUnlocked(CurrentMarketCap, RequiredMarketCap)
		? ECountryMapState::Available
		: ECountryMapState::Locked;
}

ECompanyType UWorldMapPanelWidget::ResolveSpecialtyIndustry(const FCountryInfoTable& Row)
{
	if (Row.RepresentativeIndustry != ECompanyType::None)
	{
		return Row.RepresentativeIndustry;
	}

	// 폴백. SupportedIndustries 는 "지원 목록" 이지 대표가 아니라서 여러 국가가 같은 색으로 수렴한다.
	for (const ECompanyType Industry : Row.SupportedIndustries)
	{
		if (Industry != ECompanyType::None)
		{
			return Industry;
		}
	}
	return ECompanyType::None;
}

FLinearColor UWorldMapPanelWidget::ResolveNoIndustryRingColor(const FCountryInfoTable& Row)
{
	return ResolveOwnershipRule(Row) == ECountryOwnershipRule::TradeHub
		? UCountryNameWidget::TradeHubRingColor()
		: UCountryNameWidget::MiningRingColor();
}

bool UWorldMapPanelWidget::IsUsableAccent(const FLinearColor& Accent)
{
	if (Accent.A < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	return !Accent.Equals(FLinearColor::White, KINDA_SMALL_NUMBER);
}

TArray<FCountryInfoTable> UWorldMapPanelWidget::FilterPlaceableRows(const TArray<FCountryInfoTable>& Rows)
{
	TArray<FCountryInfoTable> Placeable;
	Placeable.Reserve(Rows.Num());

	for (const FCountryInfoTable& Row : Rows)
	{
		if (Row.CountryType == ECountryType::None)
		{
			continue;
		}

		// (0,0) 은 "DT 에 안 채웠다"의 신호. 배치하면 전원이 판 좌상단에 쌓인다.
		if (!FWorldMapProjection::IsAnchorSet(Row.MapAnchor))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] %s 의 MapAnchor 가 비어 있어 배치에서 제외합니다. DT_CountryInfo 를 확인하세요."),
				*Row.DisplayName.ToString());
			continue;
		}

		Placeable.Add(Row);
	}

	return Placeable;
}

void UWorldMapPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 닫기·드로어 배선은 GameInstance 가드보다 **앞**이다 ― 초기화에 실패해도 창은 닫을 수 있어야 한다.
	if (DrawerStack)
	{
		DrawerStack->OnDisplayedWidgetChanged().RemoveAll(this);
		DrawerStack->OnDisplayedWidgetChanged().AddUObject(this, &UWorldMapPanelWidget::HandleDrawerChanged);
	}
	if (DrawerHost)
	{
		DrawerHost->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!DioramaLayer || !DioramaView)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMapPanel] DioramaLayer/DioramaView 가 WBP 트리에 없어 국가 디오라마 화면을 띄우지 않습니다."));
	}
	ApplyDioramaVisuals();
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UWorldMapPanelWidget::HandleCloseClicked);
		CloseButton->OnCloseClicked.AddDynamic(this, &UWorldMapPanelWidget::HandleCloseClicked);
	}
	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked().RemoveAll(this);
		ClaimAllButton->OnClicked().AddUObject(this, &UWorldMapPanelWidget::HandleClaimAllClicked);
		ClaimAllButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!SeaImage || !BubbleCanvas || !ClaimAllButton || !CloseButton)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMapPanel] 화면 가득 지도 위젯 일부가 WBP 트리에 없습니다. SeaImage=%s BubbleCanvas=%s ClaimAllButton=%s CloseButton=%s"),
			SeaImage ? TEXT("OK") : TEXT("없음"), BubbleCanvas ? TEXT("OK") : TEXT("없음"),
			ClaimAllButton ? TEXT("OK") : TEXT("없음"), CloseButton ? TEXT("OK") : TEXT("없음"));
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		// 여기서 조용히 빠지면 판은 뜨는데 메달리온이 한 개도 없다. 원인을 화면에서 읽을 수 없으므로 남긴다.
		UE_LOG(LogTemp, Warning, TEXT("[WorldMapPanel] GameInstance 가 없어 패널을 초기화하지 못했습니다."));
		return;
	}

	TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();

	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
	auto RegisterResource = [&](UResourceWidget* Widget, EResourceType Type, const TCHAR* BindName)
	{
		if (!Widget)
		{
			// Optional 바인딩이라 없어도 컴파일은 통과한다 ― 조용히 죽지 않게 여기서만 말한다.
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] %s 가 WBP 트리에 없어 이 자원은 표시되지 않습니다."), BindName);
			return;
		}
		ResourceWidgets.Add(Type, Widget);
		if (RMgr)
		{
			Widget->SetValue(RMgr->GetResourceAmount(Type));
		}
	};

	RegisterResource(UIE_Resource_Money, EResourceType::Money, TEXT("UIE_Resource_Money"));
	RegisterResource(UIE_Resource_Diamond, EResourceType::Diamond, TEXT("UIE_Resource_Diamond"));
	RegisterResource(UIE_Resource_MarketCap, EResourceType::MarketCap, TEXT("UIE_Resource_MarketCap"));

	BuildMedallions();
}

void UWorldMapPanelWidget::NativeDestruct()
{
	ResetFocus(false);
	// 비활성화 없이 트리에서 빠지는 경로(레벨 전환)에서도 라이브 촬영이 남지 않게 한다.
	ExitDiorama(true);
	UnbindDioramaDetail();
	if (DrawerStack)
	{
		DrawerStack->OnDisplayedWidgetChanged().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UWorldMapPanelWidget::HandleCloseClicked);
	}
	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked().RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BubbleEtaHandle);
	}

	ResourceWidgets.Empty();
	Medallions.Empty();
	RegionShades.Empty();
	BubbleRows.Empty();
	ClaimBubbles.Empty();
	SellBubbles.Empty();

	Super::NativeDestruct();
}

void UWorldMapPanelWidget::HandleDrawerChanged(UCommonActivatableWidget* NewWidget)
{
	if (DrawerHost)
	{
		DrawerHost->SetVisibility(NewWidget ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	bFocusLayoutPending = bHasFocusAnchor;
	if (!NewWidget)
	{
		// 드로어가 비었을 때가 곧 "돌아왔다" 다 ― 패널이 deactivate 되지 않으므로 NativeOnActivated 는 안 돈다.
		const ECountryType PendingCountry = PendingCountryAfterDrawerClose;
		ResetFocus(true);
		PendingCountryAfterDrawerClose = PendingCountry;
		ExitDiorama(false);
		UnbindDioramaDetail();
	}
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleCloseClicked()
{
	PendingCountryAfterDrawerClose = ECountryType::None;
	// 드로어를 띄운 채 패널을 닫으면 드로어만 허공에 남는다 ― 안쪽부터 닫는다.
	if (IsDrawerOpen())
	{
		if (UCommonActivatableWidget* Active = DrawerStack->GetActiveWidget())
		{
			DrawerStack->RemoveWidget(*Active);
			return;
		}
	}

	DeactivateWidget();
}

UCommonActivatableWidget* UWorldMapPanelWidget::PushDrawer(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	PendingCountryAfterDrawerClose = ECountryType::None;
	if (!DrawerStack)
	{
		if (!bDrawerMissingWarned)
		{
			bDrawerMissingWarned = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] DrawerStack 이 WBP 트리에 없어 하위 패널을 PromptStack 으로 보냅니다 ― 그동안 월드맵은 화면에서 사라집니다."));
		}
		return nullptr;
	}

	return DrawerStack->AddWidget(WidgetClass);
}

bool UWorldMapPanelWidget::IsDrawerOpen() const
{
	return DrawerStack != nullptr && DrawerStack->GetNumWidgets() > 0;
}

UCommonActivatableWidget* UWorldMapPanelWidget::GetActiveDrawerWidget() const
{
	return DrawerStack ? DrawerStack->GetActiveWidget() : nullptr;
}

void UWorldMapPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 국가 상세를 push 하면 컨테이너가 이 패널을 deactivate 해 Normal 로 내려간다. 재활성 때 아무도 되돌리지
	// 않으면 풀스크린 판 뒤로 MainMap 터치가 통과한다(빈 바다를 탭하면 뒤 건물이 선택된다).
	if (AMainMapPlayerController* PC = GetMainMapPlayerController())
	{
		if (PC->GetCurrentInputMode() != EInputMode::UI)
		{
			PC->GoToUIMode();
		}
	}

	// 상세를 닫고 돌아오면 판을 원위치로 ― 클릭 때 건 줌이 영구 고정되지 않게.
	ResetFocus(true);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
				this, &UWorldMapPanelWidget::HandleResourceChanged);
		}

		if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
		{
			MineMgr->OnLineCreated.AddDynamic(this, &UWorldMapPanelWidget::HandleMineLineCreated);
			MineMgr->OnLineRemoved.AddDynamic(this, &UWorldMapPanelWidget::HandleMineLineRemoved);
			MineMgr->OnLineSaturated.AddDynamic(this, &UWorldMapPanelWidget::HandleMineLineSaturated);
			MineMgr->OnResourceClaimed.AddDynamic(this, &UWorldMapPanelWidget::HandleMineResourceClaimed);
			MineMgr->OnUpgradeChanged.AddDynamic(this, &UWorldMapPanelWidget::HandleMineUpgradeChanged);
		}

		if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
		{
			FactoryMgr->OnLineStarted.AddDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineStarted);
			FactoryMgr->OnLineCompleted.AddDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineCompleted);
			FactoryMgr->OnLineClaimed.AddDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineClaimed);
			FactoryMgr->OnLineRemoved.AddDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineRemoved);
			FactoryMgr->OnUpgradeChanged.AddDynamic(this, &UWorldMapPanelWidget::HandleFactoryUpgradeChanged);
		}

		if (UWorldMapManager* Warehouse = GI->GetSubsystem<UWorldMapManager>())
		{
			Warehouse->OnProductAdded.AddDynamic(this, &UWorldMapPanelWidget::HandleProductAdded);
			Warehouse->OnProductConsumed.AddDynamic(this, &UWorldMapPanelWidget::HandleProductConsumed);
		}

		if (UTradeOrderManager* OrderMgr = GI->GetSubsystem<UTradeOrderManager>())
		{
			OrderMgr->OnOrderGenerated.AddDynamic(this, &UWorldMapPanelWidget::HandleOrderGenerated);
			OrderMgr->OnOrderAccepted.AddDynamic(this, &UWorldMapPanelWidget::HandleOrderAccepted);
			OrderMgr->OnOrderDismissed.AddDynamic(this, &UWorldMapPanelWidget::HandleOrderDismissed);
			OrderMgr->OnOrderCompleted.AddDynamic(this, &UWorldMapPanelWidget::HandleOrderCompleted);
			OrderMgr->OnOrderExpired.AddDynamic(this, &UWorldMapPanelWidget::HandleOrderExpired);
		}
	}

	if (UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem())
	{
		MiniatureUpdatedHandle = Diorama->OnMiniatureUpdated.AddUObject(this, &UWorldMapPanelWidget::HandleMiniatureUpdated);
	}

	RefreshAll();
	RefreshRoutes();
	RefreshActionBubbles();
	RequestUnlockedMiniatures();
}

void UWorldMapPanelWidget::NativeOnDeactivated()
{
	ResetFocus(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BubbleEtaHandle);
	}
	ExitDiorama(true);
	UnbindDioramaDetail();
	if (UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem())
	{
		Diorama->OnMiniatureUpdated.Remove(MiniatureUpdatedHandle);
	}
	MiniatureUpdatedHandle.Reset();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->OnUIResourceChanged.Remove(UIResourceChangedHandle);
		}
		UIResourceChangedHandle.Reset();

		if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
		{
			MineMgr->OnLineCreated.RemoveDynamic(this, &UWorldMapPanelWidget::HandleMineLineCreated);
			MineMgr->OnLineRemoved.RemoveDynamic(this, &UWorldMapPanelWidget::HandleMineLineRemoved);
			MineMgr->OnLineSaturated.RemoveDynamic(this, &UWorldMapPanelWidget::HandleMineLineSaturated);
			MineMgr->OnResourceClaimed.RemoveDynamic(this, &UWorldMapPanelWidget::HandleMineResourceClaimed);
			MineMgr->OnUpgradeChanged.RemoveDynamic(this, &UWorldMapPanelWidget::HandleMineUpgradeChanged);
		}

		if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
		{
			FactoryMgr->OnLineStarted.RemoveDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineStarted);
			FactoryMgr->OnLineCompleted.RemoveDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineCompleted);
			FactoryMgr->OnLineClaimed.RemoveDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineClaimed);
			FactoryMgr->OnLineRemoved.RemoveDynamic(this, &UWorldMapPanelWidget::HandleFactoryLineRemoved);
			FactoryMgr->OnUpgradeChanged.RemoveDynamic(this, &UWorldMapPanelWidget::HandleFactoryUpgradeChanged);
		}

		if (UWorldMapManager* Warehouse = GI->GetSubsystem<UWorldMapManager>())
		{
			Warehouse->OnProductAdded.RemoveDynamic(this, &UWorldMapPanelWidget::HandleProductAdded);
			Warehouse->OnProductConsumed.RemoveDynamic(this, &UWorldMapPanelWidget::HandleProductConsumed);
		}

		if (UTradeOrderManager* OrderMgr = GI->GetSubsystem<UTradeOrderManager>())
		{
			OrderMgr->OnOrderGenerated.RemoveDynamic(this, &UWorldMapPanelWidget::HandleOrderGenerated);
			OrderMgr->OnOrderAccepted.RemoveDynamic(this, &UWorldMapPanelWidget::HandleOrderAccepted);
			OrderMgr->OnOrderDismissed.RemoveDynamic(this, &UWorldMapPanelWidget::HandleOrderDismissed);
			OrderMgr->OnOrderCompleted.RemoveDynamic(this, &UWorldMapPanelWidget::HandleOrderCompleted);
			OrderMgr->OnOrderExpired.RemoveDynamic(this, &UWorldMapPanelWidget::HandleOrderExpired);
		}
	}

	if (AMainMapPlayerController* PC = GetMainMapPlayerController())
	{
		if (PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();
}

void UWorldMapPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (PendingCountryAfterDrawerClose != ECountryType::None && !IsDrawerOpen())
	{
		const ECountryType PendingCountry = PendingCountryAfterDrawerClose;
		PendingCountryAfterDrawerClose = ECountryType::None;
		HandleCountryFlagClicked(PendingCountry);
	}

	if (UCountryNameWidget* SelectedPin = Medallions.FindRef(SelectedCountry))
	{
		SelectedPin->UpdateFocusHighlight(InDeltaTime);
	}
	if (bHasFocusAnchor)
	{
		if (bFocusLayoutPending) { RefreshFocusLayout(false); }
		FSlateRect VisibleRect;
		FSlateRect SelectionBounds;
		if (MeasureFocusLayout(VisibleRect, SelectionBounds)
			&& (bFocusLayoutPending || !GetBoardSize().Equals(LastFocusBoardSize, 0.5)
				|| !FMath::IsNearlyEqual(VisibleRect.Right, LastFocusVisibleRect.Right, 0.5f)
				|| !FMath::IsNearlyEqual(VisibleRect.Bottom, LastFocusVisibleRect.Bottom, 0.5f)
				|| !FVector2D(SelectionBounds.Left, SelectionBounds.Top).Equals(
					FVector2D(LastFocusSelectionBounds.Left, LastFocusSelectionBounds.Top), 0.5)
				|| !FVector2D(SelectionBounds.Right, SelectionBounds.Bottom).Equals(
					FVector2D(LastFocusSelectionBounds.Right, LastFocusSelectionBounds.Bottom), 0.5)))
		{
			RefreshFocusLayout(false);
		}
	}

	TickDiorama(InDeltaTime);

	// 포커스 애니 0.25초 동안만 일한다. 메달리온 배치는 생성 때 한 번이라 매 프레임 계산이 없다.
	if (FocusDuration <= 0.0f)
	{
		return;
	}

	FocusElapsed = FMath::Min(FocusElapsed + InDeltaTime, FocusDuration);
	const float EaseAlpha = FMath::InterpEaseOut(0.0f, 1.0f, FocusElapsed / FocusDuration, 2.0f);

	ApplyFocusTransform(
		FMath::Lerp(FocusStartTranslation, FocusTargetTranslation, static_cast<double>(EaseAlpha)),
		FMath::Lerp(FocusStartScale, FocusTargetScale, EaseAlpha));

	if (FocusElapsed >= FocusDuration)
	{
		FocusDuration = 0.0f;
	}
}

AMainMapPlayerController* UWorldMapPanelWidget::GetMainMapPlayerController() const
{
	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		return Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
	}
	return nullptr;
}

void UWorldMapPanelWidget::BuildMedallions()
{
	if (!MedallionCanvas || !TableMgr)
	{
		// 둘 중 하나만 없어도 판 위에 국가가 한 개도 안 뜬다. 화면으로는 원인을 구분할 수 없다.
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMapPanel] 메달리온을 만들지 못했습니다. MedallionCanvas=%s TableManager=%s"),
			MedallionCanvas ? TEXT("OK") : TEXT("없음"),
			TableMgr ? TEXT("OK") : TEXT("없음"));
		return;
	}

	SetSelectedCountry(ECountryType::None);
	MedallionCanvas->ClearChildren();
	Medallions.Empty();
	if (BubbleCanvas)
	{
		BubbleCanvas->ClearChildren();
	}
	BubbleRows.Empty();
	ClaimBubbles.Empty();
	SellBubbles.Empty();
	AccentWarnedCountries.Empty();
	RepresentativeWarnedCountries.Empty();
	if (RegionShadeCanvas)
	{
		RegionShadeCanvas->ClearChildren();
	}
	RegionShades.Empty();

	const TSubclassOf<UUserWidget> MedallionClass = TableMgr->GetWidgetClass(EWidgetType::CountryName);
	if (!MedallionClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMapPanel] EWidgetType::CountryName 위젯 클래스를 못 찾아 메달리온을 만들지 못했습니다."));
		return;
	}

	const TArray<FCountryInfoTable> PlaceableRows = FilterPlaceableRows(TableMgr->GetAllCountryInfos());

	for (const FCountryInfoTable& Row : PlaceableRows)
	{
		UCountryNameWidget* Pin = CreateWidget<UCountryNameWidget>(this, MedallionClass);
		if (!Pin)
		{
			continue;
		}

		Pin->SetCountryType(Row.CountryType);
		Pin->SetAnchorSide(Row.LabelSide);
		Pin->SetCountryName(Row.DisplayName);
		// 국기는 원반에 싣지 않는다 ― 사각 텍스처가 원형 림을 뚫고 나오고, DT 에는 11행 중 1행만
		// 채워져 있어 한 나라만 그림이 붙는다. 식별은 원반 안 2글자 코드(CodeText)가 맡는다.

		// 재사용·동적 위젯은 매 생성마다 재바인딩한다 (feedback_dynamic_widget_rebind)
		Pin->OnFlagClicked.BindUObject(this, &UWorldMapPanelWidget::HandleCountryFlagClicked);
		Pin->OnFlagRejected.BindUObject(this, &UWorldMapPanelWidget::HandleCountryFlagRejected);

		// 정규화 앵커를 슬롯 앵커로 그대로 넘긴다 ― 픽셀로 환산하면 가로 가상폭 2304~3120 에서 어긋나고,
		// NativeConstruct 시점의 캔버스 지오메트리는 아직 0 이라 전원이 좌상단에 쌓인다.
		if (UCanvasPanelSlot* PinSlot = MedallionCanvas->AddChildToCanvas(Pin))
		{
			PinSlot->SetAnchors(FAnchors(static_cast<float>(Row.MapAnchor.X), static_cast<float>(Row.MapAnchor.Y)));
			PinSlot->SetAlignment(Pin->GetAnchorAlignment());
			PinSlot->SetPosition(FVector2D::ZeroVector);
			PinSlot->SetAutoSize(true);
		}
		Medallions.Add(Row.CountryType, Pin);

		if (!RegionShadeCanvas || !WidgetTree)
		{
			continue;
		}

		UImage* Shade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (!Shade)
		{
			continue;
		}

		// 마스크는 판 전체 크기(2048x833)라 판 사각형에 그대로 늘려 덮는다.
		UTexture2D* MaskTexture = Row.RegionMask.IsNull() ? nullptr : Row.RegionMask.LoadSynchronous();
		if (MaskTexture)
		{
			Shade->SetBrushFromTexture(MaskTexture, false);
		}
		else
		{
			// 마스크가 없으면 판을 못 덮는다. 상태는 링·상태 줄·자물쇠로 계속 전달되지만
			// 판만 보면 잠긴 나라가 진출국처럼 밝다. 매 갱신마다 짖지 않게 생성 시 한 번만 남긴다.
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] %s 의 RegionMask 가 없어 영역 덮개를 그리지 못합니다. DT_CountryInfo 와 쿠킹 폴더 등록을 확인하세요."),
				*Row.DisplayName.ToString());
		}
		Shade->SetVisibility(ESlateVisibility::Collapsed);

		if (UCanvasPanelSlot* ShadeSlot = RegionShadeCanvas->AddChildToCanvas(Shade))
		{
			ShadeSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			ShadeSlot->SetOffsets(FMargin(0.0f));
			ShadeSlot->SetAlignment(FVector2D::ZeroVector);
		}
		RegionShades.Add(Row.CountryType, Shade);
	}

	if (RouteLayer)
	{
		// 항로 끝점도 메달리온과 같은 정규화 앵커를 쓴다 ― 판 크기는 항로 레이어가 자기 지오메트리에서 실측한다.
		TMap<ECountryType, FVector2D> PlacedAnchors;
		PlacedAnchors.Reserve(PlaceableRows.Num());
		for (const FCountryInfoTable& Row : PlaceableRows)
		{
			PlacedAnchors.Add(Row.CountryType, Row.MapAnchor);
		}
		RouteLayer->SetCountryAnchors(PlacedAnchors);
	}

	RefreshAll();
	RefreshRoutes();
}

void UWorldMapPanelWidget::RefreshRoutes()
{
	if (!RouteLayer)
	{
		return;
	}

	// 생산 항로(공장 나라 → 가장 가까운 무역항)는 주문과 무관하다 ― 주문 매니저 검사보다 먼저 채운다.
	if (TableMgr)
	{
		const int64 ProductionMarketCap = GetCurrentMarketCap();
		TArray<FProductionRouteInput> ProductionInputs;
		ProductionInputs.Reserve(Medallions.Num());
		for (const TPair<ECountryType, TObjectPtr<UCountryNameWidget>>& Entry : Medallions)
		{
			bool bRowOk = false;
			const FCountryInfoTable Row = TableMgr->GetCountryInfo(Entry.Key, bRowOk);
			if (!bRowOk)
			{
				continue;
			}
			FProductionRouteInput Input;
			Input.Country = Row.CountryType;
			Input.Anchor = Row.MapAnchor;
			Input.bFactory = Row.bSupportsFactory;
			Input.bHub = Row.bIsTradeHub;
			Input.bUnlocked = ResolveCountryState(ResolveOwnershipRule(Row), CountFacilities(Row.CountryType),
				ProductionMarketCap, Row.RequiredMarketCap) != ECountryMapState::Locked;
			ProductionInputs.Add(Input);
		}
		RouteLayer->SetProductionRoutes(ProductionInputs);
	}
	else
	{
		RouteLayer->SetProductionRoutes({});
	}

	UGameInstance* GI = GetGameInstance();
	UTradeOrderManager* OrderMgr = GI ? GI->GetSubsystem<UTradeOrderManager>() : nullptr;
	UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
	UCountryMarketManager* MarketMgr = GI ? GI->GetSubsystem<UCountryMarketManager>() : nullptr;
	if (!OrderMgr || !FactoryMgr || !MarketMgr || !TableMgr)
	{
		RouteLayer->SetRoutes({});
		RouteLayer->SetIdleRoutes({});
		return;
	}

	// 배치된 국가만 후보다. FilterPlaceableRows 를 다시 부르면 미기입 경고가 주문 이벤트마다 쏟아진다.
	TArray<FCountryInfoTable> Candidates;
	Candidates.Reserve(Medallions.Num());
	for (const TPair<ECountryType, TObjectPtr<UCountryNameWidget>>& Entry : Medallions)
	{
		bool bRowOk = false;
		const FCountryInfoTable Row = TableMgr->GetCountryInfo(Entry.Key, bRowOk);
		if (bRowOk)
		{
			Candidates.Add(Row);
		}
	}

	const int64 MarketCap = GetCurrentMarketCap();

	auto GetProducers = [&](ECompanyType Industry)
	{
		TArray<FCountryProduction> Out;
		for (const FCountryInfoTable& Row : Candidates)
		{
			if (Row.SupportedIndustries.Contains(Industry))
			{
				Out.Add({ Row.CountryType, FactoryMgr->GetActiveLineCount(Row.CountryType) });
			}
		}
		return Out;
	};

	auto GetDemands = [&](ECompanyType Industry)
	{
		TArray<FCountryDemand> Out;
		for (const FCountryInfoTable& Row : Candidates)
		{
			// 잠긴(덮개로 가린) 나라로 선이 뻗으면 판이 "여기로 실어라"라고 거짓말한다.
			if (ResolveCountryState(ResolveOwnershipRule(Row), CountFacilities(Row.CountryType),
					MarketCap, Row.RequiredMarketCap)
				== ECountryMapState::Locked)
			{
				continue;
			}

			// 미정의 셀은 GetDemandRatio 가 1.0 을 돌려준다 ― 거르지 않으면 시장이 아예 없는 나라가 1등을 독식한다.
			bool bCellFound = false;
			MarketMgr->GetMarketState(Row.CountryType, Industry, bCellFound);
			if (!bCellFound)
			{
				continue;
			}

			// 초반에는 모든 셀이 만수요라 비율이 전부 1.0 이다. 그 구간의 순위를 enum 순서가 아니라
			// 실제 단가로 가르라고 PriceMul 을 같이 넘긴다 (ResolveRoute 의 2순위 축).
			bool bPriceOk = false;
			const FCountryDemandTable DemandRow = TableMgr->GetCountryDemand(Row.CountryType, Industry, bPriceOk);
			Out.Add({ Row.CountryType,
				MarketMgr->GetDemandRatio(Row.CountryType, Industry),
				bPriceOk ? DemandRow.PriceMul : 1.0f });
		}
		return Out;
	};

	const TArray<FTradeOrder> Orders = OrderMgr->GetActiveOrders();
	RouteLayer->SetRoutes(UTradeRouteWidget::BuildRoutes(Orders, GetProducers, GetDemands));
	RouteLayer->SetIdleRoutes(UTradeRouteWidget::BuildIdleRoutes(Orders, GetProducers, GetDemands));

	// 선이 0개면 앵커 검증도 그릴 게 없어 침묵한다 ― 가장 헷갈리는 상태가 로그에 흔적조차 안 남는다.
	// 산업별 후보 수를 실어 보내 원인을 생산/도착 중에서 고를 수 있게 한다. (경고는 위젯이 1회로 묶는다)
	if (Orders.Num() > 0 && RouteLayer->GetRoutes().Num() == 0 && RouteLayer->GetIdleRoutes().Num() == 0)
	{
		TSet<ECompanyType> OrderedIndustries;
		for (const FTradeOrder& Order : Orders)
		{
			OrderedIndustries.Add(Order.CompanyType);
		}

		FString Detail = FString::Printf(TEXT(" 배치국 %d."), Candidates.Num());
		for (const ECompanyType Industry : OrderedIndustries)
		{
			Detail += FString::Printf(TEXT(" [%s 생산후보 %d · 도착후보 %d]"),
				*CompanyTypeToString(Industry), GetProducers(Industry).Num(), GetDemands(Industry).Num());
		}
		RouteLayer->ReportNoRoutes(Orders.Num(), Detail);
	}
}

void UWorldMapPanelWidget::RefreshAll()
{
	// 배치된 국가만 돈다 ― DT 를 다시 거르면 미기입 경고가 시총이 오를 때마다 쏟아진다.
	TArray<ECountryType> PlacedCountries;
	Medallions.GetKeys(PlacedCountries);

	for (const ECountryType Country : PlacedCountries)
	{
		RefreshCountry(Country);
	}
}

void UWorldMapPanelWidget::RefreshCountry(ECountryType Country)
{
	if (!TableMgr || Country == ECountryType::None)
	{
		return;
	}

	bool bInfoOk = false;
	const FCountryInfoTable Row = TableMgr->GetCountryInfo(Country, bInfoOk);
	if (!bInfoOk)
	{
		return;
	}

	ApplyCountryState(Row);
}

void UWorldMapPanelWidget::ApplyCountryState(const FCountryInfoTable& Row)
{
	// ★ 상태는 국가당 한 번만 계산한다. 링 색·덮개 알파·자물쇠가 전부 이 지역 변수에서 파생된다.
	const ECountryMapState MapState = ResolveCountryState(
		ResolveOwnershipRule(Row), CountFacilities(Row.CountryType), GetCurrentMarketCap(), Row.RequiredMarketCap);

	if (const TObjectPtr<UCountryNameWidget>* FoundPin = Medallions.Find(Row.CountryType))
	{
		if (UCountryNameWidget* Pin = FoundPin->Get())
		{
			Pin->SetRingColor(UCountryNameWidget::ResolveRingColor(MapState, ResolveSpecialtyAccent(Row)));
			Pin->SetStatusLine(BuildStatusLine(Row, MapState));
			// 사유 숫자는 상태 줄이 이미 말한다 ― LockText 에 같은 값을 또 넣으면 메달리온에 숫자가 두 번 뜬다.
			// 여기서는 자물쇠 아이콘과 어두운 틴트만 켠다.
			Pin->SetLockState(MapState == ECountryMapState::Locked, FText::GetEmpty());
			Pin->SetAlertVisible(MapState == ECountryMapState::Owned && HasPendingAttention(Row.CountryType));
			// 잠긴 나라는 해금 조건(상태 줄)을 말하고 담당 산업은 미리 흘리지 않는다 ― 링 색과 같은 규칙.
			const bool bShowTags = MapState != ECountryMapState::Locked;
			bool bRefinery = false;
			if (bShowTags && Row.bSupportsMine && MapIndustryTags::IsAllOil(Row.MinableResources))
			{
				// 정유 = 디오라마와 같은 외형 단계(유전 광산의 저장 3단계). 국가 이름으로 가르지 않는다.
				FDioramaSpecInputs Inputs;
				bRefinery = OverseasDiorama::GatherInputs(GetGameInstance(), Row.CountryType, Inputs) && Inputs.MineTiers[1] >= 3;
			}
			Pin->SetIndustryTags(bShowTags ? MapIndustryTags::BuildSpecs(Row, bRefinery) : TArray<FMapIndustryTagSpec>());
			ApplyMiniature(*Pin, Row.CountryType, MapState != ECountryMapState::Locked);
			PlaceBubbleRow(Row.CountryType);
			if (SelectedCountry == Row.CountryType) { bFocusLayoutPending = true; }
		}
	}

	const TObjectPtr<UImage>* FoundShade = RegionShades.Find(Row.CountryType);
	UImage* Shade = FoundShade ? FoundShade->Get() : nullptr;
	const bool bMaskValid = (Shade != nullptr) && (Shade->GetBrush().GetResourceObject() != nullptr);
	const float ShadeAlpha = UCountryNameWidget::ResolveRegionShadeAlpha(MapState, bMaskValid);

	if (Shade)
	{
		FLinearColor ShadeColor = RegionShadeTint;
		ShadeColor.A = ShadeAlpha;
		Shade->SetColorAndOpacity(ShadeColor);
		// 마스크가 없으면 덮개를 아예 접는다 ― 브러시가 빈 UImage 는 판 전체를 흰 사각형으로 덮는다.
		// 덮개는 판 위 장식이라 보일 때도 메달리온 클릭을 뺏으면 안 된다.
		Shade->SetVisibility((bMaskValid && ShadeAlpha > KINDA_SMALL_NUMBER)
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

FLinearColor UWorldMapPanelWidget::ResolveSpecialtyAccent(const FCountryInfoTable& Row)
{
	const ECompanyType Specialty = ResolveSpecialtyIndustry(Row);

	// 산업 행이 아예 없는 국가(채광 전담 5개국·무역 허브)는 설계상 코드 색이다 ― 조달 실패가 아니다.
	if (Specialty == ECompanyType::None)
	{
		return ResolveNoIndustryRingColor(Row);
	}

	// 대표 산업을 안 채우면 지원 목록 첫 원소로 떨어지는데, 그 순서는 대표를 뜻하지 않아
	// 여러 국가가 같은 색으로 수렴한다. 조용히 넘기면 "왜 다 앰버지" 를 나중에 아무도 못 찾는다.
	// 단, 후보가 하나뿐이면 폴백이 유일해 모호하지 않다 ― 그건 경고할 일이 아니다(오경보가 되면 아무도 안 본다).
	int32 CandidateCount = 0;
	for (const ECompanyType Industry : Row.SupportedIndustries)
	{
		if (Industry != ECompanyType::None)
		{
			++CandidateCount;
		}
	}

	if (Row.RepresentativeIndustry == ECompanyType::None && CandidateCount > 1
		&& !RepresentativeWarnedCountries.Contains(Row.CountryType))
	{
		RepresentativeWarnedCountries.Add(Row.CountryType);
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMapPanel] %s 의 RepresentativeIndustry 가 비어 있어 SupportedIndustries 첫 원소로 대체합니다. DT_CountryInfo 를 확인하세요."),
			*Row.DisplayName.ToString());
	}

	bool bCompanyOk = false;
	const FCompanyInfoTable CompanyInfo = TableMgr ? TableMgr->GetCompanyInfo(Specialty, bCompanyOk) : FCompanyInfoTable();
	if (!bCompanyOk || !IsUsableAccent(CompanyInfo.AccentColor))
	{
		// 흰색·투명 accent 를 그대로 넘기면 진출국만 틀리고 나머지 두 상태는 멀쩡해 보인다 ― 그래서 여기서 잡는다.
		// 갱신마다 반복되지 않게 국가당 1회만 남긴다(다른 경고들과 같은 규칙).
		if (!AccentWarnedCountries.Contains(Row.CountryType))
		{
			AccentWarnedCountries.Add(Row.CountryType);
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] %s 의 산업 강조색을 못 구했습니다. DT_CompanyInfo 의 AccentColor 를 확인하세요. 산업 없는 국가용 색으로 대체합니다."),
				*Row.DisplayName.ToString());
		}
		return ResolveNoIndustryRingColor(Row);
	}

	return CompanyInfo.AccentColor;
}

FText UWorldMapPanelWidget::BuildStatusLine(const FCountryInfoTable& Row, ECountryMapState MapState) const
{
	if (MapState == ECountryMapState::Available)
	{
		return NSLOCTEXT("WorldMap", "CountryAvailable", "진출 가능");
	}

	if (MapState == ECountryMapState::Locked)
	{
		// 라벨판 폭 예산은 172.8px 뿐이다(§14 ― 일본 자리에서 16:10 판 오른쪽까지 남는 거리 x2).
		// AbbreviateNumber 는 값이 아무리 커져도 5자를 넘지 않아(UI_TYPOGRAPHY §5) 이 줄의 상한이
		// `시총 9990만`=151px 로 묶인다 ― 그래서 이게 렌더 하네스의 스트레스 픽스처다. 해금 요구치라 Ceil.
		return FText::Format(
			NSLOCTEXT("WorldMap", "CountryLockedStatus", "시총 {0}"),
			UGlobalUtilFunctions::AbbreviateNumber(
				Row.RequiredMarketCap, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil));
	}

	// 허브는 셀 시설이 없다. 표시할 값은 DT 가 들고 있는 특산 이름뿐이고, 비어 있으면 상태 줄도 비운다
	// (코드 폴백으로 덮지 않는다 ― 빈 셀은 "의도적 없음" 이다).
	if (ResolveOwnershipRule(Row) == ECountryOwnershipRule::TradeHub)
	{
		return Row.SpecialtyName;
	}

	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return FText::GetEmpty();
	}

	int32 MineCount = 0;
	int32 MineMax = 0;
	int32 FactoryCount = 0;
	int32 FactoryMax = 0;
	if (const UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
	{
		MineCount = MineMgr->GetActiveLineCount(Row.CountryType);
		MineMax = MineCount > 0 ? MineMgr->GetMaxLines(Row.CountryType) : 0;
	}
	if (const UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryCount = FactoryMgr->GetActiveLineCount(Row.CountryType);
		FactoryMax = FactoryCount > 0 ? FactoryMgr->GetMaxLines(Row.CountryType) : 0;
	}

	// 상태 줄은 **한 줄** 이어야 한다. 두 줄이 되면 라벨판이 31px 씩 자라고, 16:10(판 702px)에서
	// 독일 라벨은 판 위로 · 호주 라벨은 판 아래로 넘친다(§14 라벨 기하). 그래서 두 시설을 각각 적지 않는다 ―
	// "채광 2/3 · 공장 1/2" 은 236px 라 일본 자리(판 오른쪽 86px 여유)에서 판 밖으로 나간다.
	// 문법은 목업과 같은 `<무엇> <현재>/<상한>` 하나로 고정하고, 둘 다 돌 때만 합쳐 부른다(내역은 국가 패널에 있다).
	if (MineCount > 0 && FactoryCount > 0)
	{
		return FText::FromString(FString::Printf(TEXT("시설 %d/%d"),
			MineCount + FactoryCount, MineMax + FactoryMax));
	}
	if (MineCount > 0)
	{
		return FText::FromString(FString::Printf(TEXT("채광 %d/%d"), MineCount, MineMax));
	}
	if (FactoryCount > 0)
	{
		return FText::FromString(FString::Printf(TEXT("공장 %d/%d"), FactoryCount, FactoryMax));
	}

	return FText::GetEmpty();
}

int32 UWorldMapPanelWidget::CountFacilities(ECountryType Country) const
{
	// 말풍선·서랍의 잠금 판정과 같은 식을 쓴다.
	return OverseasActions::CountFacilities(GetGameInstance(), Country);
}

bool UWorldMapPanelWidget::HasPendingAttention(ECountryType Country) const
{
	return OverseasActions::CountPending(GetGameInstance(), Country) > 0;
}

int64 UWorldMapPanelWidget::GetCurrentMarketCap() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			return RMgr->GetResourceAmount(EResourceType::MarketCap);
		}
	}
	return 0;
}

FVector2D UWorldMapPanelWidget::DesignBoardSize()
{
	// 화면 가득 지도의 대륙 사각형(2560 디자인 캔버스 기준, 시안 layoutMap full).
	return FVector2D(2400.0, 976.0);
}

void UWorldMapPanelWidget::ComputeFocusTransform(const FVector2D& Anchor, const FVector2D& BoardSize,
	const FSlateRect& VisibleRect, const FSlateRect& SelectionBounds,
	FVector2D& OutTranslation, float& OutScale, const FVector2D& BoardOrigin, const FVector2D& CoverScale)
{
	constexpr double Padding = 16.0;
	const FVector2D ViewMin(VisibleRect.Left, VisibleRect.Top);
	const FVector2D ViewMax(VisibleRect.Right, VisibleRect.Bottom);
	const FVector2D SelectionMin(SelectionBounds.Left, SelectionBounds.Top);
	const FVector2D SelectionMax(SelectionBounds.Right, SelectionBounds.Bottom);
	const FVector2D Center = BoardSize * 0.5;
	const FVector2D AnchorPixels = AnchorToCanvasPosition(Anchor, BoardSize);
	const FVector2D SelectionSize = SelectionMax - SelectionMin;
	const FVector2D ViewSize = ViewMax - ViewMin;
	const double FitScale = FMath::Min(
		(ViewSize.X - Padding * 2.0) / FMath::Max(SelectionSize.X, 1.0),
		(ViewSize.Y - Padding * 2.0) / FMath::Max(SelectionSize.Y, 1.0));
	OutScale = static_cast<float>(FMath::Clamp(FitScale, 0.1, static_cast<double>(FocusZoom)));

	// 판 점 p 의 화면 위치 = BoardOrigin + Center + (p - Center) * Scale + Translation.
	const FVector2D Desired = (ViewMin + ViewMax) * 0.5 - BoardOrigin - Center
		- (AnchorPixels - Center) * OutScale;
	const FVector2D CoverHalf = Center * CoverScale * OutScale;
	const FVector2D CoverMin = ViewMax - BoardOrigin - Center - CoverHalf;
	const FVector2D CoverMax = ViewMin - BoardOrigin - Center + CoverHalf;
	const FVector2D FitMin = ViewMin + FVector2D(Padding) - BoardOrigin - Center - (SelectionMin - Center) * OutScale;
	const FVector2D FitMax = ViewMax - FVector2D(Padding) - BoardOrigin - Center - (SelectionMax - Center) * OutScale;

	const auto ClampAxis = [](double DesiredAxis, double CoverLow, double CoverHigh, double FitLow, double FitHigh)
	{
		const double Low = FMath::Max(CoverLow, FitLow);
		const double High = FMath::Min(CoverHigh, FitHigh);
		// 극단적으로 좁은 창에서는 두 제약을 동시에 만족할 수 없다. 그때도 선택 라벨은 자르지 않는다.
		return Low <= High ? FMath::Clamp(DesiredAxis, Low, High)
			: FMath::Clamp(DesiredAxis, FitLow, FMath::Max(FitLow, FitHigh));
	};
	OutTranslation = FVector2D(
		ClampAxis(Desired.X, CoverMin.X, CoverMax.X, FitMin.X, FitMax.X),
		ClampAxis(Desired.Y, CoverMin.Y, CoverMax.Y, FitMin.Y, FitMax.Y));
}

void UWorldMapPanelWidget::SetSelectedCountry(ECountryType Country)
{
	if (UCountryNameWidget* PreviousPin = Medallions.FindRef(SelectedCountry))
	{
		PreviousPin->SetFocusHighlight(false);
	}
	SelectedCountry = Country;
	if (UCountryNameWidget* SelectedPin = Medallions.FindRef(SelectedCountry))
	{
		SelectedPin->SetFocusHighlight(true);
	}
}

bool UWorldMapPanelWidget::MeasureFocusLayout(FSlateRect& OutVisibleRect, FSlateRect& OutSelectionBounds) const
{
	const FVector2D BoardSize = GetBoardSize();
	const FVector2D AnchorPixels = AnchorToCanvasPosition(FocusAnchor, BoardSize);
	OutVisibleRect = FSlateRect(FVector2D::ZeroVector, BoardSize);
	OutSelectionBounds = FSlateRect(AnchorPixels, AnchorPixels);
	if (!MapViewport || !MapTable)
	{
		return true;
	}

	const FGeometry& ViewGeometry = MapViewport->GetCachedGeometry();
	const FVector2D ViewSize = ViewGeometry.GetLocalSize();
	const FGeometry& TableGeometry = MapTable->GetCachedGeometry();
	if (ViewSize.X <= 1.0 || ViewSize.Y <= 1.0 || TableGeometry.GetLocalSize().X <= 1.0)
	{
		return false;
	}
	OutVisibleRect = FSlateRect(FVector2D::ZeroVector, ViewSize);
	// 화면 가득 지도에서는 창이 상단 바 밑까지 올라간다 ― 선택 국가는 바 아래에 둔다.
	if (SeaImage)
	{
		OutVisibleRect.Top = FMath::Min(MapTopBarHeight, static_cast<float>(ViewSize.Y) * 0.5f);
	}
	if (IsDrawerOpen() && DrawerHost)
	{
		const FGeometry& DrawerGeometry = DrawerHost->GetCachedGeometry();
		const FVector2D DrawerSize = DrawerGeometry.GetLocalSize();
		if (DrawerSize.X <= 1.0 || DrawerSize.Y <= 1.0) { return false; }
		const FVector2D DrawerMin = ViewGeometry.AbsoluteToLocal(DrawerGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D DrawerMax = ViewGeometry.AbsoluteToLocal(DrawerGeometry.LocalToAbsolute(DrawerSize));
		if (DrawerMax.Y > 0.0 && DrawerMin.Y < ViewSize.Y)
		{
			OutVisibleRect.Right = static_cast<float>(FMath::Clamp(DrawerMin.X, 0.0, ViewSize.X));
		}
	}
	if (OutVisibleRect.GetSize2f().X <= 32.0f || OutVisibleRect.GetSize2f().Y <= 32.0f) { return false; }

	if (UCountryNameWidget* SelectedPin = Medallions.FindRef(SelectedCountry))
	{
		FSlateRect LocalBounds;
		const UCanvasPanelSlot* PinSlot = Cast<UCanvasPanelSlot>(SelectedPin->Slot);
		if (!PinSlot || !SelectedPin->GetFocusVisualBounds(LocalBounds)) { return false; }
		const FVector2D PinOrigin = BoardSize * FVector2D(PinSlot->GetAnchors().Minimum)
			+ PinSlot->GetPosition() - SelectedPin->GetDesiredSize() * PinSlot->GetAlignment();
		OutSelectionBounds = FSlateRect(PinOrigin + FVector2D(LocalBounds.Left, LocalBounds.Top),
			PinOrigin + FVector2D(LocalBounds.Right, LocalBounds.Bottom));
	}
	return true;
}

void UWorldMapPanelWidget::RefreshFocusLayout(bool bAnimate)
{
	if (UCountryNameWidget* SelectedPin = Medallions.FindRef(SelectedCountry))
	{
		SelectedPin->ForceLayoutPrepass();
	}
	FSlateRect VisibleRect;
	FSlateRect SelectionBounds;
	bFocusLayoutPending = !MeasureFocusLayout(VisibleRect, SelectionBounds);
	if (bFocusLayoutPending) { return; }
	LastFocusBoardSize = GetBoardSize();
	LastFocusVisibleRect = VisibleRect;
	LastFocusSelectionBounds = SelectionBounds;
	FVector2D TargetTranslation;
	float TargetScale = 1.0f;
	ComputeFocusTransform(FocusAnchor, LastFocusBoardSize, VisibleRect, SelectionBounds, TargetTranslation, TargetScale,
		MeasureBoardOrigin(), GetSeaCoverScale());
	StartFocusTween(TargetTranslation, TargetScale, bAnimate);
}

FVector2D UWorldMapPanelWidget::GetBoardSize() const
{
	if (MapBaseImage)
	{
		const FVector2D Measured = MapBaseImage->GetCachedGeometry().GetLocalSize();
		if (Measured.X > 1.0 && Measured.Y > 1.0)
		{
			return Measured;
		}
	}
	// 아직 한 번도 레이아웃되지 않아 지오메트리가 0 인 프레임.
	return DesignBoardSize();
}

void UWorldMapPanelWidget::ApplyFocusTransform(const FVector2D& Translation, float Scale)
{
	// MapTable 이 없어도 적용 상태는 기록한다 ― 그래야 "지금 판이 어떤 변형인가" 를 위젯 밖에서 읽을 수 있다.
	AppliedFocusTranslation = Translation;
	AppliedFocusScale = Scale;

	if (!MapTable)
	{
		return;
	}
	MapTable->SetRenderTranslation(Translation);
	MapTable->SetRenderScale(FVector2D(Scale, Scale));
}

void UWorldMapPanelWidget::StartFocusTween(const FVector2D& InTargetTranslation, float InTargetScale, bool bAnimate)
{
	// 이동량 수식이 "피벗 = 판 중앙" 을 전제하므로 변형을 거는 쪽이 피벗도 책임진다.
	if (MapTable)
	{
		MapTable->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	}

	FocusStartTranslation = AppliedFocusTranslation;
	FocusStartScale = AppliedFocusScale;
	FocusTargetTranslation = InTargetTranslation;
	FocusTargetScale = InTargetScale;
	FocusElapsed = 0.0f;

	// 이미 목표면 트윈을 돌리지 않는다 ― 매 재활성마다 0.25초짜리 무의미한 Tick 이 도는 것을 막는다.
	const bool bAlreadyThere =
		FocusStartTranslation.Equals(InTargetTranslation, 0.01)
		&& FMath::IsNearlyEqual(FocusStartScale, InTargetScale, 1e-3f);

	if (!bAnimate || bAlreadyThere || FMotionGate::IsReduced())
	{
		FocusDuration = 0.0f;
		ApplyFocusTransform(InTargetTranslation, InTargetScale);
		return;
	}

	FocusDuration = FocusAnimDuration;
}

void UWorldMapPanelWidget::FocusOnAnchorImmediate(const FVector2D& Anchor)
{
	SetSelectedCountry(ECountryType::None);
	FocusAnchor = Anchor;
	bHasFocusAnchor = true;
	RefreshFocusLayout(false);
}

void UWorldMapPanelWidget::FocusOnCountry(ECountryType Country, bool bAnimate)
{
	if (!TableMgr || Country == ECountryType::None) { return; }
	bool bInfoOk = false;
	const FCountryInfoTable Row = TableMgr->GetCountryInfo(Country, bInfoOk);
	if (!bInfoOk || !FWorldMapProjection::IsAnchorSet(Row.MapAnchor)) { return; }

	SetSelectedCountry(Country);
	FocusAnchor = Row.MapAnchor;
	bHasFocusAnchor = true;
	RefreshFocusLayout(bAnimate);
}

void UWorldMapPanelWidget::ResetFocus(bool bAnimate)
{
	PendingCountryAfterDrawerClose = ECountryType::None;
	SetSelectedCountry(ECountryType::None);
	bHasFocusAnchor = false;
	bFocusLayoutPending = false;
	StartFocusTween(FVector2D::ZeroVector, 1.0f, bAnimate);
}

void UWorldMapPanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (TObjectPtr<UResourceWidget>* FoundWidget = ResourceWidgets.Find(Type))
	{
		if (UResourceWidget* Widget = FoundWidget->Get())
		{
			Widget->SetValue(NewValue);
		}
	}

	// 시가총액이 오르면 그 순간 해금되는 국가가 생긴다 ― 패널을 연 채로도 갱신돼야 한다.
	if (Type == EResourceType::MarketCap)
	{
		RefreshAll();
		// 해금은 도착지 후보를 늘린다(잠긴 나라는 항로 도착지가 아니다).
		RefreshRoutes();
		// 무역항 해금은 판매 말풍선 후보를 바꾼다.
		RequestBubbleRefresh();
	}
}

void UWorldMapPanelWidget::HandleMineLineCreated(ECountryType Country, FMineLineState LineState)
{
	RefreshCountry(Country);
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleMineLineRemoved(ECountryType Country, EResourceType Resource)
{
	RefreshCountry(Country);
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleMineLineSaturated(ECountryType Country, EResourceType Resource, bool bIsSaturated)
{
	RefreshCountry(Country);
	RequestBubbleRefresh();
}

// 공장 가동 라인 수가 곧 항로의 출발지 선정 기준이라, 라인이 늘고 줄면 선의 시작점이 바뀐다.
void UWorldMapPanelWidget::HandleFactoryLineStarted(ECountryType Country, FWorldFactoryLineState LineState)
{
	RefreshCountry(Country);
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleFactoryLineCompleted(ECountryType Country, int32 LineId)
{
	RefreshCountry(Country);
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleFactoryLineClaimed(ECountryType Country, int32 LineId, int64 FinalQty)
{
	if (bBatchAction && FinalQty > 0 && TableMgr)
	{
		bool bCountryOk = false;
		const FCountryInfoTable CountryRow = TableMgr->GetCountryInfo(Country, bCountryOk);
		ClaimSummaryLines.Add(FText::Format(NSLOCTEXT("WorldMap", "ClaimBatchComplete", "[{0}] 완제품 {1}개 수집완료"),
			CountryRow.DisplayName, FText::AsNumber(FinalQty)).ToString());
	}
	RefreshCountry(Country);
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleFactoryLineRemoved(ECountryType Country, int32 LineId)
{
	RefreshCountry(Country);
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleFactoryUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel)
{
	// 속도·저장 강화는 다음 성숙 시각을 바꾼다.
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleMineResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty)
{
	if (bBatchAction && ClaimedQty > 0 && TableMgr)
	{
		bool bCountryOk = false;
		const FCountryInfoTable CountryRow = TableMgr->GetCountryInfo(Country, bCountryOk);
		bool bResourceOk = false;
		const FResourceInfo ResourceRow = TableMgr->GetResourceInfo(Resource, bResourceOk);
		ClaimSummaryLines.Add(FText::Format(NSLOCTEXT("WorldMap", "ClaimSummaryLine", "[{0}] {1} {2}개 수집완료"),
			CountryRow.DisplayName, ResourceRow.DisplayName, FText::AsNumber(ClaimedQty)).ToString());
	}
	RefreshCountry(Country);
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleMineUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 NewLevel)
{
	// 저장 강화는 정유 이름표(석유·정유)와 다음 가득 참 시각을 바꾼다.
	RefreshCountry(Country);
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleProductAdded(FIntPoint ProductKey, int64 NewAmount)
{
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleProductConsumed(FIntPoint ProductKey, int64 NewAmount)
{
	RequestBubbleRefresh();
}

// 주문이 생기고·수락되고·사라질 때마다 파란 선과 점선이 서로 자리를 바꾼다.
void UWorldMapPanelWidget::HandleOrderGenerated(FTradeOrder Order)
{
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleOrderAccepted(int32 OrderId)
{
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleOrderDismissed(int32 OrderId, bool bWasAccepted)
{
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleOrderCompleted(int32 OrderId)
{
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleOrderExpired(int32 OrderId)
{
	RefreshRoutes();
	RequestBubbleRefresh();
}

void UWorldMapPanelWidget::HandleCountryFlagRejected(ECountryType CountryType)
{
	if (CountryType == ECountryType::None || !TableMgr)
	{
		return;
	}

	bool bInfoOk = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(CountryType, bInfoOk);
	if (!bInfoOk)
	{
		return;
	}

	// 같은 문구의 연타는 ShowRejectNotification 이 문구 키로 이미 삼킨다.
	if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		UIMgr->ShowRejectNotification(FText::Format(
			NSLOCTEXT("WorldMap", "CountryLockedReject", "시가총액 {0} 이상부터 진출할 수 있습니다"),
			UGlobalUtilFunctions::AbbreviateNumber(
				Info.RequiredMarketCap, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil)));
	}
}

void UWorldMapPanelWidget::HandleCountryFlagClicked(ECountryType CountryType)
{
	if (CountryType == ECountryType::None || !TableMgr)
	{
		return;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	const TSubclassOf<UUserWidget> DetailClass = TableMgr->GetWidgetClass(EWidgetType::CountryDetail);

	// ★ 열 수 있는지를 줌보다 먼저 판정한다. 순서를 뒤집으면 실패했을 때 "줌만 되고 창은 안 뜸" 이 된다.
	if ((!DrawerStack && !UIBase) || !DetailClass || !DetailClass->IsChildOf(UCountryDetailWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMapPanel] 국가 상세를 열지 못했습니다. UIBase=%s CountryDetail 위젯 클래스=%s (DT_WidgetClass 확인)"),
			UIBase ? TEXT("OK") : TEXT("없음"),
			DetailClass ? TEXT("OK") : TEXT("없음"));
		return;
	}

	UCountryDetailWidget* Detail = nullptr;
	if (DrawerStack && DrawerStack->GetWidgetList().Num() > 0)
	{
		// 첫 표시 전환 중에도 목록에는 이미 등록돼 있다. 활성 위젯만 보면 연타가 새 상세를 쌓는다.
		Detail = Cast<UCountryDetailWidget>(DrawerStack->GetWidgetList().Last());
		if (Detail && !Detail->IsActivated() && DrawerStack->GetActiveWidget() == Detail)
		{
			PendingCountryAfterDrawerClose = CountryType;
			return;
		}
		if (Detail && Detail->GetCurrentCountry() == CountryType && SelectedCountry == CountryType)
		{
			return;
		}
	}
	PendingCountryAfterDrawerClose = ECountryType::None;
	if (!Detail)
	{
		UCommonActivatableWidget* Pushed = PushDrawer(TSubclassOf<UCommonActivatableWidget>(DetailClass));
		if (!Pushed && UIBase) { Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(DetailClass)); }
		Detail = Cast<UCountryDetailWidget>(Pushed);
	}
	if (Detail)
	{
		Detail->SetCountry(CountryType);
		FocusOnCountry(CountryType, /*bAnimate*/ true);
		// 프롬프트 스택 폴백이면 월드맵이 통째로 가려져 확대 화면을 띄울 자리가 없다.
		if (DrawerStack)
		{
			BindDioramaDetail(*Detail);
			EnterDiorama(CountryType);
		}
	}
}

void FDioramaViewTransition::Enter()
{
	if (!bEntering && K <= 0.0f)
	{
		DelayLeft = EnterDelay;
	}
	bEntering = true;
	bLive = true;
}

void FDioramaViewTransition::Exit(bool bImmediate)
{
	bEntering = false;
	DelayLeft = 0.0f;
	if (bImmediate)
	{
		K = 0.0f;
	}
}

bool FDioramaViewTransition::Tick(float Dt)
{
	float Remaining = FMath::Max(0.0f, Dt);
	if (bEntering)
	{
		const float Waited = FMath::Min(DelayLeft, Remaining);
		DelayLeft -= Waited;
		Remaining -= Waited;
		K = FMath::Min(1.0f, K + Remaining / EnterDuration);
		// 프레임 시간 합의 부동소수 오차로 끝점 바로 앞에 머무는 것을 막는다.
		if (K >= 1.0f - KINDA_SMALL_NUMBER) { K = 1.0f; }
		return false;
	}

	K = FMath::Max(0.0f, K - Remaining * ExitSpeedMul / EnterDuration);
	if (K <= KINDA_SMALL_NUMBER) { K = 0.0f; }
	if (K <= 0.0f && bLive)
	{
		bLive = false;
		return true;
	}
	return false;
}

float FDioramaViewTransition::ViewOpacity() const
{
	const float Rest = 1.0f - K;
	return 1.0f - Rest * Rest * Rest;
}

float FDioramaViewTransition::MapOpacity() const
{
	return FMath::Lerp(1.0f, MapDimOpacity, ViewOpacity());
}

float FDioramaViewTransition::OffsetY() const
{
	return RiseOffsetPx * (1.0f - ViewOpacity());
}

FIntPoint WorldMapDiorama::ClampLiveSize(const FVector2D& AbsSize)
{
	constexpr int32 kDioViewMaxWidthPx = 1600;
	constexpr int32 kDioViewMaxHeightPx = 1440;
	const int32 WidthPx = FMath::RoundToInt32(AbsSize.X);
	const int32 HeightPx = FMath::RoundToInt32(AbsSize.Y);
	if (WidthPx < 1 || HeightPx < 1)
	{
		return FIntPoint::ZeroValue;
	}
	return FIntPoint(FMath::Min(WidthPx, kDioViewMaxWidthPx), FMath::Min(HeightPx, kDioViewMaxHeightPx));
}

EDioramaModule WorldMapDiorama::ModuleForTab(int32 Tab, EDioramaModule Primary)
{
	switch (Tab)
	{
	case UCountryDetailWidget::TAB_INDEX_FACTORY: return EDioramaModule::Factory;
	case UCountryDetailWidget::TAB_INDEX_MINE: return EDioramaModule::Mine;
	case UCountryDetailWidget::TAB_INDEX_PORT: return EDioramaModule::Port;
	default: return Primary;
	}
}

UOverseasDioramaSubsystem* UWorldMapPanelWidget::GetDioramaSubsystem() const
{
	const UWorld* OwnerWorld = GetWorld();
	return OwnerWorld ? OwnerWorld->GetSubsystem<UOverseasDioramaSubsystem>() : nullptr;
}

UMaterialInterface* UWorldMapPanelWidget::GetDioramaRTMaterial()
{
	if (!DioramaRTMaterial && !bDioramaMaterialWarned)
	{
		DioramaRTMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT.M_GachaRevealRT"))).LoadSynchronous();
		if (!DioramaRTMaterial)
		{
			// 갱신마다 다시 읽고 짖지 않게 한 번 실패하면 이 패널 수명 동안 접는다.
			bDioramaMaterialWarned = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] M_GachaRevealRT 를 못 읽어 국가 디오라마 화면과 지도 미니어처를 띄우지 않습니다."));
		}
	}
	return DioramaRTMaterial;
}

void UWorldMapPanelWidget::EnterDiorama(ECountryType Country)
{
	if (Country == ECountryType::None || !DioramaLayer || !DioramaView || !GetDioramaSubsystem())
	{
		return;
	}
	DioramaCountry = Country;
	// 정보 탭이 볼 주 시설은 무대와 같은 사양 규칙에서 얻는다 ― 국가 행 플래그를 여기서 따로 해석하면 둘이 갈린다.
	FDioramaSpecInputs Inputs;
	DioramaPrimaryModule = OverseasDiorama::GatherInputs(GetGameInstance(), Country, Inputs)
		? OverseasDiorama::AssembleSpec(Inputs).PrimaryModule()
		: EDioramaModule::Factory;
	bDioramaStartPending = true;
	TryBeginDioramaLive();
}

void UWorldMapPanelWidget::ExitDiorama(bool bImmediate)
{
	bDioramaStartPending = false;
	DioramaTransition.Exit(bImmediate);
	if (bImmediate)
	{
		if (UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem())
		{
			Diorama->EndLiveView();
		}
	}
	ApplyDioramaVisuals();
}

bool UWorldMapPanelWidget::TryBeginDioramaLive()
{
	UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem();
	UMaterialInterface* RTMaterial = GetDioramaRTMaterial();
	if (!DioramaViewMID && RTMaterial && DioramaView)
	{
		DioramaViewMID = UMaterialInstanceDynamic::Create(RTMaterial, this);
		if (DioramaViewMID)
		{
			DioramaViewMID->SetVectorParameterValue(TEXT("UVWindow"), FLinearColor(0.0f, 0.0f, 1.0f, 1.0f));
			DioramaView->SetBrushFromMaterial(DioramaViewMID);
		}
	}
	if (!Diorama || !DioramaViewMID)
	{
		bDioramaStartPending = false;
		return false;
	}

	const FIntPoint SizePx = LayoutDioramaView();
	if (SizePx.X <= 0 || SizePx.Y <= 0)
	{
		return false;
	}
	bDioramaStartPending = false;

	const UCountryDetailWidget* Detail = DioramaDetail.Get();
	const int32 MainTab = Detail ? Detail->GetCurrentTabIndex() : UCountryDetailWidget::TAB_INDEX_INFO;
	UTextureRenderTarget2D* LiveTarget = Diorama->BeginLiveView(DioramaCountry, SizePx,
		WorldMapDiorama::ModuleForTab(MainTab, DioramaPrimaryModule));
	if (!LiveTarget)
	{
		// 지을 시설이 없는 나라 ― 앞 나라 화면이 떠 있었다면 거둔다.
		ExitDiorama(false);
		return false;
	}

	DioramaViewMID->SetTextureParameterValue(TEXT("RT"), LiveTarget);
	DioramaTransition.Enter();
	Diorama->PlayLiveIntro();
	ApplyDioramaVisuals();
	return true;
}

FIntPoint UWorldMapPanelWidget::LayoutDioramaView()
{
	if (!DioramaLayer || !DioramaView || !MapViewport)
	{
		return FIntPoint::ZeroValue;
	}
	FSlateRect VisibleRect;
	FSlateRect SelectionBounds;
	const FGeometry& LayerGeometry = DioramaLayer->GetCachedGeometry();
	if (!MeasureFocusLayout(VisibleRect, SelectionBounds)
		|| LayerGeometry.GetLocalSize().X <= 1.0 || LayerGeometry.GetLocalSize().Y <= 1.0)
	{
		return FIntPoint::ZeroValue;
	}

	const FGeometry& ViewGeometry = MapViewport->GetCachedGeometry();
	const FVector2D AbsMin = ViewGeometry.LocalToAbsolute(FVector2D(VisibleRect.Left, VisibleRect.Top));
	const FVector2D AbsMax = ViewGeometry.LocalToAbsolute(FVector2D(VisibleRect.Right, VisibleRect.Bottom));
	const FIntPoint SizePx = WorldMapDiorama::ClampLiveSize(AbsMax - AbsMin);
	if (SizePx.X <= 0 || SizePx.Y <= 0)
	{
		return FIntPoint::ZeroValue;
	}

	FVector2D ViewMin = LayerGeometry.AbsoluteToLocal(AbsMin);
	FVector2D ViewSize = LayerGeometry.AbsoluteToLocal(AbsMax) - ViewMin;
	// 상한에 잘린 RT 는 가시 영역과 비율이 달라 늘어나 보인다 ― RT 비율로 가운데에 둔다(그림 바깥은 투명이라 경계가 안 보인다).
	const double TargetAspect = static_cast<double>(SizePx.X) / static_cast<double>(SizePx.Y);
	const double ViewAspect = ViewSize.X / FMath::Max(ViewSize.Y, 1.0);
	if (ViewAspect > TargetAspect * 1.01)
	{
		const double FitWidth = ViewSize.Y * TargetAspect;
		ViewMin.X += (ViewSize.X - FitWidth) * 0.5;
		ViewSize.X = FitWidth;
	}
	else if (ViewAspect < TargetAspect * 0.99)
	{
		const double FitHeight = ViewSize.X / TargetAspect;
		ViewMin.Y += (ViewSize.Y - FitHeight) * 0.5;
		ViewSize.Y = FitHeight;
	}

	if (UCanvasPanelSlot* ViewSlot = Cast<UCanvasPanelSlot>(DioramaView->Slot))
	{
		if (ViewSlot->GetAutoSize() || !ViewSlot->GetPosition().Equals(ViewMin, 0.5) || !ViewSlot->GetSize().Equals(ViewSize, 0.5))
		{
			ViewSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			ViewSlot->SetAlignment(FVector2D::ZeroVector);
			ViewSlot->SetAutoSize(false);
			ViewSlot->SetPosition(ViewMin);
			ViewSlot->SetSize(ViewSize);
		}
	}
	if (UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem())
	{
		Diorama->SetLiveViewSize(SizePx);
	}
	return SizePx;
}

void UWorldMapPanelWidget::TickDiorama(float DeltaTime)
{
	if (bDioramaStartPending)
	{
		TryBeginDioramaLive();
	}
	if (DioramaTransition.IsIdle())
	{
		return;
	}
	// 나가는 동안은 서랍이 닫혀 가시 영역이 지도 전체로 넓어진다 ― 그 크기로 따라가면 화면이 옆으로 튄다.
	if (DioramaTransition.bEntering)
	{
		LayoutDioramaView();
	}
	// 움직임 줄이기 설정이면 포커스 트윈과 같이 한 번에 끝낸다.
	if (DioramaTransition.Tick(FMotionGate::IsReduced() ? 1.0f : DeltaTime))
	{
		if (UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem())
		{
			Diorama->EndLiveView();
		}
	}
	ApplyDioramaVisuals();
}

void UWorldMapPanelWidget::ApplyDioramaVisuals()
{
	if (!DioramaLayer || !DioramaView)
	{
		return;
	}
	const bool bShown = DioramaTransition.bEntering || DioramaTransition.K > 0.0f;
	// 판 위 그림일 뿐이라 흐려진 지도의 다른 나라 클릭을 막지 않는다.
	DioramaView->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DioramaView->SetRenderOpacity(DioramaTransition.ViewOpacity());
	DioramaView->SetRenderTranslation(FVector2D(0.0, DioramaTransition.OffsetY()));
	if (MapTable)
	{
		MapTable->SetRenderOpacity(DioramaTransition.MapOpacity());
	}
}

void UWorldMapPanelWidget::BindDioramaDetail(UCountryDetailWidget& Detail)
{
	UnbindDioramaDetail();
	Detail.OnTabChanged.RemoveAll(this);
	Detail.OnTabChanged.AddDynamic(this, &UWorldMapPanelWidget::HandleDetailMainTabChanged);
	DioramaDetail = &Detail;
}

void UWorldMapPanelWidget::UnbindDioramaDetail()
{
	if (UCountryDetailWidget* BoundDetail = DioramaDetail.Get())
	{
		BoundDetail->OnTabChanged.RemoveAll(this);
	}
	DioramaDetail.Reset();
}

void UWorldMapPanelWidget::HandleDetailMainTabChanged(int32 TabIndex)
{
	UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem();
	if (!Diorama || !Diorama->IsLiveView() || Diorama->GetLiveCountry() != DioramaCountry)
	{
		return;
	}
	Diorama->FocusModule(WorldMapDiorama::ModuleForTab(TabIndex, DioramaPrimaryModule));
}

void UWorldMapPanelWidget::RequestUnlockedMiniatures()
{
	UOverseasDioramaSubsystem* Diorama = GetDioramaSubsystem();
	if (!Diorama || !TableMgr)
	{
		return;
	}
	const int64 MarketCap = GetCurrentMarketCap();
	TArray<ECountryType> UnlockedCountries;
	for (const TPair<ECountryType, TObjectPtr<UCountryNameWidget>>& Entry : Medallions)
	{
		bool bRowOk = false;
		const FCountryInfoTable Row = TableMgr->GetCountryInfo(Entry.Key, bRowOk);
		if (bRowOk && ResolveCountryState(ResolveOwnershipRule(Row), CountFacilities(Row.CountryType),
				MarketCap, Row.RequiredMarketCap) != ECountryMapState::Locked)
		{
			UnlockedCountries.Add(Entry.Key);
		}
	}
	Diorama->RequestMiniatures(UnlockedCountries);
}

void UWorldMapPanelWidget::ApplyMiniature(UCountryNameWidget& Pin, ECountryType Country, bool bUnlocked)
{
	FDioramaStillInfo StillInfo;
	UOverseasDioramaSubsystem* Diorama = bUnlocked ? GetDioramaSubsystem() : nullptr;
	UTexture* Still = Diorama ? Diorama->GetMiniature(Country, StillInfo) : nullptr;
	UMaterialInterface* RTMaterial = Still ? GetDioramaRTMaterial() : nullptr;
	if (Still && RTMaterial)
	{
		Pin.SetMiniature(Still, StillInfo, RTMaterial);
	}
	else
	{
		Pin.ClearMiniature();
	}

	// 그림 모드에서는 땅 조각 중심이 지도 앵커에 얹혀야 한다 ― 원반 중심 정렬(0.5)이면 그림이 위로 뜬다.
	if (UCanvasPanelSlot* PinSlot = Cast<UCanvasPanelSlot>(Pin.Slot))
	{
		const FVector2D PinAlignment = Pin.GetAnchorAlignment();
		if (!PinSlot->GetAlignment().Equals(PinAlignment, 1e-4))
		{
			PinSlot->SetAlignment(PinAlignment);
		}
	}
}

void UWorldMapPanelWidget::HandleMiniatureUpdated(ECountryType Country)
{
	RefreshCountry(Country);
}

FVector2D UWorldMapPanelWidget::MeasureBoardOrigin() const
{
	if (!MapViewport || !MapTable)
	{
		return FVector2D::ZeroVector;
	}
	// 변형은 MapTable 자신에만 걸리고 MapTable 은 부모를 꽉 채운다 ― 부모의 왼쪽 위가 변형 전 판 원점이다.
	const UWidget* TableFrame = MapTable->GetParent();
	if (!TableFrame)
	{
		return FVector2D::ZeroVector;
	}
	const FGeometry& FrameGeometry = TableFrame->GetCachedGeometry();
	return MapViewport->GetCachedGeometry().AbsoluteToLocal(FrameGeometry.LocalToAbsolute(FVector2D::ZeroVector));
}

FVector2D UWorldMapPanelWidget::GetSeaCoverScale() const
{
	if (!SeaImage)
	{
		return FVector2D(1.0, 1.0);
	}
	const FVector2D SeaScale = SeaImage->GetRenderTransform().Scale;
	return FVector2D(FMath::Max(1.0, SeaScale.X), FMath::Max(1.0, SeaScale.Y));
}

void UWorldMapPanelWidget::RequestBubbleRefresh()
{
	if (bBatchAction)
	{
		bBubbleRefreshPending = true;
		return;
	}
	RefreshActionBubbles();
}

void UWorldMapPanelWidget::RefreshActionBubbles()
{
	bBubbleRefreshPending = false;
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	const int32 AllPending = OverseasActions::CountAllPending(GI);
	if (ClaimAllButton)
	{
		ClaimAllButton->SetButtonText(MapBubbles::ClaimAllLabel(AllPending));
		ClaimAllButton->SetVisibility(MapBubbles::ShouldShowClaimAll(AllPending, IsDrawerOpen())
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (BubbleCanvas)
	{
		ECountryType BestPort = ECountryType::None;
		int64 BestMoney = 0;
		const bool bSell = OverseasActions::FindBestSalePort(GI, BestPort, BestMoney);

		TArray<ECountryType> PlacedCountries;
		Medallions.GetKeys(PlacedCountries);
		for (const ECountryType Country : PlacedCountries)
		{
			// 라인이 있어야 받을 것이 생기므로 잠긴 나라는 여기서 자연히 0 이다.
			const int32 Pending = OverseasActions::CountPending(GI, Country);
			const bool bSellHere = bSell && Country == BestPort;

			UMapActionBubbleWidget* ClaimBubble = Pending > 0
				? EnsureBubble(Country, EMapActionBubbleKind::Claim) : ClaimBubbles.FindRef(Country).Get();
			UMapActionBubbleWidget* SellBubble = bSellHere
				? EnsureBubble(Country, EMapActionBubbleKind::Sell) : SellBubbles.FindRef(Country).Get();
			if (ClaimBubble)
			{
				if (Pending > 0)
				{
					ClaimBubble->SetBubble(EMapActionBubbleKind::Claim, Country, MapBubbles::ClaimLabel(Pending));
				}
				ClaimBubble->SetVisibility(Pending > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
			if (SellBubble)
			{
				if (bSellHere)
				{
					SellBubble->SetBubble(EMapActionBubbleKind::Sell, Country, MapBubbles::SellLabel(BestMoney));
				}
				SellBubble->SetVisibility(bSellHere ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
			if (UHorizontalBox* BubbleRow = BubbleRows.FindRef(Country))
			{
				BubbleRow->SetVisibility((Pending > 0 || bSellHere)
					? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			}
			PlaceBubbleRow(Country);
		}
	}

	ScheduleBubbleEtaRefresh();
}

UMapActionBubbleWidget* UWorldMapPanelWidget::EnsureBubble(ECountryType Country, EMapActionBubbleKind Kind)
{
	TMap<ECountryType, TObjectPtr<UMapActionBubbleWidget>>& Bubbles =
		Kind == EMapActionBubbleKind::Claim ? ClaimBubbles : SellBubbles;
	if (UMapActionBubbleWidget* Existing = Bubbles.FindRef(Country))
	{
		return Existing;
	}

	const UCountryNameWidget* Pin = Medallions.FindRef(Country);
	const UCanvasPanelSlot* PinSlot = Pin ? Cast<UCanvasPanelSlot>(Pin->Slot) : nullptr;
	const TSubclassOf<UUserWidget> BubbleClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::MapActionBubble) : nullptr;
	if (!BubbleCanvas || !WidgetTree || !PinSlot || !BubbleClass || !BubbleClass->IsChildOf(UMapActionBubbleWidget::StaticClass()))
	{
		if (!bBubbleWidgetsWarned)
		{
			bBubbleWidgetsWarned = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[WorldMapPanel] 지도 말풍선을 만들지 못했습니다. BubbleCanvas=%s MapActionBubble 위젯 클래스=%s (DT_WidgetClass 확인)"),
				BubbleCanvas ? TEXT("OK") : TEXT("없음"), BubbleClass ? TEXT("OK") : TEXT("없음"));
		}
		return nullptr;
	}

	UHorizontalBox* BubbleRow = BubbleRows.FindRef(Country);
	if (!BubbleRow)
	{
		BubbleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (!BubbleRow)
		{
			return nullptr;
		}
		BubbleRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* RowSlot = BubbleCanvas->AddChildToCanvas(BubbleRow))
		{
			// 메달리온과 같은 정규화 앵커 ― 판 크기가 바뀌어도 같은 나라 위에 남는다.
			RowSlot->SetAnchors(PinSlot->GetAnchors());
			RowSlot->SetAlignment(FVector2D(0.5, 1.0));
			RowSlot->SetAutoSize(true);
		}
		BubbleRows.Add(Country, BubbleRow);
	}

	UMapActionBubbleWidget* Bubble = CreateWidget<UMapActionBubbleWidget>(this, BubbleClass);
	if (!Bubble)
	{
		return nullptr;
	}
	Bubble->OnBubbleClicked.BindUObject(this, &UWorldMapPanelWidget::HandleBubbleClicked);
	Bubble->SetBubble(Kind, Country, FText::GetEmpty());

	// 받기가 왼쪽, 판매가 오른쪽(시안 순서). 중간 삽입 API 가 없어 판매가 먼저 있으면 뒤로 다시 붙인다.
	UMapActionBubbleWidget* SellAfter = (Kind == EMapActionBubbleKind::Claim) ? SellBubbles.FindRef(Country).Get() : nullptr;
	if (SellAfter)
	{
		BubbleRow->RemoveChild(SellAfter);
	}
	for (UMapActionBubbleWidget* RowChild : { Bubble, SellAfter })
	{
		if (!RowChild)
		{
			continue;
		}
		if (UHorizontalBoxSlot* BubbleSlot = BubbleRow->AddChildToHorizontalBox(RowChild))
		{
			BubbleSlot->SetVerticalAlignment(VAlign_Bottom);
			BubbleSlot->SetPadding(FMargin(BubbleRow->GetChildrenCount() > 1 ? 8.0f : 0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
	Bubbles.Add(Country, Bubble);
	return Bubble;
}

void UWorldMapPanelWidget::PlaceBubbleRow(ECountryType Country)
{
	UHorizontalBox* BubbleRow = BubbleRows.FindRef(Country);
	const UCountryNameWidget* Pin = Medallions.FindRef(Country);
	UCanvasPanelSlot* RowSlot = BubbleRow ? Cast<UCanvasPanelSlot>(BubbleRow->Slot) : nullptr;
	if (!RowSlot || !Pin)
	{
		return;
	}
	// 미니어처가 뒤늦게 도착하면 그림 윗변이 올라가므로 상태 갱신 때마다 다시 잰다(같으면 손대지 않는다).
	const FVector2D Offset = MapBubbles::RowOffset(Pin->GetHeightAboveAnchor());
	if (!RowSlot->GetPosition().Equals(Offset, 0.5))
	{
		RowSlot->SetPosition(Offset);
	}
}

void UWorldMapPanelWidget::HandleBubbleClicked(EMapActionBubbleKind Kind, ECountryType Country)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI || Country == ECountryType::None)
	{
		return;
	}

	if (Kind == EMapActionBubbleKind::Claim)
	{
		RunClaimWithSummary([GI, Country]() { return OverseasActions::ClaimCountry(GI, Country); });
		return;
	}

	bBatchAction = true;
	const int64 Money = OverseasActions::SellAllAt(GI, Country);
	bBatchAction = false;
	if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
	{
		if (Money > 0)
		{
			UIMgr->ShowNotification(FText::Format(NSLOCTEXT("WorldMap", "MapSellDone", "판매 수익 {0}"),
				UGlobalUtilFunctions::AbbreviateNumber(Money)), 3.0f, ENotificationType::Success);
		}
		else
		{
			UIMgr->ShowNotification(NSLOCTEXT("WorldMap", "MapSellNothing", "팔 수 있는 제품이 없습니다"), 3.0f, ENotificationType::Normal);
		}
	}
	RefreshActionBubbles();
}

void UWorldMapPanelWidget::HandleClaimAllClicked()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	RunClaimWithSummary([GI]() { return OverseasActions::ClaimAll(GI); });
}

void UWorldMapPanelWidget::RunClaimWithSummary(TFunctionRef<int64()> Claim)
{
	ClaimSummaryLines.Reset();
	bBatchAction = true;
	const int64 Claimed = Claim();
	bBatchAction = false;

	UGameInstance* GI = GetGameInstance();
	if (UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		if (Claimed <= 0 || ClaimSummaryLines.Num() == 0)
		{
			UIMgr->ShowNotification(NSLOCTEXT("WorldMap", "ClaimNothing", "수집할 게 없습니다"), 3.0f, ENotificationType::Normal);
		}
		else
		{
			UIMgr->ShowNotification(FText::FromString(FString::Join(ClaimSummaryLines, TEXT("\n"))), 3.0f, ENotificationType::Success);
		}
	}
	ClaimSummaryLines.Reset();
	// 받은 즉시 같은 프레임에 숫자가 0 이 되고 버튼·말풍선이 사라져야 한다.
	RefreshActionBubbles();
}

void UWorldMapPanelWidget::ScheduleBubbleEtaRefresh()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	if (!World || !GI)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(BubbleEtaHandle);
	if (!IsActivated())
	{
		return;
	}

	// 판정식은 OverseasActions 의 대기 조건과 같아야 한다 ― 깨어나도 아무것도 안 바뀌면 헛돈다.
	double NextEtaSec = TNumericLimits<double>::Max();
	if (const UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
	{
		for (const ECountryType Country : MineMgr->GetActiveCountries())
		{
			if (MineMgr->IsAutoClaimEnabled(Country))
			{
				continue;
			}
			const double RateMul = static_cast<double>(MineMgr->GetRateMultiplier(Country));
			for (const FMineLineState& Line : MineMgr->GetMineLines(Country))
			{
				FMineLineState Live;
				if (!MineMgr->GetLineState(Country, Line.Resource, Live) || Live.CurrentQty > 0)
				{
					continue;
				}
				const double RatePerSec = static_cast<double>(Live.BaseRatePerSecond) * RateMul;
				if (RatePerSec > 0.0 && MineMgr->GetEffectiveMaxStorage(Country, Line.Resource) > 0)
				{
					NextEtaSec = FMath::Min(NextEtaSec, (1.0 - static_cast<double>(Live.PendingFraction)) / RatePerSec);
				}
			}
		}
	}
	if (const UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
	{
		for (const ECountryType Country : FactoryMgr->GetActiveCountries())
		{
			for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(Country))
			{
				FWorldFactoryLineState Live;
				if (!FactoryMgr->GetLineState(Country, Line.LineId, Live) || Live.bCompleted
					|| Live.RatePerSecond <= 0.0f || Live.TargetQty <= 0)
				{
					continue;
				}
				NextEtaSec = FMath::Min(NextEtaSec,
					static_cast<double>(Live.TargetQty - Live.CurrentQty) / static_cast<double>(Live.RatePerSecond));
			}
		}
	}

	if (NextEtaSec < TNumericLimits<double>::Max())
	{
		// 성숙 시각 정각에 깨우면 내림 잔차로 한 번 헛돈다 ― 도크와 같은 여유를 둔다.
		const float Delay = FMath::Max(static_cast<float>(NextEtaSec) + 0.25f, 0.5f);
		World->GetTimerManager().SetTimer(BubbleEtaHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			// 경고 도트(HasPendingAttention)도 같은 성숙을 기다린다.
			RefreshAll();
			RefreshActionBubbles();
		}), Delay, false);
	}
}
