// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/CloudEconomyData.h"

#include "Dom/JsonObject.h"
#include "UObject/Class.h"
#include "UObject/ReflectedTypeAccessors.h"

namespace CloudCurrency
{
	const FName Diamond(TEXT("diamond"));
	const FName Mileage(TEXT("mileage"));
}

namespace CloudErr
{
	const FName AuthRequired(TEXT("AUTH_REQUIRED"));
	const FName AuthInvalid(TEXT("AUTH_INVALID"));
	const FName AuthExpired(TEXT("AUTH_EXPIRED"));
	const FName PlayFabTicketInvalid(TEXT("PLAYFAB_TICKET_INVALID"));
	const FName PlayFabUnavailable(TEXT("PLAYFAB_UNAVAILABLE"));
	const FName BadRequest(TEXT("BAD_REQUEST"));
	const FName OpIdRequired(TEXT("OP_ID_REQUIRED"));
	const FName OpIdMismatch(TEXT("OP_ID_MISMATCH"));
	const FName Forbidden(TEXT("FORBIDDEN"));
	const FName CatalogMismatch(TEXT("CATALOG_MISMATCH"));
	const FName CatalogInvalid(TEXT("CATALOG_INVALID"));
	const FName InsufficientFunds(TEXT("INSUFFICIENT_FUNDS"));
	const FName InsufficientItems(TEXT("INSUFFICIENT_ITEMS"));
	const FName PurchaseLimit(TEXT("PURCHASE_LIMIT"));
	const FName BalanceOverflow(TEXT("BALANCE_OVERFLOW"));
	const FName AlreadyClaimed(TEXT("ALREADY_CLAIMED"));
	const FName NotClaimable(TEXT("NOT_CLAIMABLE"));
	const FName ClaimKeyInvalid(TEXT("CLAIM_KEY_INVALID"));
	const FName CouponNotFound(TEXT("COUPON_NOT_FOUND"));
	const FName CouponExpired(TEXT("COUPON_EXPIRED"));
	const FName CouponDisabled(TEXT("COUPON_DISABLED"));
	const FName CouponExhausted(TEXT("COUPON_EXHAUSTED"));
	const FName CouponAlreadyRedeemed(TEXT("COUPON_ALREADY_REDEEMED"));
	const FName CouponRateLimited(TEXT("COUPON_RATE_LIMITED"));
	const FName ChatRateLimited(TEXT("CHAT_RATE_LIMITED"));
	const FName RankingThrottled(TEXT("RANKING_THROTTLED"));
	const FName Maintenance(TEXT("MAINTENANCE"));
	const FName Internal(TEXT("INTERNAL"));

	const TArray<FName>& AllCodes()
	{
		static const TArray<FName> Codes = {
			AuthRequired, AuthInvalid, AuthExpired, PlayFabTicketInvalid, PlayFabUnavailable,
			BadRequest, OpIdRequired, OpIdMismatch, Forbidden, CatalogMismatch, CatalogInvalid,
			InsufficientFunds, InsufficientItems, PurchaseLimit, BalanceOverflow, AlreadyClaimed,
			NotClaimable, ClaimKeyInvalid, CouponNotFound, CouponExpired, CouponDisabled,
			CouponExhausted, CouponAlreadyRedeemed, CouponRateLimited, ChatRateLimited,
			RankingThrottled,
			Maintenance, Internal
		};
		return Codes;
	}

	bool IsPermanentFailure(FName Code)
	{
		return Code == BadRequest
			|| Code == CatalogMismatch
			|| Code == BalanceOverflow
			|| Code == AlreadyClaimed
			|| Code == ClaimKeyInvalid;
	}
}

bool FCloudEconomyData::ParseInt64(const FString& In, int64& Out)
{
	const int32 Length = In.Len();
	if (Length == 0)
	{
		return false;
	}

	int32 Index = 0;
	bool bNegative = false;
	if (In[0] == TEXT('-'))
	{
		bNegative = true;
		Index = 1;
	}

	// 부호 뒤가 비었거나 숫자가 하나도 없으면 실패. '+'·공백·'0x'·지수 표기는 서버가 쓰지 않는다
	if (Index >= Length)
	{
		return false;
	}

	const uint64 Limit = bNegative
		? static_cast<uint64>(MAX_int64) + 1ULL
		: static_cast<uint64>(MAX_int64);

	uint64 Magnitude = 0;
	for (; Index < Length; ++Index)
	{
		const TCHAR Character = In[Index];
		if (Character < TEXT('0') || Character > TEXT('9'))
		{
			return false;
		}

		const uint64 Digit = static_cast<uint64>(Character - TEXT('0'));
		if (Magnitude > (Limit - Digit) / 10ULL)
		{
			return false;
		}
		Magnitude = Magnitude * 10ULL + Digit;
	}

	if (bNegative)
	{
		Out = (Magnitude == static_cast<uint64>(MAX_int64) + 1ULL)
			? MIN_int64
			: -static_cast<int64>(Magnitude);
	}
	else
	{
		Out = static_cast<int64>(Magnitude);
	}

	return true;
}

FString FCloudEconomyData::Int64ToString(int64 In)
{
	return FString::Printf(TEXT("%lld"), In);
}

bool FCloudEconomyData::ItemTypeFromName(FName Identifier, EItemType& OutItemType)
{
	if (Identifier.IsNone())
	{
		return false;
	}

	const UEnum* ItemEnum = StaticEnum<EItemType>();
	if (!ItemEnum)
	{
		return false;
	}

	const int64 Value = ItemEnum->GetValueByNameString(Identifier.ToString());
	if (Value == INDEX_NONE)
	{
		return false;
	}

	// None/Count 는 실물 아이템이 아니다 ― 서버가 보내면 계약 위반이다
	if (Value <= static_cast<int64>(EItemType::None) || Value >= static_cast<int64>(EItemType::Count))
	{
		return false;
	}

	OutItemType = static_cast<EItemType>(Value);
	return true;
}

FName FCloudEconomyData::ItemTypeToName(EItemType ItemType)
{
	const UEnum* ItemEnum = StaticEnum<EItemType>();
	if (!ItemEnum)
	{
		return NAME_None;
	}

	return FName(*ItemEnum->GetNameStringByValue(static_cast<int64>(ItemType)));
}

FString FCloudEconomyData::MakeOpId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

namespace
{
	bool ReadInt32Field(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out)
	{
		double Raw = 0.0;
		if (!Object->TryGetNumberField(Field, Raw))
		{
			return false;
		}
		// 서버 정수는 이미 정확한 double 이다 ― 명시 캐스트로 축소변환을 드러내고 범위만 지킨다
		Out = static_cast<int32>(FMath::Clamp(Raw, static_cast<double>(MIN_int32), static_cast<double>(MAX_int32)));
		return true;
	}

	/** 서버 정수 맵(`counts`/`inventory`) → FName 맵. 비정수·음수는 담지 않고 알린다 */
	bool ReadCountMap(const TSharedPtr<FJsonObject>& Object, TMap<FName, int32>& Out, FString& OutError)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number)
			{
				OutError = FString::Printf(TEXT("'%s' 값이 숫자가 아님"), *Pair.Key);
				return false;
			}

			const double Raw = Pair.Value->AsNumber();
			const int32 Count = static_cast<int32>(FMath::Clamp(Raw, static_cast<double>(MIN_int32), static_cast<double>(MAX_int32)));
			if (Count < 0)
			{
				OutError = FString::Printf(TEXT("'%s' 수량이 음수(%d)"), *Pair.Key, Count);
				return false;
			}

			Out.Add(FName(*Pair.Key), Count);
		}
		return true;
	}
}

bool FCloudEconomyData::ParseState(const TSharedPtr<FJsonObject>& StateJson, FServerEconomyState& Out, FString& OutError)
{
	OutError.Reset();

	if (!StateJson.IsValid())
	{
		OutError = TEXT("state 객체가 없음");
		return false;
	}

	FServerEconomyState Parsed;

	const TSharedPtr<FJsonObject>* Section = nullptr;

	if (StateJson->TryGetObjectField(TEXT("wallet"), Section) && Section && Section->IsValid())
	{
		double RawDiamond = 0.0;
		if ((*Section)->TryGetNumberField(TEXT("diamond"), RawDiamond))
		{
			Parsed.Wallet.Diamond = static_cast<int64>(RawDiamond);
		}
		ReadInt32Field(*Section, TEXT("mileage"), Parsed.Wallet.Mileage);
		Parsed.bHasWallet = true;
	}

	if (StateJson->TryGetObjectField(TEXT("inventory"), Section) && Section && Section->IsValid())
	{
		if (!ReadCountMap(*Section, Parsed.Inventory, OutError))
		{
			OutError = FString::Printf(TEXT("state.inventory %s"), *OutError);
			return false;
		}
		Parsed.bHasInventory = true;
	}

	if (StateJson->TryGetObjectField(TEXT("shop"), Section) && Section && Section->IsValid())
	{
		const TSharedPtr<FJsonObject>* CountsObject = nullptr;
		if ((*Section)->TryGetObjectField(TEXT("counts"), CountsObject) && CountsObject && CountsObject->IsValid())
		{
			if (!ReadCountMap(*CountsObject, Parsed.Shop.Counts, OutError))
			{
				OutError = FString::Printf(TEXT("state.shop.counts %s"), *OutError);
				return false;
			}
		}
		(*Section)->TryGetStringField(TEXT("dailyKey"), Parsed.Shop.DailyKey);
		(*Section)->TryGetStringField(TEXT("weeklyKey"), Parsed.Shop.WeeklyKey);
		Parsed.bHasShop = true;
	}

	if (StateJson->TryGetObjectField(TEXT("attendance"), Section) && Section && Section->IsValid())
	{
		ReadInt32Field(*Section, TEXT("cycleDay"), Parsed.Attendance.CycleDay);
		ReadInt32Field(*Section, TEXT("totalDays"), Parsed.Attendance.TotalDays);
		(*Section)->TryGetStringField(TEXT("lastClaimDayKey"), Parsed.Attendance.LastClaimDayKey);
		Parsed.bHasAttendance = true;
	}

	if (StateJson->TryGetObjectField(TEXT("ranking"), Section) && Section && Section->IsValid())
	{
		FString MarketCapString;
		if ((*Section)->TryGetStringField(TEXT("marketCapStr"), MarketCapString))
		{
			// 파싱 실패를 0 으로 삼키면 시총이 조용히 리셋된 것처럼 보인다
			if (!ParseInt64(MarketCapString, Parsed.Ranking.MarketCap))
			{
				OutError = FString::Printf(TEXT("state.ranking.marketCapStr 파싱 실패 ('%s')"), *MarketCapString);
				return false;
			}
			Parsed.Ranking.bHasMarketCap = true;
		}
		ReadInt32Field(*Section, TEXT("weeklyRevenue"), Parsed.Ranking.WeeklyRevenue);
		(*Section)->TryGetStringField(TEXT("weeklyKey"), Parsed.Ranking.WeeklyKey);
		Parsed.bHasRanking = true;
	}

	StateJson->TryGetStringField(TEXT("dayKey"), Parsed.DayKey);
	StateJson->TryGetStringField(TEXT("weekKey"), Parsed.WeekKey);

	Parsed.bValid = true;
	Out = MoveTemp(Parsed);
	return true;
}

bool FCloudEconomyData::ValidateState(const FServerEconomyState& State, FString& OutError)
{
	OutError.Reset();

	if (!State.bValid)
	{
		OutError = TEXT("파싱되지 않은 state");
		return false;
	}

	if (State.bHasWallet)
	{
		if (State.Wallet.Diamond < 0)
		{
			OutError = FString::Printf(TEXT("wallet.diamond 음수(%lld)"), State.Wallet.Diamond);
			return false;
		}
		if (State.Wallet.Mileage < 0)
		{
			OutError = FString::Printf(TEXT("wallet.mileage 음수(%d)"), State.Wallet.Mileage);
			return false;
		}
	}

	if (State.bHasInventory)
	{
		for (const TPair<FName, int32>& Pair : State.Inventory)
		{
			EItemType ItemType = EItemType::None;
			if (!ItemTypeFromName(Pair.Key, ItemType))
			{
				OutError = FString::Printf(TEXT("inventory 미지 아이템 키 '%s'"), *Pair.Key.ToString());
				return false;
			}
			if (Pair.Value < 0)
			{
				OutError = FString::Printf(TEXT("inventory '%s' 수량 음수(%d)"), *Pair.Key.ToString(), Pair.Value);
				return false;
			}
		}
	}

	if (State.bHasAttendance && (State.Attendance.CycleDay < 0 || State.Attendance.TotalDays < 0))
	{
		OutError = TEXT("attendance 음수 값");
		return false;
	}

	return true;
}

TMap<EItemType, int32> FCloudEconomyData::BuildInventoryTarget(
	const TMap<EItemType, int32>& CurrentItems,
	const TMap<FName, int32>& ServerInventory)
{
	TMap<EItemType, int32> Target;

	for (const TPair<FName, int32>& Pair : ServerInventory)
	{
		EItemType ItemType = EItemType::None;
		if (ItemTypeFromName(Pair.Key, ItemType))
		{
			Target.Add(ItemType, Pair.Value);
		}
	}

	// 서버는 0 개 키를 싣지 않는다 ― 응답에 없는 로컬 보유분은 소모돼 사라진 것이므로 0 을 명시한다
	for (const TPair<EItemType, int32>& Pair : CurrentItems)
	{
		if (!Target.Contains(Pair.Key))
		{
			Target.Add(Pair.Key, 0);
		}
	}

	return Target;
}
