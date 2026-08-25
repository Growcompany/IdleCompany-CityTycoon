// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FactorySaveData.generated.h"

// Forward declaration (순환 참조 방지)
struct FFactoryUpgradeConfig;

/**
 * Factory 업그레이드 타입
 */
UENUM(BlueprintType)
enum class EFactoryUpgradeType : uint8
{
	// 기본 카테고리
	HoldProductionSpeed UMETA(DisplayName = "Hold Production Speed"),    // 홀드 생산 속도
	HoldProductionAmount UMETA(DisplayName = "Hold Production Amount"),  // 홀드 생산량

	// 자동 카테고리 (DataTable의 HQ 요구 레벨 달성 시 언락)
	AutoCollection UMETA(DisplayName = "Auto Collection"),                // 자동 수집
	AutoCollectionCapacity UMETA(DisplayName = "Auto Collection Capacity"), // 자동 수집 최대용량

	// 상태 표시 전용 (업그레이드 불가 — 자동 수집된 벽돌 재고 표시 + 수집 버튼)
	BrickStock UMETA(DisplayName = "Brick Stock")
};

/**
 * Factory 업그레이드 경로별 데이터
 */
USTRUCT(BlueprintType)
struct FFactoryUpgradeData
{
	GENERATED_BODY()

	// 현재 업그레이드 레벨
	UPROPERTY(SaveGame, BlueprintReadWrite)
	int32 Level = 0;

	// 현재 값 (계산된 결과)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	float CurrentValue = 0.0f;

	FFactoryUpgradeData()
		: Level(0), CurrentValue(0.0f)
	{
	}

	FFactoryUpgradeData(int32 InLevel, float InValue)
		: Level(InLevel), CurrentValue(InValue)
	{
	}
};

/**
 * Factory 전체 데이터 (저장용)
 */
USTRUCT(BlueprintType)
struct FFactorySaveData
{
	GENERATED_BODY()

	// Factory ID (여러 Factory가 있을 경우 구분용)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FName FactoryID;

	// 업그레이드 데이터 맵 (업그레이드 타입 -> 데이터)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TMap<EFactoryUpgradeType, FFactoryUpgradeData> UpgradeData;

	// 자동 수집으로 모은 현재 자원량
	UPROPERTY(SaveGame, BlueprintReadWrite)
	int64 AutoCollectedAmount = 0;

	// 자동 수집 언락 여부 (DataTable의 HQ 요구 레벨 달성 시 true)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool bAutoCollectionUnlocked = false;

	FFactorySaveData(FName InFactoryID = NAME_None)
		: FactoryID(InFactoryID), AutoCollectedAmount(0), bAutoCollectionUnlocked(false)
	{
		InitializeUpgradeData();
	}

private:
	void InitializeUpgradeData();
};
