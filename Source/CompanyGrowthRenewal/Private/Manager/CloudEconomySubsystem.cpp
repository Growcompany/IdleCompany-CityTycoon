// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/CloudEconomySubsystem.h"

#include "Global/CGDevSettings.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/FirebaseAuthSubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ShopManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Data/FacilityInstantFinishRules.h"
#include "Table/CloudErrorTextTable.h"
#include "Utils/CloudHttp.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* const CloudEconomyTag = TEXT("CloudEconomy");

	// 익명 네임스페이스라도 유니티 빌드에서 같은 blob 에 묶이면 전역처럼 충돌한다 - 파일별 고유 이름을 쓴다
	FString SerializeEconomyJson(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Serialized;
	}

	/** meta 는 평평한 스칼라만 허용된다(§6.3). 종류를 보존해야 `meta.count` 가 숫자로 간다 */
	TSharedPtr<FJsonObject> BuildMetaObject(const FCloudMeta& Meta)
	{
		const TSharedPtr<FJsonObject> MetaObject = MakeShared<FJsonObject>();
		for (const TPair<FString, FCloudMetaValue>& Pair : Meta)
		{
			switch (Pair.Value.Kind)
			{
			case FCloudMetaValue::EKind::Number:
				MetaObject->SetNumberField(Pair.Key, Pair.Value.NumberValue);
				break;
			case FCloudMetaValue::EKind::Bool:
				MetaObject->SetBoolField(Pair.Key, Pair.Value.bBoolValue);
				break;
			default:
				MetaObject->SetStringField(Pair.Key, Pair.Value.StringValue);
				break;
			}
		}
		return MetaObject;
	}

	bool LocalSpendLimit(FName Currency, FName Reason, int64& OutCap)
	{
		if (Currency == CloudCurrency::Mileage && Reason == FName(TEXT("mileage.exchangeLegendary")))
		{
			OutCap = 200;
			return true;
		}
		if (Currency != CloudCurrency::Diamond) return false;
		if (Reason == FName(TEXT("gacha.recruit.advanced"))) OutCap = 30;
		else if (Reason == FName(TEXT("gacha.recruit.premium"))) OutCap = 50;
		else if (Reason == FName(TEXT("trait.slotUnlock"))) OutCap = 450;
		else if (Reason == FName(TEXT("city.skipRecovery"))) OutCap = 100;
		else if (Reason == FName(TEXT("office.retryDevelopment"))) OutCap = 50;
		else if (Reason == FName(TEXT("lootbox.purchase"))) OutCap = 10;
		else if (Reason == FName(TEXT("office.expand"))) OutCap = 101;
		else return false;
		return true;
	}

	bool ReadPositiveIntegralMeta(const FCloudMeta& Meta, const TCHAR* Key, int64& OutValue)
	{
		const FCloudMetaValue* Value = Meta.Find(Key);
		if (!Value || Value->Kind != FCloudMetaValue::EKind::Number ||
			!FMath::IsFinite(Value->NumberValue) || Value->NumberValue < 1.0 ||
			Value->NumberValue > static_cast<double>(MAX_int32) ||
			FMath::FloorToDouble(Value->NumberValue) != Value->NumberValue) return false;
		OutValue = static_cast<int64>(Value->NumberValue);
		return true;
	}

	bool ValidateLocalInstantFinish(FName Reason, int64 Amount, const FCloudMeta& Meta,
		const UTableManagerSubsystem* Tables, int64& OutCap)
	{
		const bool bMine = Reason == FName(TEXT("mine.instantFinish"));
		const bool bFactory = Reason == FName(TEXT("factory.instantFinish"));
		if (!bMine && !bFactory) return false;
		FInstantFinishBalanceTable Balance;
		if (!Tables || !Tables->GetInstantFinishBalance(bMine ? FName(TEXT("Mine")) : FName(TEXT("Factory")), Balance)) return false;
		OutCap = Balance.MaxCost;
		int64 RemainingSeconds = 0;
		int64 Country = 0;
		int64 Identity = 0;
		int64 Discount = 0;
		const FCloudMetaValue* DiscountValue = Meta.Find(TEXT("discountBasisPoints"));
		const FCloudMetaValue* CycleStart = Meta.Find(TEXT("cycleStart"));
		if (!ReadPositiveIntegralMeta(Meta, TEXT("remainingSeconds"), RemainingSeconds) ||
			!ReadPositiveIntegralMeta(Meta, TEXT("country"), Country) ||
			!ReadPositiveIntegralMeta(Meta, bMine ? TEXT("resource") : TEXT("lineId"), Identity) ||
			!DiscountValue || DiscountValue->Kind != FCloudMetaValue::EKind::Number ||
			!FMath::IsFinite(DiscountValue->NumberValue) ||
			FMath::FloorToDouble(DiscountValue->NumberValue) != DiscountValue->NumberValue ||
			DiscountValue->NumberValue < 0.0 || DiscountValue->NumberValue > Balance.MaxDiscountBasisPoints ||
			!CycleStart || CycleStart->Kind != FCloudMetaValue::EKind::String || CycleStart->StringValue.IsEmpty()) return false;
		Discount = static_cast<int64>(DiscountValue->NumberValue);
		return FFacilityInstantFinishRules::CalculateCost(RemainingSeconds, Balance, static_cast<int32>(Discount)) == Amount;
	}

	bool ReadFacilityTransaction(FName Reason, int64 Amount, const FCloudMeta& Meta,
		const UTableManagerSubsystem* Tables, FFacilityInstantFinishTransaction& Out)
	{
		int64 Cap = 0;
		if (!ValidateLocalInstantFinish(Reason, Amount, Meta, Tables, Cap)) return false;
		const bool bMine = Reason == FName(TEXT("mine.instantFinish"));
		int64 Country = 0, Identity = 0, Seconds = 0;
		if (!ReadPositiveIntegralMeta(Meta, TEXT("country"), Country) ||
			!ReadPositiveIntegralMeta(Meta, bMine ? TEXT("resource") : TEXT("lineId"), Identity) ||
			!ReadPositiveIntegralMeta(Meta, TEXT("remainingSeconds"), Seconds)) return false;
		Out.Reason = Reason;
		Out.Country = static_cast<int32>(Country);
		Out.Identity = static_cast<int32>(Identity);
		Out.RemainingSeconds = Seconds;
		Out.DiscountBasisPoints = static_cast<int32>(Meta.FindChecked(TEXT("discountBasisPoints")).NumberValue);
		Out.CycleStart = Meta.FindChecked(TEXT("cycleStart")).StringValue;
		Out.Amount = Amount;
		return true;
	}

	FCloudMeta FacilityMeta(const FFacilityInstantFinishTransaction& Transaction)
	{
		FCloudMeta Meta;
		Meta.Add(TEXT("remainingSeconds"), FCloudMetaValue::FromInt(Transaction.RemainingSeconds));
		Meta.Add(TEXT("discountBasisPoints"), FCloudMetaValue::FromInt(Transaction.DiscountBasisPoints));
		Meta.Add(TEXT("country"), FCloudMetaValue::FromInt(Transaction.Country));
		Meta.Add(Transaction.Reason == FName(TEXT("mine.instantFinish")) ? TEXT("resource") : TEXT("lineId"),
			FCloudMetaValue::FromInt(Transaction.Identity));
		Meta.Add(TEXT("cycleStart"), FCloudMetaValue::FromString(Transaction.CycleStart));
		return Meta;
	}

	bool PrepareFacilityCompletion(FGameSaveData& Snapshot, const FFacilityInstantFinishTransaction& Transaction,
		UGameInstance* GI, int64& OutMineMax, bool bRequireOriginalCycle)
	{
		FDateTime CycleStart;
		if (!FDateTime::ParseIso8601(*Transaction.CycleStart, CycleStart) || !GI) return false;
		const ECountryType Country = static_cast<ECountryType>(Transaction.Country);
		const FDateTime Now = FDateTime::UtcNow();
		if (Transaction.Reason == FName(TEXT("mine.instantFinish")))
		{
			UMineManager* Mine = GI->GetSubsystem<UMineManager>();
			FCountryMineData* Data = Snapshot.WorldMap_MineData.Find(Country);
			if (!Mine || !Data) return false;
			const EResourceType Resource = static_cast<EResourceType>(Transaction.Identity);
			FMineLineState* Line = Data->ActiveLines.FindByPredicate(
				[Resource](const FMineLineState& Entry) { return Entry.Resource == Resource; });
			if (!Line || (bRequireOriginalCycle && Line->StartWallUtc.ToIso8601() != Transaction.CycleStart)) return false;
			OutMineMax = Mine->GetEffectiveMaxStorage(Country, Resource);
			const double Rate = static_cast<double>(Line->BaseRatePerSecond) *
				static_cast<double>(Mine->GetRateMultiplier(Country));
			if (OutMineMax <= 0 || !FMath::IsFinite(Rate) || Rate <= 0.0) return false;
			const double NeededSeconds = static_cast<double>(OutMineMax) / Rate + 0.01;
			if (!FMath::IsFinite(NeededSeconds) || NeededSeconds <= 0.0 || NeededSeconds > MAX_int32) return false;
			Line->StartWallUtc = Now - FTimespan::FromSeconds(NeededSeconds);
			return true;
		}
		if (Transaction.Reason == FName(TEXT("factory.instantFinish")))
		{
			FCountryFactoryData* Data = Snapshot.WorldMap_FactoryData.Find(Country);
			if (!Data) return false;
			FWorldFactoryLineState* Line = Data->ActiveLines.FindByPredicate(
				[&Transaction](const FWorldFactoryLineState& Entry) { return Entry.LineId == Transaction.Identity; });
			if (!Line || (bRequireOriginalCycle && Line->StartWallUtc.ToIso8601() != Transaction.CycleStart) || Line->TargetQty <= 0 ||
				!FMath::IsFinite(Line->RatePerSecond) || Line->RatePerSecond <= 0.0f) return false;
			const double NeededSeconds = static_cast<double>(Line->TargetQty) / static_cast<double>(Line->RatePerSecond) + 0.01;
			if (!FMath::IsFinite(NeededSeconds) || NeededSeconds <= 0.0 || NeededSeconds > MAX_int32) return false;
			Line->StartWallUtc = Now - FTimespan::FromSeconds(NeededSeconds);
			return true;
		}
		return false;
	}

	void PublishFacilityCompletion(UGameInstance* GI, const FGameSaveData& Snapshot,
		const FFacilityInstantFinishTransaction& Transaction, int64 MineMax)
	{
		const ECountryType Country = static_cast<ECountryType>(Transaction.Country);
		if (Transaction.Reason == FName(TEXT("mine.instantFinish")))
		{
			if (UMineManager* Mine = GI->GetSubsystem<UMineManager>())
			{
				Mine->LoadFromSave(Snapshot.WorldMap_MineData);
				const EResourceType Resource = static_cast<EResourceType>(Transaction.Identity);
				Mine->OnLineStorageChanged.Broadcast(Country, Resource, MineMax);
				Mine->OnLineSaturated.Broadcast(Country, Resource, true);
			}
		}
		else if (UWorldFactoryManager* Factory = GI->GetSubsystem<UWorldFactoryManager>())
		{
			Factory->LoadFromSave(Snapshot.WorldMap_FactoryData);
			Factory->OnLineCompleted.Broadcast(Country, Transaction.Identity);
		}
	}

	bool LocalConsumeLimit(FName Reason, EItemType ItemType, int32& OutCap)
	{
		if (Reason == FName(TEXT("recruit.pull")) &&
			(ItemType == EItemType::RecruitTicketNormal || ItemType == EItemType::RecruitTicketAdvanced || ItemType == EItemType::RecruitTicketPremium)) OutCap = 5;
		else if (Reason == FName(TEXT("trait.pull")) &&
			(ItemType == EItemType::BuildingTraitTicketNormal || ItemType == EItemType::BuildingTraitTicketAdvanced)) OutCap = 30;
		else if (Reason == FName(TEXT("skin.pull")) &&
			(ItemType == EItemType::SkinTicketNormal || ItemType == EItemType::SkinTicketAdvanced)) OutCap = 30;
		else if (Reason == FName(TEXT("employee.potentialReroll")) &&
			(ItemType == EItemType::BusinessCardPaper || ItemType == EItemType::BusinessCardGold || ItemType == EItemType::BusinessCardBlack)) OutCap = 1;
		else return false;
		return true;
	}

	int32 LocalTradeCredit(int32 Requested, int32 AlreadyCredited)
	{
		return FMath::Min(Requested, FMath::Max(0, 1600 - AlreadyCredited));
	}
}

void UCloudEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!UCGDevSettings::IsOfflineMode())
	{
		Collection.InitializeDependency<UFirebaseAuthSubsystem>();
	}
	Collection.InitializeDependency<USaveLoadManager>();

	UGameInstance* GI = GetGameInstance();

	if (!UCGDevSettings::IsOfflineMode())
	{
		if (UFirebaseAuthSubsystem* AuthMgr = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr)
		{
			AuthMgr->OnAuthReady.AddUObject(this, &UCloudEconomySubsystem::HandleAuthReady);
		}
	}

	if (USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr)
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UCloudEconomySubsystem::HandleGameDataLoaded);
	}

	UE_LOG(LogTemp, Log, TEXT("[CloudEconomy] 초기화 완료 (경제 잠금 상태로 시작)"));
}

void UCloudEconomySubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UFirebaseAuthSubsystem* AuthMgr = GI->GetSubsystem<UFirebaseAuthSubsystem>())
		{
			AuthMgr->OnAuthReady.RemoveAll(this);
		}
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->OnGameDataLoaded.RemoveAll(this);
		}
	}

	PendingActions.Empty();
	FacilityOpsInFlight.Empty();
	FacilityAccountMismatchLogged.Empty();
	FacilityRetryCounts.Empty();
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FacilityRetryTimer);

	Super::Deinitialize();
}

void UCloudEconomySubsystem::HandleAuthReady(bool bReady)
{
	if (UCGDevSettings::IsOfflineMode()) return;
	bAuthReady = bReady;

	if (!bReady)
	{
		SetEconomyReady(false);
		return;
	}

	FetchState();
}

void UCloudEconomySubsystem::HandleGameDataLoaded()
{
	bSaveLoaded = true;
	if (UCGDevSettings::IsOfflineMode())
	{
		RefreshLocalState();
		SetEconomyReady(true);
		return;
	}
	FetchState();
	if (bEconomyReady) ResumePendingFacilityInstantFinishes();
}

void UCloudEconomySubsystem::SetEconomyReady(bool bReady)
{
	if (bEconomyReady == bReady)
	{
		return;
	}

	bEconomyReady = bReady;

	// 풀렸으면 원인을 버린다 ― 남겨두면 다음 잠금에서 옛 원인으로 안내한다
	if (bReady)
	{
		LastBlockCode = NAME_None;
	}

	OnEconomyReady.Broadcast(bReady);
	if (bReady && !UCGDevSettings::IsOfflineMode())
	{
		FacilityRetryCounts.Empty();
		ResumePendingFacilityInstantFinishes();
	}
}

ECloudBlockState UCloudEconomySubsystem::ClassifyBlock(FName LastFailureCode, bool bQueryInProgress)
{
	// 영구 차단이 최우선 ― 재시도가 돌고 있어도 풀리지 않으므로 "동기화 중" 은 거짓말이다
	if (LastFailureCode == CloudErr::Forbidden)
	{
		return ECloudBlockState::PermanentlyBlocked;
	}

	if (bQueryInProgress)
	{
		return ECloudBlockState::Syncing;
	}

	// 코드가 없어도(백오프 대기) 재시도 예정이다 ― 멈춘 것을 "동기화 중" 으로 보이면 안 된다
	return ECloudBlockState::RetryPending;
}

FText UCloudEconomySubsystem::BlockStateMessage(ECloudBlockState State, const FText& CodeText)
{
	switch (State)
	{
	case ECloudBlockState::Syncing:
		return NSLOCTEXT("CloudEconomy", "BlockSyncing", "서버와 동기화 중입니다. 잠시만 기다려 주세요.");

	case ECloudBlockState::RetryPending:
		return NSLOCTEXT("CloudEconomy", "BlockRetryPending", "서버 연결이 끊겼습니다. 잠시 후 다시 시도합니다.");

	case ECloudBlockState::PermanentlyBlocked:
		// 안심 문구가 핵심이다 ― 지갑의 권위가 서버에 있으므로 "서버에 보관" 은 사실이고,
		// 이 말이 없으면 플레이어는 영구 차단을 재화 분실로 읽는다
		return FText::Format(
			NSLOCTEXT("CloudEconomy", "BlockForbidden",
				"{0} 고객센터에 문의해 주세요. 재화는 서버에 안전하게 보관되어 있습니다."),
			CodeText.IsEmpty() ? FCloudErrorTextRules::LastResort() : CodeText);

	default:
		return FText::GetEmpty();
	}
}

bool UCloudEconomySubsystem::ShouldKeepShortfallText(FName Code)
{
	return Code == CloudErr::InsufficientFunds || Code == CloudErr::InsufficientItems;
}

bool UCloudEconomySubsystem::IsCloudQueryInProgress() const
{
	if (UCGDevSettings::IsOfflineMode()) return !bSaveLoaded;
	if (bFetchInFlight)
	{
		return true;
	}

	// 인증이 아직 안 붙었고 백오프로 멈춘 것도 아니면 교환·갱신이 진행 중이다
	UGameInstance* GI = GetGameInstance();
	const UFirebaseAuthSubsystem* AuthMgr = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr;
	if (AuthMgr && !AuthMgr->IsAuthReady() && !AuthMgr->IsAuthBackoffLocked())
	{
		return true;
	}

	// 인증은 붙었는데 세이브 로드를 기다리는 구간도 진행 중이다(게이트 두 조건, §7.2 ③)
	return bAuthReady && !bSaveLoaded;
}

ECloudBlockState UCloudEconomySubsystem::GetBlockState() const
{
	if (bEconomyReady)
	{
		return ECloudBlockState::None;
	}
	return ClassifyBlock(LastBlockCode, IsCloudQueryInProgress());
}

FString UCloudEconomySubsystem::AcquireIdToken() const
{
	UGameInstance* GI = GetGameInstance();
	const UFirebaseAuthSubsystem* AuthMgr = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr;
	return AuthMgr ? AuthMgr->GetIdToken() : FString();
}

FCloudEconomyApplyTargets UCloudEconomySubsystem::ResolveTargets() const
{
	FCloudEconomyApplyTargets Targets;

	if (UGameInstance* GI = GetGameInstance())
	{
		Targets.Resources = GI->GetSubsystem<UResourceItemManager>();
		Targets.Items = GI->GetSubsystem<UItemInventoryManager>();
		Targets.Shop = GI->GetSubsystem<UShopManagerSubsystem>();
		Targets.Attendance = GI->GetSubsystem<UAttendanceSubsystem>();
		Targets.Recruitment = GI->GetSubsystem<URecruitmentManagerSubsystem>();
		Targets.Save = GI->GetSubsystem<USaveLoadManager>();
	}

	return Targets;
}

void UCloudEconomySubsystem::FetchState()
{
	if (UCGDevSettings::IsOfflineMode())
	{
		if (bSaveLoaded)
		{
			RefreshLocalState();
			SetEconomyReady(true);
		}
		return;
	}
	// ⑥이 ①보다 먼저 실행되면 빈 상태를 쓴다 ― 게이트는 인증과 세이브 로드 두 조건 모두다(§7.2)
	if (!bAuthReady || !bSaveLoaded)
	{
		return;
	}

	if (bFetchInFlight)
	{
		return;
	}

	FCloudRequest Request;
	Request.Verb = TEXT("GET");
	Request.Path = TEXT("/player/state");
	Request.IdToken = AcquireIdToken();
	Request.ContentType.Reset();
	Request.Tag = CloudEconomyTag;
	Request.Action = TEXT("state");

	bFetchInFlight = true;

	const double StartSeconds = FPlatformTime::Seconds();
	TWeakObjectPtr<UCloudEconomySubsystem> WeakSelf(this);
	FCloudHttp::Send(Request, [WeakSelf, StartSeconds](const FCloudResponse& Response)
	{
		UCloudEconomySubsystem* Self = WeakSelf.Get();
		if (!Self)
		{
			return;
		}

		Self->bFetchInFlight = false;
		const int32 ElapsedMs = static_cast<int32>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);

		if (!Response.bOk)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] state fail code=%s op=- ms=%d"), *Response.Code, ElapsedMs);
			// 원인을 남긴다 ― 코드가 없으면 3상태 안내가 FORBIDDEN 을 재시도로 오분류한다
			Self->LastBlockCode = Response.Code.IsEmpty() ? NAME_None : FName(*Response.Code);
			Self->SetEconomyReady(false);

			if (Response.HttpStatus == 401)
			{
				UGameInstance* GI = Self->GetGameInstance();
				if (UFirebaseAuthSubsystem* AuthMgr = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr)
				{
					// AUTH_EXPIRED=갱신 후 1회 재시도, AUTH_INVALID=교환부터(설계서 §3.2)
					TWeakObjectPtr<UCloudEconomySubsystem> RetrySelf = WeakSelf;
					AuthMgr->NotifyAuthRejected(Response.Code, [RetrySelf](bool bRecovered)
					{
						if (UCloudEconomySubsystem* Retry = RetrySelf.Get())
						{
							if (bRecovered)
							{
								Retry->FetchState();
							}
						}
					});
				}
			}
			return;
		}

		if (UGameInstance* GI = Self->GetGameInstance())
		{
			if (UFirebaseAuthSubsystem* AuthMgr = GI->GetSubsystem<UFirebaseAuthSubsystem>())
			{
				AuthMgr->NotifyAuthAccepted();
			}
		}

		FServerEconomyState Parsed;
		FString ParseError;
		if (!FCloudEconomyData::ParseState(Response.State, Parsed, ParseError))
		{
			UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] state fail code=INTERNAL op=- ms=%d msg=%s"), ElapsedMs, *ParseError);
			Self->LastBlockCode = CloudErr::Internal;
			Self->SetEconomyReady(false);
			return;
		}

		if (!Self->ApplyServerEconomyState(Parsed))
		{
			// ApplyServerEconomyState 가 이유를 로그한다
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[CloudEconomy] state ok op=- ms=%d"), ElapsedMs);
	});
}

bool UCloudEconomySubsystem::ApplyServerEconomyState(const FServerEconomyState& State)
{
	FString Error;
	if (!ApplyServerEconomyStateTo(State, ResolveTargets(), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] apply fail code=INTERNAL op=- ms=0 msg=%s"), *Error);
		LastBlockCode = CloudErr::Internal;
		return false;
	}

	LastState = State;
	SetEconomyReady(true);
	return true;
}

bool UCloudEconomySubsystem::ApplyServerEconomyStateTo(
	const FServerEconomyState& State,
	const FCloudEconomyApplyTargets& Targets,
	FString& OutError)
{
	// === 1단계: 검증. 하나라도 어긋나면 매니저를 건드리지 않는다(반쪽 상태 방지 ― 마지막에 저장 1회이므로) ===
	if (!FCloudEconomyData::ValidateState(State, OutError))
	{
		return false;
	}

	if (State.bHasWallet && (!Targets.Resources || !Targets.Recruitment))
	{
		OutError = TEXT("wallet 적용 대상(ResourceItemManager/RecruitmentManager) 없음");
		return false;
	}
	if (State.bHasInventory && !Targets.Items)
	{
		OutError = TEXT("inventory 적용 대상(ItemInventoryManager) 없음");
		return false;
	}
	if (State.bHasShop && !Targets.Shop)
	{
		OutError = TEXT("shop 적용 대상(ShopManagerSubsystem) 없음");
		return false;
	}
	if (State.bHasAttendance && !Targets.Attendance)
	{
		OutError = TEXT("attendance 적용 대상(AttendanceSubsystem) 없음");
		return false;
	}

	// === 2단계: 적용 ===
	if (State.bHasWallet)
	{
		const int64 LocalDiamond = Targets.Resources->GetResourceAmount(EResourceType::Diamond);
		if (LocalDiamond != State.Wallet.Diamond)
		{
			// 델타 저축으로 Diamond 만 건드린다 ― SetAllResources 는 전 자원을 다시 방송한다
			Targets.Resources->StoreResource(
				EResourceType::Diamond, State.Wallet.Diamond - LocalDiamond,
				/*bShouldSave=*/false, /*bShouldBroadcast=*/true);
		}

		FGachaRecruitmentData GachaData = Targets.Recruitment->GetGachaData();
		if (GachaData.Mileage.CurrentPoints != State.Wallet.Mileage)
		{
			GachaData.Mileage.CurrentPoints = State.Wallet.Mileage;
			Targets.Recruitment->SetGachaData(GachaData);
			Targets.Recruitment->OnMileageChanged.Broadcast(State.Wallet.Mileage);
		}
	}

	if (State.bHasInventory)
	{
		// 맵 교체다 ― 서버는 0 개 키를 싣지 않으므로 델타 적용이면 소모된 티켓이 로컬에 영원히 남는다(§4.2)
		Targets.Items->SetAllItems(
			FCloudEconomyData::BuildInventoryTarget(Targets.Items->GetAllItems(), State.Inventory));
	}

	if (State.bHasShop)
	{
		FShopSaveData ShopData = Targets.Shop->GetShopData();
		ShopData.PurchaseCounts = State.Shop.Counts;
		// 서버가 준 키는 "이 카운트가 유효한 현재 키" ― 그대로 넣어야 로컬이 다시 리셋하지 않는다
		ShopData.LastDailyKey = State.Shop.DailyKey;
		ShopData.LastWeeklyKey = State.Shop.WeeklyKey;
		Targets.Shop->SetShopData(ShopData);
		Targets.Shop->OnShopStockChanged.Broadcast();
	}

	if (State.bHasAttendance)
	{
		FAttendanceSaveData AttendanceData;
		AttendanceData.CycleDay = State.Attendance.CycleDay;
		AttendanceData.LastClaimDayKey = State.Attendance.LastClaimDayKey;
		AttendanceData.TotalDays = State.Attendance.TotalDays;
		Targets.Attendance->SetAttendanceData(AttendanceData);
		Targets.Attendance->OnAttendanceChanged.Broadcast();
	}

	// ranking 은 읽어 두기만 한다 ― 시총/주간매출 보고는 1-F 의 몫이고 로컬 권위가 아직 살아 있다

	if (Targets.Save)
	{
		// 적용 1회당 캐시 미러 저장 1회(§6.3 수용 기준)
		Targets.Save->SaveGameData();
	}

	return true;
}

// ===== 변경 요청 =====

bool UCloudEconomySubsystem::RejectWhenNotReady(const TCHAR* Action, FOnCloudOp& Done)
{
	if (bEconomyReady)
	{
		return false;
	}

	FCloudOpResult Result;
	Result.bOk = false;
	// 새 코드를 만들지 않는다 ― AUTH_REQUIRED 의 DT 문구가 "서버와 다시 연결하고 있습니다" 다(§7.2 안내)
	Result.Code = CloudErr::AuthRequired;
	Result.Message = TEXT("economy is locked until server state is applied");

	UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] %s fail code=%s op=- ms=0 (경제 잠금 ― 요청 미전송)"),
		Action, *Result.Code.ToString());

	Done.ExecuteIfBound(Result);
	return true;
}

bool UCloudEconomySubsystem::HasPendingFacilityInstantFinish(FName Reason, int32 Country, int32 Identity) const
{
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* Save = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	const USaveGame_GameData* SaveData = Save ? Save->GetCurrentSaveData() : nullptr;
	if (!SaveData) return false;
	return SaveData->GameData.PendingFacilityInstantFinishes.ContainsByPredicate(
		[Reason, Country, Identity](const FFacilityInstantFinishTransaction& Entry)
		{
			return Entry.Reason == Reason && Entry.Country == Country && Entry.Identity == Identity;
		});
}

void UCloudEconomySubsystem::RequestFacilityInstantFinish(FName Reason, int64 Amount,
	const FCloudMeta& Meta, FOnCloudOp Done)
{
	if (RejectWhenNotReady(TEXT("facility.instantFinish"), Done)) return;
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* Save = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	UResourceItemManager* Resources = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FFacilityInstantFinishTransaction Transaction;
	if (!Save || !Resources || !ReadFacilityTransaction(Reason, Amount, Meta, Tables, Transaction))
	{
		CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}
	if (Save->IsSaveLockedForDevMode())
	{
		UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility request rejected: save is not durable"));
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	FGameSaveData Snapshot;
	if (!Save->BuildCurrentSnapshot(Snapshot))
	{
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	const FFacilityInstantFinishTransaction* Existing = Snapshot.PendingFacilityInstantFinishes.FindByPredicate(
		[&Transaction](const FFacilityInstantFinishTransaction& Entry)
		{
			return Entry.Reason == Transaction.Reason && Entry.Country == Transaction.Country &&
				Entry.Identity == Transaction.Identity;
		});
	if (Existing)
	{
		if (Existing->CycleStart != Transaction.CycleStart || Existing->Amount != Transaction.Amount ||
			Existing->RemainingSeconds != Transaction.RemainingSeconds ||
			Existing->DiscountBasisPoints != Transaction.DiscountBasisPoints ||
			UCGDevSettings::IsOfflineMode() || FacilityOpsInFlight.Contains(Existing->OpId))
		{
			CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
			return;
		}
		DispatchFacilityInstantFinish(*Existing, MoveTemp(Done));
		return;
	}
	FGameSaveData CompletionCandidate = Snapshot;
	int64 MineMax = 0;
	if (!PrepareFacilityCompletion(CompletionCandidate, Transaction, GI, MineMax, true))
	{
		CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}
	if (UCGDevSettings::IsOfflineMode())
	{
		int64& Balance = CompletionCandidate.ResourceBank.FindOrAdd(EResourceType::Diamond);
		if (Amount <= 0 || Balance < Amount)
		{
			CompleteLocalOp(Done, false, CloudErr::InsufficientFunds, 409);
			return;
		}
		Balance -= Amount;
		if (!Save->CommitPreparedSnapshot(CompletionCandidate))
		{
			CompleteLocalOp(Done, false, CloudErr::Internal, 500);
			return;
		}
		Resources->SetAllResources(CompletionCandidate.ResourceBank, false);
		RefreshLocalState();
		PublishFacilityCompletion(GI, CompletionCandidate, Transaction, MineMax);
		Resources->OnResourceChanged.Broadcast(EResourceType::Diamond, Resources->GetResourceAmount(EResourceType::Diamond));
		CompleteLocalOp(Done, true, NAME_None, 200);
		return;
	}
	Transaction.OpId = FCloudEconomyData::MakeOpId();
	const UFirebaseAuthSubsystem* Auth = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr;
	Transaction.Pid = Auth ? Auth->GetPid() : FString();
	if (Transaction.Pid.IsEmpty())
	{
		CompleteLocalOp(Done, false, CloudErr::AuthRequired, 401);
		return;
	}
	Snapshot.PendingFacilityInstantFinishes.Add(Transaction);
	if (!Save->CommitPreparedSnapshot(Snapshot))
	{
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	DispatchFacilityInstantFinish(Transaction, MoveTemp(Done));
}

void UCloudEconomySubsystem::DispatchFacilityInstantFinish(
	const FFacilityInstantFinishTransaction& Transaction, FOnCloudOp Done)
{
	if (Transaction.OpId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility journal has no opId"));
		if (Done.IsBound()) CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	if (FacilityOpsInFlight.Contains(Transaction.OpId))
	{
		if (Done.IsBound()) CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}
	UGameInstance* GI = GetGameInstance();
	const UFirebaseAuthSubsystem* Auth = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr;
	if (Transaction.Pid.IsEmpty() || !Auth || Auth->GetPid() != Transaction.Pid)
	{
		if (!FacilityAccountMismatchLogged.Contains(Transaction.OpId))
		{
			FacilityAccountMismatchLogged.Add(Transaction.OpId);
			UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility journal account mismatch op=%s; request blocked"),
				*Transaction.OpId.Left(8));
		}
		if (Done.IsBound()) CompleteLocalOp(Done, false, CloudErr::AuthInvalid, 401);
		return;
	}
	FacilityOpsInFlight.Add(Transaction.OpId);
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("opId"), Transaction.OpId);
	Body->SetStringField(TEXT("currency"), CloudCurrency::Diamond.ToString());
	Body->SetNumberField(TEXT("amount"), static_cast<double>(Transaction.Amount));
	Body->SetStringField(TEXT("reason"), Transaction.Reason.ToString());
	Body->SetObjectField(TEXT("meta"), BuildMetaObject(FacilityMeta(Transaction)));
	const FString OpId = Transaction.OpId;
	SendEconomyOp(TEXT("/wallet/spend"), TEXT("facility.instantFinish"), OpId,
		SerializeEconomyJson(Body), FOnCloudOp::CreateWeakLambda(this,
			[this, OpId, Done](const FCloudOpResult& Result)
			{
				FinishFacilityInstantFinish(OpId, Result, Done);
			}), /*bAllowAuthRetry=*/true);
}

void UCloudEconomySubsystem::FinishFacilityInstantFinish(const FString& OpId,
	const FCloudOpResult& Result, FOnCloudOp Done)
{
	FacilityOpsInFlight.Remove(OpId);
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* Save = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	FGameSaveData Snapshot;
	if (!Save || !Save->BuildCurrentSnapshot(Snapshot))
	{
		ScheduleFacilityInstantFinishRetry(OpId);
		FCloudOpResult Failed = Result;
		Failed.bOk = false;
		Failed.Code = CloudErr::Internal;
		Done.ExecuteIfBound(Failed);
		return;
	}
	const int32 Index = Snapshot.PendingFacilityInstantFinishes.IndexOfByPredicate(
		[&OpId](const FFacilityInstantFinishTransaction& Entry) { return Entry.OpId == OpId; });
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility journal missing for op=%s"), *OpId.Left(8));
		FCloudOpResult Failed = Result;
		Failed.bOk = false;
		Failed.Code = CloudErr::Internal;
		Done.ExecuteIfBound(Failed);
		return;
	}
	const FFacilityInstantFinishTransaction Transaction = Snapshot.PendingFacilityInstantFinishes[Index];
	if (Result.bOk)
	{
		int64 MineMax = 0;
		if (!PrepareFacilityCompletion(Snapshot, Transaction, GI, MineMax, false))
		{
			UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] paid facility cycle changed op=%s"), *OpId.Left(8));
			ScheduleFacilityInstantFinishRetry(OpId);
			FCloudOpResult Failed = Result;
			Failed.bOk = false;
			Failed.Code = CloudErr::Internal;
			Done.ExecuteIfBound(Failed);
			return;
		}
		Snapshot.PendingFacilityInstantFinishes.RemoveAt(Index);
		if (!Save->CommitPreparedSnapshot(Snapshot))
		{
			UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] paid facility commit failed op=%s"), *OpId.Left(8));
			ScheduleFacilityInstantFinishRetry(OpId);
			FCloudOpResult Failed = Result;
			Failed.bOk = false;
			Failed.Code = CloudErr::Internal;
			Done.ExecuteIfBound(Failed);
			return;
		}
		PublishFacilityCompletion(GI, Snapshot, Transaction, MineMax);
		FacilityRetryCounts.Remove(OpId);
		Done.ExecuteIfBound(Result);
		return;
	}
	const bool bDefiniteNoCharge = Result.HttpStatus == 400 ||
		(Result.HttpStatus == 409 && Result.Code == CloudErr::InsufficientFunds);
	if (bDefiniteNoCharge)
	{
		Snapshot.PendingFacilityInstantFinishes.RemoveAt(Index);
		if (!Save->CommitPreparedSnapshot(Snapshot))
		{
			UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility rejection journal clear failed op=%s"), *OpId.Left(8));
			ScheduleFacilityInstantFinishRetry(OpId);
		}
	}
	else if (Result.HttpStatus == 0 || Result.HttpStatus == 401 || Result.HttpStatus == 408 ||
		Result.HttpStatus == 429 || Result.HttpStatus >= 500)
	{
		ScheduleFacilityInstantFinishRetry(OpId);
	}
	Done.ExecuteIfBound(Result);
}

void UCloudEconomySubsystem::ScheduleFacilityInstantFinishRetry(const FString& OpId)
{
	const int32 Attempt = FMath::Min(FacilityRetryCounts.FindOrAdd(OpId) + 1, 4);
	FacilityRetryCounts.Add(OpId, Attempt);
	const float DelaySeconds = Attempt == 1 ? 5.0f : Attempt == 2 ? 10.0f : Attempt == 3 ? 30.0f : 60.0f;
	UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] facility op=%s unresolved; retry in %.0fs"), *OpId.Left(8), DelaySeconds);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FacilityRetryTimer, this,
			&UCloudEconomySubsystem::ResumePendingFacilityInstantFinishes, DelaySeconds, false);
	}
}

void UCloudEconomySubsystem::ResumePendingFacilityInstantFinishes()
{
	if (!bEconomyReady || UCGDevSettings::IsOfflineMode()) return;
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* Save = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	const USaveGame_GameData* SaveData = Save ? Save->GetCurrentSaveData() : nullptr;
	if (!SaveData) return;
	const TArray<FFacilityInstantFinishTransaction> Pending = SaveData->GameData.PendingFacilityInstantFinishes;
	for (const FFacilityInstantFinishTransaction& Transaction : Pending)
	{
		if (!FacilityOpsInFlight.Contains(Transaction.OpId))
		{
			DispatchFacilityInstantFinish(Transaction, FOnCloudOp());
		}
	}
}

void UCloudEconomySubsystem::RequestSpend(FName Currency, int64 Amount, FName Reason, const FCloudMeta& Meta, FOnCloudOp Done)
{
	if (Reason == FName(TEXT("mine.instantFinish")) || Reason == FName(TEXT("factory.instantFinish")))
	{
		CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}
	if (UCGDevSettings::IsOfflineMode())
	{
		RequestLocalSpend(Currency, Amount, Reason, Meta, MoveTemp(Done));
		return;
	}
	if (RejectWhenNotReady(TEXT("wallet.spend"), Done))
	{
		return;
	}

	const FString OpId = FCloudEconomyData::MakeOpId();

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("opId"), OpId);
	Body->SetStringField(TEXT("currency"), Currency.ToString());
	Body->SetNumberField(TEXT("amount"), static_cast<double>(Amount));
	Body->SetStringField(TEXT("reason"), Reason.ToString());
	if (Meta.Num() > 0)
	{
		Body->SetObjectField(TEXT("meta"), BuildMetaObject(Meta));
	}

	SendEconomyOp(TEXT("/wallet/spend"), TEXT("wallet.spend"), OpId, SerializeEconomyJson(Body), Done, /*bAllowAuthRetry=*/true);
}

void UCloudEconomySubsystem::RequestCredit(FName Source, const FString& ClaimKey, int64 Amount, FOnCloudOp Done)
{
	if (UCGDevSettings::IsOfflineMode())
	{
		RequestLocalCredit(Source, ClaimKey, Amount, MoveTemp(Done));
		return;
	}
	if (RejectWhenNotReady(TEXT("wallet.credit"), Done))
	{
		return;
	}

	const FString OpId = FCloudEconomyData::MakeOpId();

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("opId"), OpId);
	Body->SetStringField(TEXT("source"), Source.ToString());
	Body->SetStringField(TEXT("claimKey"), ClaimKey);
	Body->SetNumberField(TEXT("amount"), static_cast<double>(Amount));

	SendEconomyOp(TEXT("/wallet/credit"), TEXT("wallet.credit"), OpId, SerializeEconomyJson(Body), Done, /*bAllowAuthRetry=*/true);
}

void UCloudEconomySubsystem::RequestConsume(FName Reason, FName Item, int32 Amount, FOnCloudOp Done)
{
	if (UCGDevSettings::IsOfflineMode())
	{
		RequestLocalConsume(Reason, Item, Amount, MoveTemp(Done));
		return;
	}
	if (RejectWhenNotReady(TEXT("item.consume"), Done))
	{
		return;
	}

	const FString OpId = FCloudEconomyData::MakeOpId();

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("opId"), OpId);
	Body->SetStringField(TEXT("reason"), Reason.ToString());
	Body->SetStringField(TEXT("item"), Item.ToString());
	Body->SetNumberField(TEXT("amount"), static_cast<double>(Amount));

	SendEconomyOp(TEXT("/item/consume"), TEXT("item.consume"), OpId, SerializeEconomyJson(Body), Done, /*bAllowAuthRetry=*/true);
}

void UCloudEconomySubsystem::CompleteLocalOp(FOnCloudOp& Done, bool bOk, FName Code, int32 HttpStatus,
	const TSharedPtr<FJsonObject>& ResultObject) const
{
	FCloudOpResult OpResult;
	OpResult.bOk = bOk;
	OpResult.Code = Code;
	OpResult.HttpStatus = HttpStatus;
	OpResult.Result = ResultObject;
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] offline operation rejected code=%s"), *Code.ToString());
	}
	Done.ExecuteIfBound(OpResult);
}

bool UCloudEconomySubsystem::EnsureLocalSaveCache(USaveLoadManager*& OutSaveManager) const
{
	UGameInstance* GI = GetGameInstance();
	OutSaveManager = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	if (!OutSaveManager || !bSaveLoaded) return false;
	if (OutSaveManager->GetCurrentSaveData()) return true;
	// A new game has no slot yet. Materialize the cache before recording a claim key.
	return OutSaveManager->SaveGameData() && OutSaveManager->GetCurrentSaveData() != nullptr;
}

void UCloudEconomySubsystem::RefreshLocalState()
{
	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* Resources = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	UItemInventoryManager* Items = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	URecruitmentManagerSubsystem* Recruitment = GI ? GI->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	if (!Resources || !Items || !Recruitment) return;

	LastState.bValid = true;
	LastState.bHasWallet = true;
	LastState.Wallet.Diamond = Resources->GetResourceAmount(EResourceType::Diamond);
	LastState.Wallet.Mileage = Recruitment->GetGachaData().Mileage.CurrentPoints;
	LastState.bHasInventory = true;
	LastState.Inventory.Empty();
	for (const TPair<EItemType, int32>& Pair : Items->GetAllItems())
	{
		if (Pair.Value > 0)
		{
			LastState.Inventory.Add(FCloudEconomyData::ItemTypeToName(Pair.Key), Pair.Value);
		}
	}
}

void UCloudEconomySubsystem::RequestLocalSpend(FName Currency, int64 Amount, FName Reason,
	const FCloudMeta& Meta, FOnCloudOp Done)
{
	if (RejectWhenNotReady(TEXT("wallet.spend"), Done)) return;
	int64 Cap = 0;
	const bool bInstantFinish = Reason == FName(TEXT("mine.instantFinish")) ||
		Reason == FName(TEXT("factory.instantFinish"));
	UGameInstance* CostGI = GetGameInstance();
	const UTableManagerSubsystem* CostTables = CostGI ? CostGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const bool bAllowedSpend = bInstantFinish
		? Currency == CloudCurrency::Diamond && ValidateLocalInstantFinish(Reason, Amount, Meta, CostTables, Cap)
		: LocalSpendLimit(Currency, Reason, Cap);
	if (!bAllowedSpend || Amount <= 0 || Amount > Cap ||
		(Reason == FName(TEXT("mileage.exchangeLegendary")) && Amount != Cap) ||
		(Reason == FName(TEXT("gacha.recruit.advanced")) && Amount != Cap) ||
		(Reason == FName(TEXT("gacha.recruit.premium")) && Amount != Cap))
	{
		CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}
	if (Reason == FName(TEXT("gacha.recruit.premium")))
	{
		const FCloudMetaValue* Count = Meta.Find(TEXT("count"));
		if (!Count || Count->Kind != FCloudMetaValue::EKind::Number || Count->NumberValue != 1.0)
		{
			CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
			return;
		}
	}

	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* Resources = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	URecruitmentManagerSubsystem* Recruitment = GI ? GI->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	USaveLoadManager* SaveManager = nullptr;
	if (!Resources || !Recruitment || !EnsureLocalSaveCache(SaveManager))
	{
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	const int32 OriginalMileage = Recruitment->GetGachaData().Mileage.CurrentPoints;
	if (Reason == FName(TEXT("gacha.recruit.premium")) && OriginalMileage == MAX_int32)
	{
		CompleteLocalOp(Done, false, CloudErr::BalanceOverflow, 409);
		return;
	}

	if (Currency == CloudCurrency::Diamond)
	{
		if (!Resources->HasResource(EResourceType::Diamond, Amount))
		{
			CompleteLocalOp(Done, false, CloudErr::InsufficientFunds, 409);
			return;
		}
		Resources->SpendResource(EResourceType::Diamond, Amount, false);
	}
	else
	{
		FGachaRecruitmentData GachaData = Recruitment->GetGachaData();
		if (GachaData.Mileage.CurrentPoints < Amount)
		{
			CompleteLocalOp(Done, false, CloudErr::InsufficientFunds, 409);
			return;
		}
		GachaData.Mileage.CurrentPoints -= static_cast<int32>(Amount);
		Recruitment->SetGachaData(GachaData);
		Recruitment->OnMileageChanged.Broadcast(GachaData.Mileage.CurrentPoints);
	}

	if (Reason == FName(TEXT("gacha.recruit.premium")))
	{
		FGachaRecruitmentData GachaData = Recruitment->GetGachaData();
		++GachaData.Mileage.CurrentPoints;
		Recruitment->SetGachaData(GachaData);
		Recruitment->OnMileageChanged.Broadcast(GachaData.Mileage.CurrentPoints);
	}

	RefreshLocalState();
	if (!SaveManager->SaveGameData())
	{
		// Persist failed: restore the local wallet before reporting failure.
		if (Currency == CloudCurrency::Diamond) Resources->StoreResource(EResourceType::Diamond, Amount, false);
		FGachaRecruitmentData GachaData = Recruitment->GetGachaData();
		GachaData.Mileage.CurrentPoints = OriginalMileage;
		Recruitment->SetGachaData(GachaData);
		Recruitment->OnMileageChanged.Broadcast(OriginalMileage);
		RefreshLocalState();
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	CompleteLocalOp(Done, true, NAME_None, 200);
}

void UCloudEconomySubsystem::RequestLocalCredit(FName Source, const FString& ClaimKey, int64 Amount, FOnCloudOp Done)
{
	if (RejectWhenNotReady(TEXT("wallet.credit"), Done)) return;
	const bool bTier = Source == FName(TEXT("codex.tierMilestone"));
	const bool bFull = Source == FName(TEXT("codex.fullMilestone"));
	const bool bTrade = Source == FName(TEXT("trade.diamond"));
	if (ClaimKey.IsEmpty() || Amount <= 0 || (bTier && Amount != 20) ||
		(bFull && Amount != 200) || (bTrade && Amount > 80) || (!bTier && !bFull && !bTrade))
	{
		CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* Resources = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	USaveLoadManager* SaveManager = nullptr;
	if (!Resources || !EnsureLocalSaveCache(SaveManager))
	{
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	USaveGame_GameData* SaveData = SaveManager->GetCurrentSaveData();
	const FString LedgerKey = Source.ToString() + TEXT(":") + ClaimKey;
	if (SaveData->GameData.OfflineEconomyClaimKeys.Contains(LedgerKey))
	{
		CompleteLocalOp(Done, false, CloudErr::AlreadyClaimed, 409);
		return;
	}
	const FString OldTradeDayKey = SaveData->GameData.OfflineTradeDiamondDayKey;
	const int32 OldTradeCredited = SaveData->GameData.OfflineTradeDiamondCreditedToday;
	int64 Credited = Amount;
	if (bTrade)
	{
		const UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>();
		const FString DayKey = Clock ? Clock->GetDayKey() : FString();
		if (DayKey.IsEmpty())
		{
			CompleteLocalOp(Done, false, CloudErr::Internal, 500);
			return;
		}
		if (SaveData->GameData.OfflineTradeDiamondDayKey != DayKey)
		{
			SaveData->GameData.OfflineTradeDiamondDayKey = DayKey;
			SaveData->GameData.OfflineTradeDiamondCreditedToday = 0;
		}
		Credited = LocalTradeCredit(static_cast<int32>(Amount),
			SaveData->GameData.OfflineTradeDiamondCreditedToday);
	}
	if (Resources->GetResourceAmount(EResourceType::Diamond) > MAX_int64 - Credited)
	{
		SaveData->GameData.OfflineTradeDiamondDayKey = OldTradeDayKey;
		SaveData->GameData.OfflineTradeDiamondCreditedToday = OldTradeCredited;
		CompleteLocalOp(Done, false, CloudErr::BalanceOverflow, 409);
		return;
	}

	SaveData->GameData.OfflineEconomyClaimKeys.Add(LedgerKey);
	if (bTrade) SaveData->GameData.OfflineTradeDiamondCreditedToday += static_cast<int32>(Credited);
	if (Credited > 0) Resources->StoreResource(EResourceType::Diamond, Credited, false);
	if (!SaveManager->SaveGameData())
	{
		if (Credited > 0) Resources->StoreResource(EResourceType::Diamond, -Credited, false);
		SaveData->GameData.OfflineEconomyClaimKeys.Remove(LedgerKey);
		SaveData->GameData.OfflineTradeDiamondDayKey = OldTradeDayKey;
		SaveData->GameData.OfflineTradeDiamondCreditedToday = OldTradeCredited;
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	RefreshLocalState();
	const TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetNumberField(TEXT("credited"), static_cast<double>(Credited));
	CompleteLocalOp(Done, true, NAME_None, 200, ResultObject);
}

void UCloudEconomySubsystem::RequestLocalConsume(FName Reason, FName Item, int32 Amount, FOnCloudOp Done)
{
	if (RejectWhenNotReady(TEXT("item.consume"), Done)) return;
	EItemType ItemType = EItemType::None;
	int32 Cap = 0;
	if (!FCloudEconomyData::ItemTypeFromName(Item, ItemType) || !LocalConsumeLimit(Reason, ItemType, Cap) ||
		Amount <= 0 || Amount > Cap)
	{
		CompleteLocalOp(Done, false, CloudErr::BadRequest, 400);
		return;
	}
	UGameInstance* GI = GetGameInstance();
	UItemInventoryManager* Items = GI ? GI->GetSubsystem<UItemInventoryManager>() : nullptr;
	URecruitmentManagerSubsystem* Recruitment = GI ? GI->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	USaveLoadManager* SaveManager = nullptr;
	if (!Items || !Recruitment || !EnsureLocalSaveCache(SaveManager))
	{
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	const int32 OriginalMileage = Recruitment->GetGachaData().Mileage.CurrentPoints;
	if (ItemType == EItemType::RecruitTicketPremium && OriginalMileage > MAX_int32 - Amount)
	{
		CompleteLocalOp(Done, false, CloudErr::BalanceOverflow, 409);
		return;
	}
	if (!Items->SpendItem(ItemType, Amount, false))
	{
		CompleteLocalOp(Done, false, CloudErr::InsufficientItems, 409);
		return;
	}
	if (ItemType == EItemType::RecruitTicketPremium)
	{
		FGachaRecruitmentData GachaData = Recruitment->GetGachaData();
		GachaData.Mileage.CurrentPoints += Amount;
		Recruitment->SetGachaData(GachaData);
		Recruitment->OnMileageChanged.Broadcast(GachaData.Mileage.CurrentPoints);
	}
	if (!SaveManager->SaveGameData())
	{
		Items->AddItem(ItemType, Amount, false);
		FGachaRecruitmentData GachaData = Recruitment->GetGachaData();
		GachaData.Mileage.CurrentPoints = OriginalMileage;
		Recruitment->SetGachaData(GachaData);
		Recruitment->OnMileageChanged.Broadcast(OriginalMileage);
		RefreshLocalState();
		CompleteLocalOp(Done, false, CloudErr::Internal, 500);
		return;
	}
	RefreshLocalState();
	const TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetNumberField(TEXT("amount"), Amount);
	CompleteLocalOp(Done, true, NAME_None, 200, ResultObject);
}

void UCloudEconomySubsystem::SendEconomyOp(
	const FString& Path,
	const FString& Action,
	const FString& OpId,
	const FString& Body,
	FOnCloudOp Done,
	bool bAllowAuthRetry)
{
	if (Action == TEXT("facility.instantFinish"))
	{
		UGameInstance* GI = GetGameInstance();
		USaveLoadManager* Save = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
		const USaveGame_GameData* SaveData = Save ? Save->GetCurrentSaveData() : nullptr;
		const FFacilityInstantFinishTransaction* Transaction = SaveData
			? SaveData->GameData.PendingFacilityInstantFinishes.FindByPredicate(
				[&OpId](const FFacilityInstantFinishTransaction& Entry) { return Entry.OpId == OpId; })
			: nullptr;
		const UFirebaseAuthSubsystem* Auth = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr;
		if (!Transaction || Transaction->Pid.IsEmpty() || !Auth || Auth->GetPid() != Transaction->Pid)
		{
			if (!FacilityAccountMismatchLogged.Contains(OpId))
			{
				FacilityAccountMismatchLogged.Add(OpId);
				UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility send account mismatch op=%s; request blocked"), *OpId.Left(8));
			}
			FCloudOpResult Mismatch;
			Mismatch.Code = CloudErr::AuthInvalid;
			Mismatch.HttpStatus = 401;
			Done.ExecuteIfBound(Mismatch);
			return;
		}
	}
	FCloudRequest Request;
	Request.Verb = TEXT("POST");
	Request.Path = Path;
	Request.Body = Body;
	Request.IdToken = AcquireIdToken();
	Request.Tag = CloudEconomyTag;
	Request.Action = Action;
	Request.OpId = OpId;

	const double StartSeconds = FPlatformTime::Seconds();
	TWeakObjectPtr<UCloudEconomySubsystem> WeakSelf(this);
	FCloudHttp::Send(Request,
		[WeakSelf, Path, Action, OpId, Body, Done, bAllowAuthRetry, StartSeconds](const FCloudResponse& Response)
	{
		if (UCloudEconomySubsystem* Self = WeakSelf.Get())
		{
			Self->HandleOpResponse(Path, Action, OpId, Body, Done, bAllowAuthRetry, StartSeconds, Response);
		}
	});
}

void UCloudEconomySubsystem::ApplyStateFromResponse(const FCloudResponse& Response)
{
	if (!Response.State.IsValid())
	{
		return;
	}

	FServerEconomyState Parsed;
	FString ParseError;
	if (!FCloudEconomyData::ParseState(Response.State, Parsed, ParseError))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] state parse fail msg=%s"), *ParseError);
		return;
	}

	// 지출·소모 응답의 state 는 부분 봉투다(wallet 만 / inventory 만) ― 절 존재 플래그가 나머지를 지킨다
	FString ApplyError;
	if (!ApplyServerEconomyStateTo(Parsed, ResolveTargets(), ApplyError))
	{
		UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] apply fail code=INTERNAL msg=%s"), *ApplyError);
		return;
	}

	// 부분 봉투는 LastState 를 통째로 덮지 않는다 ― 있는 절만 갱신한다
	if (Parsed.bHasWallet)
	{
		LastState.Wallet = Parsed.Wallet;
		LastState.bHasWallet = true;
	}
	if (Parsed.bHasInventory)
	{
		LastState.Inventory = Parsed.Inventory;
		LastState.bHasInventory = true;
	}
	if (Parsed.bHasShop)
	{
		LastState.Shop = Parsed.Shop;
		LastState.bHasShop = true;
	}
	if (Parsed.bHasAttendance)
	{
		LastState.Attendance = Parsed.Attendance;
		LastState.bHasAttendance = true;
	}
	if (Parsed.bHasRanking)
	{
		LastState.Ranking = Parsed.Ranking;
		LastState.bHasRanking = true;
	}
	if (!Parsed.DayKey.IsEmpty())
	{
		LastState.DayKey = Parsed.DayKey;
	}
	if (!Parsed.WeekKey.IsEmpty())
	{
		LastState.WeekKey = Parsed.WeekKey;
	}
	LastState.bValid = true;
}

void UCloudEconomySubsystem::HandleOpResponse(
	const FString& Path,
	const FString& Action,
	const FString& OpId,
	const FString& Body,
	FOnCloudOp Done,
	bool bAllowAuthRetry,
	double StartSeconds,
	const FCloudResponse& Response)
{
	const int32 ElapsedMs = static_cast<int32>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);
	const FString OpShort = OpId.Left(8);

	UGameInstance* GI = GetGameInstance();
	UFirebaseAuthSubsystem* AuthMgr = GI ? GI->GetSubsystem<UFirebaseAuthSubsystem>() : nullptr;
	if (Action == TEXT("facility.instantFinish"))
	{
		USaveLoadManager* Save = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
		const USaveGame_GameData* SaveData = Save ? Save->GetCurrentSaveData() : nullptr;
		const FFacilityInstantFinishTransaction* Transaction = SaveData
			? SaveData->GameData.PendingFacilityInstantFinishes.FindByPredicate(
				[&OpId](const FFacilityInstantFinishTransaction& Entry) { return Entry.OpId == OpId; })
			: nullptr;
		if (!Transaction || !AuthMgr || Transaction->Pid.IsEmpty() || AuthMgr->GetPid() != Transaction->Pid)
		{
			UE_LOG(LogTemp, Error, TEXT("[CloudEconomy] facility response account mismatch op=%s; state ignored"), *OpId.Left(8));
			FCloudOpResult Mismatch;
			Mismatch.Code = CloudErr::AuthInvalid;
			Mismatch.HttpStatus = 401;
			Done.ExecuteIfBound(Mismatch);
			return;
		}
	}

	// 401 은 같은 opId·같은 바디로 딱 1회 재시도한다 ― 지문이 바뀌면 409 OP_ID_MISMATCH 다
	if (Response.HttpStatus == 401 && bAllowAuthRetry && AuthMgr)
	{
		TWeakObjectPtr<UCloudEconomySubsystem> WeakSelf(this);
		const bool bScheduled = AuthMgr->NotifyAuthRejected(Response.Code,
			[WeakSelf, Path, Action, OpId, Body, Done](bool bRecovered)
		{
			UCloudEconomySubsystem* Self = WeakSelf.Get();
			if (!Self)
			{
				return;
			}

			if (bRecovered)
			{
				Self->SendEconomyOp(Path, Action, OpId, Body, Done, /*bAllowAuthRetry=*/false);
				return;
			}

			// 재인증을 포기했다 ― 호출부가 pending 플래그를 풀 수 있게 실패를 반드시 전달한다
			FCloudOpResult Abandoned;
			Abandoned.Code = CloudErr::AuthInvalid;
			Abandoned.Message = TEXT("re-authentication abandoned");
			Abandoned.HttpStatus = 401;
			UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] %s fail code=%s op=%s ms=0 (재인증 포기)"),
				*Action, *Abandoned.Code.ToString(), *OpId.Left(8));
			Done.ExecuteIfBound(Abandoned);
		});

		if (bScheduled)
		{
			return;
		}
	}

	FCloudOpResult Result;
	Result.bOk = Response.bOk;
	Result.Code = Response.Code.IsEmpty() ? NAME_None : FName(*Response.Code);
	Result.Message = Response.Message;
	Result.HttpStatus = Response.HttpStatus;
	Result.Result = Response.Result;

	if (Response.Root.IsValid())
	{
		const TSharedPtr<FJsonObject>* OpObject = nullptr;
		if (Response.Root->TryGetObjectField(TEXT("op"), OpObject) && OpObject && OpObject->IsValid())
		{
			(*OpObject)->TryGetBoolField(TEXT("replayed"), Result.bReplayed);
		}
	}

	// 실패 응답도 state 를 싣는다(잔액 부족 등) ― 로컬이 서버값으로 정정되는 자가 치유 경로다
	ApplyStateFromResponse(Response);

	if (Result.bOk)
	{
		if (AuthMgr)
		{
			AuthMgr->NotifyAuthAccepted();
		}
		UE_LOG(LogTemp, Log, TEXT("[CloudEconomy] %s ok code=OK op=%s ms=%d replayed=%d"),
			*Action, *OpShort, ElapsedMs, Result.bReplayed ? 1 : 0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CloudEconomy] %s fail code=%s op=%s ms=%d"),
			*Action, *Result.Code.ToString(), *OpShort, ElapsedMs);
	}

	Done.ExecuteIfBound(Result);
}

// ===== 지급 응답 해석 =====

bool UCloudEconomySubsystem::IsClaimBenign(FName Code)
{
	return Code == CloudErr::AlreadyClaimed;
}

int32 UCloudEconomySubsystem::ResolveCreditedAmount(int32 Requested, int32 ServerCredited)
{
	return FMath::Clamp(ServerCredited, 0, FMath::Max(0, Requested));
}

int32 UCloudEconomySubsystem::ReadCreditedAmount(const FCloudOpResult& OpResult, int32 Requested)
{
	int32 ServerCredited = 0;
	if (OpResult.Result.IsValid())
	{
		OpResult.Result->TryGetNumberField(TEXT("credited"), ServerCredited);
	}
	return ResolveCreditedAmount(Requested, ServerCredited);
}

// ===== pending 플래그 =====

bool UCloudEconomySubsystem::BeginPendingAction(FName ActionKey)
{
	if (ActionKey.IsNone() || PendingActions.Contains(ActionKey))
	{
		return false;
	}

	PendingActions.Add(ActionKey);
	return true;
}

void UCloudEconomySubsystem::EndPendingAction(FName ActionKey)
{
	PendingActions.Remove(ActionKey);
}
