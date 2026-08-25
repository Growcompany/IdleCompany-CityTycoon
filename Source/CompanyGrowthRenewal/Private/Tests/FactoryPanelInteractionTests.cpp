#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "UI/Panel/FactoryPanelInteractionPolicy.h"
#include "UI/Panel/FactoryPanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryPanelWorldInteractionTest,
	"CGR.Factory.PanelWorldInteraction",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryPanelWorldInteractionTest::RunTest(const FString& Parameters)
{
	UFactoryPanelWidget* Panel =
		NewObject<UFactoryPanelWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	UButton* Background = NewObject<UButton>(Panel, NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Factory panel test instance is created"), Panel)
		|| !TestNotNull(TEXT("Factory panel background is created"), Background))
	{
		return true;
	}

	Panel->SetVisibility(ESlateVisibility::Visible);
	Background->SetVisibility(ESlateVisibility::Visible);
	CGRFactoryPanelInteraction::ConfigureWorldPassThrough(Panel, Background);

	TestEqual(TEXT("Panel root does not consume empty-area hits"),
		Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Background no longer consumes or closes on world hits"),
		Background->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Exclusive UI mode is released for factory panel open"),
		CGRFactoryPanelInteraction::ResolveOpeningInputMode(EInputMode::UI), EInputMode::Normal);
	TestEqual(TEXT("Normal mode remains normal"),
		CGRFactoryPanelInteraction::ResolveOpeningInputMode(EInputMode::Normal), EInputMode::Normal);
	TestEqual(TEXT("Factory hold mode survives BuildOpen reactivation after panel close"),
		CGRFactoryPanelInteraction::ResolveOpeningInputMode(EInputMode::Factory), EInputMode::Factory);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
