#include "Office/OfficeTVPlayback.h"

#include "HAL/PlatformTime.h"

namespace
{
	constexpr int32 MaxPendingInterrupts = 3;
	// A launch drips seven reactions and up to four land during the launch sequence.
	constexpr int32 MaxPendingReactions = 7;
	constexpr float CriticalCooldownSeconds = 10.f;
	constexpr float LongAgoSeconds = 1.0e6f;
	constexpr float LockedSeconds = 1.0e6f;
}

FOfficeTVPlayback::FOfficeTVPlayback()
{
	Random.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	Reset();
}

void FOfficeTVPlayback::Reset()
{
	// The show serial keeps counting so a widget never mistakes a new showing for the old one.
	Displayed = FOfficeTVBroadcastFrame();
	Showing = EShow::None;
	Remaining = 0.f;
	ShowElapsed = 0.f;
	Suspended = FOfficeTVBroadcastFrame();
	SuspendedRemaining = 0.f;
	SuspendedElapsed = 0.f;
	bHasSuspended = false;
	PriorityQueue.Reset();
	Interrupts.Reset();
	SinceInterrupt = LongAgoSeconds;
	SinceCritical = LongAgoSeconds;
	BagPool = EOfficeTVPool::None;
	Bag.Reset();
	BagCursor = 0;
	LastPlayed.Reset();
	bWasWatchable = false;
}

void FOfficeTVPlayback::SetSeed(int32 Seed)
{
	Random.Initialize(Seed);
}

void FOfficeTVPlayback::QueuePriority(const TArray<FOfficeTVBroadcastFrame>& Sequence)
{
	PriorityQueue.Append(Sequence);
}

bool FOfficeTVPlayback::QueueInterrupt(const FOfficeTVBroadcastFrame& Frame)
{
	if (OfficeTVProgram::GetInterruptLifetime(Frame.Program) <= 0.f)
	{
		return false;
	}
	if (Frame.Program == EOfficeTVProgram::CriticalFlash)
	{
		if (SinceCritical < CriticalCooldownSeconds)
		{
			return false;
		}
		SinceCritical = 0.f;
	}
	// Reactions wait in their own line so a launch's stream never pushes out other news, or the reverse.
	const bool bReaction = Frame.Program == EOfficeTVProgram::Reactions;
	const auto InLine = [bReaction](const FPendingInterrupt& Pending)
	{
		return (Pending.Frame.Program == EOfficeTVProgram::Reactions) == bReaction;
	};
	if (Interrupts.FilterByPredicate(InLine).Num() >= (bReaction ? MaxPendingReactions : MaxPendingInterrupts))
	{
		int32 Victim = Interrupts.IndexOfByPredicate([&Frame](const FPendingInterrupt& Pending)
		{
			return Pending.Frame.Program == Frame.Program;
		});
		if (Victim == INDEX_NONE)
		{
			Victim = Interrupts.IndexOfByPredicate(InLine);
		}
		Interrupts.RemoveAt(Victim);
	}
	Interrupts.Add({Frame, 0.f});
	return true;
}

const FOfficeTVBroadcastFrame* FOfficeTVPlayback::Advance(const FOfficeTVSchedule& Schedule, float DeltaSeconds, bool bCanWatch)
{
	// The first watchable tick after an obstruction only resumes; the obstructed span is not viewing time.
	const float Viewed = bCanWatch && bWasWatchable ? FMath::Max(0.f, DeltaSeconds) : 0.f;
	bWasWatchable = bCanWatch;
	if (!bCanWatch)
	{
		return Showing != EShow::None ? &Displayed : nullptr;
	}

	SinceInterrupt += Viewed;
	SinceCritical += Viewed;
	for (FPendingInterrupt& Pending : Interrupts)
	{
		Pending.Age += Viewed;
	}
	Interrupts.RemoveAll([](const FPendingInterrupt& Pending)
	{
		return Pending.Age > OfficeTVProgram::GetInterruptLifetime(Pending.Frame.Program);
	});
	if (Showing != EShow::None)
	{
		Remaining -= Viewed;
		ShowElapsed += Viewed;
	}

	if (Showing == EShow::Priority)
	{
		if (Remaining > 0.f)
		{
			return &Displayed;
		}
		Showing = EShow::None;
	}
	if (!PriorityQueue.IsEmpty())
	{
		bHasSuspended = false;
		const FOfficeTVBroadcastFrame Next = PriorityQueue[0];
		PriorityQueue.RemoveAt(0);
		Begin(Next, EShow::Priority, OfficeTVProgram::GetDuration(Next.Program));
		return &Displayed;
	}

	if (Showing == EShow::Interrupt)
	{
		if (Remaining > 0.f)
		{
			return &Displayed;
		}
		Showing = EShow::None;
		SinceInterrupt = 0.f;
		if (bHasSuspended)
		{
			bHasSuspended = false;
			Displayed = Suspended;
			Remaining = SuspendedRemaining;
			ShowElapsed = SuspendedElapsed;
			Showing = EShow::Rotation;
			++ShowSerial;
		}
	}

	if (Schedule.Locked.IsSet())
	{
		if (Showing == EShow::Locked)
		{
			Displayed = Schedule.Locked.GetValue();
		}
		else
		{
			bHasSuspended = false;
			Begin(Schedule.Locked.GetValue(), EShow::Locked, LockedSeconds);
		}
		return &Displayed;
	}
	if (Showing == EShow::Locked)
	{
		Showing = EShow::None;
	}

	if (!Interrupts.IsEmpty() && SinceInterrupt >= OfficeTVProgram::GetInterruptGap(Interrupts[0].Frame.Program))
	{
		if (Showing == EShow::Rotation && Remaining > 0.f)
		{
			Suspended = Displayed;
			SuspendedRemaining = Remaining;
			SuspendedElapsed = ShowElapsed;
			bHasSuspended = true;
		}
		const FOfficeTVBroadcastFrame Next = Interrupts[0].Frame;
		Interrupts.RemoveAt(0);
		Begin(Next, EShow::Interrupt, OfficeTVProgram::GetDuration(Next.Program));
		return &Displayed;
	}

	if (Schedule.Pool != BagPool)
	{
		RebuildBag(Schedule, true);
		if (Showing == EShow::Rotation)
		{
			Showing = EShow::None;
		}
	}
	if (Showing == EShow::Rotation && Remaining > 0.f)
	{
		if (const FOfficeTVBroadcastFrame* Fresh = FindFrame(Schedule, Displayed.Program))
		{
			if (OfficeTVProgram::IsLiveReadout(Displayed.Program))
			{
				Displayed = *Fresh;
			}
			return &Displayed;
		}
	}
	return PickRotation(Schedule) ? &Displayed : nullptr;
}

void FOfficeTVPlayback::Begin(const FOfficeTVBroadcastFrame& Frame, EShow Kind, float Duration)
{
	Displayed = Frame;
	Showing = Kind;
	Remaining = Duration;
	ShowElapsed = 0.f;
	++ShowSerial;
	if (Kind == EShow::Rotation)
	{
		LastPlayed = Frame.Program;
	}
}

bool FOfficeTVPlayback::PickRotation(const FOfficeTVSchedule& Schedule)
{
	// A programme that became eligible mid-round joins the end of the current round.
	for (const FOfficeTVBroadcastFrame& Frame : Schedule.Rotation)
	{
		Bag.AddUnique(Frame.Program);
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		while (Bag.IsValidIndex(BagCursor))
		{
			const EOfficeTVProgram Program = Bag[BagCursor++];
			if (const FOfficeTVBroadcastFrame* Frame = FindFrame(Schedule, Program))
			{
				Begin(*Frame, EShow::Rotation, OfficeTVProgram::GetDuration(Program));
				return true;
			}
		}
		RebuildBag(Schedule, false);
	}
	Showing = EShow::None;
	return false;
}

void FOfficeTVPlayback::RebuildBag(const FOfficeTVSchedule& Schedule, bool bPoolStart)
{
	BagPool = Schedule.Pool;
	BagCursor = 0;
	Bag.Reset();
	for (const FOfficeTVBroadcastFrame& Frame : Schedule.Rotation)
	{
		Bag.AddUnique(Frame.Program);
	}
	for (int32 Index = Bag.Num() - 1; Index > 0; --Index)
	{
		Bag.Swap(Index, Random.RandRange(0, Index));
	}
	const int32 OpenerIndex = bPoolStart && Schedule.Opener.IsSet() ? Bag.IndexOfByKey(Schedule.Opener.GetValue()) : INDEX_NONE;
	if (OpenerIndex != INDEX_NONE)
	{
		Bag.RemoveAt(OpenerIndex);
		Bag.Insert(Schedule.Opener.GetValue(), 0);
	}
	else if (LastPlayed.IsSet() && Bag.Num() > 1 && Bag[0] == LastPlayed.GetValue())
	{
		Bag.Swap(0, Random.RandRange(1, Bag.Num() - 1));
	}
}

const FOfficeTVBroadcastFrame* FOfficeTVPlayback::FindFrame(const FOfficeTVSchedule& Schedule, EOfficeTVProgram Program)
{
	return Schedule.Rotation.FindByPredicate([Program](const FOfficeTVBroadcastFrame& Frame)
	{
		return Frame.Program == Program;
	});
}
