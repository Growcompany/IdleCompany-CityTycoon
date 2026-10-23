#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "UI/AnimatedActivatableWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
struct FPanelAppearFixture
{
	TStrongObjectPtr<UCommonActivatableWidget> Panel;
	UBorder* Content = nullptr;
	UBorder* Background = nullptr;
	TSharedPtr<SWidget> SlateWidget;
	const bool bPreviousReducedMotion = FMotionGate::IsReduced();

	FPanelAppearFixture(UClass* PanelClass = UAnimatedActivatableWidget::StaticClass(),
		UObject* PanelOuter = GetTransientPackage(), FName ContentName = TEXT("ModalFrame"))
		: Panel(NewObject<UCommonActivatableWidget>(PanelOuter, PanelClass, NAME_None, RF_Transient))
	{
		FMotionGate::SetReduced(false);
		Panel->Initialize();
		UWidgetTree* Tree = Panel->WidgetTree;
		UOverlay* RootOverlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
		Tree->RootWidget = RootOverlay;
		Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Content = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), ContentName);
		RootOverlay->AddChildToOverlay(Background);
		RootOverlay->AddChildToOverlay(Content);
		if (UConfirmCancelWidget* Confirmation = Cast<UConfirmCancelWidget>(Panel.Get()))
		{
			Confirmation->DefaultTitle = FText::GetEmpty();
		}
		SlateWidget = Panel->TakeWidget();
	}

	~FPanelAppearFixture()
	{
		Panel->DeactivateWidget();
		SlateWidget.Reset();
		static_cast<UUserWidget*>(Panel.Get())->ReleaseSlateResources(true);
		FMotionGate::SetReduced(bPreviousReducedMotion);
	}

	void Tick(float DeltaTime)
	{
		SlateWidget->Tick(FGeometry::MakeRoot(FVector2D(1000.f, 600.f), FSlateLayoutTransform()), 0.0, DeltaTime);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPanelAppearContentLifecycleTest,
	"CGR.UI.PanelMotion.ContentLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPanelAppearContentLifecycleTest::RunTest(const FString& Parameters)
{
	FPanelAppearFixture Fixture;
	const FWidgetTransform Authored(FVector2D(9.f, -7.f), FVector2D(0.9f, 1.1f), FVector2D(2.f, 0.f), 3.f);
	const FVector2D AuthoredPivot(0.25f, 0.75f);
	Fixture.Content->SetRenderTransform(Authored);
	Fixture.Content->SetRenderTransformPivot(AuthoredPivot);
	Fixture.Panel->ActivateWidget();
	TestTrue(TEXT("Only modal content starts smaller"), Fixture.Content->GetRenderTransform().Scale.X < Authored.Scale.X);
	TestTrue(TEXT("Full-screen root stays fixed during entrance"), Fixture.Panel->GetRenderTransform().IsIdentity());
	TestTrue(TEXT("Background stays fixed during entrance"), Fixture.Background->GetRenderTransform().IsIdentity());
	Fixture.Tick(0.08f);
	TestTrue(TEXT("Content advances toward its authored scale"), Fixture.Content->GetRenderTransform().Scale.X > Authored.Scale.X * 0.95f);
	Fixture.Panel->DeactivateWidget();
	TestTrue(TEXT("Interrupted close restores the complete authored transform"), Fixture.Content->GetRenderTransform() == Authored);
	TestEqual(TEXT("Interrupted close restores the authored pivot"), Fixture.Content->GetRenderTransformPivot(), AuthoredPivot);
	Fixture.Panel->ActivateWidget();
	TestTrue(TEXT("Reopening replays the entrance without accumulating scale"), Fixture.Content->GetRenderTransform().Scale.X < Authored.Scale.X);
	Fixture.Tick(1.f);
	TestTrue(TEXT("Completed entrance restores the complete authored transform"), Fixture.Content->GetRenderTransform() == Authored);
	TestEqual(TEXT("Completed entrance restores the authored pivot"), Fixture.Content->GetRenderTransformPivot(), AuthoredPivot);
	TestTrue(TEXT("Entrance does not deactivate the panel"), Fixture.Panel->IsActivated());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPanelAppearReducedMotionTest,
	"CGR.UI.PanelMotion.ReducedMotion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPanelAppearReducedMotionTest::RunTest(const FString& Parameters)
{
	FPanelAppearFixture Fixture;
	Fixture.Panel->ActivateWidget();
	Fixture.Tick(0.03f);
	FMotionGate::SetReduced(true);
	Fixture.Tick(0.01f);
	TestTrue(TEXT("Enabling Reduce Motion settles an in-flight entrance immediately"), Fixture.Content->GetRenderTransform().IsIdentity());
	TestTrue(TEXT("Reduced motion keeps the full-screen root stationary"), Fixture.Panel->GetRenderTransform().IsIdentity());
	Fixture.Panel->DeactivateWidget();
	Fixture.Panel->ActivateWidget();
	TestTrue(TEXT("Reduced-motion reopen has no scaled first frame"), Fixture.Content->GetRenderTransform().IsIdentity());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPanelAppearDrawerPreservationTest,
	"CGR.UI.PanelMotion.DrawerPreservation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPanelAppearDrawerPreservationTest::RunTest(const FString& Parameters)
{
	FPanelAppearFixture Fixture;
	const FEnumProperty* Direction = FindFProperty<FEnumProperty>(Fixture.Panel->GetClass(), TEXT("AppearFrom"));
	const FObjectPropertyBase* Target = FindFProperty<FObjectPropertyBase>(Fixture.Panel->GetClass(), TEXT("AppearSlideTarget"));
	if (!TestNotNull(TEXT("Slide direction exists"), Direction) || !TestNotNull(TEXT("Slide target exists"), Target))
	{
		return false;
	}
	Direction->GetUnderlyingProperty()->SetIntPropertyValue(Direction->ContainerPtrToValuePtr<void>(Fixture.Panel.Get()),
		static_cast<uint64>(EAppearSlideFrom::Right));
	Target->SetObjectPropertyValue_InContainer(Fixture.Panel.Get(), Fixture.Content);
	Fixture.Panel->ActivateWidget();
	TestTrue(TEXT("Right drawer still enters from the right"), Fixture.Content->GetRenderTransform().Translation.X > 0.f);
	TestEqual(TEXT("Drawer does not inherit the central pop scale"), Fixture.Content->GetRenderTransform().Scale, FVector2D::UnitVector);
	TestTrue(TEXT("Drawer background stays fixed"), Fixture.Panel->GetRenderTransform().IsIdentity());
	Fixture.Panel->DeactivateWidget();
	TestTrue(TEXT("Closing during a drawer entrance clears the offset"), Fixture.Content->GetRenderTransform().IsIdentity());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPanelAppearConfirmLifecycleTest,
	"CGR.UI.PanelMotion.ConfirmLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPanelAppearConfirmLifecycleTest::RunTest(const FString& Parameters)
{
	FPanelAppearFixture Fixture(UConfirmCancelWidget::StaticClass(), GetTransientPackage(), TEXT("SizeBox_128"));
	Fixture.Panel->ActivateWidget();
	const FVector2D InitialScale = Fixture.Content->GetRenderTransform().Scale;
	TestTrue(TEXT("Standalone confirmation content starts smaller"), InitialScale.X < 1.f);
	TestTrue(TEXT("Confirmation background stays fixed"), Fixture.Background->GetRenderTransform().IsIdentity());
	TestEqual(TEXT("Stack confirmation leaves opacity to the stack"), Fixture.Panel->GetRenderOpacity(), 1.f);
	Fixture.Tick(0.05f);
	TestTrue(TEXT("Confirmation entrance progresses"), Fixture.Content->GetRenderTransform().Scale.X > InitialScale.X);
	Fixture.Panel->DeactivateWidget();
	TestTrue(TEXT("Interrupted confirmation settles immediately"), Fixture.Content->GetRenderTransform().IsIdentity());
	Fixture.Panel->ActivateWidget();
	TestEqual(TEXT("Confirmation reopen does not accumulate scale"), Fixture.Content->GetRenderTransform().Scale, InitialScale);
	Fixture.Tick(0.2f);
	TestTrue(TEXT("Compact confirmation settles within 0.2 seconds"), Fixture.Content->GetRenderTransform().IsIdentity());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPanelAppearEmbeddedConfirmTest,
	"CGR.UI.PanelMotion.EmbeddedConfirm",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPanelAppearEmbeddedConfirmTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UAnimatedActivatableWidget> ParentPanel{NewObject<UAnimatedActivatableWidget>()};
	ParentPanel->Initialize();
	FPanelAppearFixture Fixture(UConfirmCancelWidget::StaticClass(), ParentPanel->WidgetTree, TEXT("SizeBox_128"));
	Fixture.Panel->ActivateWidget();
	Fixture.Tick(0.05f);
	TestTrue(TEXT("Embedded confirmation frame does not replay its parent's entrance"), Fixture.Content->GetRenderTransform().IsIdentity());
	TestEqual(TEXT("Embedded confirmation is not faded independently"), Fixture.Panel->GetRenderOpacity(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPanelAppearOverlayLifecycleTest,
	"CGR.UI.PanelMotion.OverlayLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPanelAppearOverlayLifecycleTest::RunTest(const FString& Parameters)
{
	FPanelAppearFixture Fixture;
	UAnimatedActivatableWidget* OverlayPanel = CastChecked<UAnimatedActivatableWidget>(Fixture.Panel.Get());
	const FWidgetTransform Authored(FVector2D(4.f, -3.f), FVector2D(0.9f, 1.1f), FVector2D::ZeroVector, 2.f);
	Fixture.Content->SetRenderTransform(Authored);
	OverlayPanel->SetRenderOpacity(0.65f);
	OverlayPanel->PlayOverlayAppear();
	TestFalse(TEXT("Direct viewport entrance does not activate or claim input"), OverlayPanel->IsActivated());
	TestEqual(TEXT("Overlay starts with its own fade"), OverlayPanel->GetRenderOpacity(), 0.f);
	TestTrue(TEXT("Overlay content starts smaller"), Fixture.Content->GetRenderTransform().Scale.X < Authored.Scale.X);
	TestTrue(TEXT("Overlay dim does not move"), Fixture.Background->GetRenderTransform().IsIdentity());
	Fixture.Tick(0.04f);
	TestTrue(TEXT("Overlay fade advances"), OverlayPanel->GetRenderOpacity() > 0.f && OverlayPanel->GetRenderOpacity() < 0.65f);
	OverlayPanel->PlayOverlayAppear();
	Fixture.Tick(0.3f);
	TestEqual(TEXT("Replaying an overlay preserves its authored opacity"), OverlayPanel->GetRenderOpacity(), 0.65f);
	TestTrue(TEXT("Replaying an overlay preserves its authored transform"), Fixture.Content->GetRenderTransform() == Authored);
	OverlayPanel->PlayOverlayAppear();
	FMotionGate::SetReduced(true);
	Fixture.Tick(0.01f);
	TestEqual(TEXT("Reduce Motion settles an inactive overlay fade"), OverlayPanel->GetRenderOpacity(), 0.65f);
	TestTrue(TEXT("Reduce Motion settles an inactive overlay transform"), Fixture.Content->GetRenderTransform() == Authored);
	OverlayPanel->PlayOverlayAppear();
	TestEqual(TEXT("Reduced-motion overlay has no invisible first frame"), OverlayPanel->GetRenderOpacity(), 0.65f);
	FMotionGate::SetReduced(false);
	OverlayPanel->PlayOverlayAppear();
	Fixture.SlateWidget.Reset();
	static_cast<UUserWidget*>(OverlayPanel)->ReleaseSlateResources(true);
	TestEqual(TEXT("Destroying an inactive overlay restores its opacity"), OverlayPanel->GetRenderOpacity(), 0.65f);
	TestTrue(TEXT("Destroying an inactive overlay restores its content"), Fixture.Content->GetRenderTransform() == Authored);
	return true;
}

#endif
