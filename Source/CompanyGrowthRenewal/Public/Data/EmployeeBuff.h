#pragma once

#include "CoreMinimal.h"
#include "EmployeeBuff.generated.h"

/**
 * 이벤트 효과로 부여되는 일시 직원 버프 종류
 */
UENUM(BlueprintType)
enum class EBuffType : uint8
{
	None             UMETA(DisplayName = "None"),
	ScoreMultiplier  UMETA(DisplayName = "Score x Multiplier"),     // 점수 기여 N배
	CritChance       UMETA(DisplayName = "Critical Chance Bonus"),  // 크리티컬 확률 +N
	WorkSpeed        UMETA(DisplayName = "Work Interval Multiplier") // 기여 간격 단축 (작은 Value=빠름)
};

/**
 * 단일 buff 인스턴스 — 직원에게 N초간 부여되는 일시 효과
 * SaveGame 안 함 (런타임 일시 효과)
 */
USTRUCT(BlueprintType)
struct FEmployeeBuff
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Buff")
	EBuffType Type = EBuffType::None;

	// ScoreMultiplier: 1.5 = +50% / CritChance: 0.3 = +30%p / WorkSpeed: 0.7 = 30% 빠름
	UPROPERTY(BlueprintReadOnly, Category = "Buff")
	float Value = 1.0f;

	// 남은 시간 (초). 0 이하면 만료
	UPROPERTY(BlueprintReadOnly, Category = "Buff")
	float RemainingTime = 0.0f;

	FEmployeeBuff() = default;
	FEmployeeBuff(EBuffType InType, float InValue, float InDuration)
		: Type(InType), Value(InValue), RemainingTime(InDuration) {}

	bool IsActive() const { return RemainingTime > 0.0f && Type != EBuffType::None; }
};
