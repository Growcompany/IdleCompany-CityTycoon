#include "Manager/PanelIntroSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/PanelIntroTable.h"
#include "Data/GameSaveData.h"

void UPanelIntroSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 구독 대상은 InitializeDependency 로 강제 — 없으면 init 순서상 null 구독 스킵 (GDD_TUTORIAL §4 함정)
	Collection.InitializeDependency<UTableManagerSubsystem>();
	Collection.InitializeDependency<USaveLoadManager>();

	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &UPanelIntroSubsystem::HandleGameDataLoaded);
	}
}

void UPanelIntroSubsystem::Deinitialize()
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UPanelIntroSubsystem::HandleGameDataLoaded()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData)
	{
		return;
	}
	SeenPanelKeys = SaveData->GameData.SeenPanelIntros;
	HintCounts = SaveData->GameData.GestureHintCounts;
}

void UPanelIntroSubsystem::CollectSaveData(FGameSaveData& OutData) const
{
	OutData.SeenPanelIntros = SeenPanelKeys;
	OutData.GestureHintCounts = HintCounts;
}

bool UPanelIntroSubsystem::ShouldPlay(FName PanelKey) const
{
	if (PanelKey.IsNone() || SeenPanelKeys.Contains(PanelKey))
	{
		return false;
	}
	const UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return false;
	}
	const TArray<FPanelIntroTable>* Steps = TableMgr->GetPanelIntroSteps(PanelKey);
	return Steps && Steps->Num() > 0;
}

void UPanelIntroSubsystem::MarkSeen(FName PanelKey)
{
	if (PanelKey.IsNone() || SeenPanelKeys.Contains(PanelKey))
	{
		return;
	}
	SeenPanelKeys.Add(PanelKey);
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
	UE_LOG(LogTemp, Log, TEXT("[PanelIntro] '%s' 최초 진입 안내 완료"), *PanelKey.ToString());
}

int32 UPanelIntroSubsystem::GetHintCount(FName Key) const
{
	const int32* Found = HintCounts.Find(Key);
	return Found ? *Found : 0;
}

void UPanelIntroSubsystem::IncrementHint(FName Key)
{
	if (Key.IsNone())
	{
		return;
	}
	HintCounts.FindOrAdd(Key) += 1;
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
}

void UPanelIntroSubsystem::ResetAll()
{
	SeenPanelKeys.Empty();
	HintCounts.Empty();
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}
	UE_LOG(LogTemp, Log, TEXT("[PanelIntro] 전체 초기화 — 모든 패널 안내와 제스처 힌트가 다시 재생됩니다"));
}
