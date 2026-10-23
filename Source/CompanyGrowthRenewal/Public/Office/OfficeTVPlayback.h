#pragma once

#include "CoreMinimal.h"
#include "Data/OfficeTVBroadcast.h"
#include "Math/RandomStream.h"
#include "Misc/Optional.h"

/** What the office can show right now. Built fresh on every refresh. */
struct COMPANYGROWTHRENEWAL_API FOfficeTVSchedule
{
	EOfficeTVPool Pool = EOfficeTVPool::None;

	/** One frame per eligible programme of the pool. */
	TArray<FOfficeTVBroadcastFrame> Rotation;

	/** Final development window: shown instead of the rotation while set. */
	TOptional<FOfficeTVBroadcastFrame> Locked;

	/** Airs first when the pool starts, if eligible. */
	TOptional<EOfficeTVProgram> Opener;
};

/**
 * Shuffled rotation per pool, priority sequences and short interrupts.
 * Only unobstructed viewing time counts, so menus never consume a showing.
 */
class COMPANYGROWTHRENEWAL_API FOfficeTVPlayback
{
public:
	FOfficeTVPlayback();

	void Reset();
	void SetSeed(int32 Seed);

	/** Runs to completion in order and is never cut by interrupts. */
	void QueuePriority(const TArray<FOfficeTVBroadcastFrame>& Sequence);

	/** Returns false when the frame is not interrupt news or arrives too soon after the same kind. */
	bool QueueInterrupt(const FOfficeTVBroadcastFrame& Frame);

	const FOfficeTVBroadcastFrame* Advance(const FOfficeTVSchedule& Schedule, float DeltaSeconds, bool bCanWatch);

	/** Increments whenever a different showing begins, including a resumed one. */
	uint32 GetShowSerial() const { return ShowSerial; }
	float GetShowElapsed() const { return ShowElapsed; }
	int32 GetPendingInterruptCount() const { return Interrupts.Num(); }

private:
	enum class EShow : uint8 { None, Rotation, Locked, Priority, Interrupt };

	struct FPendingInterrupt
	{
		FOfficeTVBroadcastFrame Frame;
		float Age = 0.f;
	};

	FRandomStream Random;
	FOfficeTVBroadcastFrame Displayed;
	EShow Showing = EShow::None;
	float Remaining = 0.f;
	float ShowElapsed = 0.f;
	uint32 ShowSerial = 0;

	FOfficeTVBroadcastFrame Suspended;
	float SuspendedRemaining = 0.f;
	float SuspendedElapsed = 0.f;
	bool bHasSuspended = false;

	TArray<FOfficeTVBroadcastFrame> PriorityQueue;
	TArray<FPendingInterrupt> Interrupts;
	float SinceInterrupt = 0.f;
	float SinceCritical = 0.f;

	EOfficeTVPool BagPool = EOfficeTVPool::None;
	TArray<EOfficeTVProgram> Bag;
	int32 BagCursor = 0;
	TOptional<EOfficeTVProgram> LastPlayed;
	bool bWasWatchable = false;

	void Begin(const FOfficeTVBroadcastFrame& Frame, EShow Kind, float Duration);
	bool PickRotation(const FOfficeTVSchedule& Schedule);
	void RebuildBag(const FOfficeTVSchedule& Schedule, bool bPoolStart);
	static const FOfficeTVBroadcastFrame* FindFrame(const FOfficeTVSchedule& Schedule, EOfficeTVProgram Program);
};
