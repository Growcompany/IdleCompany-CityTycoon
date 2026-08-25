// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/TradeOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Data/ProjectBoardData.h"
#include "Data/GameSaveData.h"
#include "Data/BuildingSaveData.h"
#include "Table/TradeOrderBalanceTable.h"

void UTradeOrderManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ticker 등록 — WorldMapManager와 동일한 주기로 구동
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UTradeOrderManager::TickInternal),
		WorldMapConstants::TickIntervalSec
	);

	// 첫 부팅(세이브 데이터가 비어 있음) 시 초기 보드 채움
	if (ActiveOrders.Num() == 0)
	{
		TrySpawnOrders();
	}

	UE_LOG(LogTemp, Log, TEXT("[TradeOrderManager] Initialized, Active=%d"), ActiveOrders.Num());
}

void UTradeOrderManager::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Super::Deinitialize();
}

TArray<FTradeOrder> UTradeOrderManager::GetOrdersByTier(ETradeOrderTier Tier) const
{
	TArray<FTradeOrder> Result;
	for (const FTradeOrder& O : ActiveOrders)
	{
		if (O.Tier == Tier)
		{
			Result.Add(O);
		}
	}
	return Result;
}

TArray<FTradeOrder> UTradeOrderManager::GetAcceptedOrders() const
{
	TArray<FTradeOrder> Result;
	for (const FTradeOrder& O : ActiveOrders)
	{
		if (O.bAccepted)
		{
			Result.Add(O);
		}
	}
	// 시간 적게 남은 순 정렬
	Result.Sort([](const FTradeOrder& A, const FTradeOrder& B)
	{
		return A.ExpireTime < B.ExpireTime;
	});
	return Result;
}

TArray<FTradeOrder> UTradeOrderManager::GetUnacceptedOrders() const
{
	TArray<FTradeOrder> Result;
	for (const FTradeOrder& O : ActiveOrders)
	{
		if (!O.bAccepted)
		{
			Result.Add(O);
		}
	}
	// 티어 우선 (Urgent > VIP > Normal), 같은 티어면 시간 적게 남은 순
	Result.Sort([](const FTradeOrder& A, const FTradeOrder& B)
	{
		if (A.Tier != B.Tier)
		{
			// Urgent(1) > VIP(2) > Normal(0)
			auto TierPriority = [](ETradeOrderTier T) -> int32
			{
				switch (T)
				{
				case ETradeOrderTier::Urgent: return 0;
				case ETradeOrderTier::VIP:    return 1;
				case ETradeOrderTier::Normal: return 2;
				default: return 3;
				}
			};
			return TierPriority(A.Tier) < TierPriority(B.Tier);
		}
		return A.ExpireTime < B.ExpireTime;
	});
	return Result;
}

int32 UTradeOrderManager::GetAcceptedCount() const
{
	int32 Count = 0;
	for (const FTradeOrder& O : ActiveOrders)
	{
		if (O.bAccepted) ++Count;
	}
	return Count;
}

int32 UTradeOrderManager::GetMaxAcceptedSlots() const
{
	return GetBalance().MaxAcceptedSlots;
}

bool UTradeOrderManager::AcceptOrder(int32 OrderId)
{
	if (GetAcceptedCount() >= GetMaxAcceptedSlots()) return false;

	for (FTradeOrder& O : ActiveOrders)
	{
		if (O.OrderId == OrderId && !O.bAccepted)
		{
			O.bAccepted = true;
			OnOrderAccepted.Broadcast(OrderId);
			SaveGameData();
			return true;
		}
	}
	return false;
}

bool UTradeOrderManager::DismissOrder(int32 OrderId)
{
	const int32 FoundIdx = ActiveOrders.IndexOfByPredicate(
		[OrderId](const FTradeOrder& O) { return O.OrderId == OrderId; });
	if (FoundIdx == INDEX_NONE) return false;

	const bool bWasAccepted = ActiveOrders[FoundIdx].bAccepted;
	ActiveOrders.RemoveAt(FoundIdx);

	// 수락한 주문 포기 시 콤보 리셋 (만료 동작과 일관)
	if (bWasAccepted && ComboCount > 0)
	{
		ComboCount = 0;
		OnComboChanged.Broadcast(ComboCount, GetComboMultiplier());
	}

	OnOrderDismissed.Broadcast(OrderId, bWasAccepted);
	SaveGameData();
	return true;
}

void UTradeOrderManager::MarkOrdersAsViewed(const TArray<int32>& OrderIds)
{
	if (OrderIds.Num() == 0) return;

	bool bChanged = false;
	for (FTradeOrder& O : ActiveOrders)
	{
		if (!O.bViewed && OrderIds.Contains(O.OrderId))
		{
			O.bViewed = true;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		SaveGameData();
	}
}

float UTradeOrderManager::GetComboMultiplier() const
{
	// 0~1회: x1.0 (코드 폴백 — DT 의미 없음). 2회+ DT 값.
	if (ComboCount <= 1) return 1.0f;
	const FTradeOrderBalanceData& B = GetBalance();
	if (ComboCount == 2) return B.ComboMul_2;
	if (ComboCount == 3) return B.ComboMul_3;
	if (ComboCount == 4) return B.ComboMul_4;
	return B.ComboMul_5Plus;
}

FTradeOrder UTradeOrderManager::FindMatchingOrder(ECompanyType Company, int32 ProjectIndex, bool& bOutFound) const
{
	bOutFound = false;

	// VIP > Urgent > Normal 순으로 우선순위 검색
	const FDateTime Now = FDateTime::UtcNow();
	const ETradeOrderTier Priority[] = { ETradeOrderTier::VIP, ETradeOrderTier::Urgent, ETradeOrderTier::Normal };

	for (ETradeOrderTier Tier : Priority)
	{
		for (const FTradeOrder& O : ActiveOrders)
		{
			if (O.Tier != Tier) continue;
			if (O.CompanyType != Company) continue;
			if (O.ProjectIndex != ProjectIndex) continue;
			if (O.IsExpired(Now)) continue;
			if (O.IsFulfilled()) continue;

			bOutFound = true;
			return O;
		}
	}

	return FTradeOrder();
}

bool UTradeOrderManager::ConsumeOrderQuantity(int32 OrderId, int64 FulfilledQty)
{
	if (FulfilledQty <= 0) return false;

	int32 FoundIdx = INDEX_NONE;
	for (int32 i = 0; i < ActiveOrders.Num(); ++i)
	{
		if (ActiveOrders[i].OrderId == OrderId)
		{
			FoundIdx = i;
			break;
		}
	}

	if (FoundIdx == INDEX_NONE) return false;

	FTradeOrder& Order = ActiveOrders[FoundIdx];

	// 수락한 주문만 납품 가능
	if (!Order.bAccepted) return false;

	// 남은 수량 이상 차감되지 않도록 clamp
	const int64 Actual = FMath::Min<int64>(FulfilledQty, Order.RemainingQuantity);
	if (Actual <= 0) return false;

	Order.RemainingQuantity -= Actual;

	if (Order.IsFulfilled())
	{
		const int32 CompletedId = Order.OrderId;
		ActiveOrders.RemoveAt(FoundIdx);

		// 콤보 증가
		++ComboCount;
		OnComboChanged.Broadcast(ComboCount, GetComboMultiplier());

		OnOrderCompleted.Broadcast(CompletedId);
	}
	else
	{
		OnOrderFulfilledPartial.Broadcast(Order.OrderId, Order.RemainingQuantity);
	}

	SaveGameData();
	return true;
}

FTradeOrder UTradeOrderManager::GenerateOrderByTier(ETradeOrderTier Tier, ECompanyType ForceCompany)
{
	FTradeOrder Order = CreateRandomOrder(Tier, ForceCompany);
	ActiveOrders.Add(Order);
	OnOrderGenerated.Broadcast(Order);
	SaveGameData();
	return Order;
}

bool UTradeOrderManager::RegenerateIfExpired()
{
	const int32 Before = ActiveOrders.Num();
	ExpireOldOrders();
	TrySpawnOrders();
	return ActiveOrders.Num() != Before;
}

void UTradeOrderManager::ClearAllOrders()
{
	ActiveOrders.Empty();
	SaveGameData();
}

void UTradeOrderManager::SerializeForSave(TArray<FTradeOrder>& OutOrders, int32& OutNextOrderId, int32& OutComboCount) const
{
	OutOrders = ActiveOrders;
	OutNextOrderId = NextOrderId;
	OutComboCount = ComboCount;
}

void UTradeOrderManager::LoadFromSave(const TArray<FTradeOrder>& InOrders, int32 InNextOrderId, int32 InComboCount)
{
	ActiveOrders = InOrders;
	NextOrderId = FMath::Max(1, InNextOrderId);
	ComboCount = FMath::Max(0, InComboCount);
}

bool UTradeOrderManager::TickInternal(float DeltaTime)
{
	// 매 Tick마다 만료 주문서 제거
	ExpireOldOrders();

	// 주기적 생성 시도 (DT 의 SpawnCheckIntervalSec)
	NextGenCheckSec += DeltaTime;
	if (NextGenCheckSec >= GetBalance().SpawnCheckIntervalSec)
	{
		NextGenCheckSec = 0.0f;
		TrySpawnOrders();
	}

	return true;
}

void UTradeOrderManager::ExpireOldOrders()
{
	const FDateTime Now = FDateTime::UtcNow();
	bool bRemoved = false;
	bool bAcceptedExpired = false;

	for (int32 i = ActiveOrders.Num() - 1; i >= 0; --i)
	{
		if (ActiveOrders[i].IsExpired(Now))
		{
			if (ActiveOrders[i].bAccepted)
			{
				bAcceptedExpired = true;
			}
			const int32 ExpiredId = ActiveOrders[i].OrderId;
			ActiveOrders.RemoveAt(i);
			OnOrderExpired.Broadcast(ExpiredId);
			bRemoved = true;
		}
	}

	// 수락한 주문이 만료되면 콤보 리셋
	if (bAcceptedExpired && ComboCount > 0)
	{
		ComboCount = 0;
		OnComboChanged.Broadcast(ComboCount, GetComboMultiplier());
	}

	if (bRemoved)
	{
		SaveGameData();
	}
}

void UTradeOrderManager::TrySpawnOrders()
{
	if (bReentryGuard) return;
	bReentryGuard = true;

	// 티어별 현재 수량 집계
	int32 CountPerTier[3] = { 0, 0, 0 };
	for (const FTradeOrder& O : ActiveOrders)
	{
		const int32 Idx = static_cast<int32>(O.Tier);
		if (Idx >= 0 && Idx < 3)
		{
			++CountPerTier[Idx];
		}
	}

	const FTradeOrderBalanceData& B = GetBalance();
	const int32 MaxActiveByTier[3] = { B.MaxActive_Normal, B.MaxActive_Urgent, B.MaxActive_VIP };

	bool bAdded = false;
	for (int32 TierIdx = 0; TierIdx < 3; ++TierIdx)
	{
		const ETradeOrderTier Tier = static_cast<ETradeOrderTier>(TierIdx);
		while (CountPerTier[TierIdx] < MaxActiveByTier[TierIdx])
		{
			FTradeOrder NewOrder = CreateRandomOrder(Tier);
			ActiveOrders.Add(NewOrder);
			OnOrderGenerated.Broadcast(NewOrder);
			++CountPerTier[TierIdx];
			bAdded = true;
		}
	}

	if (bAdded)
	{
		SaveGameData();
	}

	bReentryGuard = false;
}

FTradeOrder UTradeOrderManager::CreateRandomOrder(ETradeOrderTier Tier, ECompanyType ForceCompany)
{
	FTradeOrder Order;
	Order.OrderId = NextOrderId++;
	Order.Tier = Tier;

	// 회사 타입 선택: 제조업 3종 중에서만 (서비스업은 재고 개념 없어 납품 불가)
	ECompanyType PickedCompany = ForceCompany;
	if (PickedCompany == ECompanyType::None)
	{
		const ECompanyType ManuPool[] = {
			ECompanyType::Electronics,
			ECompanyType::Automobile,
			ECompanyType::Semiconductor
		};
		PickedCompany = ManuPool[FMath::RandRange(0, 2)];
	}
	Order.CompanyType = PickedCompany;

	// ProjectIndex 결정: 해당 산업 빌딩들의 최고 해금 Tier 를 구한 뒤,
	// [1..MaxTier] 중 가중 랜덤 → 뽑힌 Tier 범위 (Start, End) 안에서 균등 랜덤.
	const int32 MaxTier = GetHighestUnlockedTier(PickedCompany);
	const int32 PickedTier = PickWeightedRandomTier(MaxTier);
	int32 StartProj = 1, EndProj = TierConstants::PROJECTS_PER_TIER;
	FProjectTierProgress::GetTierProjectRange(PickedTier, StartProj, EndProj);
	Order.ProjectIndex = FMath::RandRange(StartProj, EndProj);

	// 티어별 수량/배율/기간 설정 (DT 기반)
	const FTradeOrderBalanceData& B = GetBalance();
	float DurationHours = B.Normal_DurationHoursMin;
	switch (Tier)
	{
	case ETradeOrderTier::Normal:
		Order.RequestedQuantity = FMath::RandRange(B.Normal_QtyMin, B.Normal_QtyMax);
		Order.RewardMultiplier  = FMath::FRandRange(B.Normal_RewardMultMin, B.Normal_RewardMultMax);
		DurationHours           = FMath::FRandRange(B.Normal_DurationHoursMin, B.Normal_DurationHoursMax);
		Order.DiamondBonus      = 0;
		break;

	case ETradeOrderTier::Urgent:
		Order.RequestedQuantity = FMath::RandRange(B.Urgent_QtyMin, B.Urgent_QtyMax);
		Order.RewardMultiplier  = FMath::FRandRange(B.Urgent_RewardMultMin, B.Urgent_RewardMultMax);
		DurationHours           = FMath::FRandRange(B.Urgent_DurationHoursMin, B.Urgent_DurationHoursMax);
		Order.DiamondBonus      = 0;
		break;

	case ETradeOrderTier::VIP:
		Order.RequestedQuantity = FMath::RandRange(B.VIP_QtyMin, B.VIP_QtyMax);
		Order.RewardMultiplier  = FMath::FRandRange(B.VIP_RewardMultMin, B.VIP_RewardMultMax);
		DurationHours           = B.VIP_DurationHours;
		Order.DiamondBonus      = FMath::RandRange(B.VIP_DiamondMin, B.VIP_DiamondMax);
		break;
	}

	Order.RemainingQuantity = Order.RequestedQuantity;
	Order.ExpireTime = FDateTime::UtcNow() + FTimespan::FromHours(DurationHours);

	return Order;
}

int32 UTradeOrderManager::GetHighestUnlockedTier(ECompanyType Company)
{
	// 해당 산업의 "빌딩들" 중 가장 높은 해금 Tier 를 반환.
	// Buildings(정의 = BuildingIndex + CompanyType) 와 OfficeDataMap(정의 = BuildingIndex → TierProgress) 조인.
	int32 MaxTier = 1;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return MaxTier;

	USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return MaxTier;

	USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData();
	if (!SaveData) return MaxTier;

	const FGameSaveData& GameData = SaveData->GameData;
	for (const FBuildingEntitySaveData& Bld : GameData.Buildings)
	{
		if (Bld.BuildingData.CompanyType != Company) continue;

		const FOfficeSaveData* Office = GameData.OfficeDataMap.Find(Bld.BuildingIndex);
		if (!Office) continue;

		const int32 Tier = FMath::Clamp(Office->TierProgress.CurrentTier, 1, TierConstants::MAX_TIER);
		if (Tier > MaxTier) MaxTier = Tier;
	}

	return MaxTier;
}

int32 UTradeOrderManager::PickWeightedRandomTier(int32 MaxTier) const
{
	if (MaxTier <= 1) return 1;

	// Weight[T] = TierWeightBase^(T - 1). 최신 티어가 가장 크게 편중되는 기하 분포.
	const float WeightBase = GetBalance().TierWeightBase;
	TArray<float> Weights;
	Weights.Reserve(MaxTier);
	float TotalWeight = 0.0f;
	for (int32 T = 1; T <= MaxTier; ++T)
	{
		const float W = FMath::Pow(WeightBase, static_cast<float>(T - 1));
		Weights.Add(W);
		TotalWeight += W;
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER) return MaxTier;

	const float Roll = FMath::FRandRange(0.0f, TotalWeight);
	float Accum = 0.0f;
	for (int32 Idx = 0; Idx < Weights.Num(); ++Idx)
	{
		Accum += Weights[Idx];
		if (Roll <= Accum)
		{
			return Idx + 1;  // Idx 0 → Tier 1
		}
	}
	return MaxTier;
}

const FTradeOrderBalanceData& UTradeOrderManager::GetBalance() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			return TableMgr->GetTradeOrderBalance();
		}
	}
	// TableMgr 미초기화 시 struct 헤더 기본값으로 폴백 (정적 인스턴스).
	static const FTradeOrderBalanceData Fallback;
	return Fallback;
}

void UTradeOrderManager::SaveGameData()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}
