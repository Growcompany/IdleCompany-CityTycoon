#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Utils/EmployeeEquipmentTransactionRules.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Data/GameSaveData.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentAtomicCommitTest, "CompanyGrowth.EmployeeEquipment.AtomicCommit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentAtomicCommitTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentState Live;
    Live.BannerStates.FindOrAdd(TEXT("Normal")).SinceAscendant = 4;
    FEmployeeEquipmentState Candidate = Live;
    Candidate.BannerStates.FindOrAdd(TEXT("Normal")).SinceAscendant = 14;
    Candidate.Revision = 1;
    int32 Writes = 0;
    int32 Events = 0;
    bool bCommitting = false;
    TestFalse(TEXT("Failed persistence rejects whole candidate"), EmployeeEquipmentRules::CommitCandidate(bCommitting, Candidate,
        [&Writes](const FEmployeeEquipmentState&) { ++Writes; return false; },
        [&Live](const FEmployeeEquipmentState& Value) { Live = Value; },
        [&Events](const FEmployeeEquipmentState&) { ++Events; }));
    TestEqual(TEXT("Failed ten-pull leaves live pity unchanged"), Live.BannerStates.FindRef(TEXT("Normal")).SinceAscendant, 4);
    TestEqual(TEXT("Failed persistence never publishes"), Events, 0);
    TestEqual(TEXT("Batch attempts one snapshot write"), Writes, 1);
    TestFalse(TEXT("Failed persistence releases shared commit guard"), bCommitting);
    TestTrue(TEXT("Successful persistence publishes whole candidate"), EmployeeEquipmentRules::CommitCandidate(bCommitting, Candidate,
        [&Writes](const FEmployeeEquipmentState&) { ++Writes; return true; },
        [&Live](const FEmployeeEquipmentState& Value) { Live = Value; },
        [&Events](const FEmployeeEquipmentState&) { ++Events; }));
    TestEqual(TEXT("Success updates pity atomically"), Live.BannerStates.FindRef(TEXT("Normal")).SinceAscendant, 14);
    TestEqual(TEXT("Success updates revision atomically"), Live.Revision, int64(1));
    TestEqual(TEXT("Exactly one publication"), Events, 1);
    TestFalse(TEXT("Successful publication releases shared commit guard"), bCommitting);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentReplayStateReplacementTest, "CompanyGrowth.EmployeeEquipment.ReplayStateReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentReplayStateReplacementTest::RunTest(const FString& Parameters)
{
    // Replay is resolved before catalog/SaveLoad access, so an uninitialized transient manager suffices.
    UGameInstance* TestInstance = NewObject<UGameInstance>();
    UEmployeeEquipmentManagerSubsystem* EquipmentMgr = NewObject<UEmployeeEquipmentManagerSubsystem>(TestInstance);
    FEmployeeEquipmentRequest FirstRequest;
    FirstRequest.OperationId = FGuid::NewGuid();
    FirstRequest.CatalogVersion = TEXT("ReplayFixture");
    FEmployeeEquipmentRequest NestedRequest = FirstRequest;
    NestedRequest.OperationId = FGuid::NewGuid();
    NestedRequest.Count = 10;
    FEmployeeEquipmentState Seed;
    for (int32 RequestIndex = 0; RequestIndex < 2; ++RequestIndex)
    {
        const FEmployeeEquipmentRequest& Request = RequestIndex == 0 ? FirstRequest : NestedRequest;
        FEmployeeEquipmentOperationReceipt Receipt;
        Receipt.RequestHash = EmployeeEquipmentRules::FingerprintRequest(Request);
        Receipt.Result.OperationId = Request.OperationId;
        Receipt.Result.bCommitted = true;
        Receipt.Result.Revision = 41 + RequestIndex;
        Receipt.Result.CatalogVersion = TEXT("ReplayFixture");
        FEmployeeEquipmentInstance Granted;
        Granted.InstanceId = FGuid::NewGuid();
        Granted.DefinitionId = RequestIndex == 0 ? TEXT("EQ_HEAD_01") : TEXT("EQ_BACK_02");
        Receipt.Result.GrantedInstances.Add(Granted);
        Seed.CompletedOperations.Add(Request.OperationId, Receipt);
    }
    EquipmentMgr->RestoreState(Seed);
    bool bNestedStarted = false;
    bool bStateReplaced = false;
    int32 FirstHandlerCalls = 0;
    int32 NestedHandlerCalls = 0;
    auto CheckResult = [this, &FirstRequest](const FEmployeeEquipmentResult& Result)
    {
        const bool bFirst = Result.OperationId == FirstRequest.OperationId;
        TestTrue(TEXT("Replay remains committed after state replacement"), Result.bCommitted);
        TestEqual(TEXT("Replay owns its revision"), Result.Revision, int64(bFirst ? 41 : 42));
        TestEqual(TEXT("Replay owns its grant array"), Result.GrantedInstances.Num(), 1);
        if (Result.GrantedInstances.Num() == 1)
            TestEqual(TEXT("Replay grant survives nested restore"), Result.GrantedInstances[0].DefinitionId, FName(bFirst ? TEXT("EQ_HEAD_01") : TEXT("EQ_BACK_02")));
    };
    auto ObserveAndReenter = [&](const FEmployeeEquipmentResult& Result)
    {
        CheckResult(Result);
        if (Result.OperationId == FirstRequest.OperationId)
        {
            ++FirstHandlerCalls;
            if (!bNestedStarted)
            {
                bNestedStarted = true;
                const FEmployeeEquipmentResult NestedResult = EquipmentMgr->Execute(NestedRequest);
                TestEqual(TEXT("Nested Execute returns its own operation"), NestedResult.OperationId, NestedRequest.OperationId);
                CheckResult(NestedResult);
            }
        }
        else
        {
            ++NestedHandlerCalls;
            if (!bStateReplaced)
            {
                bStateReplaced = true;
                FEmployeeEquipmentState Replacement;
                Replacement.Revision = 99;
                EquipmentMgr->RestoreState(Replacement);
            }
        }
        CheckResult(Result);
    };
    // Both handlers can run first; the second must still see the original immutable receipt.
    EquipmentMgr->OnOperationCompleted.AddLambda(ObserveAndReenter);
    EquipmentMgr->OnOperationCompleted.AddLambda(ObserveAndReenter);
    const FEmployeeEquipmentResult Returned = EquipmentMgr->Execute(FirstRequest);
    TestEqual(TEXT("Outer Execute returns its own operation"), Returned.OperationId, FirstRequest.OperationId);
    CheckResult(Returned);
    TestEqual(TEXT("Both outer handlers run"), FirstHandlerCalls, 2);
    TestEqual(TEXT("Both nested handlers run"), NestedHandlerCalls, 2);
    TestEqual(TEXT("Callback replaced manager state"), EquipmentMgr->GetState().Revision, int64(99));
    TestTrue(TEXT("Receipt storage was actually released"), EquipmentMgr->GetState().CompletedOperations.IsEmpty());
    EquipmentMgr->OnOperationCompleted.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentCommitPublicationTest, "CompanyGrowth.EmployeeEquipment.CommitPublication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentCommitPublicationTest::RunTest(const FString& Parameters)
{
    UGameInstance* TestInstance = NewObject<UGameInstance>();
    UEmployeeManager* EmployeeMgr = NewObject<UEmployeeManager>(TestInstance);
    UEmployeeEquipmentManagerSubsystem* EquipmentMgr = NewObject<UEmployeeEquipmentManagerSubsystem>(TestInstance);
    FEmployeeInstance Fired; Fired.EmployeeID = 17;
    FEmployeeInstance Survivor; Survivor.EmployeeID = 29;
    EmployeeMgr->SetEmployeeList({Fired, Survivor});
    FEmployeeEquipmentState InitialEquipment;
    InitialEquipment.Loadouts.Add(17);
    EquipmentMgr->RestoreState(InitialEquipment);
    FGameSaveData Persisted;
    Persisted.EmployeeList = {Fired, Survivor};
    Persisted.EmployeeEquipmentState = InitialEquipment;
    FGameSaveData Candidate = Persisted;
    Candidate.EmployeeList = {Survivor};
    EquipmentMgr->RemoveEmployeeFromCandidate(17, Candidate);
    bool bCommitting = false;
    int32 ChairEmployeeId = 17;
    int32 Writes = 0;
    int32 NestedWrites = 0;
    int32 Publications = 0;
    auto NestedCommit = [&]()
    {
        return EmployeeEquipmentRules::CommitCandidate(bCommitting, Persisted,
            [&](const FGameSaveData&) { ++NestedWrites; return true; },
            [&](const FGameSaveData&) { EmployeeMgr->SetEmployeeList({}); },
            [](const FGameSaveData&) {});
    };
    TestTrue(TEXT("Fire candidate commits"), EmployeeEquipmentRules::CommitCandidate(bCommitting, Candidate,
        [&](const FGameSaveData& Snapshot)
        {
            ++Writes;
            TestEqual(TEXT("Persistence precedes live employee removal"), EmployeeMgr->GetEmployeeCount(), 2);
            TestFalse(TEXT("Nested write is rejected during persistence"), NestedCommit());
            Persisted = Snapshot;
            return true;
        },
        [&](const FGameSaveData& Snapshot)
        {
            EmployeeMgr->SetEmployeeList(Snapshot.EmployeeList);
            EquipmentMgr->RestoreState(Snapshot.EmployeeEquipmentState);
            ChairEmployeeId = INDEX_NONE;
            TestFalse(TEXT("Nested write is rejected during application"), NestedCommit());
        },
        [&](const FGameSaveData&)
        {
            ++Publications;
            TestNull(TEXT("Observers cannot find fired employee"), EmployeeMgr->FindEmployee(17));
            TestNotNull(TEXT("Observers still find the other employee"), EmployeeMgr->FindEmployee(29));
            TestEqual(TEXT("Observers see the released chair"), ChairEmployeeId, INDEX_NONE);
            TestFalse(TEXT("Observers see the returned equipment"), EquipmentMgr->GetState().Loadouts.Contains(17));
            TestFalse(TEXT("Nested write/fire cannot mutate another employee during publication"), NestedCommit());
        }));
    TestEqual(TEXT("Exactly one durable snapshot"), Writes, 1);
    TestEqual(TEXT("Reentrant commits never reach persistence"), NestedWrites, 0);
    TestEqual(TEXT("Exactly one final-state publication"), Publications, 1);
    TestEqual(TEXT("Durable roster has only survivor"), Persisted.EmployeeList.Num(), 1);
    TestEqual(TEXT("Live roster has only survivor"), EmployeeMgr->GetEmployeeCount(), 1);
    TestNotNull(TEXT("Survivor remains after all callbacks"), EmployeeMgr->FindEmployee(29));
    TestFalse(TEXT("Shared commit guard released"), bCommitting);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentWeeklyPeriodTest, "CompanyGrowth.EmployeeEquipment.WeeklyPeriod", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentWeeklyPeriodTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Sunday 23:59 KST is previous week"), EmployeeEquipmentRules::WeeklyPeriodId(FDateTime(2026,9,6,14,59,59)), FString(TEXT("2026-08-31")));
    TestEqual(TEXT("Monday 00:00 KST starts new period"), EmployeeEquipmentRules::WeeklyPeriodId(FDateTime(2026,9,6,15,0,0)), FString(TEXT("2026-09-07")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentPityTest, "CompanyGrowth.EmployeeEquipment.Pity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentPityTest::RunTest(const FString& Parameters)
{
    FEmployeeEquipmentPityState Pity;
    Pity.SinceEpic = 9; Pity.SinceLegendary = 79; Pity.SinceAscendant = 199;
    TestEqual(TEXT("Highest simultaneous guarantee wins"), EmployeeEquipmentRules::AdvancePity(Pity, EEmployeeEquipmentGrade::Common, 10, 80, 200), EEmployeeEquipmentGrade::Ascendant);
    TestEqual(TEXT("Ascendant resets every counter"), Pity.SinceEpic + Pity.SinceLegendary + Pity.SinceAscendant, 0);
    Pity.SinceEpic = 9;
    TestEqual(TEXT("Epic guarantee cannot downgrade natural legendary"), EmployeeEquipmentRules::AdvancePity(Pity, EEmployeeEquipmentGrade::Legendary, 10, 80, 200), EEmployeeEquipmentGrade::Legendary);
    TestEqual(TEXT("Legendary preserves ascendant progress"), Pity.SinceAscendant, 1);
    int32 EpicResults = 0;
    Pity = {};
    for (int32 DrawIndex = 0; DrawIndex < 20; ++DrawIndex)
        EpicResults += EmployeeEquipmentRules::AdvancePity(Pity, EEmployeeEquipmentGrade::Common, 10, 80, 200) == EEmployeeEquipmentGrade::Epic ? 1 : 0;
    TestEqual(TEXT("Sequential two batches guarantee twice"), EpicResults, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentRecruitGiftPoolTest, "CompanyGrowth.EmployeeEquipment.RecruitGiftPool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentRecruitGiftPoolTest::RunTest(const FString& Parameters)
{
    TMap<FName, FEmployeeEquipmentDefinitionRow> Definitions;
    auto Add = [&Definitions](const TCHAR* Id, EEmployeeEquipmentSlot Slot, EEmployeeEquipmentGrade Grade)
    {
        FEmployeeEquipmentDefinitionRow Row;
        Row.DefinitionId = Id; Row.EquipmentSlot = Slot; Row.Grade = Grade;
        Definitions.Add(Id, Row);
    };
    Add(TEXT("EQ_OUTFIT_C1"), EEmployeeEquipmentSlot::Outfit, EEmployeeEquipmentGrade::Common);
    Add(TEXT("EQ_HEAD_C1"), EEmployeeEquipmentSlot::Head, EEmployeeEquipmentGrade::Common);
    Add(TEXT("EQ_BACK_C1"), EEmployeeEquipmentSlot::Back, EEmployeeEquipmentGrade::Common);
    Add(TEXT("EQ_HEAD_E1"), EEmployeeEquipmentSlot::Head, EEmployeeEquipmentGrade::Epic);

    const TArray<FName> CommonPool = EmployeeEquipmentRules::BuildRecruitGiftPool(Definitions, EEmployeeEquipmentGrade::Common);
    TestEqual(TEXT("Gift pool keeps only the configured grade"), CommonPool.Num(), 3);
    TestFalse(TEXT("Gift pool excludes higher grades"), CommonPool.Contains(FName(TEXT("EQ_HEAD_E1"))));
    // 정렬이 깨지면 같은 난수에서 다른 장비가 나와 재현이 불가능해진다.
    TestEqual(TEXT("Gift pool order is stable"), CommonPool[0], FName(TEXT("EQ_BACK_C1")));
    TestEqual(TEXT("Gift pool order is stable"), CommonPool[2], FName(TEXT("EQ_OUTFIT_C1")));
    TestTrue(TEXT("Grade with no definition yields an empty pool"),
        EmployeeEquipmentRules::BuildRecruitGiftPool(Definitions, EEmployeeEquipmentGrade::Ascendant).IsEmpty());

    // 같은 명령이라도 직원이 다르면 다른 영수증이어야 배치 채용에서 지급이 겹치지 않는다.
    FEmployeeEquipmentRequest First;
    First.Command = EEquipmentCommand::RecruitGift;
    First.OperationId = FGuid::NewGuid();
    First.EmployeeId = 11;
    FEmployeeEquipmentRequest Second = First;
    Second.EmployeeId = 12;
    TestNotEqual(TEXT("Recruit gift fingerprint separates employees"),
        EmployeeEquipmentRules::FingerprintRequest(First), EmployeeEquipmentRules::FingerprintRequest(Second));
    FEmployeeEquipmentRequest OtherCommand = First;
    OtherCommand.Command = EEquipmentCommand::SetSlot;
    TestNotEqual(TEXT("Recruit gift fingerprint separates commands"),
        EmployeeEquipmentRules::FingerprintRequest(First), EmployeeEquipmentRules::FingerprintRequest(OtherCommand));
    return true;
}

#endif
