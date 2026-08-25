// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorldMapBottomWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Panel/CountryRouterPanelWidget.h"
#include "UI/Panel/ProductSellModalWidget.h"
#include "Data/WorldMapTypes.h"
#include "UI/UIBase.h"
#include "Player/WorldMapPlayerController.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TradePort.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Enum/NotificationType.h"
#include "Table/CountryInfoTable.h"
#include "Table/ResourceInfo.h"
#include "Components/Button.h"

void UWorldMapBottomWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

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

	// 1. Mine — saturated 라인 수령
	if (MineMgr)
	{
		for (ECountryType Country : MineMgr->GetActiveCountries())
		{
			const FString CountryName = GetCountryDisplay(Country);
			TArray<TPair<EResourceType, int64>> PerLineSummary;
			for (const FMineLineState& Line : MineMgr->GetMineLines(Country))
			{
				FMineLineState LazyState;
				if (MineMgr->GetLineState(Country, Line.Resource, LazyState) && LazyState.bSaturated && LazyState.CurrentQty > 0)
				{
					PerLineSummary.Add(TPair<EResourceType, int64>(Line.Resource, LazyState.CurrentQty));
				}
			}
			for (const TPair<EResourceType, int64>& Entry : PerLineSummary)
			{
				MineMgr->ClaimResource(Country, Entry.Key);

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

void UWorldMapBottomWidget::OnHomeBtnClicked()
{
	AWorldMapPlayerController* PC = Cast<AWorldMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		PC->ReturnToMainMap();
	}
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

	UCommonActivatableWidget* Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
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
	UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
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

	UCommonActivatableWidget* Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	if (UCountryRouterPanelWidget* Router = Cast<UCountryRouterPanelWidget>(Pushed))
	{
		Router->InitializeForMode(Mode);
	}
}
