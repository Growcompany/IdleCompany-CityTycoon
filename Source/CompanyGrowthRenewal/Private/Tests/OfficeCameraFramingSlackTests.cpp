#include "Misc/AutomationTest.h"
#include "Player/OfficeCameraFraming.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeFramingSlackTest,
    "CompanyGrowth.Office.Framing.Slack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeFramingSlackTest::RunTest(const FString&)
{
    TestEqual(TEXT("visible anchors do not shrink the room"),
        FOfficeCameraFraming::ResolveFramingSlack(true, true,
            [](FIntPoint, bool) { return true; }), FIntPoint::ZeroValue);
    TestEqual(TEXT("ineffective slack is discarded"),
        FOfficeCameraFraming::ResolveFramingSlack(true, true,
            [](FIntPoint, bool bX) { return bX; }), FIntPoint::ZeroValue);
    TestEqual(TEXT("only the missing horizontal anchor needs slack"),
        FOfficeCameraFraming::ResolveFramingSlack(true, true,
            [](FIntPoint S, bool bX) { return !bX || S.X > 0; }), FIntPoint(1, 0));
    TestEqual(TEXT("a maximum axis cannot consume slack"),
        FOfficeCameraFraming::ResolveFramingSlack(false, true,
            [](FIntPoint S, bool bX) { return !bX && S.Y > 0; }), FIntPoint(0, 1));
    TestEqual(TEXT("fitting the second axis must not lose the first"),
        FOfficeCameraFraming::ResolveFramingSlack(true, true,
            [](FIntPoint S, bool bX) { return bX ? S.X > 0 && S.Y == 0 : S.Y > 0; }), FIntPoint(1, 0));
    TestEqual(TEXT("slack must preserve an originally visible axis"),
        FOfficeCameraFraming::ResolveFramingSlack(true, true,
            [](FIntPoint S, bool bX) { return bX ? S.Y == 0 : S.Y > 0; }), FIntPoint::ZeroValue);
    TestEqual(TEXT("the other axis can make an earlier rejected candidate useful"),
        FOfficeCameraFraming::ResolveFramingSlack(true, true,
            [](FIntPoint S, bool bX) { return bX ? S.X > 0 && S.Y > 0 : S.Y > 0; }), FIntPoint(1, 1));
    return true;
}
#endif
