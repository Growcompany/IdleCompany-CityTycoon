// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimeCycleBlueprintLibrary.generated.h"

class ATimeCycleManager;
struct FTimeCycleCode;

/*
 * TimeCycle 관련 블루프린트 유틸리티 함수
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTimeCycleBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 현재 월드의 TimeCycleManager 반환
	UFUNCTION(BlueprintPure, Category=TimeCycle, meta=(WorldContext="WorldContextObject"))
	static ATimeCycleManager* GetTimerCycleManager(const UObject* WorldContextObject);

	// 두 TimeCycleCode 비교
	UFUNCTION(BlueprintPure, Category=TimeCycle, meta=(DisplayName="Equal (FTimeCycleCode)", CompactNodeTitle="==", Keywords="== equal"))
	static bool AreTimeCycleCodeEqual(const FTimeCycleCode& TimeCycleCode1, const FTimeCycleCode& TimeCycleCode2);
};
