// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Scalability.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Utils/FWidgetAnimationUtils.h"

void USettingsManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(USaveLoadManager::StaticClass());
	LoadSettings();
}

void USettingsManagerSubsystem::SetQualityPreset(EGraphicsQualityPreset Preset)
{
	Settings.QualityPreset = Preset;
	ApplyGraphicsSettings();
	PushToSaveCache();
}

void USettingsManagerSubsystem::SetFrameRateLimit(int32 NewLimit)
{
	Settings.FrameRateLimit = FMath::Clamp(NewLimit, 30, 120);
	ApplyGraphicsSettings();
	PushToSaveCache();
}

void USettingsManagerSubsystem::SetReduceMotion(bool bReduce)
{
	if (Settings.bReduceMotion == bReduce)
	{
		return;
	}
	Settings.bReduceMotion = bReduce;
	FMotionGate::SetReduced(bReduce);
	OnReduceMotionChanged.Broadcast(bReduce);
	PushToSaveCache();
}

void USettingsManagerSubsystem::SetHandOpenBox(bool bOn)
{
	if (Settings.bHandOpenBox == bOn)
	{
		return;
	}
	Settings.bHandOpenBox = bOn;
	PushToSaveCache();
}

void USettingsManagerSubsystem::PushToSaveCache()
{
	if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveLoadManager->GetCurrentSaveData())
		{
			SaveData->GameData.GameSettings = Settings;
		}
	}
}

void USettingsManagerSubsystem::CollectSaveData(FGameSaveData& OutData) const
{
	// 캐시에 밀어넣는 것만으로는 첫 실행(슬롯 없음 → GetCurrentSaveData()==nullptr)에서 설정이 통째로 유실된다.
	// 스냅샷이 항상 여기서 받아가므로 캐시 존재 여부와 무관해진다.
	OutData.GameSettings = Settings;
}

void USettingsManagerSubsystem::SaveSettings()
{
	if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveLoadManager->GetCurrentSaveData())
		{
			SaveData->GameData.GameSettings = Settings;
			SaveLoadManager->SaveGameData();
		}
	}
}

void USettingsManagerSubsystem::LoadSettings()
{
	if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveLoadManager->GetCurrentSaveData())
		{
			Settings = SaveData->GameData.GameSettings;
			// 세이브 경계에서도 불변식 방어 — setter와 동일 클램프
			Settings.FrameRateLimit = FMath::Clamp(Settings.FrameRateLimit, 30, 120);
		}
	}
	FMotionGate::SetReduced(Settings.bReduceMotion);
	ApplyGraphicsSettings();
}

bool USettingsManagerSubsystem::IsReduceMotion(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return false;
	}
	const UWorld* CurrentWorld = WorldContextObject->GetWorld();
	if (!CurrentWorld)
	{
		return false;
	}
	const UGameInstance* GI = CurrentWorld->GetGameInstance();
	if (!GI)
	{
		return false;
	}
	const USettingsManagerSubsystem* Mgr = GI->GetSubsystem<USettingsManagerSubsystem>();
	return Mgr && Mgr->IsReduceMotionEnabled();
}

void USettingsManagerSubsystem::ApplyGraphicsSettings()
{
	// Low=0/Medium=1/High=2 → 엔진 Scalability 단일 레벨 (sg.* 일괄)
	Scalability::FQualityLevels Levels;
	Levels.SetFromSingleQualityLevel(static_cast<int32>(Settings.QualityPreset));
	Scalability::SetQualityLevels(Levels);

	// 콘솔 우선순위로 세팅 — 디바이스 프로파일의 t.MaxFPS를 확실히 이김
	if (GEngine)
	{
		GEngine->Exec(nullptr, *FString::Printf(TEXT("t.MaxFPS %d"), Settings.FrameRateLimit));
	}

	UE_LOG(LogTemp, Log, TEXT("[Settings] Quality=%d MaxFPS=%d ReduceMotion=%d"),
		static_cast<int32>(Settings.QualityPreset), Settings.FrameRateLimit, Settings.bReduceMotion ? 1 : 0);
}
