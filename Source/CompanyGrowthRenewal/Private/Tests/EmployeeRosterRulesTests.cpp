#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/EmployeeRosterRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeRosterHireSlotsTest, "CGR.Employee.RosterHireSlots",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEmployeeRosterHireSlotsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("2 owned / 12 capacity"), FEmployeeRosterRules::HireSlotCount(2, 12), 10);
	TestEqual(TEXT("full"), FEmployeeRosterRules::HireSlotCount(12, 12), 0);
	TestEqual(TEXT("over capacity never negative"), FEmployeeRosterRules::HireSlotCount(15, 12), 0);
	TestEqual(TEXT("negative owned treated as 0"), FEmployeeRosterRules::HireSlotCount(-1, 3), 3);
	TestEqual(TEXT("zero capacity"), FEmployeeRosterRules::HireSlotCount(0, 0), 0);
	return true;
}

#endif
