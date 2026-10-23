#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/SideFusionTypes.h"
#include "Table/FusionProjectTable.h"

namespace
{
	FFusionProjectTable MakeFusionRow(TArray<ECompanyType> Industries = { ECompanyType::Game, ECompanyType::IT })
	{
		FFusionProjectTable Row;
		Row.DisplayName = FText::FromString(TEXT("Test"));
		Row.RequiredIndustries = MoveTemp(Industries);
		Row.BaseRequirement = 300.0f;
		Row.DurationSeconds = 14400;
		Row.BaseMarketCap = 80000000;
		return Row;
	}

	FString JoinKeys(const TArray<FName>& Keys)
	{
		TArray<FString> Parts;
		for (FName Key : Keys) Parts.Add(Key.ToString());
		return FString::Join(Parts, TEXT(","));
	}

	// DT_FusionProject 실제 7행의 요구 업종. 키·순서는 DisplayOrder 0~6 그대로다.
	TArray<FName> ShippedOrder()
	{
		return { TEXT("game_platform"), TEXT("fintech"), TEXT("console"),
			TEXT("smarthome"), TEXT("autodrive"), TEXT("autosemi"), TEXT("smartcar") };
	}

	TMap<FName, FFusionProjectTable> ShippedRows()
	{
		TMap<FName, FFusionProjectTable> Rows;
		Rows.Add(TEXT("game_platform"), MakeFusionRow({ ECompanyType::Game, ECompanyType::IT }));
		Rows.Add(TEXT("fintech"), MakeFusionRow({ ECompanyType::IT, ECompanyType::Finance }));
		Rows.Add(TEXT("console"), MakeFusionRow({ ECompanyType::Game, ECompanyType::Semiconductor }));
		Rows.Add(TEXT("smarthome"), MakeFusionRow({ ECompanyType::Electronics, ECompanyType::IT }));
		Rows.Add(TEXT("autodrive"), MakeFusionRow({ ECompanyType::IT, ECompanyType::Automobile }));
		Rows.Add(TEXT("autosemi"), MakeFusionRow({ ECompanyType::Semiconductor, ECompanyType::Automobile }));
		Rows.Add(TEXT("smartcar"), MakeFusionRow({ ECompanyType::IT, ECompanyType::Automobile, ECompanyType::Semiconductor }));
		return Rows;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFusionCurveRewardsTest, "CGR.SideContent.Fusion.CurvesAndMilestones",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFusionCurveRewardsTest::RunTest(const FString& Parameters)
{
	FFusionProjectTable Row = MakeFusionRow();
	TestEqual(TEXT("Level zero uses base requirement"), FusionRules::Requirement(Row, 0), 300.0f);
	TestTrue(TEXT("Level one scales independently"), FMath::IsNearlyEqual(FusionRules::Requirement(Row, 1), 336.0f));
	const TArray<FMissionReward> First = FusionRules::Rewards(Row, 0);
	TestEqual(TEXT("First reward has market cap and diamonds only"), First.Num(), 2);
	if (First.Num() == 2)
	{
		TestEqual(TEXT("Base market cap"), First[0].Amount, int64(80000000));
		TestEqual(TEXT("First success diamonds"), First[1].Amount, int64(30));
		TestEqual(TEXT("First success resource"), First[1].ResourceType, EResourceType::Diamond);
	}
	const TArray<FMissionReward> Second = FusionRules::Rewards(Row, 1);
	TestEqual(TEXT("Ordinary repeat has no ticket or first bonus"), Second.Num(), 1);
	if (Second.Num() == 1)
	{
		TestEqual(TEXT("Level one market cap preserves decimal multiplier precision"), Second[0].Amount, int64(92800000));
	}
	const TArray<FMissionReward> Fifth = FusionRules::Rewards(Row, 4);
	TestEqual(TEXT("New level five milestone"), Fifth.Num(), 2);
	if (Fifth.Num() == 2)
	{
		TestEqual(TEXT("Advanced trait only"), Fifth[1].ItemType, EItemType::BuildingTraitTicketAdvanced);
		TestEqual(TEXT("Milestone amount from row"), Fifth[1].ItemAmount, 2);
	}
	TestEqual(TEXT("Next milestone repeats at new level ten"), FusionRules::Rewards(Row, 9).Num(), 2);
	TestTrue(TEXT("Unrepresentable reward fails closed"), FusionRules::Rewards(Row, MAX_int32).IsEmpty());
	Row.DurationSeconds = 0;
	TestFalse(TEXT("Missing duration invalidates project"), FusionRules::IsValidProject(Row));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFusionSelectionTest, "CGR.SideContent.Fusion.SlotValidation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFusionSelectionTest::RunTest(const FString& Parameters)
{
	const TArray<ECompanyType> Industries = { ECompanyType::Game, ECompanyType::IT };
	TArray<FFusionBuildingCandidate> Buildings = {
		{ 11, ECompanyType::Game, 300.0f, true },
		{ 22, ECompanyType::IT, 700.0f, true }
	};
	TestEqual(TEXT("Every slot meets its requirement"), FusionRules::ValidateSelection(Industries, Buildings, { 11, 22 }, 300.0f), EFusionBlockReason::None);
	TestEqual(TEXT("Missing slot count"), FusionRules::ValidateSelection(Industries, Buildings, { 11 }, 300.0f), EFusionBlockReason::SlotCountMismatch);
	TestEqual(TEXT("Empty slot"), FusionRules::ValidateSelection(Industries, Buildings, { 11, INDEX_NONE }, 300.0f), EFusionBlockReason::EmptySlot);
	TestEqual(TEXT("Same building cannot fill two slots"), FusionRules::ValidateSelection(Industries, Buildings, { 11, 11 }, 300.0f), EFusionBlockReason::DuplicateBuilding);
	TestEqual(TEXT("Unowned index rejected"), FusionRules::ValidateSelection(Industries, Buildings, { 11, 999 }, 300.0f), EFusionBlockReason::InvalidBuilding);
	TestEqual(TEXT("Industry order matters"), FusionRules::ValidateSelection(Industries, Buildings, { 22, 11 }, 300.0f), EFusionBlockReason::IndustryMismatch);
	Buildings[0].TeamPower = 299.0f;
	TestEqual(TEXT("Strong second slot cannot compensate first"), FusionRules::ValidateSelection(Industries, Buildings, { 11, 22 }, 300.0f), EFusionBlockReason::PowerShort);
	Buildings[0].bHasCrew = false;
	TestEqual(TEXT("No crew distinguished from low power"), FusionRules::ValidateSelection(Industries, Buildings, { 11, 22 }, 300.0f), EFusionBlockReason::NoCrew);
	Buildings.Add({ 33, ECompanyType::Semiconductor, 800.0f, true });
	TestEqual(TEXT("Three-slot project validates every industry"), FusionRules::ValidateSelection(
		{ ECompanyType::IT, ECompanyType::Semiconductor, ECompanyType::Game }, Buildings, { 22, 33, 11 }, 300.0f), EFusionBlockReason::NoCrew);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFusionUnavailableIndustryHiddenTest, "CGR.SideContent.Fusion.UnavailableIndustryLeavesList",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFusionUnavailableIndustryHiddenTest::RunTest(const FString& Parameters)
{
	// 출시 로스터 4종 = 게임·IT·전자·자동차. 금융·반도체는 범위 밖(DT_CompanyInfo.bAvailableAtLaunch=false).
	const auto LaunchRoster = [](ECompanyType Industry)
	{
		return Industry != ECompanyType::Finance && Industry != ECompanyType::Semiconductor;
	};
	const TMap<FName, FFusionProjectTable> Rows = ShippedRows();
	const auto FindRow = [&Rows](FName Key, FFusionProjectTable& OutRow)
	{
		const FFusionProjectTable* Found = Rows.Find(Key);
		if (Found) OutRow = *Found;
		return Found != nullptr;
	};

	TestFalse(TEXT("금융을 요구하는 콜라보는 미가용"), FusionRules::IsProjectAvailable(Rows[TEXT("fintech")], LaunchRoster));
	TestFalse(TEXT("반도체를 요구하는 콜라보는 미가용"), FusionRules::IsProjectAvailable(Rows[TEXT("console")], LaunchRoster));
	TestFalse(TEXT("3슬롯 중 하나만 미가용이어도 미가용"), FusionRules::IsProjectAvailable(Rows[TEXT("smartcar")], LaunchRoster));

	const TArray<FName> Filtered = FusionRules::FilterAvailableKeys(ShippedOrder(), FindRow, LaunchRoster);
	TestEqual(TEXT("미가용 4행이 목록에서 빠지고 순서가 보존된다"), JoinKeys(Filtered),
		FString(TEXT("game_platform,smarthome,autodrive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFusionAvailabilityFailsOpenTest, "CGR.SideContent.Fusion.AvailabilityFailsOpen",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFusionAvailabilityFailsOpenTest::RunTest(const FString& Parameters)
{
	const TMap<FName, FFusionProjectTable> Rows = ShippedRows();
	const auto FindRow = [&Rows](FName Key, FFusionProjectTable& OutRow)
	{
		const FFusionProjectTable* Found = Rows.Find(Key);
		if (Found) OutRow = *Found;
		return Found != nullptr;
	};
	// USaveLoadManager::IsIndustryAvailable 의 폴백 규율 — DT 행 결손이면 경고만 남기고 가용(true) 처리한다.
	const auto RowsMissing = [](ECompanyType) { return true; };

	TestTrue(TEXT("요구 업종이 전부 가용이면 남는다"),
		FusionRules::IsProjectAvailable(Rows[TEXT("smarthome")], RowsMissing));
	TestTrue(TEXT("DT 결손이면 금융 콜라보도 숨기지 않는다"),
		FusionRules::IsProjectAvailable(Rows[TEXT("fintech")], RowsMissing));
	TestEqual(TEXT("가용성 판정이 전부 실패하면 7행 전부 남는다"),
		JoinKeys(FusionRules::FilterAvailableKeys(ShippedOrder(), FindRow, RowsMissing)), JoinKeys(ShippedOrder()));

	// 행 자체를 못 찾은 키도 남긴다 — 숨기는 쪽으로 실패하면 임포트 누락이 컨텐츠를 조용히 지운다.
	const auto NoRows = [](FName, FFusionProjectTable&) { return false; };
	const auto BlockAll = [](ECompanyType) { return false; };
	TestEqual(TEXT("행 결손 키는 거르지 않는다"),
		JoinKeys(FusionRules::FilterAvailableKeys(ShippedOrder(), NoRows, BlockAll)), JoinKeys(ShippedOrder()));
	// 반대 축 — 행이 읽히는데 전부 미가용이면 목록은 비고, 패널은 빈 상태 경로로 간다(RefreshBoard).
	TestTrue(TEXT("행이 읽히고 전부 미가용이면 빈 목록"),
		FusionRules::FilterAvailableKeys(ShippedOrder(), FindRow, BlockAll).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFusionClaimTest, "CGR.SideContent.Fusion.ClaimExactlyOnce",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFusionClaimTest::RunTest(const FString& Parameters)
{
	FSideContentSaveData Save;
	Save.FusionActiveKey = TEXT("game_platform");
	Save.FusionEndUnix = 1000;
	Save.FusionBuildingIndices = { 11, 22 };
	Save.FusionLevels.Add(TEXT("fintech"), 7);
	FFusionClaimResult Result;
	const FFusionProjectTable Row = MakeFusionRow();
	TestFalse(TEXT("Timer before end cannot claim"), FusionRules::ConsumeClaim(Save, Row, 999, true, Result));
	TestFalse(TEXT("Local clock cannot authorize claim"), FusionRules::ConsumeClaim(Save, Row, 1000, false, Result));
	TestEqual(TEXT("Rejected claim preserves active key"), Save.FusionActiveKey, FName(TEXT("game_platform")));
	TestTrue(TEXT("Exact end with server time claims"), FusionRules::ConsumeClaim(Save, Row, 1000, true, Result));
	TestEqual(TEXT("Claimed collaboration advances once"), Save.FusionLevels.FindRef(TEXT("game_platform")), 1);
	TestEqual(TEXT("Other collaboration remains unchanged"), Save.FusionLevels.FindRef(TEXT("fintech")), 7);
	TestEqual(TEXT("Claim result preserves old level"), Result.PreviousLevel, 0);
	TestEqual(TEXT("Claim result preserves new level"), Result.NewLevel, 1);
	TestTrue(TEXT("Run cleared before reward notification"), Save.FusionActiveKey.IsNone() && Save.FusionEndUnix == 0 && Save.FusionBuildingIndices.IsEmpty());
	TestFalse(TEXT("Repeated/reentrant claim rejected"), FusionRules::ConsumeClaim(Save, Row, 1001, true, Result));
	return true;
}

#endif
