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
	LaborerCount UMETA(DisplayName = "Laborer Count"),                    // 인부 수 (구 AutoCollection 슬롯)
	AutoCollectionCapacity UMETA(DisplayName = "Auto Collection Capacity"), // 자리를 비운 동안 받을 수 있는 벽돌 상한
	FactoryExpansion UMETA(DisplayName = "Factory Expansion"),           // 공장 전체 증축
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

	UPROPERTY(SaveGame, BlueprintReadWrite)
	FName OwningPlotId = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadWrite)
	FTransform PlacementTransform = FTransform::Identity;

	// 업그레이드 데이터 맵 (업그레이드 타입 -> 데이터)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TMap<EFactoryUpgradeType, FFactoryUpgradeData> UpgradeData;

	// 자동 수집 언락 여부 (DataTable의 HQ 요구 레벨 달성 시 true)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool bAutoCollectionUnlocked = false;

	// 도시 밖에서 진행 중인 온라인 생산의 1개 미만 잔량. 정수 벽돌은 즉시 자원 은행에 지급한다.
	UPROPERTY(SaveGame)
	double AwayProductionFraction = 0.0;

	FFactorySaveData(FName InFactoryID = NAME_None)
		: FactoryID(InFactoryID), bAutoCollectionUnlocked(false)
	{
		InitializeUpgradeData();
	}

private:
	void InitializeUpgradeData();
};

// 도시 밖 온라인 생산의 세션 시계. 레벨 로딩·앱 백그라운드 구간은 Pause/Resume으로 제외한다.
struct FFactoryAwayProductionClock
{
	double RatePerSecond = 0.0;
	double Fraction = 0.0;
	double LastActiveSeconds = 0.0;
	bool bRunning = false;

	bool Start(double InRate, double InFraction, double Now)
	{
		if (!FMath::IsFinite(InRate) || InRate < 0.0 || !FMath::IsFinite(Now) || Now < 0.0)
		{
			return false;
		}
		RatePerSecond = InRate;
		Fraction = FMath::IsFinite(InFraction) && InFraction >= 0.0 && InFraction < 1.0
			? InFraction : 0.0;
		LastActiveSeconds = Now;
		bRunning = true;
		return true;
	}

	bool Accrue(double Now, int64 MaxPayable, int64& OutBricks)
	{
		OutBricks = 0;
		if (!bRunning || !FMath::IsFinite(Now) || Now < 0.0 || MaxPayable < 0)
		{
			return false;
		}
		if (Now < LastActiveSeconds)
		{
			LastActiveSeconds = Now;
			return true;
		}
		const double Total = Fraction + RatePerSecond * (Now - LastActiveSeconds);
		if (!FMath::IsFinite(Total) || Total < 0.0 || Total >= static_cast<double>(MAX_int64))
		{
			return false;
		}
		const double Whole = FMath::FloorToDouble(Total);
		if (Whole > static_cast<double>(MaxPayable))
		{
			return false;
		}
		const int64 Bricks = static_cast<int64>(Whole);
		if (Bricks > MaxPayable) return false;
		OutBricks = Bricks;
		Fraction = Total - Whole;
		LastActiveSeconds = Now;
		return true;
	}

	void Pause() { bRunning = false; }
	bool Resume(double Now)
	{
		if (!FMath::IsFinite(Now) || Now < 0.0) return false;
		LastActiveSeconds = Now;
		bRunning = true;
		return true;
	}
};
