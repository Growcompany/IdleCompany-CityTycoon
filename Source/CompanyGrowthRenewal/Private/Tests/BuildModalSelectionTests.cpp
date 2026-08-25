#include "Misc/AutomationTest.h"
#include "Groups/CommonButtonGroupBase.h"
#include "UI/Element/Buttons/IndustryButtonWidget.h"
#include "UI/Panel/BuildModalWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRBuildModalSelectionTest,
	"CGR.BuildModal.SelectionSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRBuildModalSelectionTest::RunTest(const FString& Parameters)
{
	UBuildModalWidget* BuildModal =
		NewObject<UBuildModalWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("BuildModal test instance is created"), BuildModal))
	{
		return true;
	}

	BuildModal->IndustryButtonGroup =
		NewObject<UCommonButtonGroupBase>(BuildModal, NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Industry button group is created"), BuildModal->IndustryButtonGroup))
	{
		return true;
	}

	BuildModal->SelectedCompanyType = ECompanyType::Game;
	ECompanyType ResolvedType = ECompanyType::Game;
	TestFalse(TEXT("No selected industry blocks construction instead of using the stale Game cache"),
		BuildModal->TryGetSelectedCompanyType(ResolvedType));
	TestTrue(TEXT("Failed selection resolution clears the output type"),
		ResolvedType == ECompanyType::None);

	UIndustryButtonWidget* ITTile =
		NewObject<UIndustryButtonWidget>(BuildModal, NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("IT industry tile is created"), ITTile))
	{
		return true;
	}

	ITTile->SetCompanyType(ECompanyType::IT);
	ITTile->SetIsSelectable(true);
	BuildModal->IndustryButtonGroup->AddWidget(ITTile);
	BuildModal->IndustryButtonGroup->SelectButtonAtIndex(0);

	ResolvedType = ECompanyType::None;
	TestTrue(TEXT("Current group selection resolves successfully"),
		BuildModal->TryGetSelectedCompanyType(ResolvedType));
	TestTrue(TEXT("Current IT selection wins over the stale Game cache"),
		ResolvedType == ECompanyType::IT);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
