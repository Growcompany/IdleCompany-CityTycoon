#pragma once

#include "CoreMinimal.h"
#include "Data/OfficeTVBroadcast.h"
#include "Office/OfficeTVPlayback.h"
#include "Templates/Function.h"

enum class EQualityGrade : uint8;
struct FProjectEventData;
class UWorld;

/** Values the game clears at launch, read while the result window is still pending. */
struct COMPANYGROWTHRENEWAL_API FOfficeTVLaunchRecord
{
	bool bValid = false;
	int32 ReviewScore = 0;
	TArray<int32> CriticScores;
	bool bFirstDiscovery = false;
	FName Genre;
	FName Material;
	float DevelopmentSeconds = 0.f;

	/** Employee id and work count, ordered by employee id (hiring order). */
	TArray<TPair<int32, int32>> Participants;
};

/** Short-lived news the screen can only learn from game signals. Newest first. */
struct COMPANYGROWTHRENEWAL_API FOfficeTVNewsBuffers
{
	struct FWork
	{
		int32 EmployeeId = INDEX_NONE;
		int32 Slot = INDEX_NONE;
		float Score = 0.f;
	};

	TArray<FWork> RecentWork;

	/** Cumulative revenue at the last operation tick, the moment the office top bar redraws it. */
	int32 TickBuilding = INDEX_NONE;
	double TickTotalRevenue = 0.0;
};

namespace OfficeTVBroadcastSource
{
	using FSupportFilter = TFunctionRef<bool(EOfficeTVProgram)>;

	/** Builds the rotation for the current office state from the player's own company only. */
	COMPANYGROWTHRENEWAL_API void Gather(UWorld* World, const FOfficeTVNewsBuffers& News, FSupportFilter IsSupported,
		FOfficeTVSchedule& Out);

	COMPANYGROWTHRENEWAL_API bool CaptureLaunchRecord(UWorld* World, FOfficeTVLaunchRecord& Out);

	/** Celebration, critic scores, first discovery and credits, in airing order. */
	COMPANYGROWTHRENEWAL_API void BuildLaunchSequence(UWorld* World, const FOfficeTVLaunchRecord& Record,
		TArray<FOfficeTVBroadcastFrame>& Out);

	COMPANYGROWTHRENEWAL_API bool BuildTierUp(UWorld* World, int32 NewTier, FOfficeTVBroadcastFrame& Out);
	COMPANYGROWTHRENEWAL_API bool BuildCritical(UWorld* World, int32 EmployeeId, int32 Slot, float Score,
		FOfficeTVBroadcastFrame& Out);
	COMPANYGROWTHRENEWAL_API bool BuildFever(UWorld* World, FOfficeTVBroadcastFrame& Out);
	COMPANYGROWTHRENEWAL_API bool BuildIssueResult(UWorld* World, const FProjectEventData& Event, int32 ChoiceIndex,
		FOfficeTVBroadcastFrame& Out);
	COMPANYGROWTHRENEWAL_API bool BuildPromotion(UWorld* World, int32 EmployeeId, int32 OldLevel, int32 NewLevel,
		FOfficeTVBroadcastFrame& Out);
	COMPANYGROWTHRENEWAL_API bool BuildNewHire(UWorld* World, int32 EmployeeId, FOfficeTVBroadcastFrame& Out);
	/** One launch reaction under the operating project's title. */
	COMPANYGROWTHRENEWAL_API bool BuildReaction(UWorld* World, const FOfficeTVBroadcastEntry& Reaction,
		FOfficeTVBroadcastFrame& Out);

	/** Final development window: 30% of the run, kept between 3 and 10 seconds. */
	COMPANYGROWTHRENEWAL_API float CountdownWindow(float TotalSeconds);

	/** True only on the exact 30th, 100th or 365th day after hiring. */
	COMPANYGROWTHRENEWAL_API bool IsAnniversary(const FDateTime& Hired, const FDateTime& Now, int32& OutDays);
	COMPANYGROWTHRENEWAL_API FName GradeLetter(EQualityGrade Grade);

	/** Korean subject particle for the given word: "이" after a final consonant, otherwise "가". */
	COMPANYGROWTHRENEWAL_API FString SubjectParticle(const FString& Word);
}
