#include "Misc/AutomationTest.h"
#include "CommonTextBlock.h"
#include "Components/ScaleBox.h"
#include "UI/Element/Common/ResourceWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRResourceWidgetFixedShellFeedbackTest,
	"CGR.ResourceWidget.FixedShellFeedback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRResourceWidgetFixedShellFeedbackTest::RunTest(const FString& Parameters)
{
	UResourceWidget* ResourceWidget =
		NewObject<UResourceWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("ResourceWidget test instance is created"), ResourceWidget))
	{
		return true;
	}

	UScaleBox* AmountScaleBox =
		NewObject<UScaleBox>(ResourceWidget, NAME_None, RF_Transient);
	UCommonTextBlock* AmountText =
		NewObject<UCommonTextBlock>(ResourceWidget, NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Amount ScaleBox is created"), AmountScaleBox)
		|| !TestNotNull(TEXT("Amount text is created"), AmountText))
	{
		return true;
	}

	AmountScaleBox->AddChild(AmountText);
	ResourceWidget->ResourceAmountText = AmountText;
	ResourceWidget->BumpElapsed = 0.f;
	ResourceWidget->BumpPeak = 0.10f;
	ResourceWidget->MilestoneElapsed = 0.f;
	ResourceWidget->UpdateFeedbackAnim(0.1f);

	TestEqual(TEXT("Milestone keeps shell scale fixed"),
		ResourceWidget->GetRenderTransform().Scale, FVector2D(1.f, 1.f));
	TestTrue(TEXT("Milestone still punches amount wrapper"),
		AmountScaleBox->GetRenderTransform().Scale.X > 1.f);

	UResourceWidget* DefaultPulseWidget =
		NewObject<UResourceWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Default pulse ResourceWidget is created"), DefaultPulseWidget))
	{
		return true;
	}

	UScaleBox* DefaultPulseScaleBox =
		NewObject<UScaleBox>(DefaultPulseWidget, NAME_None, RF_Transient);
	UCommonTextBlock* DefaultPulseText =
		NewObject<UCommonTextBlock>(DefaultPulseWidget, NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Default pulse ScaleBox is created"), DefaultPulseScaleBox)
		|| !TestNotNull(TEXT("Default pulse text is created"), DefaultPulseText))
	{
		return true;
	}

	DefaultPulseScaleBox->AddChild(DefaultPulseText);
	DefaultPulseWidget->ResourceAmountText = DefaultPulseText;
	DefaultPulseWidget->PlayBump();
	DefaultPulseWidget->UpdateFeedbackAnim(0.13f);

	const FVector2D DefaultPulseScale = DefaultPulseScaleBox->GetRenderTransform().Scale;
	TestEqual(TEXT("Default pulse X scale peaks at 1.08"), DefaultPulseScale.X, 1.08, 0.001);
	TestEqual(TEXT("Default pulse Y scale peaks at 1.08"), DefaultPulseScale.Y, 1.08, 0.001);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
