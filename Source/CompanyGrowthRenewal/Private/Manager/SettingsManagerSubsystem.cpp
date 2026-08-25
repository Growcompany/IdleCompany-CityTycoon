// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Scalability.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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
}

void USettingsManagerSubsystem::SetFrameRateLimit(int32 NewLimit)
{
	Settings.FrameRateLimit = FMath::Clamp(NewLimit, 30, 120);
	ApplyGraphicsSettings();
}

void USettingsManagerSubsystem::SetReduceMotion(bool bReduce)
{
	if (Settings.bReduceMotion == bReduce)
	{
		return;
	}
	Settings.bReduceMotion = bReduce;
	OnReduceMotionChanged.Broadcast(bReduce);
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
