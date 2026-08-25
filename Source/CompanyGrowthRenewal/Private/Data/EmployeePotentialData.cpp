#include "Data/EmployeePotentialData.h"
#include "Manager/TableManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// 파일 고유 네임스페이스 — 익명 ns 상수는 유니티 빌드 TU 병합 시 타 cpp 와 재정의 충돌한다
namespace PotentialOdds
{
	struct FBaseChance
	{
		ELootBoxRarity Rarity;
		float Percent;
	};

	// 등급 추첨 기본 확률 (높은 등급부터). RollPotentialRarity 와 확률표 UI 가 공유하는 단일 진실 —
	// 두 곳에 숫자를 따로 두면 표가 실제와 조용히 어긋난다.
	static const FBaseChance BaseTable[] = {
		{ ELootBoxRarity::Legendary,  0.5f },
		{ ELootBoxRarity::Epic,       2.0f },
		{ ELootBoxRarity::Rare,       7.5f },
		{ ELootBoxRarity::Unusual,   20.0f },
		{ ELootBoxRarity::Common,    70.0f },
	};
}

namespace
{
	// 크리 줄 환산 스케일 (Live Coding 튜닝) — 값 1~25 → 확률 +0.04~1%p / 배수 +0.02~0.5
	constexpr float CritChancePerValue = 0.0004f;
	constexpr float CritDamagePerValue = 0.02f;

	// BlueprintFunctionLibrary static 함수에서 GameInstance/Subsystem 접근 헬퍼
	UTableManagerSubsystem* GetTableMgrForPotential()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.WorldType != EWorldType::Game && Ctx.WorldType != EWorldType::PIE) continue;
			if (UWorld* World = Ctx.World())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					if (UTableManagerSubsystem* Mgr = GI->GetSubsystem<UTableManagerSubsystem>())
					{
						return Mgr;
					}
				}
			}
		}
		return nullptr;
	}
}

EEmployeeRank UEmployeePotentialHelper::GetRankFromEnhancementLevel(int32 EnhancementLevel)
{
	return UEmployeeTypeHelper::GetRankFromEnhancementLevel(EnhancementLevel);
}

int32 UEmployeePotentialHelper::GetPotentialSlotsForRank(EEmployeeRank Rank)
{
	uint8 RankValue = static_cast<uint8>(Rank);

	if (RankValue <= 2) return 1;
	if (RankValue <= 5) return 2;
	return 3;
}

int32 UEmployeePotentialHelper::GetAdditionalSlotsForRank(EEmployeeRank Rank)
{
	uint8 RankValue = static_cast<uint8>(Rank);

	if (RankValue < 5) return 0;
	if (RankValue <= 6) return 1;
	return 2;
}

bool UEmployeePotentialHelper::ResetPotentialAbility(FPotentialAbility& InOutPotential, int32 AvailableSlots, ELootBoxRarity MaxCeiling)
{
	ELootBoxRarity NewRarity = RollPotentialRarity(InOutPotential.MaxAchievedRarity);

	// 큐브 등급 상한 클램프 — 단 래칫(MaxAchievedRarity) 아래로는 못 내림: 낮은 큐브는 위로 못 밀 뿐, 달성 등급을 하락시키지 않음
	const ELootBoxRarity EffectiveCeiling = static_cast<uint8>(MaxCeiling) > static_cast<uint8>(InOutPotential.MaxAchievedRarity)
		? MaxCeiling : InOutPotential.MaxAchievedRarity;
	if (static_cast<uint8>(NewRarity) > static_cast<uint8>(EffectiveCeiling))
	{
		NewRarity = EffectiveCeiling;
	}

	InOutPotential.CurrentRarity = NewRarity;

	if (static_cast<uint8>(NewRarity) > static_cast<uint8>(InOutPotential.MaxAchievedRarity))
	{
		InOutPotential.MaxAchievedRarity = NewRarity;
	}

	InOutPotential.Options.Empty();
	for (int32 i = 0; i < AvailableSlots; ++i)
	{
		InOutPotential.Options.Add(GenerateRandomPotentialOption(NewRarity));
	}

	return true;
}

bool UEmployeePotentialHelper::ResetAdditionalOption(FAdditionalOption& InOutAdditional, int32 AvailableSlots)
{
	ELootBoxRarity NewRarity = RollAdditionalRarity();
	InOutAdditional.CurrentRarity = NewRarity;

	InOutAdditional.Options.Empty();
	for (int32 i = 0; i < AvailableSlots; ++i)
	{
		InOutAdditional.Options.Add(GenerateRandomAdditionalOption(NewRarity));
	}

	return true;
}

ELootBoxRarity UEmployeePotentialHelper::RollPotentialRarity(ELootBoxRarity MaxAchievedRarity)
{
	const float Roll = FMath::FRand() * 100.0f;

	ELootBoxRarity NewRarity = ELootBoxRarity::Common;
	float Cumulative = 0.f;
	for (const PotentialOdds::FBaseChance& Entry : PotentialOdds::BaseTable)
	{
		Cumulative += Entry.Percent;
		if (Roll < Cumulative)
		{
			NewRarity = Entry.Rarity;
			break;
		}
	}

	if (static_cast<uint8>(NewRarity) < static_cast<uint8>(MaxAchievedRarity))
	{
		return MaxAchievedRarity;
	}

	return NewRarity;
}

TArray<FPotentialRarityOdds> UEmployeePotentialHelper::GetBaseRarityOdds()
{
	TArray<FPotentialRarityOdds> Out;
	for (const PotentialOdds::FBaseChance& Entry : PotentialOdds::BaseTable)
	{
		FPotentialRarityOdds Row;
		Row.Rarity = Entry.Rarity;
		Row.Percent = Entry.Percent;
		Out.Add(Row);
	}
	return Out;
}

TArray<FPotentialRarityOdds> UEmployeePotentialHelper::GetEffectiveRarityOdds(ELootBoxRarity MaxAchievedRarity, ELootBoxRarity CardCeiling)
{
	// ResetPotentialAbility 와 같은 순서로 접는다: 기본 롤 → 래칫 → 상한 클램프.
	// 상한은 래칫 아래로 못 내려가므로 EffectiveCeiling 도 그쪽 계산을 그대로 미러한다.
	const uint8 Achieved = static_cast<uint8>(MaxAchievedRarity);
	const uint8 EffectiveCeiling = FMath::Max(static_cast<uint8>(CardCeiling), Achieved);

	TMap<uint8, float> Folded;
	for (const PotentialOdds::FBaseChance& Entry : PotentialOdds::BaseTable)
	{
		const uint8 Ratcheted = FMath::Max(static_cast<uint8>(Entry.Rarity), Achieved);
		Folded.FindOrAdd(FMath::Min(Ratcheted, EffectiveCeiling)) += Entry.Percent;
	}

	TArray<FPotentialRarityOdds> Out;
	for (const TPair<uint8, float>& Pair : Folded)
	{
		FPotentialRarityOdds Row;
		Row.Rarity = static_cast<ELootBoxRarity>(Pair.Key);
		Row.Percent = Pair.Value;
		Out.Add(Row);
	}
	Out.Sort([](const FPotentialRarityOdds& A, const FPotentialRarityOdds& B)
	{
		return static_cast<uint8>(A.Rarity) > static_cast<uint8>(B.Rarity);
	});
	return Out;
}

ELootBoxRarity UEmployeePotentialHelper::RollAdditionalRarity()
{
	float Roll = FMath::FRand() * 100.0f;

	if (Roll < 0.3f) return ELootBoxRarity::Legendary;
	if (Roll < 1.5f) return ELootBoxRarity::Epic;
	if (Roll < 7.0f) return ELootBoxRarity::Rare;
	if (Roll < 25.0f) return ELootBoxRarity::Unusual;
	return ELootBoxRarity::Common;
}

void UEmployeePotentialHelper::GetValueRangeForRarity(ELootBoxRarity Rarity, float& OutMin, float& OutMax)
{
	switch (Rarity)
	{
	case ELootBoxRarity::Common:
		OutMin = 1.0f;
		OutMax = 3.0f;
		break;
	case ELootBoxRarity::Unusual:
		OutMin = 3.0f;
		OutMax = 6.0f;
		break;
	case ELootBoxRarity::Rare:
		OutMin = 6.0f;
		OutMax = 10.0f;
		break;
	case ELootBoxRarity::Epic:
		OutMin = 10.0f;
		OutMax = 15.0f;
		break;
	case ELootBoxRarity::Legendary:
		OutMin = 15.0f;
		OutMax = 25.0f;
		break;
	default:
		OutMin = 1.0f;
		OutMax = 3.0f;
		break;
	}
}

FPotentialOptionLine UEmployeePotentialHelper::GenerateRandomPotentialOption(ELootBoxRarity Rarity)
{
	FPotentialOptionLine Result;
	Result.Rarity = Rarity;

	// 롤 풀 5종 (spec 2026-07-08-potential-cube-wiring) — 나머지 enum 은 풀에서만 제외(재확장 대비 유지)
	TArray<EPotentialOptionType> AvailableOptions = {
		EPotentialOptionType::WorkEfficiency,
		EPotentialOptionType::IncomeBonus,
		EPotentialOptionType::ExpGain
	};

	// Epic+ 잭팟 라인
	if (static_cast<uint8>(Rarity) >= static_cast<uint8>(ELootBoxRarity::Epic))
	{
		AvailableOptions.Add(EPotentialOptionType::CriticalChance);
		AvailableOptions.Add(EPotentialOptionType::CriticalDamage);
	}

	int32 RandomIndex = FMath::RandRange(0, AvailableOptions.Num() - 1);
	Result.OptionType = AvailableOptions[RandomIndex];

	float Min, Max;
	GetValueRangeForRarity(Rarity, Min, Max);
	Result.Value = FMath::FRandRange(Min, Max);

	return Result;
}

FAdditionalOptionLine UEmployeePotentialHelper::GenerateRandomAdditionalOption(ELootBoxRarity Rarity)
{
	FAdditionalOptionLine Result;
	Result.Rarity = Rarity;

	TArray<EAdditionalOptionType> AvailableOptions = {
		EAdditionalOptionType::AllEmployeeBonus,
		EAdditionalOptionType::BuildingIncome,
		EAdditionalOptionType::CompanyMarketCap,
		EAdditionalOptionType::AllProjectBonus,
		EAdditionalOptionType::FactorySpeed,
		EAdditionalOptionType::EmployeeExpBonus,
		EAdditionalOptionType::ResourceGeneration
	};

	int32 RandomIndex = FMath::RandRange(0, AvailableOptions.Num() - 1);
	Result.OptionType = AvailableOptions[RandomIndex];

	float Min, Max;
	GetValueRangeForRarity(Rarity, Min, Max);
	Result.Value = FMath::FRandRange(Min, Max);

	return Result;
}

ELootBoxRarity UEmployeePotentialHelper::GetCubeCeiling(EItemType CubeType)
{
	switch (CubeType)
	{
	case EItemType::BusinessCardPaper: return ELootBoxRarity::Rare;
	case EItemType::BusinessCardGold:  return ELootBoxRarity::Epic;
	case EItemType::BusinessCardBlack: return ELootBoxRarity::Legendary;
	default:                           return ELootBoxRarity::Common;
	}
}

int32 UEmployeePotentialHelper::GetAdditionalResetCost()
{
	return 100;
}

FText UEmployeePotentialHelper::GetPotentialOptionDescription(const FPotentialOptionLine& Option)
{
	FText OptionName = GetPotentialOptionName(Option.OptionType);
	return FText::Format(FText::FromString(TEXT("{0} +{1}%")), OptionName, FText::AsNumber(FMath::RoundToInt(Option.Value)));
}

FText UEmployeePotentialHelper::GetAdditionalOptionDescription(const FAdditionalOptionLine& Option)
{
	FText OptionName = GetAdditionalOptionName(Option.OptionType);
	return FText::Format(FText::FromString(TEXT("{0} +{1}%")), OptionName, FText::AsNumber(FMath::RoundToInt(Option.Value)));
}

FText UEmployeePotentialHelper::GetPotentialOptionName(EPotentialOptionType OptionType)
{
	if (UTableManagerSubsystem* Mgr = GetTableMgrForPotential())
	{
		const FText Label = Mgr->GetPotentialOptionDisplayName(OptionType);
		if (!Label.IsEmpty())
		{
			return Label;
		}
	}
	return FText::FromString(TEXT("알 수 없음"));
}

FText UEmployeePotentialHelper::GetAdditionalOptionName(EAdditionalOptionType OptionType)
{
	if (UTableManagerSubsystem* Mgr = GetTableMgrForPotential())
	{
		const FText Label = Mgr->GetAdditionalOptionDisplayName(OptionType);
		if (!Label.IsEmpty())
		{
			return Label;
		}
	}
	return FText::FromString(TEXT("알 수 없음"));
}

FPotentialModifiers UEmployeePotentialHelper::AggregateModifiers(const FPotentialAbility& Potential)
{
	FPotentialModifiers Mods;
	for (const FPotentialOptionLine& Line : Potential.Options)
	{
		switch (Line.OptionType)
		{
		case EPotentialOptionType::WorkEfficiency: Mods.ScoreMult     += Line.Value / 100.0f; break;
		case EPotentialOptionType::IncomeBonus:    Mods.IncomeMult    += Line.Value / 100.0f; break;
		case EPotentialOptionType::ExpGain:        Mods.ExpMult       += Line.Value / 100.0f; break;
		case EPotentialOptionType::CriticalChance: Mods.CritChanceAdd += Line.Value * CritChancePerValue; break;
		case EPotentialOptionType::CriticalDamage: Mods.CritDamageAdd += Line.Value * CritDamagePerValue; break;
		default: break; // 풀 외 enum(재확장 대비 잔존)은 무시
		}
	}
	return Mods;
}
