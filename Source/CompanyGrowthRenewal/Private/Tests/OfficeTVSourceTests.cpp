#include "Misc/AutomationTest.h"
#include "Office/OfficeTVBroadcastSource.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeTVSourceRulesTest,
	"CGR.Office.Television.SourceRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeTVSourceRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A 15 second development keeps most of its rotation"), OfficeTVBroadcastSource::CountdownWindow(15.f), 4.5f);
	TestEqual(TEXT("Very short runs still count down three seconds"), OfficeTVBroadcastSource::CountdownWindow(5.f), 3.f);
	TestEqual(TEXT("Long runs cap the countdown at ten seconds"), OfficeTVBroadcastSource::CountdownWindow(100.f), 10.f);

	const FDateTime Hired(2026, 1, 1, 9, 30);
	int32 Days = 0;
	TestTrue(TEXT("Day 30 is an anniversary"), OfficeTVBroadcastSource::IsAnniversary(Hired, Hired + FTimespan::FromDays(30) + FTimespan::FromHours(5), Days));
	TestEqual(TEXT("It reports thirty days"), Days, 30);
	TestTrue(TEXT("Day 100 is an anniversary"), OfficeTVBroadcastSource::IsAnniversary(Hired, Hired + FTimespan::FromDays(100), Days));
	TestTrue(TEXT("Day 365 is an anniversary"), OfficeTVBroadcastSource::IsAnniversary(Hired, Hired + FTimespan::FromDays(365), Days));
	TestFalse(TEXT("Day 31 is not"), OfficeTVBroadcastSource::IsAnniversary(Hired, Hired + FTimespan::FromDays(31), Days));
	TestFalse(TEXT("The hiring day is not"), OfficeTVBroadcastSource::IsAnniversary(Hired, Hired, Days));
	TestFalse(TEXT("An unset hiring date is never an anniversary"), OfficeTVBroadcastSource::IsAnniversary(FDateTime(), FDateTime(1, 1, 31), Days));

	// TestEqual with FString ignores case, so compare the exact characters.
	TestTrue(TEXT("사운드 takes 가"), OfficeTVBroadcastSource::SubjectParticle(TEXT("사운드")).Equals(TEXT("가"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("그래픽 takes 이"), OfficeTVBroadcastSource::SubjectParticle(TEXT("그래픽")).Equals(TEXT("이"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("기획 takes 이"), OfficeTVBroadcastSource::SubjectParticle(TEXT("기획")).Equals(TEXT("이"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("QA takes 가"), OfficeTVBroadcastSource::SubjectParticle(TEXT("QA")).Equals(TEXT("가"), ESearchCase::CaseSensitive));
	return true;
}

#endif
