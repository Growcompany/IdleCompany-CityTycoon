// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/SoundCategory.h"
#include "GameAudioSettings.generated.h"

// 저장용 오디오 설정 구조체
USTRUCT(BlueprintType)
struct FGameAudioSettings
{
	GENERATED_BODY()

	// 마스터 볼륨 (0.0 ~ 1.0)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	float MasterVolume = 1.0f;

	// 배경음악 볼륨 (0.0 ~ 1.0)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	float MusicVolume = 0.7f;

	// 효과음 볼륨 (0.0 ~ 1.0)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	float SFXVolume = 0.8f;

	// UI 사운드 볼륨 (0.0 ~ 1.0)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	float UIVolume = 0.9f;

	// 환경음 볼륨 (0.0 ~ 1.0)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	float AmbientVolume = 0.6f;

	// 음소거 상태
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	bool bMuted = false;

	// 카테고리별 음소거 — 볼륨 값을 보존한 채 그 소리만 끔 (설정창 행별 스피커 버튼)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	bool bMusicMuted = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	bool bSFXMuted = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Audio")
	bool bUIMuted = false;

	bool IsCategoryMuted(ESoundCategory Category) const
	{
		switch (Category)
		{
			case ESoundCategory::Music: return bMusicMuted;
			case ESoundCategory::SFX:   return bSFXMuted;
			case ESoundCategory::UI:    return bUIMuted;
			default:                    return false;
		}
	}

	void SetCategoryMuted(ESoundCategory Category, bool bMute)
	{
		switch (Category)
		{
			case ESoundCategory::Music: bMusicMuted = bMute; break;
			case ESoundCategory::SFX:   bSFXMuted = bMute; break;
			case ESoundCategory::UI:    bUIMuted = bMute; break;
			default: break;
		}
	}

	// 특정 카테고리 볼륨 가져오기
	float GetVolumeForCategory(ESoundCategory Category) const
	{
		if (bMuted) return 0.0f;
		if (IsCategoryMuted(Category)) return 0.0f;

		float CategoryVolume = 1.0f;
		switch (Category)
		{
			case ESoundCategory::Music:   CategoryVolume = MusicVolume; break;
			case ESoundCategory::SFX:     CategoryVolume = SFXVolume; break;
			case ESoundCategory::UI:      CategoryVolume = UIVolume; break;
			case ESoundCategory::Ambient: CategoryVolume = AmbientVolume; break;
			default: break;
		}

		return MasterVolume * CategoryVolume;
	}
};
