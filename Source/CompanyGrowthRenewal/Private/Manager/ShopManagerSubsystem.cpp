#include "Manager/ShopManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/GameClockSubsystem.h"
#include "Data/GameClockRules.h"
#include "Manager/QuestSubsystem.h"
#include "Table/ShopItemTable.h"
#include "Utils/EquipmentTicketShopRules.h"

namespace
{
	// 장비권 기간 키도 서버 시간 권위를 따른다(무효 시 로컬 UTC 폴백은 상점만 허용)
	FDateTime ClockUtcNow(UGameInstance* GI)
	{
		const UGameClockSubsystem* Clock = GI ? GI->GetSubsystem<UGameClockSubsystem>() : nullptr;
		return Clock ? FDateTime::FromUnixTimestamp(Clock->GetServerNowUnix()) : FDateTime::UtcNow();
	}
}

bool UShopManagerSubsystem::PurchaseItem(FName RowName)
{
	if (bTicketPurchaseExecuting) return false;
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

    if (EquipmentTicketShopRules::IsTicket(Row.Item))
    {
        FEquipmentTicketShopRequest Request;
        FEquipmentTicketShopQuote Quote;
        FName Error;
        return PrepareEquipmentTicketPurchase(RowName, Request, Quote, Error) && PurchaseEquipmentTickets(Request).bCommitted;
    }
    // 번들은 Item 이 None 이고 BundleItems 가 내용물을 들고 있다
    if (Row.Item == EItemType::None && !Row.IsBundle()) return false;
    CheckAndPerformResets();

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

	// 지급 — 번들이면 구성품 전부, 단일이면 한 칸 (세이브는 아래에서 1회)
	if (UItemInventoryManager* ItemMgr = GetGameInstance()->GetSubsystem<UItemInventoryManager>())
	{
		for (const FShopBundleEntry& Grant : Row.GetGrantEntries())
		{
			if (Grant.Item != EItemType::None && Grant.Quantity > 0)
			{
				ItemMgr->AddItem(Grant.Item, Grant.Quantity, false);
			}
		}
	}

	if (Row.LimitCount > 0)
	{
		ShopData.PurchaseCounts.FindOrAdd(RowName) += 1;
	}

	if (UQuestSubsystem* QuestMgr = GetGameInstance()->GetSubsystem<UQuestSubsystem>()) QuestMgr->ReportCounter(EQuestCounter::ShopPurchase, 1);

	OnShopStockChanged.Broadcast();
	SaveGameData();

	UE_LOG(LogTemp, Log, TEXT("[ShopManager] Purchased %s (item=%d x%d)"), *RowName.ToString(), (int32)Row.Item, Row.Quantity);
	return true;
}

bool UShopManagerSubsystem::QuoteEquipmentTickets(FName RowName, FEquipmentTicketShopQuote& OutQuote, FName& OutError) const
{
    OutQuote = FEquipmentTicketShopQuote();
    OutError = NAME_None;
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>();
    FShopItemTable Row;
    if (!Tables || !Resources)
    {
        OutError = TEXT("StateNotReady");
        return false;
    }
    if (!Tables->GetShopItemRow(RowName, Row))
    {
        OutError = TEXT("InvalidProduct");
        return false;
    }
    if (!EquipmentTicketShopRules::ValidateRow(RowName, Row, OutError)) return false;
    OutQuote.RowName = RowName;
    OutQuote.Item = Row.Item;
    OutQuote.Quantity = Row.Quantity;
    OutQuote.Currency = Row.Currency;
    OutQuote.Price = Row.Price;
    OutQuote.LimitCount = Row.LimitCount;
    OutQuote.Tab = Row.Tab;
    OutQuote.RemainingCount = EquipmentTicketShopRules::RemainingCount(RowName, Row, ShopData, ClockUtcNow(GetGameInstance()));
    OutQuote.bCanAfford = Resources->GetResourceAmount(Row.Currency == EShopCurrency::Money ? EResourceType::Money : EResourceType::Diamond) >= Row.Price;
    OutQuote.CatalogFingerprint = EquipmentTicketShopRules::CatalogFingerprint(RowName, Row);
    return true;
}

bool UShopManagerSubsystem::PrepareEquipmentTicketPurchase(FName RowName, FEquipmentTicketShopRequest& OutRequest, FEquipmentTicketShopQuote& OutQuote, FName& OutError) const
{
    OutRequest = FEquipmentTicketShopRequest();
    if (!QuoteEquipmentTickets(RowName, OutQuote, OutError)) return false;
    const USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
    const UItemInventoryManager* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
    if (bTicketPurchaseExecuting || !SaveMgr || !SaveMgr->CanCommitEquipmentTransaction() || !Items) OutError = TEXT("StateNotReady");
    else if (OutQuote.RemainingCount == 0) OutError = TEXT("PurchaseLimit");
    else if (!OutQuote.bCanAfford) OutError = TEXT("InsufficientCurrency");
    else if (Items->GetItemCount(OutQuote.Item) < 0 || Items->GetItemCount(OutQuote.Item) > MAX_int32 - OutQuote.Quantity) OutError = TEXT("BalanceOverflow");
    if (!OutError.IsNone()) return false;
    OutRequest.OperationId = FGuid::NewGuid();
    OutRequest.RowName = RowName;
    OutRequest.CatalogFingerprint = OutQuote.CatalogFingerprint;
    return true;
}

FEquipmentTicketShopResult UShopManagerSubsystem::PurchaseEquipmentTickets(const FEquipmentTicketShopRequest& Request)
{
    FEquipmentTicketShopResult Result;
    Result.OperationId = Request.OperationId;
    if (EquipmentTicketShopRules::TryReplay(Request, ShopData, Result))
    {
        if (!Result.bCommitted) UE_LOG(LogTemp, Warning, TEXT("[TicketShop] Rejected %s: %s"), *Request.OperationId.ToString(), *Result.ErrorCode.ToString());
        return Result;
    }
    if (bTicketPurchaseExecuting) Result.ErrorCode = TEXT("Busy");
    else
    {
        TGuardValue<bool> PurchaseGuard(bTicketPurchaseExecuting, true);
        USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
        const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        FShopItemTable Row;
        FGameSaveData Candidate;
        if (!SaveMgr || !SaveMgr->CanCommitEquipmentTransaction() || !Tables) Result.ErrorCode = TEXT("StateNotReady");
        else if (!Tables->GetShopItemRow(Request.RowName, Row)) Result.ErrorCode = TEXT("InvalidProduct");
        else if (!SaveMgr->BuildCurrentSnapshot(Candidate)) Result.ErrorCode = TEXT("StateNotReady");
        else if (EquipmentTicketShopRules::BuildPurchaseCandidate(Request, Row, ClockUtcNow(GetGameInstance()), Candidate, Result))
        {
            if (!SaveMgr->CommitEquipmentSnapshot(Candidate))
            {
                Result = FEquipmentTicketShopResult();
                Result.OperationId = Request.OperationId;
                Result.ErrorCode = TEXT("SaveFailed");
            }
        }
    }
    if (!Result.bCommitted) { UE_LOG(LogTemp, Warning, TEXT("[TicketShop] Rejected %s: %s"), *Request.OperationId.ToString(), *Result.ErrorCode.ToString()); }
    else { UE_LOG(LogTemp, Log, TEXT("[TicketShop] Purchased %s: item=%d quantity=%d operation=%s"), *Request.RowName.ToString(), int32(Result.Item), Result.Quantity, *Request.OperationId.ToString()); }
    // 장비권 구매도 상점 구매 카운터에 셈(재생 경로는 위에서 조기 반환되어 중복 없음)
    if (Result.bCommitted) { if (UQuestSubsystem* QuestMgr = GetGameInstance()->GetSubsystem<UQuestSubsystem>()) QuestMgr->ReportCounter(EQuestCounter::ShopPurchase, 1); }
    return Result;
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

    if (EquipmentTicketShopRules::IsTicket(Row.Item))
    {
        FEquipmentTicketShopQuote Quote;
        FName Error;
        return QuoteEquipmentTickets(RowName, Quote, Error) && Quote.bCanAfford;
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
    if (EquipmentTicketShopRules::IsTicket(Row.Item))
    {
        FEquipmentTicketShopQuote Quote;
        FName Error;
        return QuoteEquipmentTickets(RowName, Quote, Error) ? Quote.RemainingCount : 0;
    }
	if (Row.LimitCount <= 0)
	{
		return -1;
	}

	return FMath::Max(0, Row.LimitCount - ShopData.PurchaseCounts.FindRef(RowName));
}

void UShopManagerSubsystem::CheckAndPerformResets()
{
	if (bTicketPurchaseExecuting) return;
	UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	if (!Clock) return;

	// 서버 시간이 아직 없으면 로컬 UTC+9 키로 판정한다 — 상점만 허용되는 관대함(출석/퀘스트는 수령 자체를 막는다)
	const FString DayKey = Clock->GetDayKey();
	const FString WeekKey = Clock->GetWeekKey();
	bool bChanged = false;

	if (ShopData.LastDailyKey != DayKey)
	{
		ResetTabCounts(EShopTab::Daily);
		ShopData.LastDailyKey = DayKey;
		bChanged = true;
	}

	if (ShopData.LastWeeklyKey != WeekKey)
	{
		ResetTabCounts(EShopTab::Weekly);
		ShopData.LastWeeklyKey = WeekKey;
		bChanged = true;
	}

	if (bChanged)
	{
		// 로컬 시각 리셋은 기기 시간 조작에 열려 있다 — 리셋마다 한 줄 남겨 사후 추적이 되게 한다
		if (!Clock->IsServerTimeValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[Shop] 로컬 시간 기준 리셋(bAuthoritative=false) day=%s week=%s"), *DayKey, *WeekKey);
		}
		OnShopStockChanged.Broadcast();
		SaveGameData();
	}
}

FTimespan UShopManagerSubsystem::GetTimeUntilDailyReset() const
{
	const UGameClockSubsystem* Clock = GetGameInstance()->GetSubsystem<UGameClockSubsystem>();
	// 시계가 없어도 0(=지금 리셋)을 돌려주면 UI 가 계속 "리셋 중"으로 보인다 — 로컬 KST 자정까지로 폴백
	return FTimespan::FromSeconds(static_cast<double>(
		Clock ? Clock->GetSecondsUntilDayRoll() : FGameClockRules::SecondsUntilNextDayKst(FDateTime::UtcNow().ToUnixTimestamp())));
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
        FShopItemTable Row;
        if (TableMgr->GetShopItemRow(Name, Row) && !EquipmentTicketShopRules::IsTicket(Row.Item)) ShopData.PurchaseCounts.Remove(Name);
	}
}

void UShopManagerSubsystem::SaveGameData()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}
