// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#include "TimeCycle/TimeCycleBlueprintLibrary.h"
#include "TimeCycle/TimeCycleCode.h"
#include "TimeCycle/TimeCycleManager.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ATimeCycleManager* UTimeCycleBlueprintLibrary::GetTimerCycleManager(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
		return nullptr;

	for (TActorIterator<ATimeCycleManager> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

bool UTimeCycleBlueprintLibrary::AreTimeCycleCodeEqual(const FTimeCycleCode& TimeCycleCode1, const FTimeCycleCode& TimeCycleCode2)
{
	return TimeCycleCode1 == TimeCycleCode2;
}
