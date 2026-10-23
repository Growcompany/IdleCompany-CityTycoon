// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/ItemType.h"

class FJsonObject;

/**
 * 서버 경제 응답 타입 + 에러코드 상수 (설계서 §6.0 봉투 · §6.3 계약 · 부록 A).
 * 반사(USTRUCT) 를 붙이지 않는다 ― 블루프린트 노출 대상이 아니고 int64/TSharedPtr 를 그대로 담는다.
 */

/** `state.wallet`. diamond 는 int64 로 받는다(로컬 ResourceBank 가 int64) */
struct FServerWallet
{
	int64 Diamond = 0;
	int32 Mileage = 0;
};

/** `state.shop`. DailyKey/WeeklyKey 는 이 카운트가 유효한 **현재 서버 키** 다(만료 키가 아님) */
struct FServerShopState
{
	TMap<FName, int32> Counts;
	FString DailyKey;
	FString WeeklyKey;
};

/** `state.attendance` */
struct FServerAttendanceState
{
	int32 CycleDay = 0;
	FString LastClaimDayKey;
	int32 TotalDays = 0;
};

/** `state.ranking`. marketCapStr 만 십진 문자열, weeklyRevenue 는 숫자 */
struct FServerRankingState
{
	int64 MarketCap = 0;
	bool bHasMarketCap = false;
	int32 WeeklyRevenue = 0;
	FString WeeklyKey;
};

/**
 * 서버 `state` 봉투 1건.
 * ⚠ 절(section)마다 존재 플래그가 따로 있다 ― `/wallet/spend` 는 `{wallet,dayKey,weekKey}` 만,
 *   `/item/consume` 은 `{inventory,dayKey,weekKey}` 만 돌려준다. 없는 절을 "빈 값"으로 적용하면 지갑·인벤토리가 날아간다.
 */
struct FServerEconomyState
{
	bool bValid = false;

	bool bHasWallet = false;
	bool bHasInventory = false;
	bool bHasShop = false;
	bool bHasAttendance = false;
	bool bHasRanking = false;

	FServerWallet Wallet;

	/** 키 = `EItemType` 식별자 문자열. 서버는 **0 개 키를 싣지 않는다**(§4.2) ― 적용은 맵 교체다 */
	TMap<FName, int32> Inventory;

	FServerShopState Shop;
	FServerAttendanceState Attendance;
	FServerRankingState Ranking;

	FString DayKey;
	FString WeekKey;
};

/** 변경 요청 1건의 결과. Message 는 서버 영문 개발자용이라 화면에 그대로 쓰지 않는다(DT 문구로 치환) */
struct FCloudOpResult
{
	bool bOk = false;
	FName Code;
	FString Message;

	/** true = 이미 처리된 요청 ― 로컬 효과(가챠 RNG 등)를 다시 적용하지 마라 */
	bool bReplayed = false;

	int32 HttpStatus = 0;

	/** 엔드포인트별 `result` 객체(없으면 무효) */
	TSharedPtr<FJsonObject> Result;
};

/** `meta` 스칼라 1개. JSON 종류를 보존한다 ― `meta.count` 를 문자열로 보내면 서버가 400 을 준다 */
struct FCloudMetaValue
{
	enum class EKind : uint8
	{
		Number,
		String,
		Bool
	};

	EKind Kind = EKind::Number;
	double NumberValue = 0.0;
	FString StringValue;
	bool bBoolValue = false;

	static FCloudMetaValue FromInt(int64 InValue)
	{
		FCloudMetaValue Value;
		Value.Kind = EKind::Number;
		Value.NumberValue = static_cast<double>(InValue);
		return Value;
	}

	static FCloudMetaValue FromString(const FString& InValue)
	{
		FCloudMetaValue Value;
		Value.Kind = EKind::String;
		Value.StringValue = InValue;
		return Value;
	}

	static FCloudMetaValue FromBool(bool bInValue)
	{
		FCloudMetaValue Value;
		Value.Kind = EKind::Bool;
		Value.bBoolValue = bInValue;
		return Value;
	}
};

using FCloudMeta = TMap<FString, FCloudMetaValue>;

/**
 * 서버 경제 요청 1건의 **호출부(UI)** 결과.
 * bOk = 효과가 실제로 적용됐는가 / Code = 실패 원인(성공·원인불명이면 `NAME_None`).
 * 코드를 함께 주지 않으면 호출부가 "부족합니다" 한 문구로 서버·네트워크 실패까지 덮는다 ―
 * 플레이어는 다이아가 충분한데 "보석이 부족합니다" 를 본다.
 */
DECLARE_DELEGATE_TwoParams(FOnCloudEconomyOpDone, bool /*bOk*/, FName /*Code*/);

/** `/wallet/spend`·`/wallet/credit` 의 통화 식별자(설계서 §6.3). reason 의 통화와 다르면 서버가 400 */
namespace CloudCurrency
{
	COMPANYGROWTHRENEWAL_API extern const FName Diamond;
	COMPANYGROWTHRENEWAL_API extern const FName Mileage;
}

/** 부록 A 에러코드 ― 문자열은 서버 `STATUS_BY_CODE` 와 바이트 단위로 같다. 임의 코드 추가 금지 */
namespace CloudErr
{
	COMPANYGROWTHRENEWAL_API extern const FName AuthRequired;
	COMPANYGROWTHRENEWAL_API extern const FName AuthInvalid;
	COMPANYGROWTHRENEWAL_API extern const FName AuthExpired;
	COMPANYGROWTHRENEWAL_API extern const FName PlayFabTicketInvalid;
	COMPANYGROWTHRENEWAL_API extern const FName PlayFabUnavailable;
	COMPANYGROWTHRENEWAL_API extern const FName BadRequest;
	COMPANYGROWTHRENEWAL_API extern const FName OpIdRequired;
	COMPANYGROWTHRENEWAL_API extern const FName OpIdMismatch;
	COMPANYGROWTHRENEWAL_API extern const FName Forbidden;
	COMPANYGROWTHRENEWAL_API extern const FName CatalogMismatch;
	COMPANYGROWTHRENEWAL_API extern const FName CatalogInvalid;
	COMPANYGROWTHRENEWAL_API extern const FName InsufficientFunds;
	COMPANYGROWTHRENEWAL_API extern const FName InsufficientItems;
	COMPANYGROWTHRENEWAL_API extern const FName PurchaseLimit;
	COMPANYGROWTHRENEWAL_API extern const FName BalanceOverflow;
	COMPANYGROWTHRENEWAL_API extern const FName AlreadyClaimed;
	COMPANYGROWTHRENEWAL_API extern const FName NotClaimable;
	COMPANYGROWTHRENEWAL_API extern const FName ClaimKeyInvalid;
	COMPANYGROWTHRENEWAL_API extern const FName CouponNotFound;
	COMPANYGROWTHRENEWAL_API extern const FName CouponExpired;
	COMPANYGROWTHRENEWAL_API extern const FName CouponDisabled;
	COMPANYGROWTHRENEWAL_API extern const FName CouponExhausted;
	COMPANYGROWTHRENEWAL_API extern const FName CouponAlreadyRedeemed;
	COMPANYGROWTHRENEWAL_API extern const FName CouponRateLimited;
	COMPANYGROWTHRENEWAL_API extern const FName ChatRateLimited;
	COMPANYGROWTHRENEWAL_API extern const FName RankingThrottled;
	COMPANYGROWTHRENEWAL_API extern const FName Maintenance;
	COMPANYGROWTHRENEWAL_API extern const FName Internal;

	/** 부록 A 전 코드. 문구 커버리지 테스트가 이 목록을 돈다 */
	COMPANYGROWTHRENEWAL_API const TArray<FName>& AllCodes();

	/** 부록 A "영수증(영구)" = 예 ― 같은 opId 재시도에 같은 실패가 온다(조건 해소로 뚫리지 않는다) */
	COMPANYGROWTHRENEWAL_API bool IsPermanentFailure(FName Code);
}

/** int64 십진 문자열 왕복 + `state` 파싱. 전부 순수 함수라 자동화가 매니저 없이 검증한다 */
struct COMPANYGROWTHRENEWAL_API FCloudEconomyData
{
	/**
	 * 십진 문자열 → int64. 실패를 0 으로 삼키지 않는다 ― 시총이 조용히 리셋된 것처럼 보인다.
	 * 실패 시 Out 을 건드리지 않는다. 공백·부호 관용·지수·16진수 전부 거부.
	 */
	static bool ParseInt64(const FString& In, int64& Out);

	static FString Int64ToString(int64 In);

	/** 응답 `state` → 구조체. 절 존재 플래그를 채운다. marketCapStr 가 있는데 파싱 실패면 false */
	static bool ParseState(const TSharedPtr<FJsonObject>& StateJson, FServerEconomyState& Out, FString& OutError);

	/** 미지 아이템 키·음수 수량 검사. 적용 전에 통과해야 매니저를 건드린다(반쪽 적용 방지) */
	static bool ValidateState(const FServerEconomyState& State, FString& OutError);

	/** `EItemType` 식별자 문자열 ↔ enum. 표시명이 아니라 식별자다(서버 계약) */
	static bool ItemTypeFromName(FName Identifier, EItemType& OutItemType);
	static FName ItemTypeToName(EItemType ItemType);

	/**
	 * 서버 인벤토리 → 로컬에 그대로 넣을 **완전한** 목표 맵.
	 * 응답에 없는 로컬 키는 0 을 명시해 담는다 ― 키를 빼면 `OnItemChanged` 가 안 울려 UI 에 유령 수량이 남는다.
	 */
	static TMap<EItemType, int32> BuildInventoryTarget(
		const TMap<EItemType, int32>& CurrentItems,
		const TMap<FName, int32>& ServerInventory);

	/** 서버 opId 정규식(소문자 하이픈 GUID) 을 통과하는 새 opId */
	static FString MakeOpId();
};
