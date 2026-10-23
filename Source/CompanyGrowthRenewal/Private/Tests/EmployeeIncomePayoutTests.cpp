#include "Misc/AutomationTest.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGREmployeeIncomePayoutTest,
	"CGR.Employee.IncomePayoutAboveInt32",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGREmployeeIncomePayoutTest::RunTest(const FString& Parameters)
{
	double PendingIncome = 3000000000.25;
	const int64 Paid = UEmployeeBehaviorComponent::ResolveAccumulatedIncomePayout(PendingIncome);
	TestEqual(TEXT("A 3 billion payout remains positive and exact"), Paid, static_cast<int64>(3000000000));
	TestTrue(TEXT("The fractional remainder carries forward"), FMath::IsNearlyEqual(PendingIncome, 0.25));

	PendingIncome += 0.8;
	TestEqual(TEXT("Carried fractions eventually pay one more Money"),
		UEmployeeBehaviorComponent::ResolveAccumulatedIncomePayout(PendingIncome), static_cast<int64>(1));
	TestTrue(TEXT("Only the new fractional remainder remains"), FMath::IsNearlyEqual(PendingIncome, 0.05));
	return true;
}

#endif
