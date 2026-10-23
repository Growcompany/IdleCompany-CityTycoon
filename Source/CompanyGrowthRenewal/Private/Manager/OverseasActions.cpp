#include "Manager/OverseasActions.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TradePort.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProductRecipeTable.h"
#include "Data/ProductionOrderData.h"
#include "Data/WorldMapTypes.h"
#include "Engine/Texture2D.h"
#include "Enum/CompanyType.h"
#include "Enum/CountryMapState.h"
#include "Enum/ResourceType.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "Engine/GameInstance.h"

namespace OverseasActions
{
	namespace
	{
		int64 AddClaimedSaturated(int64 Total, int64 Amount)
		{
			return Amount > MAX_int64 - Total ? MAX_int64 : Total + Amount;
		}

		// 도크 모두 받기·수령 도트와 같은 조건이어야 한다 ― 갈리면 눌러도 0개인 말풍선이 뜬다.
		bool IsFactoryLineReady(const FWorldFactoryLineState& Live)
		{
			return Live.bCompleted && Live.CurrentQty > 0;
		}

		bool IsMineLineReady(const FMineLineState& Live)
		{
			return Live.CurrentQty > 0;
		}

		int32 CountReadyFactoryLines(const UWorldFactoryManager& FactoryMgr, ECountryType Country)
		{
			int32 Count = 0;
			for (const FWorldFactoryLineState& Line : FactoryMgr.GetActiveLines(Country))
			{
				FWorldFactoryLineState Live;
				if (FactoryMgr.GetLineState(Country, Line.LineId, Live) && IsFactoryLineReady(Live))
				{
					++Count;
				}
			}
			return Count;
		}

		int32 CountReadyMineLines(const UMineManager& MineMgr, ECountryType Country)
		{
			if (MineMgr.IsAutoClaimEnabled(Country))
			{
				return 0;
			}

			int32 Count = 0;
			for (const FMineLineState& Line : MineMgr.GetMineLines(Country))
			{
				FMineLineState Live;
				if (MineMgr.GetLineState(Country, Line.Resource, Live) && IsMineLineReady(Live))
				{
					++Count;
				}
			}
			return Count;
		}

		int64 ClaimReadyFactoryLines(UWorldFactoryManager& FactoryMgr, ECountryType Country)
		{
			int64 Claimed = 0;
			for (const FWorldFactoryLineState& Line : FactoryMgr.GetActiveLines(Country))
			{
				FWorldFactoryLineState Live;
				if (FactoryMgr.GetLineState(Country, Line.LineId, Live) && IsFactoryLineReady(Live))
				{
					const int64 PaidQty = FactoryMgr.ClaimLineAndGetQuantity(Country, Line.LineId);
					Claimed = AddClaimedSaturated(Claimed, PaidQty);
				}
			}
			return Claimed;
		}

		int64 ClaimMineLines(UMineManager& MineMgr, ECountryType Country)
		{
			int64 Claimed = 0;
			for (const FMineLineState& Line : MineMgr.GetMineLines(Country))
			{
				FMineLineState Live;
				if (!MineMgr.GetLineState(Country, Line.Resource, Live) || !IsMineLineReady(Live))
				{
					continue;
				}
				if (MineMgr.ClaimResource(Country, Line.Resource))
				{
					Claimed = AddClaimedSaturated(Claimed, Live.CurrentQty);
				}
			}
			return Claimed;
		}

		bool IsRowUnlocked(const UGameInstance& GI, const FCountryInfoTable& Row)
		{
			const UResourceItemManager* ResMgr = GI.GetSubsystem<UResourceItemManager>();
			const int64 MarketCap = ResMgr ? ResMgr->GetResourceAmount(EResourceType::MarketCap) : 0;
			return UWorldMapPanelWidget::ResolveCountryState(UWorldMapPanelWidget::ResolveOwnershipRule(Row),
				CountFacilities(&GI, Row.CountryType), MarketCap, Row.RequiredMarketCap) != ECountryMapState::Locked;
		}
	}

	int32 CountPending(const UGameInstance* GI, ECountryType Country)
	{
		if (!GI)
		{
			return 0;
		}

		int32 Count = 0;
		if (const UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
		{
			Count += CountReadyFactoryLines(*FactoryMgr, Country);
		}
		if (const UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
		{
			Count += CountReadyMineLines(*MineMgr, Country);
		}
		return Count;
	}

	int32 CountAllPending(const UGameInstance* GI)
	{
		if (!GI)
		{
			return 0;
		}

		int32 Count = 0;
		if (const UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
		{
			for (const ECountryType Country : FactoryMgr->GetActiveCountries())
			{
				Count += CountReadyFactoryLines(*FactoryMgr, Country);
			}
		}
		if (const UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
		{
			for (const ECountryType Country : MineMgr->GetActiveCountries())
			{
				Count += CountReadyMineLines(*MineMgr, Country);
			}
		}
		return Count;
	}

	int64 ClaimCountry(UGameInstance* GI, ECountryType Country)
	{
		return AddClaimedSaturated(ClaimCountryFactory(GI, Country), ClaimCountryMine(GI, Country));
	}

	int64 ClaimCountryFactory(UGameInstance* GI, ECountryType Country)
	{
		UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
		return FactoryMgr ? ClaimReadyFactoryLines(*FactoryMgr, Country) : 0;
	}

	int64 ClaimCountryMine(UGameInstance* GI, ECountryType Country)
	{
		UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
		return MineMgr ? ClaimMineLines(*MineMgr, Country) : 0;
	}

	int32 StartLineFromOrder(UGameInstance* GI, ECountryType Country, int32 OrderID, int32 Quantity, FText& OutFailMessage)
	{
		OutFailMessage = NSLOCTEXT("ProductionStartPopup", "StartFailed", "생산을 시작할 수 없습니다");

		UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		UProductionOrderManager* OrderMgr = GI ? GI->GetSubsystem<UProductionOrderManager>() : nullptr;
		UWorldFactoryManager* FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
		UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
		UWorldMapManager* WorldMapMgr = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
		if (!TableMgr || !OrderMgr || !FactoryMgr || !ResourceMgr || Quantity <= 0)
		{
			return 0;
		}

		const FProductionOrder* OrderPtr = OrderMgr->GetOrderByID(OrderID);
		if (!OrderPtr || OrderPtr->IsConsumed())
		{
			return 0;
		}
		// 시작 뒤 ConsumeQuantity 가 배열을 건드릴 수 있어 값으로 들고 간다.
		const FProductionOrder Order = *OrderPtr;
		EResourceType Bottleneck = EResourceType::None;
		bool bSupplyBound = false;
		if (Quantity > OrderMgr->GetMaxProducible(Order, Bottleneck, bSupplyBound))
		{
			OutFailMessage = NSLOCTEXT("ProductionStartPopup", "QuantityUnavailable", "생산 가능한 수량이 달라졌습니다. 재료와 주문 수량을 확인해 주세요");
			return 0;
		}

		bool bRecipeOk = false;
		const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
		if (!bRecipeOk)
		{
			return 0;
		}

		// 표시는 fail-open 이지만 소비는 fail-closed 다 — 재고 주인을 못 얻은 채로 진행하면
		// 차감만 건너뛴 채 라인이 돌아 칩 없이 프리미엄 제품이 나온다.
		int32 InputIndex = 0;
		int32 InputQty = 0;
		const bool bNeedsProductInput = Recipe.GetProductInput(InputIndex, InputQty);
		if (bNeedsProductInput && !WorldMapMgr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OverseasActions] WorldMapManager 없음 — 제품 입력을 차감할 수 없어 생산을 중단"));
			return 0;
		}

		// 표시 데이터 단일 진실 = DT_Project_*
		bool bProjOk = false;
		const FProjectData Proj = TableMgr->GetProjectData(Order.CompanyType, Order.ProjectIndex, bProjOk);
		const FText DisplayName = (bProjOk && !Proj.ProjectName.IsEmpty())
			? Proj.ProjectName : FText::FromString(Order.ProductName);
		TSoftObjectPtr<UTexture2D> IconSoftPtr;
		if (bProjOk)
		{
			IconSoftPtr = Proj.Icon;
		}

		// 레시피의 기본 속도를 넘기고 공장 강화 배율은 StartProduction 에서 한 번 적용한다.
		const float SafeTimePerUnit = (Recipe.ProductionTimeSec > 0.0f) ? Recipe.ProductionTimeSec : 5.0f;
		const float BaseRate = 1.0f / SafeTimePerUnit;

		// StartProduction 이 최종 권위라, 실패하면 차감 전에 빠져나온다.
		const int32 NewLineId = FactoryMgr->StartProduction(
			Country, Order.CompanyType, Order.ProjectIndex, DisplayName, IconSoftPtr, BaseRate,
			static_cast<int64>(Quantity), Order.Grade);
		if (NewLineId <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OverseasActions] StartProduction 실패 — 재료 미차감 (Country=%d Order=%d)"),
				static_cast<int32>(Country), OrderID);
			return NewLineId;
		}

		TArray<TPair<EResourceType, int32>> Materials;
		Recipe.CollectMaterials(Materials);
		for (const TPair<EResourceType, int32>& M : Materials)
		{
			const int64 Spend = static_cast<int64>(M.Value) * static_cast<int64>(Quantity);
			if (Spend > 0)
			{
				ResourceMgr->SpendResource(M.Key, Spend, /*bShouldSave=*/false);
			}
		}

		if (bNeedsProductInput)
		{
			const int64 Spend = static_cast<int64>(InputQty) * static_cast<int64>(Quantity);
			if (!WorldMapMgr->ConsumeProduct(MakeProductKey(Order.CompanyType, InputIndex), Spend, /*bShouldSave=*/false))
			{
				// 상한 검사를 통과했는데 차감이 실패하면 칩 없이 라인이 돈다 — 조용히 넘기면 무한 제작이 된다
				UE_LOG(LogTemp, Warning,
					TEXT("[OverseasActions] 제품 입력 차감 실패 — Company=%d, InputIndex=%d, Spend=%lld (라인 %d 는 이미 시작됨)"),
					static_cast<int32>(Order.CompanyType), InputIndex, Spend, NewLineId);
			}
		}

		OrderMgr->ConsumeQuantity(Order.OrderID, Quantity);
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->RequestDeferredSave();
		}
		OutFailMessage = FText::GetEmpty();
		return NewLineId;
	}

	int64 ClaimAll(UGameInstance* GI)
	{
		if (!GI)
		{
			return 0;
		}

		int64 Claimed = 0;
		if (UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
		{
			for (const ECountryType Country : MineMgr->GetActiveCountries())
			{
				Claimed = AddClaimedSaturated(Claimed, ClaimMineLines(*MineMgr, Country));
			}
		}
		if (UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
		{
			for (const ECountryType Country : FactoryMgr->GetActiveCountries())
			{
				Claimed = AddClaimedSaturated(Claimed, ClaimReadyFactoryLines(*FactoryMgr, Country));
			}
		}
		return Claimed;
	}

	bool IsCountryUnlocked(const UGameInstance* GI, ECountryType Country)
	{
		const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!TableMgr)
		{
			return false;
		}

		bool bRowOk = false;
		const FCountryInfoTable Row = TableMgr->GetCountryInfo(Country, bRowOk);
		return bRowOk && IsRowUnlocked(*GI, Row);
	}

	int32 CountFacilities(const UGameInstance* GI, ECountryType Country)
	{
		if (!GI)
		{
			return 0;
		}

		int32 Total = 0;
		if (const UMineManager* MineMgr = GI->GetSubsystem<UMineManager>())
		{
			Total += MineMgr->GetActiveLineCount(Country);
		}
		if (const UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>())
		{
			Total += FactoryMgr->GetActiveLineCount(Country);
		}
		return Total;
	}

	int64 PreviewSellAll(const UGameInstance* GI, ECountryType Port)
	{
		const UWorldMapManager* Warehouse = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
		const UTradePort* TradePort = GI ? GI->GetSubsystem<UTradePort>() : nullptr;
		if (!Warehouse || !TradePort)
		{
			return 0;
		}

		int64 Money = 0;
		for (const FIntPoint& Key : Warehouse->GetAllProductKeys())
		{
			const int64 Amount = Warehouse->GetProductAmount(Key);
			if (Amount <= 0)
			{
				continue;
			}
			Money += TradePort->PreviewSell(Port, static_cast<ECompanyType>(Key.X), Key.Y, Amount).MoneyGained;
		}
		return Money;
	}

	bool FindBestSalePort(const UGameInstance* GI, ECountryType& OutPort, int64& OutMoney)
	{
		OutPort = ECountryType::None;
		OutMoney = 0;

		const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (!TableMgr)
		{
			return false;
		}

		for (const FCountryInfoTable& Row : TableMgr->GetAllCountryInfos())
		{
			if (!Row.bIsTradeHub || !IsRowUnlocked(*GI, Row))
			{
				continue;
			}

			const int64 Money = PreviewSellAll(GI, Row.CountryType);
			// 같은 값이면 enum 앞쪽 ― 결과를 DT 맵 순회 순서에 맡기지 않는다.
			const bool bBetter = Money > OutMoney
				|| (Money > 0 && Money == OutMoney && Row.CountryType < OutPort);
			if (bBetter)
			{
				OutPort = Row.CountryType;
				OutMoney = Money;
			}
		}
		return OutPort != ECountryType::None;
	}

	int64 SellAllAt(UGameInstance* GI, ECountryType Port)
	{
		UWorldMapManager* Warehouse = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
		UTradePort* TradePort = GI ? GI->GetSubsystem<UTradePort>() : nullptr;
		if (!Warehouse || !TradePort)
		{
			return 0;
		}

		int64 Money = 0;
		for (const FIntPoint& Key : Warehouse->GetAllProductKeys())
		{
			const int64 Amount = Warehouse->GetProductAmount(Key);
			if (Amount <= 0)
			{
				continue;
			}
			Money += TradePort->SellProduct(Port, static_cast<ECompanyType>(Key.X), Key.Y, Amount).MoneyGained;
		}
		return Money;
	}
}
