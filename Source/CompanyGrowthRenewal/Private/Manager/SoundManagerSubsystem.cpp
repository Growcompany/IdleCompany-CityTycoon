// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/SoundManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

USoundManagerSubsystem::USoundManagerSubsystem()
{
	// UI Sound DataTable
	static ConstructorHelpers::FObjectFinder<UDataTable> UIT(
		TEXT("DataTable'/Game/CompanyGrowth/Table/Audio/DT_UISounds.DT_UISounds'")
	);
	if (UIT.Succeeded())
	{
		UISoundDataTable = UIT.Object;
	}

	// Game SFX DataTable
	static ConstructorHelpers::FObjectFinder<UDataTable> GFX(
		TEXT("DataTable'/Game/CompanyGrowth/Table/Audio/DT_GameSFX.DT_GameSFX'")
	);
	if (GFX.Succeeded())
	{
		GameSFXDataTable = GFX.Object;
	}

	// Music DataTable
	static ConstructorHelpers::FObjectFinder<UDataTable> MT(
		TEXT("DataTable'/Game/CompanyGrowth/Table/Audio/DT_Music.DT_Music'")
	);
	if (MT.Succeeded())
	{
		MusicDataTable = MT.Object;
	}
}

void USoundManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InitializeUISoundTable();
	InitializeGameSFXTable();
	InitializeMusicTable();
	InitializeAudioSettings();

	UE_LOG(LogTemp, Log, TEXT("SoundManagerSubsystem initialized"));
}

void USoundManagerSubsystem::Deinitialize()
{
	if (MusicAudioComponent && MusicAudioComponent->IsPlaying())
	{
		MusicAudioComponent->Stop();
	}

	Super::Deinitialize();
}

void USoundManagerSubsystem::InitializeUISoundTable()
{
	if (!UISoundDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("UISoundDataTable is not set!"));
		return;
	}

	TArray<FName> RowNames = UISoundDataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FUISoundTable* Row = UISoundDataTable->FindRow<FUISoundTable>(RowName, TEXT("InitializeUISoundTable"));
		if (Row && Row->SoundTag.IsValid())
		{
			UISoundTable.Add(Row->SoundTag, *Row);
		}
		else if (Row)
		{
			// Tag 없는 UI row 는 lookup 불가 — 데이터 누락 즉시 드러내기
			UE_LOG(LogTemp, Warning, TEXT("UI sound row '%s' has no SoundTag, skipped"), *RowName.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Loaded %d UI sounds"), UISoundTable.Num());
}

void USoundManagerSubsystem::InitializeGameSFXTable()
{
	if (!GameSFXDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameSFXDataTable is not set!"));
		return;
	}

	TArray<FName> RowNames = GameSFXDataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FGameSFXTable* Row = GameSFXDataTable->FindRow<FGameSFXTable>(RowName, TEXT("InitializeGameSFXTable"));
		if (Row)
		{
			GameSFXTable.Add(RowName, *Row);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Loaded %d game SFX"), GameSFXTable.Num());
}

void USoundManagerSubsystem::InitializeMusicTable()
{
	if (!MusicDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicDataTable is not set!"));
		return;
	}

	TArray<FName> RowNames = MusicDataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FMusicTable* Row = MusicDataTable->FindRow<FMusicTable>(RowName, TEXT("InitializeMusicTable"));
		if (Row)
		{
			// 같은 MusicType에 여러 음악이 있으면 Priority 가 높은 것만 저장
			if (!MusicTable.Contains(Row->MusicType) ||
				MusicTable[Row->MusicType].Priority < Row->Priority)
			{
				MusicTable.Add(Row->MusicType, *Row);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Loaded %d music tracks"), MusicTable.Num());
}

void USoundManagerSubsystem::InitializeAudioSettings()
{
	LoadAudioSettings();
}

void USoundManagerSubsystem::PlayUISound(FGameplayTag SoundTag)
{
	if (!SoundTag.IsValid())
	{
		return;
	}

	bool bSuccess = false;
	FUISoundTable SoundData = GetUISoundData(SoundTag, bSuccess);

	if (!bSuccess || !SoundData.Sound.ToSoftObjectPath().IsValid())
	{
		// 미등록 태그 = 의도적 무음(클립 미배정). 고빈도 경로에서 매 호출 경고를 뱉으므로 Verbose
		UE_LOG(LogTemp, Verbose, TEXT("PlayUISound: not found for tag %s"), *SoundTag.ToString());
		return;
	}

	USoundBase* LoadedSound = SoundData.Sound.LoadSynchronous();
	if (!LoadedSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayUISound: load failed for tag %s"), *SoundTag.ToString());
		return;
	}

	float FinalVolume = CalculateFinalVolume(ESoundCategory::UI, SoundData.VolumeMultiplier);

	UGameplayStatics::PlaySound2D(
		GetWorld(),
		LoadedSound,
		FinalVolume,
		SoundData.PitchMultiplier
	);
}

void USoundManagerSubsystem::PlayUISoundWithParams(FGameplayTag SoundTag, float VolumeScale, float PitchScale)
{
	if (!SoundTag.IsValid())
	{
		return;
	}

	bool bSuccess = false;
	FUISoundTable SoundData = GetUISoundData(SoundTag, bSuccess);
	if (!bSuccess || !SoundData.Sound.ToSoftObjectPath().IsValid())
	{
		UE_LOG(LogTemp, Verbose, TEXT("PlayUISoundWithParams: not found for tag %s"), *SoundTag.ToString());
		return;
	}

	USoundBase* LoadedSound = SoundData.Sound.LoadSynchronous();
	if (!LoadedSound)
	{
		return;
	}

	const float FinalVolume = CalculateFinalVolume(ESoundCategory::UI, SoundData.VolumeMultiplier) * VolumeScale;
	UGameplayStatics::PlaySound2D(GetWorld(), LoadedSound, FinalVolume, SoundData.PitchMultiplier * PitchScale);
}

void USoundManagerSubsystem::PlaySound(FName SoundID)
{
	bool bSuccess = false;
	FGameSFXTable SoundData = GetGameSFXData(SoundID, bSuccess);

	if (!bSuccess || !SoundData.Sound.ToSoftObjectPath().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySound: not found or invalid: %s"), *SoundID.ToString());
		return;
	}

	USoundBase* LoadedSound = SoundData.Sound.LoadSynchronous();
	if (!LoadedSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySound: load failed: %s"), *SoundID.ToString());
		return;
	}

	float FinalVolume = CalculateFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier);

	UGameplayStatics::PlaySound2D(
		GetWorld(),
		LoadedSound,
		FinalVolume,
		SoundData.PitchMultiplier
	);
}

void USoundManagerSubsystem::PlaySoundWithVolume(FName SoundID, float VolumeScale, float PitchScale)
{
	bool bSuccess = false;
	FGameSFXTable SoundData = GetGameSFXData(SoundID, bSuccess);
	if (!bSuccess || !SoundData.Sound.ToSoftObjectPath().IsValid()) return;
	USoundBase* LoadedSound = SoundData.Sound.LoadSynchronous();
	if (!LoadedSound) return;
	float FinalVolume = CalculateFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier * VolumeScale);
	UGameplayStatics::PlaySound2D(GetWorld(), LoadedSound, FinalVolume, SoundData.PitchMultiplier * PitchScale);
}

float USoundManagerSubsystem::GetFinalVolume(ESoundCategory Category, float VolumeMultiplier) const
{
	return CalculateFinalVolume(Category, VolumeMultiplier);
}

void USoundManagerSubsystem::PlaySoundWithDuration(FName SoundID, float MaxDuration, float StartOffset)
{
	bool bSuccess = false;
	FGameSFXTable SoundData = GetGameSFXData(SoundID, bSuccess);

	if (!bSuccess || !SoundData.Sound.ToSoftObjectPath().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySoundWithDuration: not found or invalid: %s"), *SoundID.ToString());
		return;
	}

	USoundBase* LoadedSound = SoundData.Sound.LoadSynchronous();
	if (!LoadedSound)
	{
		return;
	}

	float FinalVolume = CalculateFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier);

	// StartTime 인자로 wav 안 StartOffset 지점부터 재생 — ramp-up 무음 구간 skip 용
	UAudioComponent* AudioComp = UGameplayStatics::SpawnSound2D(
		GetWorld(),
		LoadedSound,
		FinalVolume,
		SoundData.PitchMultiplier,
		StartOffset
	);

	if (AudioComp && MaxDuration > 0.0f)
	{
		FTimerHandle FadeHandle;
		TWeakObjectPtr<UAudioComponent> WeakComp(AudioComp);
		GetWorld()->GetTimerManager().SetTimer(FadeHandle, FTimerDelegate::CreateLambda(
			[WeakComp]()
			{
				if (WeakComp.IsValid())
				{
					WeakComp->FadeOut(0.3f, 0.0f);
				}
			}), MaxDuration, false);
	}
}

void USoundManagerSubsystem::PlaySoundAtLocation(FName SoundID, FVector Location)
{
	bool bSuccess = false;
	FGameSFXTable SoundData = GetGameSFXData(SoundID, bSuccess);

	if (!bSuccess || !SoundData.Sound.ToSoftObjectPath().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySoundAtLocation: not found or invalid: %s"), *SoundID.ToString());
		return;
	}

	USoundBase* LoadedSound = SoundData.Sound.LoadSynchronous();
	if (!LoadedSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaySoundAtLocation: load failed: %s"), *SoundID.ToString());
		return;
	}

	float FinalVolume = CalculateFinalVolume(ESoundCategory::SFX, SoundData.VolumeMultiplier);

	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		LoadedSound,
		Location,
		FRotator::ZeroRotator,
		FinalVolume,
		SoundData.PitchMultiplier
	);
}

void USoundManagerSubsystem::PlayMusic(EMusicType MusicType, bool bForceRestart)
{
	// 같은 타입이고 + 컴포넌트가 살아 + 실제 재생 중일 때만 skip.
	// 레벨 전환 후엔 컴포넌트가 GC 로 사라져 enum 만으로 판단하면 false-skip 발생
	if (CurrentMusicType == MusicType && !bForceRestart
		&& IsValid(MusicAudioComponent) && MusicAudioComponent->IsPlaying())
	{
		return;
	}

	// 이전 음악 정리 — FadeOut 후 ref 끊고 GC 에 맡김 (audio engine 이 fade 끝까지 hold)
	if (IsValid(MusicAudioComponent) && MusicAudioComponent->IsPlaying())
	{
		FMusicTable* CurrentMusicData = MusicTable.Find(CurrentMusicType);
		float FadeOutDuration = CurrentMusicData ? CurrentMusicData->FadeOutDuration : 1.0f;
		MusicAudioComponent->FadeOut(FadeOutDuration, 0.0f);
	}
	MusicAudioComponent = nullptr;

	bool bSuccess = false;
	FMusicTable MusicData = GetMusicData(MusicType, bSuccess);

	if (!bSuccess || !MusicData.Music.ToSoftObjectPath().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Music not found: %d"), (int32)MusicType);
		// state reset — 다음 PlayMusic 호출 시 가드에 막히지 않게
		CurrentMusicType = EMusicType::None;
		return;
	}

	USoundBase* LoadedMusic = MusicData.Music.LoadSynchronous();
	if (!LoadedMusic)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load music for type: %d"), (int32)MusicType);
		CurrentMusicType = EMusicType::None;
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float FinalVolume = CalculateFinalVolume(ESoundCategory::Music, 1.0f);

	// CreateSound2D: AudioComponent 만 만들고 자동 활성화 안 함 → World audio device 에 정상 attach.
	// 이후 FadeIn 이 0 → FinalVolume 페이드와 함께 재생 시작
	MusicAudioComponent = UGameplayStatics::CreateSound2D(
		World, LoadedMusic, FinalVolume, 1.0f, 0.0f, nullptr, false);

	if (!MusicAudioComponent)
	{
		return;
	}

	MusicAudioComponent->FadeIn(MusicData.FadeInDuration, FinalVolume);

	CurrentMusicType = MusicType;
}

void USoundManagerSubsystem::StopMusic(float FadeOutDuration)
{
	if (MusicAudioComponent && MusicAudioComponent->IsPlaying())
	{
		MusicAudioComponent->FadeOut(FadeOutDuration, 0.0f);
		CurrentMusicType = EMusicType::None;
		UE_LOG(LogTemp, Log, TEXT("Stopped music"));
	}
}

void USoundManagerSubsystem::SetMasterVolume(float Volume)
{
	AudioSettings.MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	UpdateMusicVolume();
	OnVolumeChanged.Broadcast(ESoundCategory::Master, AudioSettings.MasterVolume);
	UE_LOG(LogTemp, Log, TEXT("Master volume set to: %f"), AudioSettings.MasterVolume);
}

void USoundManagerSubsystem::SetCategoryVolume(ESoundCategory Category, float Volume)
{
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);

	switch (Category)
	{
	case ESoundCategory::Music:
		AudioSettings.MusicVolume = Volume;
		UpdateMusicVolume();
		break;
	case ESoundCategory::SFX:
		AudioSettings.SFXVolume = Volume;
		break;
	case ESoundCategory::UI:
		AudioSettings.UIVolume = Volume;
		break;
	case ESoundCategory::Ambient:
		AudioSettings.AmbientVolume = Volume;
		break;
	default:
		break;
	}

	OnVolumeChanged.Broadcast(Category, Volume);
	UE_LOG(LogTemp, Log, TEXT("Category %s volume set to: %f"), *EnumToString(Category), Volume);
}

void USoundManagerSubsystem::SetMuted(bool bMute)
{
	AudioSettings.bMuted = bMute;
	UpdateMusicVolume();
	OnVolumeChanged.Broadcast(ESoundCategory::Master, bMute ? 0.0f : AudioSettings.MasterVolume);
	UE_LOG(LogTemp, Log, TEXT("Muted: %s"), bMute ? TEXT("true") : TEXT("false"));
}

float USoundManagerSubsystem::GetCategoryVolume(ESoundCategory Category) const
{
	return AudioSettings.GetVolumeForCategory(Category);
}

void USoundManagerSubsystem::SetCategoryMuted(ESoundCategory Category, bool bMute)
{
	AudioSettings.SetCategoryMuted(Category, bMute);
	UpdateMusicVolume();
	OnVolumeChanged.Broadcast(Category, AudioSettings.GetVolumeForCategory(Category));
}

bool USoundManagerSubsystem::IsCategoryMuted(ESoundCategory Category) const
{
	return AudioSettings.IsCategoryMuted(Category);
}

void USoundManagerSubsystem::ApplyAudioSettings(const FGameAudioSettings& NewSettings)
{
	AudioSettings = NewSettings;
	UpdateMusicVolume();
	UE_LOG(LogTemp, Log, TEXT("Audio settings applied"));
}

void USoundManagerSubsystem::SaveAudioSettings()
{
	if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		USaveGame_GameData* SaveData = SaveLoadManager->GetCurrentSaveData();
		if (SaveData)
		{
			SaveData->GameData.AudioSettings = AudioSettings;
			SaveLoadManager->SaveGameData();
			UE_LOG(LogTemp, Log, TEXT("Audio settings saved"));
		}
	}
}

void USoundManagerSubsystem::LoadAudioSettings()
{
	if (USaveLoadManager* SaveLoadManager = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		USaveGame_GameData* SaveData = SaveLoadManager->GetCurrentSaveData();
		if (SaveData)
		{
			AudioSettings = SaveData->GameData.AudioSettings;
			UpdateMusicVolume();
			UE_LOG(LogTemp, Log, TEXT("Audio settings loaded"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("No save data found, using default audio settings"));
		}
	}
}

FUISoundTable USoundManagerSubsystem::GetUISoundData(FGameplayTag SoundTag, bool& bOutSuccess) const
{
	if (const FUISoundTable* Found = UISoundTable.Find(SoundTag))
	{
		bOutSuccess = true;
		return *Found;
	}

	bOutSuccess = false;
	return FUISoundTable();
}

FGameSFXTable USoundManagerSubsystem::GetGameSFXData(FName SoundID, bool& bOutSuccess) const
{
	if (const FGameSFXTable* Found = GameSFXTable.Find(SoundID))
	{
		bOutSuccess = true;
		return *Found;
	}

	bOutSuccess = false;
	return FGameSFXTable();
}

FMusicTable USoundManagerSubsystem::GetMusicData(EMusicType MusicType, bool& bOutSuccess) const
{
	if (const FMusicTable* Found = MusicTable.Find(MusicType))
	{
		bOutSuccess = true;
		return *Found;
	}

	bOutSuccess = false;
	return FMusicTable();
}

float USoundManagerSubsystem::CalculateFinalVolume(ESoundCategory Category, float VolumeMultiplier) const
{
	return AudioSettings.GetVolumeForCategory(Category) * VolumeMultiplier;
}

void USoundManagerSubsystem::UpdateMusicVolume()
{
	if (MusicAudioComponent)
	{
		float FinalVolume = CalculateFinalVolume(ESoundCategory::Music, 1.0f);
		MusicAudioComponent->SetVolumeMultiplier(FinalVolume);
	}
}
