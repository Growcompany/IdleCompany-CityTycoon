#include "Misc/AutomationTest.h"
#include "Utils/TutorialDisciplineSeed.h"
#include "Table/MissionTable.h"
#include "Table/ProjectDataTable.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTutorialDisciplineSeedTest, "CGR.Tutorial.DisciplineSeed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTutorialDisciplineSeedTest::RunTest(const FString& Parameters)
{
	const UDataTable* Missions = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/Mission/DT_Mission.DT_Mission"));
	const UDataTable* Projects = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/Company/DT_Project_Game.DT_Project_Game"));
	if (!TestNotNull(TEXT("Mission data"), Missions) || !TestNotNull(TEXT("Project data"), Projects)) return false;
	const FMissionTable* Recruit = Missions->FindRow<FMissionTable>(TEXT("M6_FirstRecruit"), TEXT("TutorialSeedTest"));
	const FMissionTable* Launch = Missions->FindRow<FMissionTable>(TEXT("M7_InHouseProject"), TEXT("TutorialSeedTest"));
	if (!TestNotNull(TEXT("Recruit mission"), Recruit) || !TestNotNull(TEXT("Launch mission"), Launch)) return false;
	TestTrue(TEXT("First recruit has a specific discipline"), Recruit->RequiredRecruitDiscipline != EProductionDiscipline::Count);
	const auto Seed = TutorialDisciplineSeed::BuildSeedList(Recruit->RequiredRecruitDiscipline, static_cast<int32>(Recruit->ConditionAmount));
	TestEqual(TEXT("Seed covers the configured reach target"), static_cast<int64>(Seed.Num()), Recruit->ConditionAmount);
	TArray<FProjectData*> Rows;
	Projects->GetAllRows(TEXT("TutorialSeedTest"), Rows);
	const FProjectData* First = nullptr;
	for (const FProjectData* Row : Rows) if (Row->ProjectIndex == Launch->RequiredProjectIndex) First = Row;
	if (!TestNotNull(TEXT("Configured first project exists"), First)) return false;
	const int32 Weights[] = {First->Weight_Plan, First->Weight_Dev, First->Weight_Graphics, First->Weight_Sound, First->Weight_Server, First->Weight_QA};
	int32 ActiveCount = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Weights); ++Index)
	{
		if (Weights[Index] <= 0) continue;
		++ActiveCount;
		for (EProductionDiscipline Discipline : Seed)
			TestEqual(TEXT("Every tutorial recruit specializes in the first project's required work"), static_cast<int32>(Discipline), Index);
	}
	TestEqual(TEXT("First project teaches one discipline"), ActiveCount, 1);
	TestTrue(TEXT("Unrestricted missions do not seed random recruitment"), TutorialDisciplineSeed::BuildSeedList(EProductionDiscipline::Count, 2).IsEmpty());
	return true;
}
#endif
