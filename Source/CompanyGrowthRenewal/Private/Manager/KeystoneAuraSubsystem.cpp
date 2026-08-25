#include "Manager/KeystoneAuraSubsystem.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Data/BuildingEnhancementData.h"
#include "Table/BuildingData.h"
#include "Engine/World.h"

void UKeystoneAuraSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UKeystoneAuraSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UEntityManager* EM = GetEntityManager())
	{
		BuildingCountChangedHandle = EM->OnBuildingCountChanged.AddUObject(
			this, &UKeystoneAuraSubsystem::HandleBuildingCountChanged);
	}
	RecomputeAuras();
}

void UKeystoneAuraSubsystem::Deinitialize()
{
	if (CachedEntityManager && BuildingCountChangedHandle.IsValid())
	{
		CachedEntityManager->OnBuildingCountChanged.Remove(BuildingCountChangedHandle);
		BuildingCountChangedHandle.Reset();
	}
	// 월드 전환 시 죽은 구독자 잔류 방지
	OnAurasRecomputed.Clear();
	Super::Deinitialize();
}

void UKeystoneAuraSubsystem::HandleBuildingCountChanged()
{
	RecomputeAuras();
}

UEntityManager* UKeystoneAuraSubsystem::GetEntityManager() const
{
	if (!CachedEntityManager)
	{
		if (UWorld* W = GetWorld())
		{
			CachedEntityManager = W->GetSubsystem<UEntityManager>();
		}
	}
	return CachedEntityManager;
}

UTableManagerSubsystem* UKeystoneAuraSubsystem::GetTableManager() const
{
	if (!CachedTableManager)
	{
		if (UWorld* W = GetWorld())
		{
			if (UGameInstance* GI = W->GetGameInstance())
			{
				CachedTableManager = GI->GetSubsystem<UTableManagerSubsystem>();
			}
		}
	}
	return CachedTableManager;
}

float UKeystoneAuraSubsystem::GetAuraPercent(int32 BuildingIndex, EBuildingTraitTarget Target) const
{
	if (const TMap<EBuildingTraitTarget, float>* Inner = AuraCache.Find(BuildingIndex))
	{
		if (const float* Val = Inner->Find(Target))
		{
			return *Val;
		}
	}
	return 0.0f;
}

bool UKeystoneAuraSubsystem::IsInAnyAuraRange(int32 BuildingIndex) const
{
	const TMap<EBuildingTraitTarget, float>* Inner = AuraCache.Find(BuildingIndex);
	return Inner && Inner->Num() > 0;
}

void UKeystoneAuraSubsystem::RecomputeAuras()
{
	RecomputeAurasInternal();
	// early return 경로에서도 캐시는 이미 비워졌으므로 항상 알린다 — 배지가 "밖" 으로 갱신돼야 한다
	OnAurasRecomputed.Broadcast();
}

void UKeystoneAuraSubsystem::RecomputeAurasInternal()
{
	AuraCache.Reset();

	UEntityManager* EM = GetEntityManager();
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!EM || !TMgr) return;

	const TArray<ABuildingBaseActor*>& AllBuildings = EM->GetBuildings();

	for (ABuildingBaseActor* Keystone : AllBuildings)
	{
		if (!Keystone) continue;

		bool bOk = false;
		const FBuildingData KData = TMgr->GetBuildingData(Keystone->GetBuildingID(), bOk);
		if (!bOk || !KData.KeystoneAura.bIsKeystone) continue;

		const FKeystoneAuraData& Aura = KData.KeystoneAura;
		if (Aura.AuraTarget == EBuildingTraitTarget::None) continue;

		const int32 Level = FMath::Max(1, Keystone->GetMonumentLevel());
		// 반경=층수(Level)에서 파생, 버프 효과(%)=별도 KeystoneAuraPower 업그레이드에서 파생
		const int32 AuraPowerLevel = Keystone->GetEnhancementLevel(
			EBuildingEnhancementType::KeystoneAuraPower);
		const float Percent = Aura.BasePercent
			* UBuildingEnhancementHelper::CalculateEffectMultiplier(
				EBuildingEnhancementType::KeystoneAuraPower, AuraPowerLevel);
		if (Percent <= 0.0f) continue;

		const FVector KeyLoc = Keystone->GetActorLocation();
		const float RadiusCm = (Aura.BaseRadiusCells + (Level - 1) * Aura.RadiusPerLevelCells) * FootprintCellSize;
		const float RadiusSq = RadiusCm * RadiusCm;

		for (ABuildingBaseActor* Neighbor : AllBuildings)
		{
			if (!Neighbor || Neighbor == Keystone) continue;

			// 모뉴먼트는 버프 대상에서 제외(이웃 일반 빌딩만).
			bool bNeighborOk = false;
			const FBuildingData NData = TMgr->GetBuildingData(Neighbor->GetBuildingID(), bNeighborOk);
			if (bNeighborOk && NData.KeystoneAura.bIsKeystone) continue;

			if (!Aura.bGlobal)
			{
				const float DistSq = FVector::DistSquared2D(KeyLoc, Neighbor->GetActorLocation());
				if (DistSq > RadiusSq) continue;
			}

			AuraCache.FindOrAdd(Neighbor->GetBuildingIndex()).FindOrAdd(Aura.AuraTarget) += Percent;
		}
	}
}

TArray<ABuildingBaseActor*> UKeystoneAuraSubsystem::GetBuildingsInKeystoneZone(ABuildingBaseActor* Keystone) const
{
	TArray<ABuildingBaseActor*> Result;
	if (!Keystone || !Keystone->IsKeystoneMonument()) return Result;

	UEntityManager* EM = GetEntityManager();
	UTableManagerSubsystem* TMgr = GetTableManager();
	if (!EM || !TMgr) return Result;

	bool bOk = false;
	const FBuildingData KData = TMgr->GetBuildingData(Keystone->GetBuildingID(), bOk);
	if (!bOk || !KData.KeystoneAura.bIsKeystone) return Result;

	const FKeystoneAuraData& Aura = KData.KeystoneAura;

	// 반경식은 RecomputeAuras 와 동일(레벨=층수에서 파생).
	const int32 Level = FMath::Max(1, Keystone->GetMonumentLevel());
	const FVector KeyLoc = Keystone->GetActorLocation();
	const float RadiusCm = (Aura.BaseRadiusCells + (Level - 1) * Aura.RadiusPerLevelCells) * FootprintCellSize;
	const float RadiusSq = RadiusCm * RadiusCm;

	for (ABuildingBaseActor* Neighbor : EM->GetBuildings())
	{
		if (!Neighbor || Neighbor == Keystone) continue;

		// 모뉴먼트는 버프 대상에서 제외(이웃 일반 빌딩만).
		bool bNeighborOk = false;
		const FBuildingData NData = TMgr->GetBuildingData(Neighbor->GetBuildingID(), bNeighborOk);
		if (bNeighborOk && NData.KeystoneAura.bIsKeystone) continue;

		if (!Aura.bGlobal)
		{
			if (FVector::DistSquared2D(KeyLoc, Neighbor->GetActorLocation()) > RadiusSq) continue;
		}

		Result.Add(Neighbor);
	}

	return Result;
}
