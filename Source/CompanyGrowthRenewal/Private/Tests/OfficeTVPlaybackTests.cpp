#include "Misc/AutomationTest.h"
#include "Office/OfficeTVPlayback.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FOfficeTVBroadcastFrame MakeFrame(EOfficeTVProgram Program, const TCHAR* Title = TEXT(""))
	{
		FOfficeTVBroadcastFrame Frame;
		Frame.Program = Program;
		Frame.Title = FText::FromString(Title);
		return Frame;
	}

	FOfficeTVSchedule MakeSchedule(EOfficeTVPool Pool, std::initializer_list<EOfficeTVProgram> Programs)
	{
		FOfficeTVSchedule Schedule;
		Schedule.Pool = Pool;
		for (const EOfficeTVProgram Program : Programs)
		{
			Schedule.Rotation.Add(MakeFrame(Program));
		}
		return Schedule;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVLaunchVisibilityTest,
	"CGR.Office.Television.LaunchVisibleAfterModal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVLaunchVisibilityTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Idle, {EOfficeTVProgram::Advertisement});
	Playback.Advance(Schedule, 0.f, true);
	Playback.QueuePriority({MakeFrame(EOfficeTVProgram::LaunchCelebration, TEXT("Just released"))});
	TestEqual(TEXT("Launch waits behind the results window"),
		Playback.Advance(Schedule, 60.f, false)->Program, EOfficeTVProgram::Advertisement);
	TestEqual(TEXT("Closing the window starts the full launch shot"),
		Playback.Advance(Schedule, 60.f, true)->Program, EOfficeTVProgram::LaunchCelebration);
	Playback.Advance(Schedule, 5.f, true);
	Playback.Advance(Schedule, 90.f, false);
	TestEqual(TEXT("A second menu cannot consume the remaining launch time"),
		Playback.Advance(Schedule, 90.f, true)->Program, EOfficeTVProgram::LaunchCelebration);
	TestEqual(TEXT("Nine visible seconds still show the launch"),
		Playback.Advance(Schedule, 4.f, true)->Program, EOfficeTVProgram::LaunchCelebration);
	TestEqual(TEXT("Ten visible seconds resume ordinary rotation"),
		Playback.Advance(Schedule, 1.f, true)->Program, EOfficeTVProgram::Advertisement);
	Playback.Reset();
	TestEqual(TEXT("Re-entry alone does not replay a launch"),
		Playback.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::Advertisement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVEmployeeRotationTest,
	"CGR.Office.Television.DevelopmentEmployeeStory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVEmployeeRotationTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Developing,
		{EOfficeTVProgram::Development, EOfficeTVProgram::EmployeeHighlight});
	Schedule.Opener = EOfficeTVProgram::Development;
	Schedule.Rotation[1].PortraitEmployeeId = 10;
	TestEqual(TEXT("Development opens with its teaser"),
		Playback.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::Development);
	TestEqual(TEXT("The worker appears during a short development"),
		Playback.Advance(Schedule, 4.f, true)->PortraitEmployeeId, 10);
	Schedule.Rotation[1].PortraitEmployeeId = 20;
	TestEqual(TEXT("A live rank change does not replace the face mid-sentence"),
		Playback.Advance(Schedule, 1.f, true)->PortraitEmployeeId, 10);
	TestEqual(TEXT("The teaser returns after the employee story"),
		Playback.Advance(Schedule, 3.f, true)->Program, EOfficeTVProgram::Development);
	Schedule.Rotation.SetNum(1);
	TestEqual(TEXT("No employee data means no invented worker story"),
		Playback.Advance(Schedule, 4.f, true)->Program, EOfficeTVProgram::Development);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVShuffleTest,
	"CGR.Office.Television.ShuffleCoversPoolOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVShuffleTest::RunTest(const FString& Parameters)
{
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Idle, {EOfficeTVProgram::Anniversary,
		EOfficeTVProgram::TierCodex, EOfficeTVProgram::HallOfFame, EOfficeTVProgram::TopRevenue, EOfficeTVProgram::Upcoming});
	for (int32 Seed = 1; Seed <= 25; ++Seed)
	{
		FOfficeTVPlayback Playback;
		Playback.SetSeed(Seed);
		TArray<EOfficeTVProgram> Picks;
		Picks.Add(Playback.Advance(Schedule, 0.f, true)->Program);
		for (int32 Index = 1; Index < 10; ++Index)
		{
			Picks.Add(Playback.Advance(Schedule, 6.f, true)->Program);
		}
		TSet<EOfficeTVProgram> FirstRound;
		TSet<EOfficeTVProgram> SecondRound;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			FirstRound.Add(Picks[Index]);
			SecondRound.Add(Picks[Index + 5]);
		}
		TestEqual(FString::Printf(TEXT("Seed %d: every programme airs once in the first round"), Seed), FirstRound.Num(), 5);
		TestEqual(FString::Printf(TEXT("Seed %d: every programme airs once in the second round"), Seed), SecondRound.Num(), 5);
		TestNotEqual(FString::Printf(TEXT("Seed %d: no back-to-back repeat across rounds"), Seed), Picks[4], Picks[5]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVLockedCountdownTest,
	"CGR.Office.Television.LockedCountdownWins", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVLockedCountdownTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Developing, {EOfficeTVProgram::Development});
	Schedule.Locked = MakeFrame(EOfficeTVProgram::LaunchCountdown, TEXT("7"));
	TestEqual(TEXT("The final window replaces rotation"),
		Playback.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::LaunchCountdown);
	Schedule.Locked->Title = FText::FromString(TEXT("6"));
	TestEqual(TEXT("The countdown keeps reading live values"),
		Playback.Advance(Schedule, 1.f, true)->Title.ToString(), FString(TEXT("6")));
	TestTrue(TEXT("Interrupts are still accepted"), Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::CriticalFlash)));
	TestEqual(TEXT("An interrupt never cuts the countdown"),
		Playback.Advance(Schedule, 1.f, true)->Program, EOfficeTVProgram::LaunchCountdown);
	Playback.Advance(Schedule, 6.f, true);
	TestEqual(TEXT("Stale critical news expires while the countdown plays"), Playback.GetPendingInterruptCount(), 0);
	Schedule.Locked.Reset();
	TestEqual(TEXT("Rotation returns after the window"),
		Playback.Advance(Schedule, 1.f, true)->Program, EOfficeTVProgram::Development);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVInterruptResumeTest,
	"CGR.Office.Television.InterruptResumesShow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVInterruptResumeTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	Playback.SetSeed(3);
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Idle,
		{EOfficeTVProgram::TierCodex, EOfficeTVProgram::HallOfFame});
	const EOfficeTVProgram First = Playback.Advance(Schedule, 0.f, true)->Program;
	const uint32 FirstSerial = Playback.GetShowSerial();
	Playback.Advance(Schedule, 4.f, true);
	Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::Fever));
	TestEqual(TEXT("The interrupt airs at once"),
		Playback.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::Fever);
	TestTrue(TEXT("An interrupt is a new showing"), Playback.GetShowSerial() != FirstSerial);
	TestEqual(TEXT("The paused programme resumes"),
		Playback.Advance(Schedule, 3.f, true)->Program, First);
	TestEqual(TEXT("It keeps its remaining time"),
		Playback.Advance(Schedule, 1.5f, true)->Program, First);
	TestNotEqual(TEXT("Then the next programme follows"),
		Playback.Advance(Schedule, 0.5f, true)->Program, First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVCriticalLimitTest,
	"CGR.Office.Television.CriticalRateLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVCriticalLimitTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Idle, {EOfficeTVProgram::TierCodex});
	Playback.Advance(Schedule, 0.f, true);
	TestTrue(TEXT("The first critical is news"), Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::CriticalFlash)));
	Playback.Advance(Schedule, 5.f, true);
	TestFalse(TEXT("A second critical within ten seconds is not"),
		Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::CriticalFlash)));
	Playback.Advance(Schedule, 5.f, true);
	TestTrue(TEXT("Ten visible seconds later it is news again"),
		Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::CriticalFlash)));
	TestFalse(TEXT("Programmes that are not interrupts are refused"),
		Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::HallOfFame)));

	FOfficeTVPlayback Crowded;
	Crowded.Advance(Schedule, 0.f, true);
	Crowded.QueuePriority({MakeFrame(EOfficeTVProgram::LaunchCelebration)});
	Crowded.Advance(Schedule, 0.f, true);
	Crowded.QueueInterrupt(MakeFrame(EOfficeTVProgram::IssueResult));
	Crowded.QueueInterrupt(MakeFrame(EOfficeTVProgram::Promotion));
	Crowded.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire));
	Crowded.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire, TEXT("Second hire")));
	TestEqual(TEXT("At most three interrupts wait"), Crowded.GetPendingInterruptCount(), 3);

	FOfficeTVPlayback Spaced;
	Spaced.Advance(Schedule, 0.f, true);
	Spaced.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire, TEXT("A")));
	Spaced.QueueInterrupt(MakeFrame(EOfficeTVProgram::Promotion, TEXT("B")));
	TestEqual(TEXT("The first interrupt airs"), Spaced.Advance(Schedule, 0.f, true)->Title.ToString(), FString(TEXT("A")));
	TestEqual(TEXT("Regular rotation returns between interrupts"),
		Spaced.Advance(Schedule, 4.f, true)->Program, EOfficeTVProgram::TierCodex);
	TestEqual(TEXT("Eight visible seconds keep the gap"),
		Spaced.Advance(Schedule, 7.f, true)->Program, EOfficeTVProgram::TierCodex);
	TestEqual(TEXT("The next interrupt follows after the gap"),
		Spaced.Advance(Schedule, 1.f, true)->Title.ToString(), FString(TEXT("B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVReactionCutInTest,
	"CGR.Office.Television.ReactionCutsInOnArrival", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVReactionCutInTest::RunTest(const FString& Parameters)
{
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Operating, {EOfficeTVProgram::RevenueTicker});
	FOfficeTVPlayback Playback;
	Playback.Advance(Schedule, 0.f, true);
	TestTrue(TEXT("A reaction is news"), Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::Reactions, TEXT("R1"))));
	TestEqual(TEXT("It airs as soon as it arrives"), Playback.Advance(Schedule, 0.f, true)->Title.ToString(), FString(TEXT("R1")));
	TestEqual(TEXT("Rotation returns after it"), Playback.Advance(Schedule, 6.f, true)->Program, EOfficeTVProgram::RevenueTicker);
	Playback.Advance(Schedule, 2.f, true);
	Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::Reactions, TEXT("R2")));
	TestEqual(TEXT("The next reaction does not wait out the eight-second gap"),
		Playback.Advance(Schedule, 0.f, true)->Title.ToString(), FString(TEXT("R2")));

	FOfficeTVPlayback Launch;
	Launch.Advance(Schedule, 0.f, true);
	Launch.QueuePriority({MakeFrame(EOfficeTVProgram::LaunchCelebration), MakeFrame(EOfficeTVProgram::CriticScores),
		MakeFrame(EOfficeTVProgram::Credits)});
	Launch.Advance(Schedule, 0.f, true);
	const TCHAR* Early[] = {TEXT("E1"), TEXT("E2"), TEXT("E3"), TEXT("E4")};
	for (const TCHAR* Title : Early)
	{
		Launch.QueueInterrupt(MakeFrame(EOfficeTVProgram::Reactions, Title));
	}
	Launch.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire, TEXT("Hire")));
	TestEqual(TEXT("Four reactions from the launch sequence and other news all wait"), Launch.GetPendingInterruptCount(), 5);
	Launch.Advance(Schedule, 10.f, true);
	Launch.Advance(Schedule, 6.f, true);
	TestEqual(TEXT("The launch sequence keeps its order"), Launch.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::Credits);
	TestEqual(TEXT("Every early reaction airs after the credits, oldest first"),
		Launch.Advance(Schedule, 8.f, true)->Title.ToString(), FString(TEXT("E1")));
	for (int32 Index = 1; Index < 4; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Reaction %d follows without a gap"), Index + 1),
			Launch.Advance(Schedule, 6.f, true)->Title.ToString(), FString(Early[Index]));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVInterruptMenuTest,
	"CGR.Office.Television.InterruptWaitsBehindMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVInterruptMenuTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Idle, {EOfficeTVProgram::TierCodex});
	Playback.Advance(Schedule, 0.f, true);
	Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire));
	Playback.Advance(Schedule, 100.f, false);
	TestEqual(TEXT("A hire made inside the recruitment window airs after it closes"),
		Playback.Advance(Schedule, 100.f, true)->Program, EOfficeTVProgram::NewHire);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVPriorityTest,
	"CGR.Office.Television.PriorityNotInterrupted", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVPriorityTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Operating, {EOfficeTVProgram::Operations});
	Playback.Advance(Schedule, 0.f, true);
	Playback.QueuePriority({MakeFrame(EOfficeTVProgram::LaunchCelebration), MakeFrame(EOfficeTVProgram::Credits)});
	TestEqual(TEXT("The launch sequence starts"),
		Playback.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::LaunchCelebration);
	Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire));
	TestEqual(TEXT("An interrupt waits for the celebration"),
		Playback.Advance(Schedule, 3.f, true)->Program, EOfficeTVProgram::LaunchCelebration);
	TestEqual(TEXT("The sequence continues with the credits"),
		Playback.Advance(Schedule, 7.f, true)->Program, EOfficeTVProgram::Credits);
	TestEqual(TEXT("The waiting interrupt airs after the sequence"),
		Playback.Advance(Schedule, 8.f, true)->Program, EOfficeTVProgram::NewHire);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVResetTest,
	"CGR.Office.Television.ResetDropsPending", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVResetTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Playback;
	const FOfficeTVSchedule Schedule = MakeSchedule(EOfficeTVPool::Idle, {EOfficeTVProgram::TierCodex});
	Playback.Advance(Schedule, 0.f, true);
	Playback.QueuePriority({MakeFrame(EOfficeTVProgram::TierUp)});
	Playback.QueueInterrupt(MakeFrame(EOfficeTVProgram::NewHire));
	Playback.Reset();
	TestEqual(TEXT("Re-entering the office shows only the current rotation"),
		Playback.Advance(Schedule, 0.f, true)->Program, EOfficeTVProgram::TierCodex);
	TestEqual(TEXT("No interrupt survives the reset"), Playback.GetPendingInterruptCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVStoryFrameTest,
	"CGR.Office.Television.StoryFrameFrozen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVStoryFrameTest::RunTest(const FString& Parameters)
{
	FOfficeTVPlayback Story;
	FOfficeTVSchedule StorySchedule = MakeSchedule(EOfficeTVPool::Idle, {EOfficeTVProgram::HallOfFame});
	StorySchedule.Rotation[0].Title = FText::FromString(TEXT("A"));
	Story.Advance(StorySchedule, 0.f, true);
	StorySchedule.Rotation[0].Title = FText::FromString(TEXT("B"));
	TestEqual(TEXT("A story keeps the frame it started with"),
		Story.Advance(StorySchedule, 1.f, true)->Title.ToString(), FString(TEXT("A")));

	FOfficeTVPlayback Readout;
	FOfficeTVSchedule ReadoutSchedule = MakeSchedule(EOfficeTVPool::Operating, {EOfficeTVProgram::RevenueTicker});
	ReadoutSchedule.Rotation[0].Value = FText::FromString(TEXT("1"));
	Readout.Advance(ReadoutSchedule, 0.f, true);
	ReadoutSchedule.Rotation[0].Value = FText::FromString(TEXT("2"));
	TestEqual(TEXT("A readout refreshes during its showing"),
		Readout.Advance(ReadoutSchedule, 1.f, true)->Value.ToString(), FString(TEXT("2")));
	const uint32 Serial = Readout.GetShowSerial();
	Readout.Advance(ReadoutSchedule, 1.f, true);
	TestEqual(TEXT("Refreshing is not a new showing"), Readout.GetShowSerial(), Serial);
	return true;
}

#endif
