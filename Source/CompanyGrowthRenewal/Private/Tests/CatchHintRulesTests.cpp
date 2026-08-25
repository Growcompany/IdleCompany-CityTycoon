#include "Misc/AutomationTest.h"
#include "UI/Element/Common/CatchHintRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRCatchHintRulesTest,
	"CGR.Office.CatchHintRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRCatchHintRulesTest::RunTest(const FString& Parameters)
{
	using namespace CatchHintRules;
	TestEqual(TEXT("telegraph -> doze key"), ResolveKey(EFatigueSlackPhase::Telegraph), KeyDoze);
	TestEqual(TEXT("slumping -> doze key"), ResolveKey(EFatigueSlackPhase::Slumping), KeyDoze);
	TestEqual(TEXT("bolting -> bolt key"), ResolveKey(EFatigueSlackPhase::Bolting), KeyBolt);
	TestEqual(TEXT("none -> no key"), ResolveKey(EFatigueSlackPhase::None), FName(NAME_None));
	TestEqual(TEXT("doze label"), ResolveLabel(EFatigueSlackPhase::Telegraph).ToString(), FString(TEXT("탭해서 깨우기")));
	TestEqual(TEXT("bolt label"), ResolveLabel(EFatigueSlackPhase::Bolting).ToString(), FString(TEXT("탭해서 불러오기")));
	TestTrue(TEXT("none label empty"), ResolveLabel(EFatigueSlackPhase::None).IsEmpty());
	TestEqual(TEXT("graduation count is 3"), GraduationCount, 3);
	TestTrue(TEXT("offset up-right"), HintOffset.X > 0.f && HintOffset.Y < 0.f);
	return true;
}

#endif
