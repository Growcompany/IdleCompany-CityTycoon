// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/GameSettings.h"
#include "SettingsManagerSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnReduceMotionChanged, bool);

/**
 * 게임 설정 매니저 (그래픽 품질/FPS/연출 감소)
 * 저장은 오디오 설정과 동일하게 GameSlot(FGameSaveData.GameSettings) 경유 — 저장 경로 단일화.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USettingsManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetQualityPreset(EGraphicsQualityPreset Preset);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetFrameRateLimit(int32 NewLimit);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetReduceMotion(bool bReduce);

	UFUNCTION(BlueprintPure, Category = "Settings")
	EGraphicsQualityPreset GetQualityPreset() const { return Settings.QualityPreset; }

	UFUNCTION(BlueprintPure, Category = "Settings")
	int32 GetFrameRateLimit() const { return Settings.FrameRateLimit; }

	UFUNCTION(BlueprintPure, Category = "Settings")
	bool IsReduceMotionEnabled() const { return Settings.bReduceMotion; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SaveSettings();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void LoadSettings();

	// 셰이크/juice 게이트용 축약 — 월드 컨텍스트만으로 조회
	static bool IsReduceMotion(const UObject* WorldContextObject);

	FOnReduceMotionChanged OnReduceMotionChanged;

private:
	void ApplyGraphicsSettings();

	UPROPERTY()
	FGameSettings Settings;
};
