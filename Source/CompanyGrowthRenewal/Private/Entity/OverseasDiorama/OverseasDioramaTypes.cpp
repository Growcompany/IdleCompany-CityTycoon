#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"

#include "Data/MineUpgradeData.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Engine/GameInstance.h"
#include "Manager/MineManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/WorldFactoryManager.h"
#include "Table/CountryInfoTable.h"

namespace
{
	constexpr int32 kDioFactoryMinSlots = 3;
	constexpr int32 kDioFactoryMaxSlots = 6;
	constexpr int32 kDioMineMinUnits = 1;
	constexpr int32 kDioMineMaxUnits = 5;
	constexpr int32 kDioMaxTier = 3;

	int32 FactoryTierFor(const UTableManagerSubsystem* TableMgr, const UWorldFactoryManager* FactoryMgr,
		ECountryType Country, EWorldFactoryUpgradeType Type)
	{
		FWorldFactoryUpgradeDefinition Def;
		if (!TableMgr->GetWorldFactoryUpgradeDefinition(Type, Def))
		{
			UE_LOG(LogTemp, Warning, TEXT("[OverseasDiorama] 공장 강화 정의 없음 type=%d"), static_cast<int32>(Type));
			return 0;
		}
		return OverseasDiorama::AppearanceTier(FactoryMgr->GetUpgradeLevel(Country, Type), Def.AppearanceLevels);
	}

	int32 MineTierFor(const UTableManagerSubsystem* TableMgr, const UMineManager* MineMgr,
		ECountryType Country, EMineUpgradeType Type)
	{
		FMineUpgradeDefinition Def;
		if (!TableMgr->GetMineUpgradeDefinition(Type, Def))
		{
			UE_LOG(LogTemp, Warning, TEXT("[OverseasDiorama] 광산 강화 정의 없음 type=%d"), static_cast<int32>(Type));
			return 0;
		}
		return OverseasDiorama::AppearanceTier(MineMgr->GetUpgradeLevel(Country, Type), Def.AppearanceLevels);
	}
}

bool FDioramaMineSpec::IsAllOil() const
{
	if (Units.Num() == 0)
	{
		return false;
	}
	for (const FDioramaMineUnit& Unit : Units)
	{
		if (Unit.Resource != EResourceType::Oil)
		{
			return false;
		}
	}
	return true;
}

bool FOverseasDioramaSpec::HasModule(EDioramaModule Module) const
{
	switch (Module)
	{
	case EDioramaModule::Factory: return Factory.bPresent;
	case EDioramaModule::Mine: return Mine.bPresent;
	case EDioramaModule::Port: return Port.bPresent;
	}
	return false;
}

EDioramaModule FOverseasDioramaSpec::PrimaryModule() const
{
	if (Factory.bPresent)
	{
		return EDioramaModule::Factory;
	}
	if (Mine.bPresent)
	{
		return EDioramaModule::Mine;
	}
	return EDioramaModule::Port;
}

uint32 FOverseasDioramaSpec::GetLookHash() const
{
	uint32 Hash = GetTypeHash(Country);
	Hash = HashCombine(Hash, GetTypeHash(Factory.bPresent));
	Hash = HashCombine(Hash, GetTypeHash(Factory.Industry));
	Hash = HashCombine(Hash, GetTypeHash(Factory.Slots.Num()));
	for (EDioramaSlotState State : Factory.Slots)
	{
		Hash = HashCombine(Hash, GetTypeHash(State));
	}
	Hash = HashCombine(Hash, GetTypeHash(Factory.AutomationTier));
	Hash = HashCombine(Hash, GetTypeHash(Factory.QualityTier));
	Hash = HashCombine(Hash, GetTypeHash(Factory.StorageTier));
	Hash = HashCombine(Hash, GetTypeHash(Factory.RushTier));

	Hash = HashCombine(Hash, GetTypeHash(Mine.bPresent));
	Hash = HashCombine(Hash, GetTypeHash(Mine.Units.Num()));
	for (const FDioramaMineUnit& Unit : Mine.Units)
	{
		Hash = HashCombine(Hash, GetTypeHash(Unit.Resource));
		Hash = HashCombine(Hash, GetTypeHash(Unit.State));
	}
	Hash = HashCombine(Hash, GetTypeHash(Mine.RateTier));
	Hash = HashCombine(Hash, GetTypeHash(Mine.StorageTier));
	Hash = HashCombine(Hash, GetTypeHash(Mine.OfflineTier));
	Hash = HashCombine(Hash, GetTypeHash(Mine.bAutoClaim));

	Hash = HashCombine(Hash, GetTypeHash(Port.bPresent));
	Hash = HashCombine(Hash, GetTypeHash(Port.CraneCount));
	return Hash;
}

int32 OverseasDiorama::AppearanceTier(int32 Level, const TArray<int32>& AppearanceLevels)
{
	if (AppearanceLevels.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OverseasDiorama] AppearanceLevels 비어 있음 ― 외형 0단계로 표시 (DT 재임포트 확인)"));
		return 0;
	}
	int32 Tier = 0;
	for (int32 Threshold : AppearanceLevels)
	{
		if (Threshold <= Level)
		{
			++Tier;
		}
	}
	return FMath::Clamp(Tier, 0, kDioMaxTier);
}

EDioramaMineKind OverseasDiorama::MineKindFor(EResourceType Resource)
{
	switch (Resource)
	{
	case EResourceType::Oil: return EDioramaMineKind::Pump;
	case EResourceType::Wood: return EDioramaMineKind::Lumber;
	default: return EDioramaMineKind::Pit;
	}
}

FOverseasDioramaSpec OverseasDiorama::AssembleSpec(const FDioramaSpecInputs& In)
{
	FOverseasDioramaSpec Spec;
	Spec.Country = In.Country;

	if (In.bFactory)
	{
		FDioramaFactorySpec& F = Spec.Factory;
		F.bPresent = true;
		F.Industry = In.Industry;
		const int32 SlotCount = FMath::Clamp(In.FactoryMaxLines, kDioFactoryMinSlots, kDioFactoryMaxSlots);
		F.Slots.Init(EDioramaSlotState::Empty, SlotCount);
		for (int32 i = 0; i < FMath::Min(SlotCount, In.FactoryLineCompleted.Num()); ++i)
		{
			F.Slots[i] = In.FactoryLineCompleted[i] ? EDioramaSlotState::Completed : EDioramaSlotState::Running;
		}
		F.AutomationTier = FMath::Clamp(In.FactoryTiers[0], 0, kDioMaxTier);
		F.QualityTier = FMath::Clamp(In.FactoryTiers[1], 0, kDioMaxTier);
		F.StorageTier = FMath::Clamp(In.FactoryTiers[2], 0, kDioMaxTier);
		F.RushTier = FMath::Clamp(In.FactoryTiers[3], 0, kDioMaxTier);
	}

	if (In.bMine && In.MinableResources.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OverseasDiorama] 채광국인데 MinableResources 가 비어 광산을 짓지 않음 country=%d"),
			static_cast<int32>(In.Country));
	}
	else if (In.bMine)
	{
		FDioramaMineSpec& M = Spec.Mine;
		M.bPresent = true;
		const int32 UnitCount = FMath::Clamp(In.MineMaxLines, kDioMineMinUnits, kDioMineMaxUnits);
		const int32 ActiveCount = FMath::Min(UnitCount, In.MineActiveLines.Num());
		for (int32 i = 0; i < ActiveCount; ++i)
		{
			M.Units.Add(In.MineActiveLines[i]);
		}
		// 빈 자리는 아직 캐지 않는 자원부터 보여 준다(같은 자원만 반복되면 나라의 자원 구성이 안 읽힌다).
		TArray<EResourceType> IdleOrder;
		for (EResourceType Resource : In.MinableResources)
		{
			if (!M.Units.ContainsByPredicate([Resource](const FDioramaMineUnit& Unit) { return Unit.Resource == Resource; }))
			{
				IdleOrder.Add(Resource);
			}
		}
		for (EResourceType Resource : In.MinableResources)
		{
			IdleOrder.AddUnique(Resource);
		}
		for (int32 k = 0; M.Units.Num() < UnitCount; ++k)
		{
			FDioramaMineUnit Idle;
			Idle.Resource = IdleOrder[k % IdleOrder.Num()];
			M.Units.Add(Idle);
		}
		M.RateTier = FMath::Clamp(In.MineTiers[0], 0, kDioMaxTier);
		M.StorageTier = FMath::Clamp(In.MineTiers[1], 0, kDioMaxTier);
		M.OfflineTier = FMath::Clamp(In.MineTiers[2], 0, kDioMaxTier);
		M.bAutoClaim = In.bMineAutoClaim;
	}

	if (In.bPort)
	{
		Spec.Port.bPresent = true;
		Spec.Port.CraneCount = (Spec.Factory.bPresent || Spec.Mine.bPresent) ? 2 : 3;
	}
	return Spec;
}

bool OverseasDiorama::GatherInputs(const UGameInstance* GI, ECountryType Country, FDioramaSpecInputs& Out)
{
	if (!GI)
	{
		return false;
	}
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const UWorldFactoryManager* FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
	const UMineManager* MineMgr = GI->GetSubsystem<UMineManager>();
	if (!TableMgr || !FactoryMgr || !MineMgr)
	{
		return false;
	}

	bool bFound = false;
	const FCountryInfoTable Row = TableMgr->GetCountryInfo(Country, bFound);
	if (!bFound)
	{
		return false;
	}

	Out = FDioramaSpecInputs();
	Out.Country = Country;
	Out.bFactory = Row.bSupportsFactory;
	Out.bMine = Row.bSupportsMine;
	Out.bPort = Row.bIsTradeHub;
	Out.Industry = Row.RepresentativeIndustry;
	Out.MinableResources = Row.MinableResources;

	if (Out.bFactory)
	{
		Out.FactoryMaxLines = FactoryMgr->GetMaxLines(Country);
		for (const FWorldFactoryLineState& Stored : FactoryMgr->GetActiveLines(Country))
		{
			FWorldFactoryLineState Live;
			Out.FactoryLineCompleted.Add(FactoryMgr->GetLineState(Country, Stored.LineId, Live) && Live.bCompleted);
		}
		Out.FactoryTiers[0] = FactoryTierFor(TableMgr, FactoryMgr, Country, EWorldFactoryUpgradeType::Automation);
		Out.FactoryTiers[1] = FactoryTierFor(TableMgr, FactoryMgr, Country, EWorldFactoryUpgradeType::QualityControl);
		Out.FactoryTiers[2] = FactoryTierFor(TableMgr, FactoryMgr, Country, EWorldFactoryUpgradeType::StorageExpansion);
		Out.FactoryTiers[3] = FactoryTierFor(TableMgr, FactoryMgr, Country, EWorldFactoryUpgradeType::RushProduction);
	}

	if (Out.bMine)
	{
		Out.MineMaxLines = MineMgr->GetMaxLines(Country);
		for (const FMineLineState& Stored : MineMgr->GetMineLines(Country))
		{
			FMineLineState Live;
			if (!MineMgr->GetLineState(Country, Stored.Resource, Live))
			{
				continue;
			}
			FDioramaMineUnit Unit;
			Unit.Resource = Stored.Resource;
			Unit.State = Live.bSaturated ? EDioramaSlotState::Completed : EDioramaSlotState::Running;
			const int64 Cap = FMath::Max<int64>(1, MineMgr->GetEffectiveMaxStorage(Country, Stored.Resource));
			Unit.Fill01 = FMath::Clamp(static_cast<float>(static_cast<double>(Live.CurrentQty) / static_cast<double>(Cap)), 0.f, 1.f);
			Out.MineActiveLines.Add(Unit);
		}
		Out.MineTiers[0] = MineTierFor(TableMgr, MineMgr, Country, EMineUpgradeType::MiningRate);
		Out.MineTiers[1] = MineTierFor(TableMgr, MineMgr, Country, EMineUpgradeType::MiningStorage);
		Out.MineTiers[2] = MineTierFor(TableMgr, MineMgr, Country, EMineUpgradeType::OfflineCatchupBoost);
		Out.bMineAutoClaim = MineTierFor(TableMgr, MineMgr, Country, EMineUpgradeType::AutoClaim) >= 1;
	}
	return true;
}
