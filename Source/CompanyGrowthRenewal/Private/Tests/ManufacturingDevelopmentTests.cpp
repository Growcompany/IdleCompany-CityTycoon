#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/GameSaveData.h"
#include "Data/ManufacturingDevelopmentRules.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManufacturingAccrualTest,
    "CGR.Manufacturing.DevelopmentAccrual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManufacturingAccrualTest::RunTest(const FString& Parameters)
{
    const double Whole = ManufacturingDevelopmentRules::Accrue(12.5, 3.0, 2.0);
    const double Split = ManufacturingDevelopmentRules::Accrue(
        ManufacturingDevelopmentRules::Accrue(12.5, 3.0, 0.75), 3.0, 1.25);
    TestTrue(TEXT("Splitting elapsed time preserves points"), FMath::IsNearlyEqual(Whole, Split));
    TestEqual(TEXT("Negative time does not accrue"), ManufacturingDevelopmentRules::Accrue(12.5, 3.0, -1.0), 12.5);
    TestEqual(TEXT("NaN rate does not accrue"), ManufacturingDevelopmentRules::Accrue(12.5,
        std::numeric_limits<double>::quiet_NaN(), 1.0), 12.5);
    TestEqual(TEXT("Corrupt negative balance resets to zero"),
        ManufacturingDevelopmentRules::Accrue(-2.0, 0.0, 1.0), 0.0);
    TestEqual(TEXT("NaN balance resets before accrual"), ManufacturingDevelopmentRules::Accrue(
        std::numeric_limits<double>::quiet_NaN(), 2.0, 1.0), 2.0);
    TestEqual(TEXT("Empty roster has zero rate"), ManufacturingDevelopmentRules::SumRates(TArray<double>()), 0.0);
    TMap<int32, double> BuildingRates;
    BuildingRates.Add(1, 2.0);
    BuildingRates.Add(2, 4.0);
    TestEqual(TEXT("Other building rate does not affect first building"),
        ManufacturingDevelopmentRules::Accrue(0.0, BuildingRates.FindRef(1), 1.0), 2.0);

    const FDateTime Start(2026, 10, 19, 0, 0, 0);
    TArray<ManufacturingDevelopmentRules::FTimeInterval> Online;
    Online.Emplace(Start + FTimespan::FromSeconds(5), Start + FTimespan::FromSeconds(10));
    Online.Emplace(Start + FTimespan::FromSeconds(10), Start + FTimespan::FromSeconds(15));
    TestEqual(TEXT("Initial login subtracts already credited online intervals"),
        ManufacturingDevelopmentRules::UncoveredSeconds(Start, Start + FTimespan::FromSeconds(20), Online), 10.0);
    TestEqual(TEXT("Warm resume excludes online time before background"),
        ManufacturingDevelopmentRules::UncoveredSeconds(Start + FTimespan::FromSeconds(15),
            Start + FTimespan::FromSeconds(25), Online), 10.0);
    Online.Emplace(Start + FTimespan::FromSeconds(23), Start + FTimespan::FromSeconds(25));
    TestEqual(TEXT("Warm resume excludes online time after foreground"),
        ManufacturingDevelopmentRules::UncoveredSeconds(Start + FTimespan::FromSeconds(15),
            Start + FTimespan::FromSeconds(25), Online), 8.0);
    TestEqual(TEXT("Repeated offline interval after a settled end has no credit"),
        ManufacturingDevelopmentRules::UncoveredSeconds(Start, Start + FTimespan::FromSeconds(20), Online,
            Start + FTimespan::FromSeconds(20)), 0.0);

    // 서버 LastSync보다 늦은 로컬 포인트 저장 뒤 프로세스가 재시작된 경우를 실제 SaveGame 형식으로 검증한다.
    TStrongObjectPtr<USaveGame_GameData> FirstSession(NewObject<USaveGame_GameData>());
    FirstSession->GameData.OfficeDataMap.FindOrAdd(7).ManufacturingDevelopmentPoints[0] = 480.0;
    FirstSession->GameData.ManufacturingOnlineIntervals.Emplace(
        Start, Start + FTimespan::FromMinutes(8));
    TArray<uint8> SaveBytes;
    if (!TestTrue(TEXT("Serialize manufacturing points and coverage together"),
        UGameplayStatics::SaveGameToMemory(FirstSession.Get(), SaveBytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> RestartedSession(
        Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(SaveBytes)));
    if (!TestNotNull(TEXT("Restore manufacturing ledger in a new save object"), RestartedSession.Get())) return false;
    const FDateTime ServerIntervalEnd = Start + FTimespan::FromMinutes(9);
    const double RestartUncovered = ManufacturingDevelopmentRules::UncoveredSeconds(Start,
        ServerIntervalEnd, RestartedSession->GameData.ManufacturingOnlineIntervals,
        RestartedSession->GameData.ManufacturingLastOfflineSettledEndUtc);
    TestEqual(TEXT("Cold restart credits only the minute since locally saved online progress"),
        RestartUncovered, 60.0);
    TArray<FManufacturingOnlineInterval> DelayedCallbackIntervals = RestartedSession->GameData.ManufacturingOnlineIntervals;
    DelayedCallbackIntervals.Emplace(
        ServerIntervalEnd, ServerIntervalEnd + FTimespan::FromMinutes(1));
    TestEqual(TEXT("New online progress before a delayed server callback preserves the background gap"),
        ManufacturingDevelopmentRules::UncoveredSeconds(Start,
            ServerIntervalEnd + FTimespan::FromMinutes(1),
            DelayedCallbackIntervals), 60.0);
    FOfficeSaveData& RestartedOffice = RestartedSession->GameData.OfficeDataMap.FindOrAdd(7);
    RestartedOffice.ManufacturingDevelopmentPoints[0] = ManufacturingDevelopmentRules::Accrue(
        RestartedOffice.ManufacturingDevelopmentPoints[0], 1.0, RestartUncovered * 0.2);
    RestartedSession->GameData.ManufacturingLastOfflineSettledEndUtc = ServerIntervalEnd;
    TestEqual(TEXT("Eight online minutes and one offline minute are not double credited"),
        RestartedOffice.ManufacturingDevelopmentPoints[0], 492.0);
    SaveBytes.Reset();
    if (!TestTrue(TEXT("Serialize settled checkpoint"),
        UGameplayStatics::SaveGameToMemory(RestartedSession.Get(), SaveBytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> NextSession(
        Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(SaveBytes)));
    if (!TestNotNull(TEXT("Restore settled checkpoint"), NextSession.Get())) return false;
    TestEqual(TEXT("Repeated stale server interval after another restart has no credit"),
        ManufacturingDevelopmentRules::UncoveredSeconds(Start, ServerIntervalEnd,
            NextSession->GameData.ManufacturingOnlineIntervals,
            NextSession->GameData.ManufacturingLastOfflineSettledEndUtc), 0.0);

    const FDateTime MeasuredEnd = Start + FTimespan::FromSeconds(60);
    const FDateTime CallbackNow = Start + FTimespan::FromSeconds(180);
    TArray<FManufacturingOnlineInterval> DuringCallbackDelay;
    DuringCallbackDelay.Emplace(MeasuredEnd, CallbackNow);
    TestEqual(TEXT("Delayed callback settles measured offline interval, not callback reception time"),
        ManufacturingDevelopmentRules::UncoveredSeconds(
            MeasuredEnd - FTimespan::FromSeconds(60), MeasuredEnd, DuringCallbackDelay), 60.0);
    TestEqual(TEXT("Using callback reception time would incorrectly erase that offline interval"),
        ManufacturingDevelopmentRules::UncoveredSeconds(
            CallbackNow - FTimespan::FromSeconds(60), CallbackNow, DuringCallbackDelay), 0.0);
    return true;
}

#endif
