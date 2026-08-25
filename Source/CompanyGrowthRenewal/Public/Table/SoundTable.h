// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Enum/MusicType.h"
#include "SoundTable.generated.h"

class USoundBase;

/**
 * UI 사운드 DataTable Row
 * Lookup: SoundTag (FGameplayTag, UI.Sound.* 계층)
 * Category 는 항상 ESoundCategory::UI, 2D 재생 고정
 */
USTRUCT(BlueprintType)
struct FUISoundTable : public FTableRowBase
{
	GENERATED_BODY()

	// UI.Sound.* 만 노출. 기획자가 코드 수정 없이 새 태그 추가 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (Categories = "UI.Sound"))
	FGameplayTag SoundTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;
};

/**
 * 게임 SFX DataTable Row
 * Lookup: RowName (FName) — Building/Gacha/Employee 등 비-UI 사운드
 * Category 는 항상 ESoundCategory::SFX
 */
USTRUCT(BlueprintType)
struct FGameSFXTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;

	// PlaySoundAtLocation 으로 재생할 때만 의미. PlaySound(2D) 경로에서는 무시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	bool bIs3DSound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (EditCondition = "bIs3DSound"))
	float AttenuationDistance = 1000.0f;
};

/**
 * 배경음악 DataTable Row
 * Row Name 예시: "BGM_Main", "BGM_WorldMap", "BGM_Office_Game"
 */
USTRUCT(BlueprintType)
struct FMusicTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EMusicType MusicType = EMusicType::Main;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> Music;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float FadeInDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float FadeOutDuration = 1.0f;

	// 같은 MusicType에 여러 음악 등록 시 우선순위 (높을수록 우선)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 Priority = 0;
};
