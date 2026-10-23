#include "Misc/AutomationTest.h"
#include "Core/CGGameInstance.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/GameSaveData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Global/CGDevSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Table/IndustryProfileTable.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UI/Panel/OfflineReportModalWidget.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRProjectRevenueGrowthTest,
	"CGR.Operation.ProjectRevenueGrowth",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRProjectRevenueGrowthTest::RunTest(const FString& Parameters)
{
	FIndustryProfileRow Profile;
	Profile.RevenueGrowthPerProject = 1.16;
	Profile.RevenueGrowthStartIndex = 2;

	TestEqual(TEXT("Project 1 base is protected"),
		UProjectOperationManager::ComputeBaseRevenuePerSec(1, Profile), 200.0);
	TestEqual(TEXT("Project 2 base is protected"),
		UProjectOperationManager::ComputeBaseRevenuePerSec(2, Profile), 400.0);
	TestTrue(TEXT("Project 3 grows one step"), FMath::IsNearlyEqual(
		UProjectOperationManager::ComputeBaseRevenuePerSec(3, Profile), 696.0, 0.001));
	TestTrue(TEXT("Project 4 grows two steps"), FMath::IsNearlyEqual(
		UProjectOperationManager::ComputeBaseRevenuePerSec(4, Profile), 1076.48, 0.001));

	Profile.RevenueGrowthPerProject = 0.0;
	TestEqual(TEXT("Malformed profile cannot silently use old revenue"),
		UProjectOperationManager::ComputeBaseRevenuePerSec(4, Profile), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROperationPayoutSplitTest,
	"CGR.Operation.PayoutSplit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROperationPayoutSplitTest::RunTest(const FString& Parameters)
{
	// 정수부만 지급하고 소수부는 버퍼에 남긴다
	const FOperationTickPayout Partial = ResolveOperationTickPayout(false, 0.0, 2.9);
	TestEqual(TEXT("2.9원은 2원만 지급"), Partial.PayoutNow, static_cast<int64>(2));
	TestTrue(TEXT("소수부 0.9 이월"), FMath::IsNearlyEqual(Partial.NewPending, 0.9, 1e-6));

	// 1원 미만은 지급 0, 전액 이월 — 여기서 버리면 저수익 건물이 영원히 0원이 된다
	const FOperationTickPayout Sub = ResolveOperationTickPayout(false, 0.0, 0.3);
	TestEqual(TEXT("0.3원은 지급 없음"), Sub.PayoutNow, static_cast<int64>(0));
	TestTrue(TEXT("0.3 전액 이월"), FMath::IsNearlyEqual(Sub.NewPending, 0.3, 1e-6));

	// 이월 누적으로 결국 지급된다 (0.3원/틱 × 4틱 = 1.2 → 1원)
	double Pending = 0.0;
	int64 Paid = 0;
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		const FOperationTickPayout Step = ResolveOperationTickPayout(false, Pending, 0.3);
		Pending = Step.NewPending;
		Paid += Step.PayoutNow;
	}
	TestEqual(TEXT("4틱 누적 후 1원 지급"), Paid, static_cast<int64>(1));

	// 오피스 안에서는 프로젝트 수익을 지급하지 않는다 (직원 수익이 대신 지급 — 이중 지급 방지)
	const FOperationTickPayout InOffice = ResolveOperationTickPayout(true, 5.0, 100.0);
	TestEqual(TEXT("오피스 안 지급 0"), InOffice.PayoutNow, static_cast<int64>(0));
	TestTrue(TEXT("오피스 안 버퍼 불변"), FMath::IsNearlyEqual(InOffice.NewPending, 5.0, 1e-6));

	// 음수·NaN 방어 — 버퍼를 0으로 정리하고 지급하지 않는다
	const FOperationTickPayout Negative = ResolveOperationTickPayout(false, -3.0, 0.0);
	TestEqual(TEXT("음수 버퍼 지급 0"), Negative.PayoutNow, static_cast<int64>(0));
	TestTrue(TEXT("음수 버퍼 0으로 정리"), FMath::IsNearlyEqual(Negative.NewPending, 0.0, 1e-6));

	// NaN 방어 — /fp:fast 가 비교식 가드를 접으므로 IsFinite(비트 검사)로 막는다. 회귀 시 INT64_MIN 이 나온다.
	const FOperationTickPayout NotANumber = ResolveOperationTickPayout(false, 0.0, std::numeric_limits<double>::quiet_NaN());
	TestEqual(TEXT("NaN 수익 지급 0"), NotANumber.PayoutNow, static_cast<int64>(0));
	TestTrue(TEXT("NaN 버퍼가 유한값으로 정리"), FMath::IsFinite(NotANumber.NewPending));
	TestTrue(TEXT("NaN 버퍼 0"), NotANumber.NewPending == 0.0);

	// Inf 방어 — NaN 과 같은 경로로 INT64_MIN 이 나온다
	// ⚠ 아래 `== 0.0` 은 /fp:fast 에서 NaN 도 true 다. 위 IsFinite 단언을 지우면 이 축이 통째로 사라진다.
	const FOperationTickPayout Infinite = ResolveOperationTickPayout(false, 0.0, std::numeric_limits<double>::infinity());
	TestEqual(TEXT("Inf 수익 지급 0"), Infinite.PayoutNow, static_cast<int64>(0));
	TestTrue(TEXT("Inf 버퍼가 유한값으로 정리"), FMath::IsFinite(Infinite.NewPending));
	TestTrue(TEXT("Inf 버퍼 0"), Infinite.NewPending == 0.0);

	// 유한한 거대값 — IsFinite 를 통과하므로 크기 가드가 따로 없으면 여기서 다시 INT64_MIN 이 나온다
	const FOperationTickPayout Huge = ResolveOperationTickPayout(false, 0.0, 1.0e19);
	TestEqual(TEXT("거대 수익은 2^53 으로 클램프"), Huge.PayoutNow, static_cast<int64>(9007199254740992.0));
	TestTrue(TEXT("거대 수익 버퍼가 유한값"), FMath::IsFinite(Huge.NewPending));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROfflineCollectOnceTest,
	"CGR.Operation.OfflineCollectOnce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROfflineCollectOnceTest::RunTest(const FString& Parameters)
{
	bool bCollected = false;

	// [전체 수거] 경로 — 수거하고 플래그를 세운다
	TestTrue(TEXT("첫 수거는 통과"), ShouldCollectOfflineVault(bCollected));
	bCollected = true;

	// 이어지는 [닫기] 경로 — 이미 수거했으므로 건너뛴다("빈 금고" 토스트·이중 지급 방지)
	TestFalse(TEXT("두 번째 수거는 차단"), ShouldCollectOfflineVault(bCollected));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGROperationLaunchForecastTest,
	"CGR.Operation.LaunchForecastMatchesOperation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGROperationLaunchForecastTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_OperationLaunchForecast")))
	{
		AddWarning(TEXT("Skipped: run with -saveddirsuffix=OperationLaunchForecast to isolate fixture saves"));
		return true;
	}
	if (!TestTrue(TEXT("Engine is available"), GEngine != nullptr)
		|| !TestNull(TEXT("No playable game instance is active"), UCGGameInstance::GetInstance())) return false;

	const FString SlotName(TEXT("GameSlot"));
	if (!TestFalse(TEXT("Isolated fixture slot starts empty"), UGameplayStatics::DoesSaveGameExist(SlotName, 0))) return false;
	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;
	const bool bPreviousWipeOnLaunch = Settings->bWipeSaveOnLaunch;
	Settings->BackendMode = ECGBackendMode::Local;
	Settings->bWipeSaveOnLaunch = false;
	UCGGameInstance* Instance = nullptr;
	ON_SCOPE_EXIT
	{
		if (Instance)
		{
			UWorld* TestWorld = static_cast<UGameInstance*>(Instance)->GetWorld();
			static_cast<UGameInstance*>(Instance)->Shutdown();
			if (TestWorld) GEngine->DestroyWorldContext(TestWorld);
		}
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		Settings->BackendMode = PreviousMode;
		Settings->bWipeSaveOnLaunch = bPreviousWipeOnLaunch;
	};

	const UDataTable* EnhancementTable = LoadObject<UDataTable>(nullptr,
		TEXT("/Game/CompanyGrowth/Table/Building/DT_BuildingEnhancementDefinition.DT_BuildingEnhancementDefinition"));
	if (!TestNotNull(TEXT("Actual enhancement table exists"), EnhancementTable)) return false;
	const FBuildingEnhancementDefinition* GradeDefinition = EnhancementTable->FindRow<FBuildingEnhancementDefinition>(
		TEXT("ProjectGrade"), TEXT("Operation launch forecast fixture"));
	if (!TestNotNull(TEXT("Project grade enhancement exists"), GradeDefinition)) return false;
	if (!TestTrue(TEXT("Grade effect is finite and positive"),
		FMath::IsFinite(GradeDefinition->EffectPerLevel) && GradeDefinition->EffectPerLevel > 0.0f)) return false;
	// A ten-percent quality bonus takes the fixture's raw C score into the B band.
	const double RequiredGradeLevel = FMath::CeilToDouble(0.1 / static_cast<double>(GradeDefinition->EffectPerLevel));
	if (!TestTrue(TEXT("Grade promotion fixture fits the authored upgrade range"),
		RequiredGradeLevel <= MAX_int32 && (GradeDefinition->MaxLevel <= 0 || RequiredGradeLevel <= GradeDefinition->MaxLevel))) return false;
	const int32 GradeLevel = static_cast<int32>(RequiredGradeLevel);
	USaveGame_GameData* Seed = NewObject<USaveGame_GameData>(GetTransientPackage());
	Seed->GameData.bTutorialCompleted = true;
	auto AddBuilding = [Seed, GradeLevel](int32 BuildingID, bool bMarketing, bool bEmployeeIncome)
	{
		FBuildingEntitySaveData Building;
		Building.BuildingIndex = BuildingID;
		Building.BuildingData.CompanyType = ECompanyType::IT;
		Building.BuildingData.EnhancementLevels.Add(EBuildingEnhancementType::ProjectGrade, GradeLevel);
		if (bMarketing)
		{
			Building.BuildingData.EnhancementLevels.Add(EBuildingEnhancementType::MarketingPower, 60);
		}
		Seed->GameData.Buildings.Add(Building);
		FOfficeSaveData& Office = Seed->GameData.OfficeDataMap.Add(BuildingID);
		if (bEmployeeIncome)
		{
			FEmployeeInstance& Employee = Office.EmployeeList.AddDefaulted_GetRef();
			Employee.EmployeeID = BuildingID;
			FPotentialOptionLine& IncomeLine = Employee.PotentialAbility.Options.AddDefaulted_GetRef();
			IncomeLine.OptionType = EPotentialOptionType::IncomeBonus;
			IncomeLine.Value = 25.0f;
		}
	};
	AddBuilding(17, true, true);
	AddBuilding(18, false, false);
	AddBuilding(19, true, false);
	AddBuilding(20, false, true);
	if (!TestTrue(TEXT("Fixture save is written only in isolated slot"), UGameplayStatics::SaveGameToSlot(Seed, SlotName, 0))) return false;

	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	USaveLoadManager* Saves = Instance->GetSubsystem<USaveLoadManager>();
	UProjectOperationManager* Operations = Instance->GetSubsystem<UProjectOperationManager>();
	if (!TestTrue(TEXT("Real operation and save subsystems exist"), Saves && Operations)) return false;
	if (!TestTrue(TEXT("Fixture save loads"), Saves->LoadGameData())) return false;

	FStageProgressData Stage;
	Stage.CompanyType = ECompanyType::IT;
	Stage.ProjectNumber = 4;
	Stage.ProjectID = 4;
	Stage.ProjectName = TEXT("LaunchForecastFixture");
	FStepRoundData& Step = Stage.Steps.AddDefaulted_GetRef();
	Step.DisciplineSlot = 0;
	Step.Weight = 1;
	Step.TargetScore = 100.0f;
	Step.AcquiredScore = 85.0f;
	if (!TestEqual(TEXT("Raw stage quality is C"), Stage.CalculateQualityGrade(), EQualityGrade::C)) return false;

	const FOperationLaunchForecast Base = Operations->GetLaunchForecast(Stage, 18);
	const FOperationLaunchForecast Marketing = Operations->GetLaunchForecast(Stage, 19);
	const FOperationLaunchForecast Employee = Operations->GetLaunchForecast(Stage, 20);
	Stage.EventRewardMultiplier = 1.25f;
	const FOperationLaunchForecast Forecast = Operations->GetLaunchForecast(Stage, 17);
	TestEqual(TEXT("Forecast does not register operations"), Operations->GetActiveOperationCount(), 0);
	TestEqual(TEXT("Grade enhancement promotes C to B"), Forecast.EffectiveGrade, EQualityGrade::B);
	TestTrue(TEXT("Promoted grade extends operation beyond raw C duration"),
		Forecast.OperationSeconds > Operations->ComputeEffectiveOperationTime(EQualityGrade::C, Stage.ProjectNumber, 17));
	TestTrue(TEXT("Marketing multiplier affects forecast"), Marketing.TotalRevenue > Base.TotalRevenue);
	TestTrue(TEXT("Employee income option affects forecast"), Employee.TotalRevenue > Base.TotalRevenue);
	TestTrue(TEXT("Development event multiplier affects forecast"), Forecast.TotalRevenue > Marketing.TotalRevenue);
	TestTrue(TEXT("Effective quality increases forecast over quality-one baseline"), Forecast.TotalRevenue > Forecast.BaseTotalRevenue);

	const int32 OperationIndex = Operations->StartOperation(Stage, 17);
	const FOperationData* Actual = Operations->GetOperationByIndex(OperationIndex);
	if (!TestNotNull(TEXT("StartOperation registers the operation"), Actual)) return false;
	TestEqual(TEXT("StartOperation registers exactly one operation"), Operations->GetActiveOperationCount(), 1);
	TestEqual(TEXT("Forecast grade equals launched grade"), Forecast.EffectiveGrade, Actual->QualityGrade);
	TestTrue(TEXT("Forecast quality revenue multiplier is retained in launched operation"),
		FMath::IsNearlyEqual(Forecast.QualityMultiplier, Actual->QualityRevenueMultiplier));
	TestTrue(TEXT("Forecast duration equals launched duration"), FMath::IsNearlyEqual(Forecast.OperationSeconds, Actual->TotalOperationTime));
	TestTrue(TEXT("Forecast integral uses launched peak and decay"), FMath::IsNearlyEqual(
		Forecast.TotalRevenue,
		UProjectOperationManager::ComputeLaunchRevenue(Actual->ActualRevenuePerSecond, 1.0f, 1.0f,
			Actual->TotalOperationTime, Actual->HalfLifeFrac), 0.1f));

	UQuestSubsystem* Quests = Instance->GetSubsystem<UQuestSubsystem>();
	if (!TestNotNull(TEXT("Real lifetime quest ledger exists"), Quests)) return false;
	USaveGame_GameData* CurrentSave = Saves->GetCurrentSaveData();
	if (!TestNotNull(TEXT("Launch statistics use the isolated save"), CurrentSave)) return false;
	TestEqual(TEXT("First launch increases absolute project statistic"), CurrentSave->GameData.TotalProjectsCompleted, 1);
	TArray<FQuestEntry> LifetimeEntries;
	Quests->GetEntries(EQuestPeriod::Lifetime, LifetimeEntries);
	auto FindLifetimeCurrent = [&LifetimeEntries](FName QuestID)
	{
		for (const FQuestEntry& Entry : LifetimeEntries)
		{
			if (Entry.QuestID == QuestID) return Entry.Current;
		}
		return static_cast<int64>(-1);
	};
	TestEqual(TEXT("Launch achievement reads the first launch"), FindLifetimeCurrent(TEXT("A_Launch_1")), static_cast<int64>(1));
	Operations->AddEmployeeRevenue(17, 1000000000000000.0);
	Operations->AddEmployeeRevenue(17, 3000.25);
	TestEqual(TEXT("Small employee receipts survive a large accumulated operation total"),
		Actual->TotalRevenueEarned, 1000000000003000.25);
	Operations->UpdateOperations(Actual->TotalOperationTime);
	const FProjectReportData* Report = Operations->GetPendingReport(17);
	if (!TestNotNull(TEXT("Completed operation produces a report"), Report)) return false;
	TestTrue(TEXT("The completed report retains the large accumulated total and its top-up"),
		Report->TotalRevenueEarned >= 1000000000003000.25);
	TestTrue(TEXT("Report retains the launch-confirmed quality revenue multiplier"),
		FMath::IsNearlyEqual(Forecast.QualityMultiplier, Report->QualityRevenueMultiplier));

	Step.AcquiredScore = 200.0f;
	Operations->StartOperation(Stage, 17);
	CurrentSave = Saves->GetCurrentSaveData();
	TestEqual(TEXT("Second launch increases absolute project statistic"), CurrentSave->GameData.TotalProjectsCompleted, 2);
	TestEqual(TEXT("Launched S grade increases absolute S statistic"), CurrentSave->GameData.SGradeProjectsCompleted, 1);
	LifetimeEntries.Reset();
	Quests->GetEntries(EQuestPeriod::Lifetime, LifetimeEntries);
	TestEqual(TEXT("S achievement reads the launched grade"), FindLifetimeCurrent(TEXT("A_SGrade_1")), static_cast<int64>(1));
	return true;
}

#endif
