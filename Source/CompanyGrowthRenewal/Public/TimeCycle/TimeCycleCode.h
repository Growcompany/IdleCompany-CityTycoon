// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimeCycleCode.generated.h"

USTRUCT(BlueprintType)
struct COMPANYGROWTHRENEWAL_API FTimeCycleCode
{
	GENERATED_BODY()

	static FTimeCycleCode FromSeconds(int32 InSeconds);
	int32 ToSeconds() const;

	bool IsValid() const;
	bool operator==(const FTimeCycleCode& Other) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleCode, meta=(ClampMin="0", UIMin="0", ClampMax="23", UIMax="23"))
	int32 Hours = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleCode, meta=(ClampMin="0", UIMin="0", ClampMax="59", UIMax="59"))
	int32 Minutes = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleCode, meta=(ClampMin="0", UIMin="0", ClampMax="59", UIMax="59"))
	int32 Seconds = 0;
};
