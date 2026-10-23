#include "Misc/AutomationTest.h"
#include "Data/EmployeeTrainingRules.h"
#include "Data/EmployeeStatsData.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Data/GameSaveData.h"
#include "Manager/EmployeeManager.h"
#include "Engine/GameInstance.h"
#include "UObject/UnrealType.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeTrainingEarnTest,
    "CGR.Employee.Training.EarningDoesNotAutoLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeTrainingEarnTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UEmployeeManager> Employees(NewObject<UEmployeeManager>(GameInstance.Get()));
    FEmployeeInstance Employee;
    Employee.EmployeeID = 1;
    Employee.DisciplinePoints = {0, 5, 1, 0, 1, 1};
    Employee.Stats.ExpGain = 10;
    FEmployeeInstance Other = Employee;
    Other.EmployeeID = 2;
    Employees->SetEmployeeList({Employee, Other});
    const double Expected = static_cast<double>(1000.f * Employees->GetExpGainMultiplier(Employee));
    TestTrue(TEXT("Earn project experience"), Employees->AddExperience(1, 1000.f));
    TestEqual(TEXT("Earned XP belongs to the target employee"), Employees->GetExperience(1), Expected);
    TestEqual(TEXT("Another employee receives no XP"), Employees->GetExperience(2), 0.0);
    const FDoubleProperty* ExperienceProperty = FindFProperty<FDoubleProperty>(
        FEmployeeInstance::StaticStruct(), TEXT("Experience"));
    if (TestNotNull(TEXT("Personal XP is stored as double"), ExperienceProperty))
    {
        TestTrue(TEXT("Personal XP participates in save serialization"),
            ExperienceProperty->HasAnyPropertyFlags(CPF_SaveGame));
    }
    TestEqual(TEXT("Earning never automatically levels employee"), Employees->FindEmployee(1)->Level, 1);
    TestEqual(TEXT("Earning never changes work power"), Employees->FindEmployee(1)->DisciplinePoints[1], 5);
    const FIntProperty* StatPointsProperty = FindFProperty<FIntProperty>(
        FEmployeeInstance::StaticStruct(), TEXT("UnspentStatPoints"));
    if (TestNotNull(TEXT("Employee SP is an integer field"), StatPointsProperty))
    {
        TestTrue(TEXT("Employee SP participates in save serialization"),
            StatPointsProperty->HasAnyPropertyFlags(CPF_SaveGame));
        TestEqual(TEXT("Earning XP does not award SP"),
            StatPointsProperty->GetPropertyValue_InContainer(Employees->FindEmployee(1)), 0);
    }
    TestFalse(TEXT("Missing employee cannot generate XP"), Employees->AddExperience(9, 1000.f));
    TestFalse(TEXT("Nonfinite base grant is rejected"), Employees->AddExperience(1, std::numeric_limits<float>::quiet_NaN()));
    TestFalse(TEXT("Negative resolved grant is rejected"), Employees->CommitResolvedExperience(1, -1.0));
    TestFalse(TEXT("Invalid resolved grant cannot poison employee XP"), Employees->CommitResolvedExperience(1, std::numeric_limits<double>::infinity()));
    TestEqual(TEXT("Invalid grant preserves XP"), Employees->GetExperience(1), Expected);
    TestFalse(TEXT("Level-up without save service fails"), Employees->LevelUpEmployee(1));
    TestEqual(TEXT("Failed operation retains XP"), Employees->GetExperience(1), Expected);
    Employees->FindEmployee(1)->UnspentStatPoints = 1;
    TestFalse(TEXT("Allocation without save service fails"),
        Employees->AllocateStatPoint(1, EEmployeeStatIndex::ExpGain));
    TestEqual(TEXT("Failed allocation retains SP"), Employees->GetAvailableStatPoints(1), 1);
    TestEqual(TEXT("Failed allocation retains selected stat"), Employees->FindEmployee(1)->Stats.ExpGain, 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeTrainingSpendTest,
    "CGR.Employee.Training.PersonalSpend", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeTrainingSpendTest::RunTest(const FString& Parameters)
{
    FEmployeeInstance First;
    First.DisciplinePoints = {1, 5, 0, 0, 1, 1};
    FEmployeeInstance Second = First;
    First.Experience = 450.0;
    const FIntProperty* StatPointsProperty = FindFProperty<FIntProperty>(
        FEmployeeInstance::StaticStruct(), TEXT("UnspentStatPoints"));
    if (StatPointsProperty)
    {
        TestEqual(TEXT("New employees start with no spendable SP"),
            StatPointsProperty->GetPropertyValue_InContainer(&First), 0);
    }
    TestTrue(TEXT("Train first employee"), EmployeeTrainingRules::TryLevelUp(First));
    if (TestNotNull(TEXT("Level-up has employee SP storage"), StatPointsProperty))
    {
        TestEqual(TEXT("A successful manual level-up awards two SP"),
            StatPointsProperty->GetPropertyValue_InContainer(&First), EmployeeTrainingRules::StatPointsPerLevel);
    }
    TestEqual(TEXT("Exactly one level"), First.Level, 2);
    TestEqual(TEXT("Primary work power grows"), First.DisciplinePoints[1], 8);
    TestEqual(TEXT("Other disciplines stay innate"), First.DisciplinePoints[0], 1);
    TestEqual(TEXT("Personal cost deducted once with excess retained"), First.Experience, 300.0);
    TestFalse(TEXT("Another employee cannot spend first employee XP"), EmployeeTrainingRules::TryLevelUp(Second));
    TestEqual(TEXT("Second employee stays at level one"), Second.Level, 1);
    Second.Experience = 150.0;
    TestTrue(TEXT("Second employee can spend own XP"), EmployeeTrainingRules::TryLevelUp(Second));
    TestEqual(TEXT("Second employee gains one level"), Second.Level, 2);
    TestEqual(TEXT("Second spend does not touch first XP"), First.Experience, 300.0);
    if (StatPointsProperty)
    {
        TestEqual(TEXT("Second employee receives their own SP"),
            StatPointsProperty->GetPropertyValue_InContainer(&Second), EmployeeTrainingRules::StatPointsPerLevel);
        TestEqual(TEXT("Training another employee leaves first SP unchanged"),
            StatPointsProperty->GetPropertyValue_InContainer(&First), EmployeeTrainingRules::StatPointsPerLevel);
    }
    TestTrue(TEXT("Exact next-level cost can be spent"), EmployeeTrainingRules::TryLevelUp(First));
    TestEqual(TEXT("Exact spend leaves zero XP"), First.Experience, 0.0);
    TestFalse(TEXT("Insufficient personal XP cannot buy next level"), EmployeeTrainingRules::TryLevelUp(First));
    TestEqual(TEXT("Failed spend preserves XP"), First.Experience, 0.0);
    TestEqual(TEXT("Failed spend preserves work power"), First.DisciplinePoints[1], 11);
    if (StatPointsProperty)
    {
        TestEqual(TEXT("Failed level-up awards no SP"),
            StatPointsProperty->GetPropertyValue_InContainer(&First), EmployeeTrainingRules::StatPointsPerLevel * 2);
    }
    First.Level = EmployeeTrainingRules::MaxLevel;
    First.Experience = 1e9;
    TestFalse(TEXT("Maximum level never spends"), EmployeeTrainingRules::TryLevelUp(First));
    TestEqual(TEXT("Maximum level preserves XP"), First.Experience, 1e9);
    First.Level = 1;
    First.Experience = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Nonfinite XP cannot train"), EmployeeTrainingRules::TryLevelUp(First));
    First.Experience = 150.0;
    First.DisciplinePoints[1] = MAX_int32;
    TestFalse(TEXT("Corrupt work power cannot overflow"), EmployeeTrainingRules::TryLevelUp(First));
    First.DisciplinePoints.Empty();
    TestFalse(TEXT("Invalid profile cannot spend"), EmployeeTrainingRules::TryLevelUp(First));
    if (StatPointsProperty)
    {
        First.DisciplinePoints = {1, 5, 0, 0, 1, 1};
        StatPointsProperty->SetPropertyValue_InContainer(&First, MAX_int32);
        TestFalse(TEXT("SP overflow cannot spend personal XP"), EmployeeTrainingRules::TryLevelUp(First));
        TestEqual(TEXT("SP overflow leaves personal XP unchanged"), First.Experience, 150.0);
        TestEqual(TEXT("SP overflow leaves level unchanged"), First.Level, 1);
        StatPointsProperty->SetPropertyValue_InContainer(&First, MAX_int32 - 1);
        TestFalse(TEXT("Two-SP reward cannot overflow"), EmployeeTrainingRules::TryLevelUp(First));
        StatPointsProperty->SetPropertyValue_InContainer(&First, MAX_int32 - EmployeeTrainingRules::StatPointsPerLevel);
        TestTrue(TEXT("Two-SP reward may reach exact int32 maximum"), EmployeeTrainingRules::TryLevelUp(First));
        TestEqual(TEXT("Exact SP boundary"), StatPointsProperty->GetPropertyValue_InContainer(&First), MAX_int32);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeTrainingRollbackEarnedXPTest,
    "CGR.Employee.Training.RollbackPreservesEarnedXP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeTrainingRollbackEarnedXPTest::RunTest(const FString& Parameters)
{
    FEmployeeInstance Employee;
    Employee.DisciplinePoints = {1, 5, 0, 0, 1, 1};
    Employee.Experience = 170.0;
    const FEmployeeInstance BeforeLevelUp = Employee;
    TestTrue(TEXT("Prepare a manual level-up"), EmployeeTrainingRules::TryLevelUp(Employee));
    const double ExperienceAfterSpend = Employee.Experience;
    Employee.Experience += 7.5;
    EmployeeTrainingRules::RollbackPreservingEarnedExperience(Employee, BeforeLevelUp, ExperienceAfterSpend);
    TestEqual(TEXT("Failed save restores level"), Employee.Level, 1);
    TestEqual(TEXT("Failed save restores primary discipline"), Employee.DisciplinePoints[1], 5);
    TestEqual(TEXT("Failed save restores SP"), Employee.UnspentStatPoints, 0);
    TestEqual(TEXT("Failed save refunds cost and retains newly earned XP"), Employee.Experience, 177.5);

    Employee.UnspentStatPoints = 2;
    const FEmployeeInstance BeforeStatSpend = Employee;
    TestTrue(TEXT("Prepare an SP purchase"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::WorkSpeed));
    const double ExperienceAfterStatSpend = Employee.Experience;
    Employee.Experience += 3.25;
    EmployeeTrainingRules::RollbackPreservingEarnedExperience(Employee, BeforeStatSpend, ExperienceAfterStatSpend);
    TestEqual(TEXT("Failed save restores SP after stat purchase"), Employee.UnspentStatPoints, 2);
    TestEqual(TEXT("Failed save restores stat after stat purchase"), Employee.Stats.WorkSpeed, BeforeStatSpend.Stats.WorkSpeed);
    TestEqual(TEXT("Failed save retains XP earned during SP save"), Employee.Experience, 180.75);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeManufacturingDisciplineTest,
    "CGR.Employee.Training.ManufacturingThreeDisciplines", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeManufacturingDisciplineTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Electronics has three fields"), GetDisciplineCountForCompany(ECompanyType::Electronics), 3);
    TestEqual(TEXT("Automobile has three fields"), GetDisciplineCountForCompany(ECompanyType::Automobile), 3);
    TestEqual(TEXT("Game keeps six fields"), GetDisciplineCountForCompany(ECompanyType::Game), 6);

    FEmployeeInstance Employee;
    Employee.DisciplinePoints = MakeDisciplineProfile(EProductionDiscipline::Graphics, ELootBoxRarity::Common, 3);
    TestEqual(TEXT("Manufacturing hire stores only three fields"), Employee.DisciplinePoints.Num(), 3);
    TestEqual(TEXT("Manufacturing primary is the selected field"), GetPrimaryDisciplineSlot(Employee), 2);
    Employee.Experience = 160.0;
    const int32 BeforeTraining = Employee.DisciplinePoints[2];
    TestTrue(TEXT("Manufacturing employee may train"), EmployeeTrainingRules::TryLevelUp(Employee));
    TestEqual(TEXT("Training adds three to manufacturing primary"), Employee.DisciplinePoints[2], BeforeTraining + 3);
    TestEqual(TEXT("Manufacturing employee retains excess personal XP"), Employee.Experience, 10.0);
    TestEqual(TEXT("Quality field derives QA department"), Employee.Department, EEmployeeDepartment::QA);

    auto SumPoints = [](const FEmployeeInstance& Value)
    {
        int64 Sum = 0;
        for (const int32 Points : Value.DisciplinePoints) { Sum += Points; }
        return Sum;
    };
    const int64 Total = SumPoints(Employee);
    TestTrue(TEXT("Move to project reshapes to six fields"), ReshapeDisciplineProfile(Employee, 6));
    TestEqual(TEXT("Project transfer has six stored fields"), Employee.DisciplinePoints.Num(), 6);
    TestEqual(TEXT("Transfer preserves all earned points"), SumPoints(Employee), Total);
    TestTrue(TEXT("Move back to manufacturing reshapes to three fields"), ReshapeDisciplineProfile(Employee, 3));
    TestEqual(TEXT("Manufacturing return has three stored fields"), Employee.DisciplinePoints.Num(), 3);
    TestEqual(TEXT("Return preserves all earned points"), SumPoints(Employee), Total);
    TestEqual(TEXT("Return keeps mapped primary field"), GetPrimaryDisciplineSlot(Employee), 2);

    FEmployeeInstance ProjectEmployee;
    ProjectEmployee.DisciplinePoints = {1, 2, 1, 0, 1, 9};
    const int64 ProjectTotal = SumPoints(ProjectEmployee);
    TestTrue(TEXT("Six-field project profile moves to manufacturing"), ReshapeDisciplineProfile(ProjectEmployee, 3));
    TestEqual(TEXT("Project to manufacturing has three stored fields"), ProjectEmployee.DisciplinePoints.Num(), 3);
    TestEqual(TEXT("Project to manufacturing preserves total"), SumPoints(ProjectEmployee), ProjectTotal);
    TestEqual(TEXT("Project primary maps by modulo three"), GetPrimaryDisciplineSlot(ProjectEmployee), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeStatAllocationTest,
    "CGR.Employee.Training.StatAllocation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeStatAllocationTest::RunTest(const FString& Parameters)
{
    FEmployeeInstance Employee;
    Employee.UnspentStatPoints = 1;
    Employee.Stats.WorkSpeed = 4;
    Employee.Stats.CritChance = 5;
    Employee.Stats.Composure = 6;
    Employee.Stats.ExpGain = 7;
    Employee.Stats.Stamina = 8;
    Employee.Stats.CritDamage = 9;

    TestEqual(TEXT("Six base stats are available"), static_cast<int32>(EEmployeeStatIndex::Count), 6);
    TestEqual(TEXT("Critical performance is indexed"),
        UEmployeeStatsHelper::GetStatValueByIndex(Employee.Stats, static_cast<uint8>(EEmployeeStatIndex::CritDamage)), 9);
    TestEqual(TEXT("Overall includes critical performance"), UEmployeeStatsHelper::CalculateOverall(Employee.Stats), 39);
    const TArray<int32> SpawnProfile = MakeStatProfile();
    TestEqual(TEXT("Spawn profile contains six stats"), SpawnProfile.Num(), 6);
    int32 SpawnTotal = 0;
    for (int32 Value : SpawnProfile)
    {
        TestTrue(TEXT("Every spawned stat has minimum three"), Value >= 3);
        SpawnTotal += Value;
    }
    TestEqual(TEXT("Spawn profile retains total twenty five"), SpawnTotal, 25);
    FEmployeeStats SpawnStats;
    ApplyStatProfile(SpawnStats, SpawnProfile);
    TestEqual(TEXT("Sixth spawn value maps to critical performance"), SpawnStats.CritDamage, SpawnProfile[5]);

    TestFalse(TEXT("Invalid stat index cannot spend"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::Count));
    TestEqual(TEXT("Invalid stat leaves SP unchanged"), Employee.UnspentStatPoints, 1);
    TestTrue(TEXT("Allocate one point to selected base stat"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::ExpGain));
    TestEqual(TEXT("Only selected base stat increases"), Employee.Stats.ExpGain, 8);
    TestEqual(TEXT("WorkSpeed remains unchanged"), Employee.Stats.WorkSpeed, 4);
    TestEqual(TEXT("CritChance remains unchanged"), Employee.Stats.CritChance, 5);
    TestEqual(TEXT("Composure remains unchanged"), Employee.Stats.Composure, 6);
    TestEqual(TEXT("Stamina remains unchanged"), Employee.Stats.Stamina, 8);
    TestEqual(TEXT("Critical performance remains unchanged"), Employee.Stats.CritDamage, 9);
    TestEqual(TEXT("One SP consumed"), Employee.UnspentStatPoints, 0);
    TestFalse(TEXT("Zero SP cannot allocate again"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::ExpGain));
    TestEqual(TEXT("Insufficient SP leaves selected stat unchanged"), Employee.Stats.ExpGain, 8);

    Employee.UnspentStatPoints = 1;
    Employee.Stats.WorkSpeed = MAX_int32;
    TestFalse(TEXT("Base stat cannot overflow"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::WorkSpeed));
    TestEqual(TEXT("Overflow leaves SP unchanged"), Employee.UnspentStatPoints, 1);
    TestEqual(TEXT("Overflow leaves stat unchanged"), Employee.Stats.WorkSpeed, MAX_int32);

    Employee.EnhancementLevel = 1;
    Employee.Stats.ExpGain = MAX_int32 - 5;
    TestFalse(TEXT("Allocation cannot overflow effective stat after star bonus"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::ExpGain));
    TestEqual(TEXT("Effective overflow leaves SP unchanged"), Employee.UnspentStatPoints, 1);
    TestEqual(TEXT("Effective overflow leaves base stat unchanged"), Employee.Stats.ExpGain, MAX_int32 - 5);
    Employee.Stats.ExpGain = MAX_int32 - 6;
    TestTrue(TEXT("Allocation may reach exact effective int32 maximum"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::ExpGain));
    TestEqual(TEXT("Exact boundary consumes one SP"), Employee.UnspentStatPoints, 0);
    TestEqual(TEXT("Exact boundary base value increases"), Employee.Stats.ExpGain, MAX_int32 - 5);

    Employee.UnspentStatPoints = 1;
    TestTrue(TEXT("SP invests in sixth stat"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::CritDamage));
    TestEqual(TEXT("Sixth stat increases once"), Employee.Stats.CritDamage, 10);
    TestEqual(TEXT("Sixth stat investment spends one SP"), Employee.UnspentStatPoints, 0);
    TestFalse(TEXT("Sixth stat cannot spend without SP"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::CritDamage));
    TestEqual(TEXT("Sixth stat stays unchanged without SP"), Employee.Stats.CritDamage, 10);
    Employee.UnspentStatPoints = 1;
    Employee.Stats.CritDamage = MAX_int32 - 5;
    TestFalse(TEXT("Sixth effective stat cannot overflow after star bonus"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::CritDamage));
    TestEqual(TEXT("Sixth stat overflow preserves SP"), Employee.UnspentStatPoints, 1);
    Employee.Stats.WorkSpeed = 4;
    Employee.Stats.ExpGain = 8;
    Employee.Stats.CritDamage = 10;
    TestEqual(TEXT("Effective overall counts sixth star bonus"),
        UEmployeeStatsHelper::CalculateEffectiveOverall(Employee.Stats, 1),
        UEmployeeStatsHelper::CalculateOverall(Employee.Stats) + 6 * UEmployeeTypeHelper::GetEnhanceStatBonus(1));
    TestTrue(TEXT("Base critical multiplier includes stat and potential additively"),
        FMath::IsNearlyEqual(EmployeeStatTuning::CriticalMultiplier(10, 0.5f), 2.6f));
    TestEqual(TEXT("Sixth stat uses multiplier felt unit"),
        UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(EEmployeeStatIndex::CritDamage), 10).ToString(),
        FString(TEXT("2.10배")));
    TestEqual(TEXT("Sixth stat delta uses multiplier felt unit"),
        UEmployeeStatsHelper::GetStatFeltDeltaText(static_cast<uint8>(EEmployeeStatIndex::CritDamage), 10, 15).ToString(),
        FString(TEXT("+0.05배")));

    TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UEmployeeManager> Employees(NewObject<UEmployeeManager>(GameInstance.Get()));
    Employee.UnspentStatPoints = 1;
    Employee.Stats.WorkSpeed = MAX_int32;
    Employee.EmployeeID = 1;
    FEmployeeInstance Other = Employee;
    Other.EmployeeID = 2;
    Other.Stats.WorkSpeed = 4;
    Employees->SetEmployeeList({Employee, Other});
    TestEqual(TEXT("Employee SP query is scoped by ID"), Employees->GetAvailableStatPoints(2), 1);
    TestEqual(TEXT("Missing employee has no SP"), Employees->GetAvailableStatPoints(9), 0);
    TestFalse(TEXT("Overflow blocks allocation for target employee"),
        Employees->CanAllocateStatPoint(1, EEmployeeStatIndex::WorkSpeed));
    TestTrue(TEXT("Other employee remains allocatable"),
        Employees->CanAllocateStatPoint(2, EEmployeeStatIndex::WorkSpeed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeTrainingSaveTest,
    "CGR.Employee.Training.SaveRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeTrainingSaveTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USaveGame_GameData> Save(NewObject<USaveGame_GameData>());
    FGameSaveData& Original = Save->GameData;
    FEmployeeInstance Employee;
    Employee.EmployeeID = 7;
    Employee.Experience = 1234.5;
    Employee.DisciplinePoints = {2, 5, 0, 0, 0, 1};
    TestTrue(TEXT("Spend employee XP before saving"), EmployeeTrainingRules::TryLevelUp(Employee));
    Original.EmployeeList.Add(Employee);
    Original.OfficeDataMap.FindOrAdd(3).EmployeeList.Add(Employee);
    FEmployeeInstance Spent = Employee;
    Spent.EmployeeID = 8;
    TestTrue(TEXT("Spend saved employee's SP"),
        EmployeeTrainingRules::TryAllocateStatPoint(Spent, EEmployeeStatIndex::CritDamage));
    Original.OfficeDataMap.FindChecked(3).EmployeeList.Add(Spent);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Serialize real save format"), UGameplayStatics::SaveGameToMemory(Save.Get(), Bytes))) return false;
    TStrongObjectPtr<USaveGame_GameData> RestoredSave(Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    if (!TestNotNull(TEXT("Deserialize real save format"), RestoredSave.Get())) return false;
    const FGameSaveData& Loaded = RestoredSave->GameData;
    TestEqual(TEXT("Roster personal XP survives real save round trip"), Loaded.EmployeeList[0].Experience, 1084.5);
    const FEmployeeInstance& Restored = Loaded.OfficeDataMap.FindChecked(3).EmployeeList[0];
    TestEqual(TEXT("Personal XP restores with fractions"), Restored.Experience, 1084.5);
    const FIntProperty* StatPointsProperty = FindFProperty<FIntProperty>(
        FEmployeeInstance::StaticStruct(), TEXT("UnspentStatPoints"));
    if (TestNotNull(TEXT("Saved employee SP is present"), StatPointsProperty))
    {
        TestEqual(TEXT("Unspent SP survives real save round trip"),
            StatPointsProperty->GetPropertyValue_InContainer(&Restored), EmployeeTrainingRules::StatPointsPerLevel);
    }
    TestEqual(TEXT("Selected employee level restores"), Restored.Level, 2);
    TestEqual(TEXT("Primary growth restores"), Restored.DisciplinePoints[1], 8);
    const FEmployeeInstance& RestoredSpent = Loaded.OfficeDataMap.FindChecked(3).EmployeeList[1];
    TestEqual(TEXT("Spent SP remains spent after load"), RestoredSpent.UnspentStatPoints, EmployeeTrainingRules::StatPointsPerLevel - 1);
    TestEqual(TEXT("Allocated sixth base stat survives save and load"), RestoredSpent.Stats.CritDamage, 1);
    const FArrayProperty* StatLevelsProperty = FindFProperty<FArrayProperty>(
        FEmployeeInstance::StaticStruct(), TEXT("StatLevels"));
    if (TestNotNull(TEXT("Stat levels are an array field"), StatLevelsProperty))
    {
        TestTrue(TEXT("Stat levels participate in save serialization"), StatLevelsProperty->HasAnyPropertyFlags(CPF_SaveGame));
    }
    TestEqual(TEXT("Invested stat level survives save and load"),
        EmployeeTrainingRules::GetStatLevel(RestoredSpent, EEmployeeStatIndex::CritDamage), 1);
    TestEqual(TEXT("Uninvested stat level stays zero after load"),
        EmployeeTrainingRules::GetStatLevel(RestoredSpent, EEmployeeStatIndex::WorkSpeed), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeStatLevelCostTest,
    "CGR.Employee.Training.StatLevelCost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeStatLevelCostTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Level-up grants two SP"), EmployeeTrainingRules::StatPointsPerLevel, 2);
    TestEqual(TEXT("Lv.0 costs one SP"), EmployeeTrainingRules::StatPointCost(0), 1);
    TestEqual(TEXT("Lv.9 costs one SP"), EmployeeTrainingRules::StatPointCost(9), 1);
    TestEqual(TEXT("Lv.10 costs two SP"), EmployeeTrainingRules::StatPointCost(10), 2);
    TestEqual(TEXT("Lv.19 costs two SP"), EmployeeTrainingRules::StatPointCost(19), 2);
    TestEqual(TEXT("Lv.20 costs three SP"), EmployeeTrainingRules::StatPointCost(20), 3);
    TestEqual(TEXT("Corrupt negative level costs one SP"), EmployeeTrainingRules::StatPointCost(-4), 1);

    FEmployeeInstance Employee;
    Employee.Stats.WorkSpeed = 7;
    Employee.Stats.CritChance = 5;
    TestEqual(TEXT("Spawned stat values are not invested levels"),
        EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::WorkSpeed), 0);
    TestEqual(TEXT("Empty level array reads as Lv.0"),
        EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::CritDamage), 0);

    Employee.StatLevels.SetNumZeroed(static_cast<int32>(EEmployeeStatIndex::Count));
    Employee.StatLevels[static_cast<int32>(EEmployeeStatIndex::WorkSpeed)] = 9;
    Employee.StatLevels[static_cast<int32>(EEmployeeStatIndex::CritChance)] = 20;
    Employee.UnspentStatPoints = 3;
    TestTrue(TEXT("Lv.9 stat costs one SP"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::WorkSpeed));
    TestEqual(TEXT("Lv.9 investment spends one SP"), Employee.UnspentStatPoints, 2);
    TestEqual(TEXT("Investment raises stat level"),
        EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::WorkSpeed), 10);
    TestEqual(TEXT("Investment still raises stored stat by one"), Employee.Stats.WorkSpeed, 8);
    TestEqual(TEXT("Lv.10 next cost is two SP"),
        EmployeeTrainingRules::StatPointCost(EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::WorkSpeed)), 2);
    TestFalse(TEXT("Two SP cannot buy a Lv.20 stat"),
        EmployeeTrainingRules::CanAllocateStatPoint(Employee, EEmployeeStatIndex::CritChance));
    TestFalse(TEXT("Partial SP cannot spend on Lv.20 stat"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::CritChance));
    TestEqual(TEXT("Partial SP failure keeps SP"), Employee.UnspentStatPoints, 2);
    TestEqual(TEXT("Partial SP failure keeps level"),
        EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::CritChance), 20);
    TestEqual(TEXT("Partial SP failure keeps stat value"), Employee.Stats.CritChance, 5);

    Employee.UnspentStatPoints = 1;
    TestFalse(TEXT("One SP cannot buy a Lv.10 stat"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::WorkSpeed));
    TestEqual(TEXT("Insufficient SP keeps Lv.10"), EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::WorkSpeed), 10);
    TestEqual(TEXT("Insufficient SP keeps stored stat"), Employee.Stats.WorkSpeed, 8);
    TestTrue(TEXT("One SP still buys a cheap stat"), EmployeeTrainingRules::CanAllocateAnyStatPoint(Employee));
    for (int32 Index = 0; Index < static_cast<int32>(EEmployeeStatIndex::Count); ++Index)
    {
        Employee.StatLevels[Index] = FMath::Max(Employee.StatLevels[Index], 10);
    }
    TestFalse(TEXT("No affordable stat when every cost exceeds SP"), EmployeeTrainingRules::CanAllocateAnyStatPoint(Employee));

    Employee.UnspentStatPoints = 3;
    TestTrue(TEXT("Three SP buy a Lv.20 stat"),
        EmployeeTrainingRules::TryAllocateStatPoint(Employee, EEmployeeStatIndex::CritChance));
    TestEqual(TEXT("Lv.20 investment spends three SP"), Employee.UnspentStatPoints, 0);
    TestEqual(TEXT("Lv.20 becomes Lv.21"), EmployeeTrainingRules::GetStatLevel(Employee, EEmployeeStatIndex::CritChance), 21);
    TestFalse(TEXT("Zero SP buys nothing"), EmployeeTrainingRules::CanAllocateAnyStatPoint(Employee));

    FEmployeeInstance Trainee;
    Trainee.DisciplinePoints = {1, 5, 0, 0, 1, 1};
    Trainee.Experience = 150.0;
    TestTrue(TEXT("Level-up succeeds"), EmployeeTrainingRules::TryLevelUp(Trainee));
    TestEqual(TEXT("Level-up grants StatPointsPerLevel"), Trainee.UnspentStatPoints, EmployeeTrainingRules::StatPointsPerLevel);
    TestTrue(TEXT("Fresh SP can buy a stat"), EmployeeTrainingRules::TryAllocateStatPoint(Trainee, EEmployeeStatIndex::Stamina));
    TestEqual(TEXT("First allocation sizes the level array"), Trainee.StatLevels.Num(), static_cast<int32>(EEmployeeStatIndex::Count));
    TestEqual(TEXT("Only the selected stat levels"), EmployeeTrainingRules::GetStatLevel(Trainee, EEmployeeStatIndex::Stamina), 1);
    TestEqual(TEXT("Other stats stay Lv.0"), EmployeeTrainingRules::GetStatLevel(Trainee, EEmployeeStatIndex::ExpGain), 0);

    TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UEmployeeManager> Employees(NewObject<UEmployeeManager>(GameInstance.Get()));
    Employee.EmployeeID = 3;
    Employee.UnspentStatPoints = 2;
    Employees->SetEmployeeList({Employee});
    TestEqual(TEXT("Manager reads stat level by ID"), Employees->GetStatLevel(3, EEmployeeStatIndex::CritChance), 21);
    TestEqual(TEXT("Manager reads next cost by ID"), Employees->GetStatPointCost(3, EEmployeeStatIndex::CritChance), 3);
    TestEqual(TEXT("Manager cost for Lv.10 stat"), Employees->GetStatPointCost(3, EEmployeeStatIndex::WorkSpeed), 2);
    TestEqual(TEXT("Missing employee has no cost"), Employees->GetStatPointCost(9, EEmployeeStatIndex::WorkSpeed), 0);
    TestFalse(TEXT("Manager blocks unaffordable stat"), Employees->CanAllocateStatPoint(3, EEmployeeStatIndex::CritChance));
    TestTrue(TEXT("Manager allows affordable stat"), Employees->CanAllocateStatPoint(3, EEmployeeStatIndex::WorkSpeed));
    TestTrue(TEXT("Manager sees an affordable stat"), Employees->CanAllocateAnyStatPoint(3));
    TestFalse(TEXT("Missing employee cannot allocate"), Employees->CanAllocateAnyStatPoint(9));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeStatFeltReadoutTest,
    "CGR.Employee.Training.FeltRiskAndDrowsiness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEmployeeStatFeltReadoutTest::RunTest(const FString& Parameters)
{
    const uint8 Composure = static_cast<uint8>(EEmployeeStatIndex::Composure);
    const FString BaseRisk = UEmployeeStatsHelper::GetStatFeltText(Composure, 0).ToString();
    const FString StrengthenedRisk = UEmployeeStatsHelper::GetStatFeltText(Composure, 10).ToString();
    const FString RiskDelta = UEmployeeStatsHelper::GetStatFeltDeltaText(Composure, 0, 10).ToString();
    TestTrue(TEXT("Composure shows a one-decimal percentage"), BaseRisk.Contains(TEXT(".")) && BaseRisk.EndsWith(TEXT("%")));
    TestTrue(TEXT("Strengthening lowers the displayed departure chance"), FCString::Atof(*StrengthenedRisk) < FCString::Atof(*BaseRisk));
    TestTrue(TEXT("Composure delta is a negative percentage-point change"), RiskDelta.StartsWith(TEXT("-")) && RiskDelta.EndsWith(TEXT("%p")));
    const FString RiskExplanation = UEmployeeStatsHelper::GetStatEffectText(Composure, 10).ToString();
    TestTrue(TEXT("Composure explanation identifies the eligible ten-second window"),
        RiskExplanation.Contains(TEXT("이탈 판정이 가능한 10초")) && RiskExplanation.Contains(TEXT("상황 보너스 제외")));

    const uint8 Stamina = static_cast<uint8>(EEmployeeStatIndex::Stamina);
    const FString BaseDrowsy = UEmployeeStatsHelper::GetStatFeltText(Stamina, 0).ToString();
    const FString StrengthenedDrowsy = UEmployeeStatsHelper::GetStatFeltText(Stamina, 10).ToString();
    const FString DrowsyDelta = UEmployeeStatsHelper::GetStatFeltDeltaText(Stamina, 0, 10).ToString();
    TestTrue(TEXT("Stamina shows fractional seconds"), BaseDrowsy.Contains(TEXT(".")) && BaseDrowsy.EndsWith(TEXT("초")));
    TestTrue(TEXT("Strengthening visibly increases the drowsiness-free time"),
        StrengthenedDrowsy != BaseDrowsy && FCString::Atof(*StrengthenedDrowsy) > FCString::Atof(*BaseDrowsy));
    TestTrue(TEXT("Stamina delta keeps fractional seconds"), DrowsyDelta.StartsWith(TEXT("+")) && DrowsyDelta.Contains(TEXT(".")) && DrowsyDelta.EndsWith(TEXT("초")));
    const FString DrowsyExplanation = UEmployeeStatsHelper::GetStatEffectText(Stamina, 10).ToString();
    TestTrue(TEXT("Stamina explanation starts from no fatigue"),
        DrowsyExplanation.Contains(TEXT("피로가 없는 상태에서")) && DrowsyExplanation.Contains(TEXT("체력이 높을수록")));
    return true;
}
#endif
