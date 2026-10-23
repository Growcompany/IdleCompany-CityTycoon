#include "Misc/AutomationTest.h"
#include "Input/UIActionBindingHandle.h"
#include "UI/UIBase.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "UObject/SoftObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRUIBaseWorldInputFallbackTest,
	"CGR.UI.InputConfig.RootWorldInputFallback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRUIBaseWorldInputFallbackTest::RunTest(const FString& Parameters)
{
	const TSoftClassPtr<UUIBase> RootBlueprintPath(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/UI_Base.UI_Base_C")));
	UClass* RootBlueprintClass = RootBlueprintPath.LoadSynchronous();
	if (!TestNotNull(TEXT("Authored UI_Base class loads"), RootBlueprintClass))
	{
		return false;
	}

	const UUIBase* RootBlueprintCDO = Cast<UUIBase>(RootBlueprintClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Authored UI_Base inherits UUIBase"), RootBlueprintCDO))
	{
		return false;
	}

	// 자식 패널이 닫히면 라우터가 요청할 루트 설정을 실제 클래스의 가상 호출로 검증한다.
	const UCommonActivatableWidget* RootWidgets[] = { GetDefault<UUIBase>(), RootBlueprintCDO };
	for (const UCommonActivatableWidget* RootWidget : RootWidgets)
	{
		const FString RootName = RootWidget->GetClass()->GetName();
		const TOptional<FUIInputConfig> InputConfig = RootWidget->GetDesiredInputConfig();
		if (!TestTrue(RootName + TEXT(" supplies the fallback when a panel closes"), InputConfig.IsSet()))
		{
			continue;
		}

		TestEqual(RootName + TEXT(" allows gameplay and CommonUI actions"),
			InputConfig->GetInputMode(), ECommonInputMode::All);
		TestEqual(RootName + TEXT(" forwards the first world mouse press and captures drags"),
			InputConfig->GetMouseCaptureMode(), EMouseCaptureMode::CaptureDuringMouseDown);
		TestFalse(RootName + TEXT(" keeps the cursor visible during world drags"),
			InputConfig->HideCursorDuringViewportCapture());
		TestFalse(RootName + TEXT(" does not ignore movement"), InputConfig->bIgnoreMoveInput);
		TestFalse(RootName + TEXT(" does not ignore camera look"), InputConfig->bIgnoreLookInput);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRWorldMapMenuInputConfigTest,
	"CGR.UI.InputConfig.WorldMapRemainsMenu",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRWorldMapMenuInputConfigTest::RunTest(const FString& Parameters)
{
	const UCommonActivatableWidget* WorldMapWidget = GetDefault<UWorldMapPanelWidget>();
	const TOptional<FUIInputConfig> InputConfig = WorldMapWidget->GetDesiredInputConfig();
	if (!TestTrue(TEXT("World map supplies its own input config"), InputConfig.IsSet()))
	{
		return false;
	}

	TestEqual(TEXT("Full-screen world map keeps menu input while open"),
		InputConfig->GetInputMode(), ECommonInputMode::Menu);
	TestEqual(TEXT("World map leaves the cursor free for map and dock interaction"),
		InputConfig->GetMouseCaptureMode(), EMouseCaptureMode::NoCapture);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
