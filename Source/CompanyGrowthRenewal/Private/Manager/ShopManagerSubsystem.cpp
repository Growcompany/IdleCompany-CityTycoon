#include "Manager/ShopManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Table/ShopItemTable.h"

bool UShopManagerSubsystem::PurchaseItem(FName RowName)
{
	CheckAndPerformResets();

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return false;
	}

	FShopItemTable Row;
	if (!TableMgr->GetShopItemRow(RowName, Row) || Row.Item == EItemType::None)
	{
		return false;
	}

	// 한도 검증
	if (Row.LimitCount > 0 && ShopData.PurchaseCounts.FindRef(RowName) >= Row.LimitCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShopManager] Purchase blocked - limit reached: %s"), *RowName.ToString());
		return false;
	}

	// 재화 차감 (가챠 패턴: 차감은 bShouldSave=false, 마지막에 통합 세이브 1회)
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();

	switch (Row.Currency)
	{
	case EShopCurrency::Money:
		if (!ResourceMgr || !ResourceMgr->HasResource(EResourceType::Money, Row.Price))
		{
			return false;
		}
		ResourceMgr->SpendResource(EResourceType::Money, Row.Price, false);
		break;

	case EShopCurrency::Diamond:
		if (!ResourceMgr || !ResourceMgr->HasResource(EResourceType::Diamond, Row.Price))
		{
			return false;
		}
		ResourceMgr->SpendResource(EResourceType::Diamond, Row.Price, false);
		break;

	case EShopCurrency::Mileage:
		if (!RecruitMgr || !RecruitMgr->SpendMileage(static_cast<int32>(Row.Price), false))
		{
			return false;
		}
		break;

	default:
		return false;
	}

	// 지급
	if (UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
	{
		ItemMgr->AddItem(Row.Item, Row.Quantity, false);
	}

	if (Row.LimitCount > 0)
	{
		ShopData.PurchaseCounts.FindOrAdd(RowName) += 1;
	}

	OnShopStockChanged.Broadcast();
	SaveGameData();

	UE_LOG(LogTemp, Log, TEXT("[ShopManager] Purchased %s (item=%d x%d)"), *RowName.ToString(), (int32)Row.Item, Row.Quantity);
	return true;
}

bool UShopManagerSubsystem::CanAfford(FName RowName) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return false;
	}

	FShopItemTable Row;
	if (!TableMgr->GetShopItemRow(RowName, Row))
	{
		return false;
	}

	const UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	const URecruitmentManagerSubsystem* RecruitMgr = GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>();

	switch (Row.Currency)
	{
	case EShopCurrency::Money:   return ResourceMgr && ResourceMgr->HasResource(EResourceType::Money, Row.Price);
	case EShopCurrency::Diamond: return ResourceMgr && ResourceMgr->HasResource(EResourceType::Diamond, Row.Price);
	case EShopCurrency::Mileage: return RecruitMgr && RecruitMgr->GetMileagePoints() >= static_cast<int32>(Row.Price);
	default: return false;
	}
}

int32 UShopManagerSubsystem::GetRemainingCount(FName RowName) const
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return 0;
	}

	FShopItemTable Row;
	if (!TableMgr->GetShopItemRow(RowName, Row))
	{
		return 0;
	}
	if (Row.LimitCount <= 0)
	{
		return -1;
	}

	return FMath::Max(0, Row.LimitCount - ShopData.PurchaseCounts.FindRef(RowName));
}

bool UShopManagerSubsystem::ManualResetDaily()
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr || !ResourceMgr->HasResource(EResourceType::Diamond, ManualResetDiamondCost))
	{
		return false;
	}

	ResourceMgr->SpendResource(EResourceType::Diamond, ManualResetDiamondCost, false);
	ResetTabCounts(EShopTab::Daily);
	ShopData.LastDailyReset = FDateTime::Now();

	OnShopStockChanged.Broadcast();
	SaveGameData();
	return true;
}

void UShopManagerSubsystem::CheckAndPerformResets()
{
	const FDateTime Now = FDateTime::Now();
	const FDateTime TodayMidnight(Now.GetYear(), Now.GetMonth(), Now.GetDay());
	// EDayOfWeek: Monday=0 — 이번 주 월요일 00:00
	const FDateTime WeekStart = TodayMidnight - FTimespan::FromDays(static_cast<int32>(Now.GetDayOfWeek()));

	bool bChanged = false;

	if (ShopData.LastDailyReset < TodayMidnight)
	{
		ResetTabCounts(EShopTab::Daily);
		ShopData.LastDailyReset = Now;
		bChanged = true;
	}

	if (ShopData.LastWeeklyReset < WeekStart)
	{
		ResetTabCounts(EShopTab::Weekly);
		ShopData.LastWeeklyReset = Now;
		bChanged = true;
	}

	if (bChanged)
	{
		OnShopStockChanged.Broadcast();
		SaveGameData();
	}
}

FTimespan UShopManagerSubsystem::GetTimeUntilDailyReset() const
{
	const FDateTime Now = FDateTime::Now();
	const FDateTime NextMidnight = FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay()) + FTimespan::FromDays(1);
	return NextMidnight - Now;
}

void UShopManagerSubsystem::ResetTabCounts(EShopTab Tab)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	for (const FName& Name : TableMgr->GetShopItemRowsForTab(Tab))
	{
		ShopData.PurchaseCounts.Remove(Name);
	}
}

void UShopManagerSubsystem::SaveGameData()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}
