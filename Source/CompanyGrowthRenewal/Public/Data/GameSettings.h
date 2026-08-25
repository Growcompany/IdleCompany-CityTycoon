// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameSettings.generated.h"

UENUM(BlueprintType)
enum class EGraphicsQualityPreset : uint8
{
	Low    UMETA(DisplayName = "낮음"),
	Medium UMETA(DisplayName = "중간"),
	High   UMETA(DisplayName = "높음")
};

// 저장용 게임 설정 (그래픽/연출) — 오디오는 FGameAudioSettings가 별도 소유
USTRUCT(BlueprintType)
struct FGameSettings
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Settings")
	EGraphicsQualityPreset QualityPreset = EGraphicsQualityPreset::Medium;

	// 30 | 60 | 120 — 120은 기기 주사율에 따라 vsync가 자동 캡 (60Hz 패널이면 사실상 60)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Settings")
	int32 FrameRateLimit = 120;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Settings")
	bool bReduceMotion = false;
};
