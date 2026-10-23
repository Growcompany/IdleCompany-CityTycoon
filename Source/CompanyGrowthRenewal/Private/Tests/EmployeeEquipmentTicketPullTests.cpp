#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Utils/EmployeeEquipmentTransactionRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketQuoteTest, "CompanyGrowth.EmployeeEquipment.TicketQuote", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketQuoteTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentPullQuote Quote;
    TestTrue(TEXT("Insufficient tickets still return a usable quote"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentAdvanced"), EItemType::EmployeeEquipmentTicketAdvanced, 10, 3, Quote));
    TestFalse(TEXT("Three tickets cannot pay for ten pulls"), Quote.bCanPull);
    TestEqual(TEXT("Shortage is explicit"), Quote.MissingTickets, 7);
    TestEqual(TEXT("Required count is never reduced"), Quote.RequiredTickets, 10);
    TestEqual(TEXT("Quote retains the precise ticket tier"), Quote.TicketType, EItemType::EmployeeEquipmentTicketAdvanced);
    TestEqual(TEXT("Shortage reason"), Quote.ErrorCode, FName(TEXT("InsufficientTickets")));
    TestTrue(TEXT("Ten matching tickets pay for ten pulls"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentPremium"), EItemType::EmployeeEquipmentTicketPremium, 10, 10, Quote));
    TestTrue(TEXT("Matching balance is affordable"), Quote.bCanPull);
    TestEqual(TEXT("Affordable quote has no shortage"), Quote.MissingTickets, 0);
    TestTrue(TEXT("Thirty matching tickets pay for thirty pulls"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentPremium"), EItemType::EmployeeEquipmentTicketPremium, 30, 30, Quote));
    TestTrue(TEXT("Thirty pull quote is affordable"), Quote.bCanPull);
    TestEqual(TEXT("Thirty pull quote preserves full cost"), Quote.RequiredTickets, 30);
    TestTrue(TEXT("Thirty pull shortage quote is usable"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentPremium"), EItemType::EmployeeEquipmentTicketPremium, 30, 29, Quote));
    TestFalse(TEXT("Twenty nine tickets never pay for thirty pulls"), Quote.bCanPull);
    TestEqual(TEXT("Thirty pull shortage is one ticket"), Quote.MissingTickets, 1);
    for (const int32 InvalidCount : {-1, 0, 31})
        TestFalse(TEXT("Out of range pull quantities are rejected"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentNormal"), EItemType::EmployeeEquipmentTicketNormal, InvalidCount, 100, Quote));
    TestFalse(TEXT("Unsupported quantity rejected"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentNormal"), EItemType::EmployeeEquipmentTicketNormal, 9, 10, Quote));
    TestFalse(TEXT("Recruit ticket cannot pay for gear"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentNormal"), EItemType::RecruitTicketNormal, 1, 10, Quote));
    TestFalse(TEXT("Corrupt negative balance rejected"), EmployeeEquipmentRules::QuoteTicketPull(TEXT("EquipmentNormal"), EItemType::EmployeeEquipmentTicketNormal, 1, -1, Quote));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentTicketBannerRulesTest, "CompanyGrowth.EmployeeEquipment.TicketBannerRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentTicketBannerRulesTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentBannerRow Advanced;
    Advanced.BannerId = TEXT("Advanced"); Advanced.PityGroupId = TEXT("Advanced");
    Advanced.TicketType = EItemType::EmployeeEquipmentTicketAdvanced;
    Advanced.GradeWeights = {0,0,5500,3500,800,200};
    Advanced.EpicPity = 5; Advanced.LegendaryPity = 30; Advanced.AscendantPity = 100;
    TestTrue(TEXT("Advanced tier allows zero weight low grades"), EmployeeEquipmentRules::ValidateBannerSettings(Advanced));
    TestFalse(TEXT("Common pool is not required"), EmployeeEquipmentRules::IsGradeReachable(Advanced, EEmployeeEquipmentGrade::Common));
    TestTrue(TEXT("Ascendant guarantee requires a pool"), EmployeeEquipmentRules::IsGradeReachable(Advanced, EEmployeeEquipmentGrade::Ascendant));
    FEmployeeEquipmentBannerRow Invalid = Advanced;
    Invalid.GradeWeights[2] = 5501;
    TestFalse(TEXT("Weights must sum exactly to ten thousand"), EmployeeEquipmentRules::ValidateBannerSettings(Invalid));
    Invalid = Advanced; Invalid.GradeWeights[0] = -1; Invalid.GradeWeights[2] += 1;
    TestFalse(TEXT("Negative weights cannot hide in valid sum"), EmployeeEquipmentRules::ValidateBannerSettings(Invalid));
    FEmployeeEquipmentPityState Pity;
    Pity.SinceEpic = 4;
    const TArray<int32> Next = EmployeeEquipmentRules::NextGradeWeights(Advanced, Pity);
    TestEqual(TEXT("Guaranteed next pull has no rare weight"), Next[2], 0);
    TestEqual(TEXT("Rare weight upgrades into epic"), Next[3], 9000);
    TestEqual(TEXT("Legendary natural rate preserved"), Next[4], 800);
    TestEqual(TEXT("Read-only quote does not consume pity"), Pity.SinceEpic, 4);
    return true;
}
#endif
