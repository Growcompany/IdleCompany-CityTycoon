// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Table/SoundTable.h"
#include "Data/GameAudioSettings.h"
#include "Enum/SoundCategory.h"
#include "Enum/MusicType.h"
#include "Components/AudioComponent.h"
#include "SoundManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVolumeChanged, ESoundCategory, Category, float, NewVolume);

UCLASS()
class COMPANYGROWTHRENEWAL_API USoundManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	USoundManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ===== 사운드 재생 =====

	/** UI 사운드 재생 (GameplayTag 기반) — UI 도메인 전용 진입점 */
	UFUNCTION(BlueprintCallable, Category = "Sound Manager", meta = (Categories = "UI.Sound"))
	void PlayUISound(FGameplayTag SoundTag);

	/** UI 사운드 + 콜별 볼륨/피치 스케일 (DT 배율에 곱) — 고빈도 반복음의 연속 감쇠/피치 랜덤용 */
	UFUNCTION(BlueprintCallable, Category = "Sound Manager", meta = (Categories = "UI.Sound"))
	void PlayUISoundWithParams(FGameplayTag SoundTag, float VolumeScale, float PitchScale = 1.0f);

	/** 게임 SFX 재생 (2D, FName) — Building/Gacha/Employee 등. UI 는 PlayUISound 사용 */
	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void PlaySound(FName SoundID);

	/** 게임 SFX 재생 (3D, 위치 기반) — 월드 공간 SFX. UI 는 PlayUISound 사용 */
	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void PlaySoundAtLocation(FName SoundID, FVector Location);

	// 길이가 긴 SFX 의 꼬리를 자르거나 시작 무음 구간을 건너뛰고 싶을 때 사용
	// MaxDuration 후 0.3s fade out, StartOffset 만큼 wav 안으로 점프해서 재생 시작
	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void PlaySoundWithDuration(FName SoundID, float MaxDuration, float StartOffset = 0.0f);

	// PlaySound 동일하되 DT VolumeMultiplier 에 추가 곱해질 envelope 인자 — 시간별 envelope 페이딩 등에 사용
	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void PlaySoundWithVolume(FName SoundID, float VolumeScale, float PitchScale = 1.0f);

	// 호출부가 직접 SpawnSound* 로 AudioComponent 를 들고 있어야 할 때(루프음 등) 마스터/카테고리 볼륨·음소거를 반영한 최종 볼륨
	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	float GetFinalVolume(ESoundCategory Category, float VolumeMultiplier = 1.0f) const;

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void PlayMusic(EMusicType MusicType, bool bForceRestart = false);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void StopMusic(float FadeOutDuration = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	EMusicType GetCurrentMusicType() const { return CurrentMusicType; }

	// ===== 볼륨 설정 =====

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void SetCategoryVolume(ESoundCategory Category, float Volume);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void SetMuted(bool bMute);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void SetCategoryMuted(ESoundCategory Category, bool bMute);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	bool IsCategoryMuted(ESoundCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	float GetCategoryVolume(ESoundCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	FGameAudioSettings GetAudioSettings() const { return AudioSettings; }

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void ApplyAudioSettings(const FGameAudioSettings& NewSettings);

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void SaveAudioSettings();

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	void LoadAudioSettings();

	// ===== 델리게이트 =====

	UPROPERTY(BlueprintAssignable, Category = "Sound Manager")
	FOnVolumeChanged OnVolumeChanged;

	// ===== DataTable 접근 =====

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	FUISoundTable GetUISoundData(FGameplayTag SoundTag, bool& bOutSuccess) const;

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	FGameSFXTable GetGameSFXData(FName SoundID, bool& bOutSuccess) const;

	UFUNCTION(BlueprintCallable, Category = "Sound Manager")
	FMusicTable GetMusicData(EMusicType MusicType, bool& bOutSuccess) const;

private:
	// DataTables
	UPROPERTY(EditDefaultsOnly, Category = "Data Tables")
	UDataTable* UISoundDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Data Tables")
	UDataTable* GameSFXDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Data Tables")
	UDataTable* MusicDataTable = nullptr;

	// 캐시 — UI 는 Tag 직접 키, GameSFX 는 RowName 키
	UPROPERTY()
	TMap<FGameplayTag, FUISoundTable> UISoundTable;

	UPROPERTY()
	TMap<FName, FGameSFXTable> GameSFXTable;

	UPROPERTY()
	TMap<EMusicType, FMusicTable> MusicTable;

	UPROPERTY()
	FGameAudioSettings AudioSettings;

	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicAudioComponent = nullptr;

	UPROPERTY()
	EMusicType CurrentMusicType = EMusicType::None;

	void InitializeUISoundTable();
	void InitializeGameSFXTable();
	void InitializeMusicTable();
	void InitializeAudioSettings();

	float CalculateFinalVolume(ESoundCategory Category, float VolumeMultiplier) const;
	void UpdateMusicVolume();
};
