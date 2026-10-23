// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "UI/Element/Common/TouchTargetUtils.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr float Glyph = 56.f;
constexpr float Touch = 154.f;

struct FTouchTargetFixture
{
	TStrongObjectPtr<USizeBox> RootSizeBox{
		NewObject<USizeBox>(GetTransientPackage(), NAME_None, RF_Transient)};
	USizeBox* GlyphSize = NewObject<USizeBox>(RootSizeBox.Get());
	UButton* Button = NewObject<UButton>(RootSizeBox.Get());

	/** "?"·"X" 는 Overlay 안에 놓인다 — 실제 WBP 와 같은 슬롯 타입을 만들어야 분기가 검사된다. */
	UOverlaySlot* PlaceInOverlay()
	{
		UOverlay* Host = NewObject<UOverlay>(RootSizeBox.Get());
		return Host->AddChildToOverlay(Button);
	}

	/** 구 WBP 는 버튼을 SizeBox 에 직접 넣었다 — 그 대체 경로. */
	USizeBoxSlot* PlaceInSizeBox()
	{
		USizeBox* Host = NewObject<USizeBox>(RootSizeBox.Get());
		return Cast<USizeBoxSlot>(Host->AddChild(Button));
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTouchTargetOverlaySlotWidensHitOnlyTest,
	"CGR.UI.TouchTarget.OverlaySlotWidensHitOnly",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTouchTargetOverlaySlotWidensHitOnlyTest::RunTest(const FString& Parameters)
{
	FTouchTargetFixture Fixture;
	UOverlaySlot* Slot = Fixture.PlaceInOverlay();
	if (!TestNotNull(TEXT("Overlay slot exists"), Slot)) { return false; }

	UITouchTarget::ApplyGlyphAndTouchExtent(
		Fixture.RootSizeBox.Get(), Fixture.GlyphSize, Fixture.Button, Glyph, Touch);

	// 표시 크기는 Glyph 로 고정 — Touch 가 커도 부모가 보는 desired 는 변하지 않는다.
	TestEqual(TEXT("Root width stays at glyph extent"), Fixture.RootSizeBox->GetWidthOverride(), Glyph);
	TestEqual(TEXT("Root height stays at glyph extent"), Fixture.RootSizeBox->GetHeightOverride(), Glyph);
	TestEqual(TEXT("Glyph box width stays at glyph extent"), Fixture.GlyphSize->GetWidthOverride(), Glyph);
	TestEqual(TEXT("Glyph box height stays at glyph extent"), Fixture.GlyphSize->GetHeightOverride(), Glyph);

	const float Expected = -(Touch - Glyph) * 0.5f;
	const FMargin Padding = Slot->GetPadding();
	TestEqual(TEXT("Left inset is half the touch surplus, negative"), Padding.Left, Expected);
	TestEqual(TEXT("Top inset matches left"), Padding.Top, Expected);
	TestEqual(TEXT("Right inset matches left"), Padding.Right, Expected);
	TestEqual(TEXT("Bottom inset matches left"), Padding.Bottom, Expected);

	// Fill 이 아니면 AlignChild 가 클램프 경로를 타서 히트가 조용히 Glyph 로 돌아간다.
	TestEqual(TEXT("Horizontal alignment is Fill"), Slot->GetHorizontalAlignment(), HAlign_Fill);
	TestEqual(TEXT("Vertical alignment is Fill"), Slot->GetVerticalAlignment(), VAlign_Fill);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTouchTargetSizeBoxSlotFallbackTest,
	"CGR.UI.TouchTarget.SizeBoxSlotFallback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTouchTargetSizeBoxSlotFallbackTest::RunTest(const FString& Parameters)
{
	FTouchTargetFixture Fixture;
	USizeBoxSlot* Slot = Fixture.PlaceInSizeBox();
	if (!TestNotNull(TEXT("SizeBox slot exists"), Slot)) { return false; }

	UITouchTarget::ApplyGlyphAndTouchExtent(
		Fixture.RootSizeBox.Get(), Fixture.GlyphSize, Fixture.Button, Glyph, Touch);

	TestEqual(TEXT("SizeBox slot gets the same negative inset"),
		Slot->GetPadding().Left, -(Touch - Glyph) * 0.5f);
	TestEqual(TEXT("Horizontal alignment is Fill"), Slot->GetHorizontalAlignment(), HAlign_Fill);
	TestEqual(TEXT("Vertical alignment is Fill"), Slot->GetVerticalAlignment(), VAlign_Fill);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTouchTargetSmallerTouchClampsToGlyphTest,
	"CGR.UI.TouchTarget.SmallerTouchClampsToGlyph",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTouchTargetSmallerTouchClampsToGlyphTest::RunTest(const FString& Parameters)
{
	FTouchTargetFixture Fixture;
	UOverlaySlot* Slot = Fixture.PlaceInOverlay();
	if (!TestNotNull(TEXT("Overlay slot exists"), Slot)) { return false; }

	// Touch 를 Glyph 보다 작게 두면 양수 패딩이 되어 글리프가 잘린다 — 그래서 하한이 Glyph 다.
	UITouchTarget::ApplyGlyphAndTouchExtent(
		Fixture.RootSizeBox.Get(), Fixture.GlyphSize, Fixture.Button, Glyph, 20.f);

	TestEqual(TEXT("Touch below glyph yields no inset"), Slot->GetPadding().Left, 0.f);
	TestEqual(TEXT("Glyph box still sized to glyph extent"), Fixture.GlyphSize->GetWidthOverride(), Glyph);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTouchTargetMissingBoxesLeaveSlotUntouchedTest,
	"CGR.UI.TouchTarget.MissingBoxesLeaveSlotUntouched",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTouchTargetMissingBoxesLeaveSlotUntouchedTest::RunTest(const FString& Parameters)
{
	FTouchTargetFixture Fixture;
	UOverlaySlot* Slot = Fixture.PlaceInOverlay();
	if (!TestNotNull(TEXT("Overlay slot exists"), Slot)) { return false; }

	const FMargin Authored(7.f);
	Slot->SetPadding(Authored);

	// 2층 구조로 이관되지 않은 WBP ― 구운 값을 덮지 않고 그대로 두어야 한다.
	UITouchTarget::ApplyGlyphAndTouchExtent(nullptr, Fixture.GlyphSize, Fixture.Button, Glyph, Touch);
	TestEqual(TEXT("No root size box leaves authored padding"), Slot->GetPadding().Left, Authored.Left);

	UITouchTarget::ApplyGlyphAndTouchExtent(Fixture.RootSizeBox.Get(), nullptr, Fixture.Button, Glyph, Touch);
	TestEqual(TEXT("No glyph size box leaves authored padding"), Slot->GetPadding().Left, Authored.Left);

	// 버튼이 없어도 크기 주입은 되어야 한다(슬롯만 건너뛴다).
	UITouchTarget::ApplyGlyphAndTouchExtent(Fixture.RootSizeBox.Get(), Fixture.GlyphSize, nullptr, Glyph, Touch);
	TestEqual(TEXT("Null button still sizes the glyph box"), Fixture.GlyphSize->GetWidthOverride(), Glyph);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
