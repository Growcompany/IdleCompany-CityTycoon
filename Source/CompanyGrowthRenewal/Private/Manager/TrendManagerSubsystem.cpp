#include "Manager/TrendManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"

namespace
{
	// 현재 소재가 풀에 없으면 뺄 것도 없다 — 상한을 늘 Num()-2 로 두면 마지막 칸이 영영 안 뽑힌다
	FName PickNextTrend(const TArray<FName>& Pool, FName Current)
	{
		const int32 RestNum = Pool.Num() - (Pool.Contains(Current) ? 1 : 0);
		return UTrendManagerSubsystem::PickTrendFromPool(Pool, Current, FMath::RandRange(0, FMath::Max(0, RestNum - 1)));
	}
}

void UTrendManagerSubsystem::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			States = SaveData->GameData.TrendStates;
			bLoaded = true;
		}
	}
}

FTrendState& UTrendManagerSubsystem::EnsureTrend(ECompanyType Industry)
{
	FTrendState& State = States.FindOrAdd(Industry);
	if (State.TrendMaterial.IsNone())
	{
		RotateTrend(Industry, State);
	}
	return State;
}

void UTrendManagerSubsystem::RotateTrend(ECompanyType Industry, FTrendState& State)
{
	State.LaunchesLeft = LaunchesPerTrend;

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}
	// 같은 소재 연속 방지 + 소재 미태깅 산업(None) 판정은 PickNextTrend 한 곳에만 둔다
	State.TrendMaterial = PickNextTrend(TableMgr->GetMaterialsForIndustry(Industry), State.TrendMaterial);
}

void UTrendManagerSubsystem::SyncToSave()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			SaveData->GameData.TrendStates = States;
		}
	}
}

FName UTrendManagerSubsystem::GetTrendMaterial(ECompanyType Industry)
{
	EnsureLoaded();
	return EnsureTrend(Industry).TrendMaterial;
}

int32 UTrendManagerSubsystem::GetLaunchesLeft(ECompanyType Industry)
{
	EnsureLoaded();
	return EnsureTrend(Industry).LaunchesLeft;
}

bool UTrendManagerSubsystem::IsTrendMaterial(ECompanyType Industry, FName Material)
{
	EnsureLoaded();
	const FTrendState& State = EnsureTrend(Industry);
	return !State.TrendMaterial.IsNone() && State.TrendMaterial == Material;
}

FName UTrendManagerSubsystem::PickTrendFromPool(const TArray<FName>& Pool, FName Exclude, int32 RandomIndex)
{
	TArray<FName> Rest = Pool;
	Rest.Remove(Exclude);
	if (Rest.Num() == 0)
	{
		return NAME_None;
	}
	return Rest[FMath::Clamp(RandomIndex, 0, Rest.Num() - 1)];
}

bool UTrendManagerSubsystem::RerollTrend(ECompanyType Industry)
{
	EnsureLoaded();
	FTrendState& State = EnsureTrend(Industry);

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return false;
	}

	const FName Picked = PickNextTrend(TableMgr->GetMaterialsForIndustry(Industry), State.TrendMaterial);
	if (Picked.IsNone())
	{
		return false;  // 현재 소재를 빼면 남는 게 없다 — 상태를 건드리지 않는다
	}

	State.TrendMaterial = Picked;
	State.LaunchesLeft = LaunchesPerTrend;
	SyncToSave();
	OnTrendChanged.Broadcast(Industry, Picked);
	UE_LOG(LogTemp, Log, TEXT("[Trend] %s 트렌드 갱신 -> %s"),
		*UEnum::GetValueAsString(Industry), *Picked.ToString());
	return true;
}

void UTrendManagerSubsystem::NotifyProjectLaunched(ECompanyType Industry)
{
	EnsureLoaded();
	FTrendState& State = EnsureTrend(Industry);
	State.LaunchesLeft--;
	if (State.LaunchesLeft <= 0)
	{
		RotateTrend(Industry, State);
		OnTrendChanged.Broadcast(Industry, State.TrendMaterial);
		UE_LOG(LogTemp, Log, TEXT("[Trend] %s 트렌드 교체 -> %s"),
			*UEnum::GetValueAsString(Industry), *State.TrendMaterial.ToString());
	}
	SyncToSave();
}
