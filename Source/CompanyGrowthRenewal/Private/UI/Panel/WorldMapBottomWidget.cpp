// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorldMapBottomWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Panel/CountryRouterPanelWidget.h"
#include "UI/Panel/ProductSellModalWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "Data/WorldMapTypes.h"
#include "UI/UIBase.h"
#include "Player/LegacyWorldMapPlayerController.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TradePort.h"
#include "Manager/MineManager.h"
#include "Manager/OverseasActions.h"
#include "Manager/WorldFactoryManager.h"
#include "Enum/NotificationType.h"
#include "Table/CountryInfoTable.h"
#include "Table/ResourceInfo.h"
#include "Components/Button.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UWorldMapBottomWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 수령 대기 도트 — 구독 먼저, 첫 평가는 즉시(활성화 시점의 성숙분은 델리게이트가 알려주지 않는다).
	BindCollectBadgeEvents();
	RefreshCollectBadge();

	if (HomeBtn)
	{
		HomeBtn->OnClicked.AddDynamic(this, &UWorldMapBottomWidget::OnHomeBtnClicked);
	}

	// 액션바 3개 — UCommonButtonBase native 이벤트
	if (ResourcePanelBtn)
	{
		ResourcePanelBtn->OnClicked().AddUObject(this, &UWorldMapBottomWidget::OnResourcePanelClicked);
	}
	if (FactoryPanelBtn)
	{
		FactoryPanelBtn->OnClicked().AddUObject(this, &UWorldMapBottomWidget::OnFactoryPanelClicked);
	}
	if (ProductsPanelBtn)
	{
		ProductsPanelBtn->OnClicked().AddUObject(this, &UWorldMapBottomWidget::OnProductsPanelClicked);
	}
	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().AddUObject(this, &UWorldMapBottomWidget::OnCollectAllClicked);
	}
}

void UWorldMapBottomWidget::NativeOnDeactivated()
{
	if (HomeBtn)
	{
		HomeBtn->OnClicked.RemoveDynamic(this, &UWorldMapBottomWidget::OnHomeBtnClicked);
	}

	if (ResourcePanelBtn) ResourcePanelBtn->OnClicked().RemoveAll(this);
	if (FactoryPanelBtn)  FactoryPanelBtn->OnClicked().RemoveAll(this);
	if (ProductsPanelBtn) ProductsPanelBtn->OnClicked().RemoveAll(this);
	if (CollectAllButton) CollectAllButton->OnClicked().RemoveAll(this);

	UnbindCollectBadgeEvents();

	Super::NativeOnDeactivated();
}

void UWorldMapBottomWidget::OnCollectAllClicked()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UMineManager* MineMgr = GI->GetSubsystem<UMineManager>();
	UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr) return;

	// 라인 단위 메시지 누적 — "[국가] 자원/제품 N개 수집완료" format.
	// 헬퍼: 국가명 lookup. DT_CountryInfo 가 단일 진실. 실패 시 enum 숫자 fallback.
	auto GetCountryDisplay = [TableMgr](ECountryType Country) -> FString
	{
		if (TableMgr)
		{
			bool bOk = false;
			const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bOk);
			if (bOk && !Info.DisplayName.IsEmpty())
			{
				return Info.DisplayName.ToString();
			}
		}
		return FString::Printf(TEXT("국가%d"), (int32)Country);
	};

	TArray<FString> Lines;

	// 1. Mine — 정수 수량이 쌓인 라인 수령
	if (MineMgr)
	{
		for (ECountryType Country : MineMgr->GetActiveCountries())
		{
			const FString CountryName = GetCountryDisplay(Country);
			TArray<TPair<EResourceType, int64>> PerLineSummary;
			for (const FMineLineState& Line : MineMgr->GetMineLines(Country))
			{
				FMineLineState LazyState;
				if (MineMgr->GetLineState(Country, Line.Resource, LazyState) && LazyState.CurrentQty > 0)
				{
					PerLineSummary.Add(TPair<EResourceType, int64>(Line.Resource, LazyState.CurrentQty));
				}
			}
			for (const TPair<EResourceType, int64>& Entry : PerLineSummary)
			{
				if (!MineMgr->ClaimResource(Country, Entry.Key)) continue;

				FString ResName;
				if (TableMgr)
				{
					bool bOk = false;
					const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.Key, bOk);
					if (bOk && !Info.DisplayName.IsEmpty())
					{
						ResName = Info.DisplayName.ToString();
					}
				}
				if (ResName.IsEmpty())
				{
					ResName = FString::Printf(TEXT("자원%d"), (int32)Entry.Key);
				}
				Lines.Add(FString::Printf(TEXT("[%s] %s %lld개 수집완료"), *CountryName, *ResName, Entry.Value));
			}
		}
	}

	// 2. Factory — completed 라인 수령
	if (FactoryMgr)
	{
		for (ECountryType Country : FactoryMgr->GetActiveCountries())
		{
			const FString CountryName = GetCountryDisplay(Country);
			TArray<TTuple<int32, FString, int64>> PerLineSummary;  // LineId, ProductName, Qty
			for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(Country))
			{
				FWorldFactoryLineState LazyState;
				if (FactoryMgr->GetLineState(Country, Line.LineId, LazyState) && LazyState.bCompleted && LazyState.CurrentQty > 0)
				{
					const FString ProductName = LazyState.ProductName.IsEmpty()
						? FString::Printf(TEXT("제품 %d"), LazyState.ProductIndex)
						: LazyState.ProductName.ToString();
					PerLineSummary.Add(MakeTuple(Line.LineId, ProductName, LazyState.CurrentQty));
				}
			}
			for (const TTuple<int32, FString, int64>& Entry : PerLineSummary)
			{
				FactoryMgr->ClaimLine(Country, Entry.Get<0>());
				Lines.Add(FString::Printf(TEXT("[%s] %s %lld개 수집완료"),
					*CountryName, *Entry.Get<1>(), Entry.Get<2>()));
			}
		}
	}

	if (Lines.Num() == 0)
	{
		UIMgr->ShowNotification(FText::FromString(TEXT("수집할 게 없습니다")), 3.0f, ENotificationType::Normal);
		return;
	}

	const FString Summary = FString::Join(Lines, TEXT("\n"));
	UIMgr->ShowNotification(FText::FromString(Summary), 3.0f, ENotificationType::Success);
}

void UWorldMapBottomWidget::BindCollectBadgeEvents()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
	{
		MineMgr->OnLineCreated.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleMineLineCreated);
		MineMgr->OnLineSaturated.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleMineLineSaturated);
		MineMgr->OnResourceClaimed.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleMineResourceClaimed);
		MineMgr->OnUpgradeChanged.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleMineUpgradeChanged);
	}
	if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryMgr->OnLineStarted.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleFactoryLineStarted);
		FactoryMgr->OnLineCompleted.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleFactoryLineCompleted);
		FactoryMgr->OnLineClaimed.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleFactoryLineClaimed);
		FactoryMgr->OnUpgradeChanged.AddUniqueDynamic(this, &UWorldMapBottomWidget::HandleFactoryUpgradeChanged);
	}
}

void UWorldMapBottomWidget::UnbindCollectBadgeEvents()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollectBadgeEtaHandle);
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
	{
		MineMgr->OnLineCreated.RemoveDynamic(this, &UWorldMapBottomWidget::HandleMineLineCreated);
		MineMgr->OnLineSaturated.RemoveDynamic(this, &UWorldMapBottomWidget::HandleMineLineSaturated);
		MineMgr->OnResourceClaimed.RemoveDynamic(this, &UWorldMapBottomWidget::HandleMineResourceClaimed);
		MineMgr->OnUpgradeChanged.RemoveDynamic(this, &UWorldMapBottomWidget::HandleMineUpgradeChanged);
	}
	if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
	{
		FactoryMgr->OnLineStarted.RemoveDynamic(this, &UWorldMapBottomWidget::HandleFactoryLineStarted);
		FactoryMgr->OnLineCompleted.RemoveDynamic(this, &UWorldMapBottomWidget::HandleFactoryLineCompleted);
		FactoryMgr->OnLineClaimed.RemoveDynamic(this, &UWorldMapBottomWidget::HandleFactoryLineClaimed);
		FactoryMgr->OnUpgradeChanged.RemoveDynamic(this, &UWorldMapBottomWidget::HandleFactoryUpgradeChanged);
	}
}

void UWorldMapBottomWidget::SetCollectBadgeVisible(bool bVisible)
{
	// WBP 에 놓인 전용 마크가 1순위, 없으면 버튼 내장 뱃지로 위임한다.
	if (IsValid(CollectAllAlertMark))
	{
		if (bVisible)
		{
			CollectAllAlertMark->SetMark(EAlertMarkSize::Large, EAlertMarkColor::Green);
			CollectAllAlertMark->Show();
		}
		else
		{
			CollectAllAlertMark->Hide();
		}
		return;
	}

	if (CollectAllButton)
	{
		if (bVisible) { CollectAllButton->ShowBadge(); } else { CollectAllButton->HideBadge(); }
	}
}

void UWorldMapBottomWidget::RefreshCollectBadge()
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GetWorld();
	if (!GI || !World) return;

	World->GetTimerManager().ClearTimer(CollectBadgeEtaHandle);

	UMineManager* MineMgr = GI->GetSubsystem<UMineManager>();
	UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();

	// 판정식은 OnCollectAllClicked 의 수령 조건과 글자 그대로 같아야 한다 — 갈리면 눌러도 0개인 거짓 약속이 된다.
	const bool bAnyReady = OverseasActions::CountAllPending(GI) > 0;
	double NextEtaSec = TNumericLimits<double>::Max();

	if (MineMgr)
	{
		for (ECountryType Country : MineMgr->GetActiveCountries())
		{
			if (MineMgr->IsAutoClaimEnabled(Country)) continue;
			const double RateMul = static_cast<double>(MineMgr->GetRateMultiplier(Country));
			for (const FMineLineState& Line : MineMgr->GetMineLines(Country))
			{
				FMineLineState LazyState;
				if (!MineMgr->GetLineState(Country, Line.Resource, LazyState)) continue;
				if (LazyState.CurrentQty > 0)
				{
					continue;
				}

				const double RatePerSec = static_cast<double>(LazyState.BaseRatePerSecond) * RateMul;
				const int64 LineMax = MineMgr->GetEffectiveMaxStorage(Country, Line.Resource);
				if (RatePerSec <= 0.0 || LineMax <= 0) continue;

				NextEtaSec = FMath::Min(NextEtaSec,
					(1.0 - static_cast<double>(LazyState.PendingFraction)) / RatePerSec);
			}
		}
	}

	if (FactoryMgr)
	{
		for (ECountryType Country : FactoryMgr->GetActiveCountries())
		{
			for (const FWorldFactoryLineState& Line : FactoryMgr->GetActiveLines(Country))
			{
				FWorldFactoryLineState LazyState;
				if (!FactoryMgr->GetLineState(Country, Line.LineId, LazyState)) continue;
				if (LazyState.bCompleted && LazyState.CurrentQty > 0)
				{
					continue;
				}

				if (LazyState.RatePerSecond <= 0.0f || LazyState.TargetQty <= 0) continue;

				NextEtaSec = FMath::Min(NextEtaSec,
					static_cast<double>(LazyState.TargetQty - LazyState.CurrentQty) / static_cast<double>(LazyState.RatePerSecond));
			}
		}
	}

	SetCollectBadgeVisible(bAnyReady);

	if (NextEtaSec < TNumericLimits<double>::Max())
	{
		// floor 잔차와 프레임 경계를 넘기려고 여유를 더한다 — 정확히 성숙 시각에 깨우면 한 번 헛되게 돈다.
		const float Delay = FMath::Max(static_cast<float>(NextEtaSec) + 0.25f, 0.5f);
		World->GetTimerManager().SetTimer(
			CollectBadgeEtaHandle,
			FTimerDelegate::CreateUObject(this, &UWorldMapBottomWidget::RefreshCollectBadge),
			Delay,
			false);
	}
}

UCommonActivatableWidget* UWorldMapBottomWidget::FindOwningActivatable() const
{
	return GetTypedOuter<UCommonActivatableWidget>();
}

void UWorldMapBottomWidget::OnHomeBtnClicked()
{
	// 구 WorldMap 레벨 경로 ― 레벨이 남아 있는 동안은 그대로 둔다(레벨 정리와 함께 없어질 분기).
	UWorld* CurrentWorld = GetWorld();
	if (ALegacyWorldMapPlayerController* PC = CurrentWorld
		? Cast<ALegacyWorldMapPlayerController>(CurrentWorld->GetFirstPlayerController()) : nullptr)
	{
		PC->ReturnToMainMap();
		return;
	}

	// 패널 경로 ― MainMap 위에 얹힌 UI_WorldMapPanel 의 자식이라 위 컨트롤러가 없다. 패널을 닫는 것이 곧 복귀다.
	if (UCommonActivatableWidget* OwningPanel = FindOwningActivatable())
	{
		OwningPanel->DeactivateWidget();
		return;
	}

	// 두 경로 다 없으면 화면상 아무 일도 안 일어난다 ― 조용히 지나가면 "버튼이 죽었다" 를 로그로 못 본다.
	UE_LOG(LogTemp, Warning,
		TEXT("[WorldMapBottom] 홈 버튼이 닫을 대상을 못 찾았습니다. 레벨 컨트롤러도, 이 도크를 품은 패널도 없습니다."));
}

void UWorldMapBottomWidget::OnResourcePanelClicked()
{
	PushRouterPanel(ECountryRouterMode::Resource);
}

void UWorldMapBottomWidget::OnFactoryPanelClicked()
{
	PushRouterPanel(ECountryRouterMode::Factory);
}

void UWorldMapBottomWidget::OnProductsPanelClicked()
{
	// [물품] = 자유판매 진입점 (메모리상 2-트랙: TradeBoard 납품 / [물품] 자유판매)
	// WorldMapManager의 ProductInventory를 SellableItem 배열로 변환 후 모달에 주입.
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	UWorldMapManager* WorldMapMgr = GI->GetSubsystem<UWorldMapManager>();
	UTradePort* TradePort = GI->GetSubsystem<UTradePort>();
	if (!TableMgr || !UIMgr || !WorldMapMgr || !TradePort) return;

	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ProductSellModal);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMapBottom] ProductSellModal widget class not registered"));
		return;
	}

	// ProductInventory → FSellableItem 변환
	TArray<FSellableItem> Items;
	const TArray<FIntPoint> Keys = WorldMapMgr->GetAllProductKeys();
	Items.Reserve(Keys.Num());
	for (const FIntPoint& Key : Keys)
	{
		const int64 Qty = WorldMapMgr->GetProductAmount(Key);
		if (Qty <= 0) continue;

		FSellableItem Item;
		Item.Industry = static_cast<ECompanyType>(Key.X);
		Item.ProjectIndex = Key.Y;
		Item.Quantity = Qty;
		Item.BasePrice = TradePort->GetBasePrice(Item.Industry, Item.ProjectIndex);
		Item.Grade = WorldMapMgr->GetProductGrade(Key);
		// DisplayName은 모달이 fallback 처리 ("{Industry} T{ProjectIndex}")
		Items.Add(Item);
	}

	// 드로어가 1순위 ― PromptStack 은 애니메이티드 스위처라 올리는 순간 월드맵이 화면에서 사라진다.
	UWorldMapPanelWidget* MapPanel = Cast<UWorldMapPanelWidget>(FindOwningActivatable());
	UCommonActivatableWidget* Pushed = MapPanel
		? MapPanel->PushDrawer(TSubclassOf<UCommonActivatableWidget>(Cls))
		: nullptr;
	if (!Pushed)
	{
		Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	}

	if (UProductSellModalWidget* Modal = Cast<UProductSellModalWidget>(Pushed))
	{
		Modal->SetItems(Items);
	}
}

void UWorldMapBottomWidget::PushPanel(EWidgetType PanelType)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIMgr) return;

	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(PanelType);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMapBottom] Widget class not found for type %d"), (int32)PanelType);
		return;
	}

	// 드로어가 1순위 ― PromptStack 은 애니메이티드 스위처라 올리는 순간 월드맵이 화면에서 사라진다.
	UWorldMapPanelWidget* MapPanel = Cast<UWorldMapPanelWidget>(FindOwningActivatable());
	UCommonActivatableWidget* Pushed = MapPanel
		? MapPanel->PushDrawer(TSubclassOf<UCommonActivatableWidget>(Cls))
		: nullptr;
	if (!Pushed)
	{
		UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	}
}

void UWorldMapBottomWidget::PushRouterPanel(ECountryRouterMode Mode)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIMgr) return;

	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::CountryRouterPanel);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMapBottom] CountryRouterPanel widget class not found"));
		return;
	}

	// 드로어가 1순위 ― PromptStack 은 애니메이티드 스위처라 올리는 순간 월드맵이 화면에서 사라진다.
	UWorldMapPanelWidget* MapPanel = Cast<UWorldMapPanelWidget>(FindOwningActivatable());
	UCommonActivatableWidget* Pushed = MapPanel
		? MapPanel->PushDrawer(TSubclassOf<UCommonActivatableWidget>(Cls))
		: nullptr;
	if (!Pushed)
	{
		Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	}

	if (UCountryRouterPanelWidget* Router = Cast<UCountryRouterPanelWidget>(Pushed))
	{
		Router->InitializeForMode(Mode);
	}
}
