#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "Tests/GachaRevealTestTables.h"
#include "Enum/WidgetType.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Data/EmployeeEquipmentData.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

struct FGachaRevealResultTestAccessor
{
	struct FRig
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UTableManagerSubsystem> Tables;
		TStrongObjectPtr<UGachaRevealPresentationWidget> Widget;
	};

	static bool Make(FAutomationTestBase& Test, FRig& Rig)
	{
		const TSubclassOf<UUserWidget> Class = GachaRevealTestTables::RegisteredWidgetClass(EWidgetType::GachaRevealPresentation);
		if (!Test.TestNotNull(TEXT("GachaRevealPresentation DT_WidgetClass"), Class.Get())) { return false; }
		Rig.World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(Rig.World); }
		Rig.Tables = GachaRevealTestTables::MakeWidgetTables();
		Rig.Widget.Reset(CreateWidget<UGachaRevealPresentationWidget>(Rig.World, Class));
		if (!Test.TestNotNull(TEXT("reveal widget"), Rig.Widget.Get())) { return false; }
		Rig.Widget->TablesOverride = Rig.Tables.Get();
		Rig.Widget->ReduceMotionOverride = true;
		Rig.Widget->SetDisableExitFade(true);
		Test.AddExpectedMessagePlain(TEXT("[GachaRevealBox] TableManager 없음"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0);
		return true;
	}

	static void Destroy(FRig& Rig)
	{
		Rig.Widget.Reset();
		if (Rig.World)
		{
			if (GEngine) { GEngine->DestroyWorldContext(Rig.World); }
			Rig.World->DestroyWorld(false);
			Rig.World = nullptr;
		}
	}

	static void Begin(FRig& Rig, const TArray<int32>& Grades)
	{
		TArray<FGachaRevealEntry> Entries;
		TArray<FGachaRevealCardData> Cards;
		TArray<FText> Meta;
		for (int32 Index = 0; Index < Grades.Num(); ++Index)
		{
			FGachaRevealEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.PullIndex = Index;
			Entry.Grade = Grades[Index];
			FGachaRevealCardData& Card = Cards.AddDefaulted_GetRef();
			Card.Kind = EGachaRevealKind::Equipment;
			Card.Grade = Entry.Grade;
			Card.Name = FText::FromString(FString::Printf(TEXT("Result%d"), Index));
			Meta.Add(FText::FromString(TEXT("머리 · 장비")));
		}
		Rig.Widget->BeginReveal(EGachaRevealKind::Equipment, EGachaBoxType::EquipmentNormal,
			FText::FromString(TEXT("시험")), Entries, Cards, Meta);
		Rig.Widget->DebugSkipToResult();
	}

	static int32 TallySum(const UGachaRevealPresentationWidget& Widget)
	{
		int32 Sum = 0;
		for (int32 Count : Widget.TallyCounts) { Sum += Count; }
		return Sum;
	}

	static FText BestCardName(const UGachaRevealPresentationWidget& Widget)
	{
		const UGachaRevealCardWidget* Card = Widget.WidgetTree ? Cast<UGachaRevealCardWidget>(Widget.WidgetTree->FindWidget(TEXT("BestCard"))) : nullptr;
		return Card ? Card->GetData().Name : FText::GetEmpty();
	}
	static int32 BestTooltipIndex(const UGachaRevealPresentationWidget& Widget) { return Widget.BestResultIndex(); }
	static bool SortButtonsMatchMode(const UGachaRevealPresentationWidget& Widget)
	{
		return Widget.SortGradeControl.IsValid() && Widget.SortOrderControl.IsValid()
			&& Widget.SortGradeControl->GetSelected() == (Widget.SortMode == EGachaRevealSort::Grade)
			&& Widget.SortOrderControl->GetSelected() == (Widget.SortMode == EGachaRevealSort::PullOrder);
	}
	static void SetBestPriority(UGachaRevealPresentationWidget& Widget, int32 NewIndex, int32 RecommendedIndex,
		const TArray<int32>& PullIndices)
	{
		Widget.Recommended.Reset();
		if (RecommendedIndex != INDEX_NONE) { Widget.Recommended.Add(RecommendedIndex); }
		for (int32 Index = 0; Index < Widget.Entries.Num(); ++Index)
		{
			Widget.Entries[Index].PullIndex = PullIndices[Index];
			Widget.Entries[Index].bNew = Index == NewIndex;
			Widget.CardData[Index].bNew = Index == NewIndex;
			Widget.CardData[Index].bRecommended = Index == RecommendedIndex;
		}
		Widget.ConfigureResultDetails();
	}

	static void SortPullOrder(UGachaRevealPresentationWidget& Widget) { Widget.OnSortOrderClicked(); }
	static void SortGrade(UGachaRevealPresentationWidget& Widget) { Widget.OnSortGradeClicked(); }
	static const FGachaRevealRect& Cell(const UGachaRevealPresentationWidget& Widget, int32 Index) { return Widget.CardMotion[Index].Cell; }
	static const FGachaRevealRect& From(const UGachaRevealPresentationWidget& Widget, int32 Index) { return Widget.CardMotion[Index].SettleFrom; }
	static UWidget* Named(const UGachaRevealPresentationWidget& Widget, const TCHAR* Name)
	{
		return Widget.WidgetTree ? Widget.WidgetTree->FindWidget(FName(Name)) : nullptr;
	}
	static int32 CompareIndex(const UGachaRevealPresentationWidget& Widget, const TCHAR* Name)
	{
		const UWidgetSwitcher* Compare = Cast<UWidgetSwitcher>(Named(Widget, Name));
		return Compare && Compare->GetVisibility() != ESlateVisibility::Collapsed ? Compare->GetActiveWidgetIndex() : INDEX_NONE;
	}
	static FString Text(const UGachaRevealPresentationWidget& Widget, const TCHAR* Name)
	{
		const UTextBlock* Label = Cast<UTextBlock>(Named(Widget, Name));
		return Label ? Label->GetText().ToString() : FString();
	}
	static float GaugeWidth(const UGachaRevealPresentationWidget& Widget)
	{
		const USizeBox* Fill = Cast<USizeBox>(Named(Widget, TEXT("PityFillBox")));
		return Fill ? Fill->GetWidthOverride() : -1.f;
	}
	static void SetCompareEquipment(UGachaRevealPresentationWidget& Widget)
	{
		Widget.RevealEmployeeId = 1;
		for (FGachaRevealEntry& Entry : Widget.Entries)
		{
			Entry.SlotKey = static_cast<int32>(EEmployeeEquipmentSlot::Head);
			Entry.bEmployeeKnown = true;
			Entry.EquippedGrade = 1;
		}
		Widget.Entries[1].bNew = true;
		Widget.CardData[1].bNew = true;
		Widget.Recommended.Reset();
		Widget.Recommended.Add(0);
		Widget.AwardContext.bOwnedBefore = {true, false};
		Widget.ConfigureResultDetails();
	}
	static void SetBestEquippedGrade(UGachaRevealPresentationWidget& Widget, int32 Grade)
	{
		Widget.Entries[1].EquippedGrade = Grade;
		Widget.ConfigureResultDetails();
	}
	static void SetCompareTrait(UGachaRevealPresentationWidget& Widget, bool bKnown)
	{
		Widget.Kind = EGachaRevealKind::Trait;
		Widget.RevealEmployeeId = INDEX_NONE;
		Widget.Recommended.Reset();
		Widget.AwardContext.bOwnedBefore.Reset();
		if (bKnown) { Widget.AwardContext.bOwnedBefore = {true, false}; }
		Widget.ConfigureResultDetails();
	}
	static void SetCompareSkin(UGachaRevealPresentationWidget& Widget, bool bKnown)
	{
		Widget.Kind = EGachaRevealKind::Skin;
		Widget.bSkinDuplicates.Reset();
		if (bKnown) { Widget.bSkinDuplicates = {false, false}; }
		Widget.Entries[1].MileageGained = 0;
		Widget.ConfigureResultDetails();
	}
	static void SetCompareSkinDuplicate(UGachaRevealPresentationWidget& Widget)
	{
		Widget.bSkinDuplicates = {false, true};
		Widget.Entries[1].bNew = false;
		Widget.CardData[1].bNew = false;
		Widget.Entries[1].Grade = 5;
		Widget.CardData[1].Grade = 5;
		Widget.Entries[1].MileageGained = 4;
		Widget.ConfigureResultDetails();
	}
	static void SetMileage(UGachaRevealPresentationWidget& Widget, EGachaRevealKind Kind, int32 Balance, int32 Gained)
	{
		Widget.Kind = Kind;
		Widget.AwardContext.MileageBalance = Balance;
		Widget.AwardContext.MileageGained = Gained;
		Widget.UpdateResultGauge();
	}
	static UCanvasPanelSlot* CardSlot(const UGachaRevealPresentationWidget& Widget, int32 Index)
	{
		return Widget.DealtCards.IsValidIndex(Index) && Widget.DealtCards[Index]
			? Cast<UCanvasPanelSlot>(Widget.DealtCards[Index]->Slot) : nullptr;
	}
	static void SetRecommendedInstance(UGachaRevealPresentationWidget& Widget, int32 EmployeeId, const FGuid& InstanceId)
	{
		Widget.RevealEmployeeId = EmployeeId;
		Widget.EquipmentInstanceIds = {InstanceId};
		FGachaRevealEntry Entry;
		Entry.SlotKey = static_cast<int32>(EEmployeeEquipmentSlot::Head);
		Entry.bEmployeeKnown = true;
		Widget.Entries = {Entry};
		Widget.Recommended.Reset();
		Widget.Recommended.Add(0);
	}
	static FEmployeeEquipmentRequest Request(const UGachaRevealPresentationWidget& Widget, UEmployeeEquipmentManagerSubsystem& Equipment)
	{
		return Widget.MakeRecommendedRequest(0, Equipment);
	}
	static void RestoreDetails(UGachaRevealPresentationWidget& Widget) { Widget.ConfigureResultDetails(); }
	static void SetRecommendedPair(UGachaRevealPresentationWidget& Widget, const FGuid& HeadId, const FGuid& EyewearId)
	{
		FGachaRevealEntry HeadEntry;
		HeadEntry.Grade = 4; HeadEntry.EquippedGrade = 1; HeadEntry.bEmployeeKnown = true;
		HeadEntry.SlotKey = static_cast<int32>(EEmployeeEquipmentSlot::Head);
		FGachaRevealEntry EyewearEntry = HeadEntry;
		EyewearEntry.PullIndex = 1;
		EyewearEntry.SlotKey = static_cast<int32>(EEmployeeEquipmentSlot::Eyewear);
		Widget.Entries = {HeadEntry, EyewearEntry};
		Widget.EquipmentInstanceIds = {HeadId, EyewearId};
	}
	static TArray<int32> Pending(const UGachaRevealPresentationWidget& Widget, const UEmployeeEquipmentManagerSubsystem& Equipment)
	{
		return Widget.PendingRecommendedIndices(Equipment);
	}
	static float PityAt(UGachaRevealPresentationWidget& Widget, const TArray<int32>& Grades, int32 Before, int32 After, float Progress)
	{
		Widget.Kind = EGachaRevealKind::Equipment;
		Widget.Entries.Reset();
		for (int32 Grade : Grades) { Widget.Entries.AddDefaulted_GetRef().Grade = Grade; }
		Widget.AwardContext.PityBefore = Before;
		Widget.AwardContext.PityAfter = After;
		Widget.AwardContext.PityThreshold = 40;
		return Widget.PityFillValueAt(Progress);
	}

	static void Set(UGachaRevealPresentationWidget& Widget, EGachaRevealKind Kind,
		const TArray<FGachaRevealEntry>& Entries, const FGachaRevealAwardContext& Award)
	{
		Widget.Kind = Kind;
		Widget.Entries = Entries;
		Widget.AwardContext = Award;
		Widget.bSkinDuplicates.Init(false, Entries.Num());
		for (int32 Index = 0; Index < Entries.Num(); ++Index) { Widget.bSkinDuplicates[Index] = Entries[Index].MileageGained > 0; }
		Widget.Recommended.Reset();
		for (int32 Index : GachaRevealModel::PickRecommended(Entries)) { Widget.Recommended.Add(Index); }
		Widget.TallyLatinTexts.SetNum(6);
		Widget.TallyKoreanTexts.SetNum(6);
		static const TCHAR* LatinNames[] = {TEXT("C"), TEXT("B"), TEXT("A"), TEXT("S"), TEXT("SS"), TEXT("SSS")};
		static const TCHAR* KoreanNames[] = {TEXT("일반"), TEXT("고급"), TEXT("레어"), TEXT("에픽"), TEXT("레전드"), TEXT("신화")};
		for (int32 Grade = 0; Grade < 6; ++Grade)
		{
			UTextBlock* Latin = NewObject<UTextBlock>(&Widget);
			Latin->SetText(FText::FromString(LatinNames[Grade]));
			Widget.TallyLatinTexts[Grade] = Latin;
			UTextBlock* Korean = NewObject<UTextBlock>(&Widget);
			Korean->SetText(FText::FromString(KoreanNames[Grade]));
			Widget.TallyKoreanTexts[Grade] = Korean;
		}
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultTallySumTest,
	"CGR.Gacha.Reveal.Result.TallySum", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultTallySumTest::RunTest(const FString& Parameters)
{
	for (int32 Count : {1, 10, 30})
	{
		FGachaRevealResultTestAccessor::FRig Rig;
		if (!FGachaRevealResultTestAccessor::Make(*this, Rig)) { FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
		TArray<int32> Grades;
		for (int32 Index = 0; Index < Count; ++Index) { Grades.Add(Index % 6); }
		FGachaRevealResultTestAccessor::Begin(Rig, Grades);
		TestEqual(*FString::Printf(TEXT("%d pulls tally sum"), Count), FGachaRevealResultTestAccessor::TallySum(*Rig.Widget), Count);
		FGachaRevealResultTestAccessor::Destroy(Rig);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultSortTooltipTest,
	"CGR.Gacha.Reveal.Result.SortAndBest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultSortTooltipTest::RunTest(const FString& Parameters)
{
	FGachaRevealResultTestAccessor::FRig Rig;
	if (!FGachaRevealResultTestAccessor::Make(*this, Rig)) { FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
	FGachaRevealResultTestAccessor::Begin(Rig, {1, 4, 4, 0, 3, 2, 1, 0, 5, 1});
	TestTrue(TEXT("result starts with grade sort selected"), FGachaRevealResultTestAccessor::SortButtonsMatchMode(*Rig.Widget));
	TestEqual(TEXT("best detail takes highest actual result"), FGachaRevealResultTestAccessor::BestCardName(*Rig.Widget).ToString(), FString(TEXT("Result8")));
	TestEqual(TEXT("best card tap maps to highest actual result"), FGachaRevealResultTestAccessor::BestTooltipIndex(*Rig.Widget), 8);
	FGachaRevealResultTestAccessor::SortPullOrder(*Rig.Widget);
	TestTrue(TEXT("pull-order toggle updates selected tab"), FGachaRevealResultTestAccessor::SortButtonsMatchMode(*Rig.Widget));
	const FGachaRevealRect First = FGachaRevealResultTestAccessor::Cell(*Rig.Widget, 0);
	const FGachaRevealRect Last = FGachaRevealResultTestAccessor::Cell(*Rig.Widget, 9);
	TestTrue(TEXT("pull order starts with first award"), First.X < Last.X || First.Y < Last.Y);
	Rig.Widget->DebugAdvanceTime(.2f);
	const UCanvasPanelSlot* MidSlot = FGachaRevealResultTestAccessor::CardSlot(*Rig.Widget, 0);
	if (!MidSlot) { AddError(TEXT("card canvas slot missing mid-sort")); FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
	const FVector2D MidPos = MidSlot->GetPosition();
	FGachaRevealResultTestAccessor::SortGrade(*Rig.Widget);
	TestTrue(TEXT("grade toggle updates selected tab"), FGachaRevealResultTestAccessor::SortButtonsMatchMode(*Rig.Widget));
	const FGachaRevealRect& InterruptedFrom = FGachaRevealResultTestAccessor::From(*Rig.Widget, 0);
	TestTrue(TEXT("repeated toggle starts at visible position"), FVector2D::Distance(MidPos, FVector2D(InterruptedFrom.X, InterruptedFrom.Y)) < 1.f);
	FGachaRevealResultTestAccessor::SortPullOrder(*Rig.Widget);
	Rig.Widget->DebugAdvanceTime(.5f);
	if (UCanvasPanelSlot* CanvasSlot = FGachaRevealResultTestAccessor::CardSlot(*Rig.Widget, 0))
	{
		TestTrue(TEXT("first award reached pull-order slot"), FVector2D::Distance(CanvasSlot->GetPosition(), FVector2D(First.X, First.Y)) < 1.f);
	}
	else { AddError(TEXT("card canvas slot missing")); }
	FGachaRevealResultTestAccessor::Destroy(Rig);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultBestPriorityTest,
	"CGR.Gacha.Reveal.Result.BestPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultBestPriorityTest::RunTest(const FString& Parameters)
{
	FGachaRevealResultTestAccessor::FRig Rig;
	if (!FGachaRevealResultTestAccessor::Make(*this, Rig)) { FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
	FGachaRevealResultTestAccessor::Begin(Rig, {4, 4, 4, 3});
	FGachaRevealResultTestAccessor::SetBestPriority(*Rig.Widget, 3, INDEX_NONE, {0, 1, 2, 3});
	TestEqual(TEXT("higher grade outranks lower-grade NEW"), FGachaRevealResultTestAccessor::BestTooltipIndex(*Rig.Widget), 0);
	TestEqual(TEXT("higher grade fills best card"), FGachaRevealResultTestAccessor::BestCardName(*Rig.Widget).ToString(), FString(TEXT("Result0")));
	FGachaRevealResultTestAccessor::SetBestPriority(*Rig.Widget, 2, 1, {0, 1, 2, 3});
	TestEqual(TEXT("equal grade NEW outranks recommendation"), FGachaRevealResultTestAccessor::BestTooltipIndex(*Rig.Widget), 2);
	TestEqual(TEXT("best card shows equal-grade NEW"), FGachaRevealResultTestAccessor::BestCardName(*Rig.Widget).ToString(), FString(TEXT("Result2")));
	FGachaRevealResultTestAccessor::SortPullOrder(*Rig.Widget);
	TestEqual(TEXT("pull-order toggle keeps grade-sort best representative"), FGachaRevealResultTestAccessor::BestTooltipIndex(*Rig.Widget), 2);
	TestEqual(TEXT("pull-order toggle keeps best card"), FGachaRevealResultTestAccessor::BestCardName(*Rig.Widget).ToString(), FString(TEXT("Result2")));
	FGachaRevealResultTestAccessor::SetBestPriority(*Rig.Widget, INDEX_NONE, 1, {0, 1, 2, 3});
	TestEqual(TEXT("equal grade recommendation outranks earlier pull"), FGachaRevealResultTestAccessor::BestTooltipIndex(*Rig.Widget), 1);
	TestEqual(TEXT("recommended result fills best card"), FGachaRevealResultTestAccessor::BestCardName(*Rig.Widget).ToString(), FString(TEXT("Result1")));
	FGachaRevealResultTestAccessor::SetBestPriority(*Rig.Widget, INDEX_NONE, INDEX_NONE, {2, 1, 0, 3});
	TestEqual(TEXT("equal grade and status use earliest PullIndex"), FGachaRevealResultTestAccessor::BestTooltipIndex(*Rig.Widget), 2);
	TestEqual(TEXT("best card follows earliest PullIndex"), FGachaRevealResultTestAccessor::BestCardName(*Rig.Widget).ToString(), FString(TEXT("Result2")));
	FGachaRevealResultTestAccessor::Destroy(Rig);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultCompareContextTest,
	"CGR.Gacha.Reveal.Result.CompareContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultCompareContextTest::RunTest(const FString& Parameters)
{
	FGachaRevealResultTestAccessor::FRig Rig;
	if (!FGachaRevealResultTestAccessor::Make(*this, Rig)) { FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
	FGachaRevealResultTestAccessor::Begin(Rig, {4, 4});
	UGachaRevealPresentationWidget& Widget = *Rig.Widget;
	FGachaRevealResultTestAccessor::SetCompareEquipment(Widget);
	TestEqual(TEXT("NEW best card can be better than equipped without the recommendation badge"), FGachaRevealResultTestAccessor::BestTooltipIndex(Widget), 1);
	for (const TCHAR* Prefix : {TEXT("Best"), TEXT("Single")})
	{
		TestEqual(*FString::Printf(TEXT("%s compares actual equipped grade"), Prefix),
			FGachaRevealResultTestAccessor::CompareIndex(Widget, *FString::Printf(TEXT("%sCompare"), Prefix)), 0);
	}
	FGachaRevealResultTestAccessor::SetBestEquippedGrade(Widget, 4);
	TestEqual(TEXT("equal equipped grade is plain"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("BestCompare")), 1);
	FGachaRevealResultTestAccessor::SetBestEquippedGrade(Widget, INDEX_NONE);
	TestEqual(TEXT("empty equipped slot is better"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("BestCompare")), 0);

	FGachaRevealResultTestAccessor::SetCompareTrait(Widget, true);
	TestEqual(TEXT("confirmed NEW trait uses good comparison"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("BestCompare")), 0);
	TestEqual(TEXT("trait NEW copy"), FGachaRevealResultTestAccessor::Text(Widget, TEXT("BestCompareGoodText")), FString(TEXT("새로 얻은 특성입니다")));
	TestEqual(TEXT("single trait NEW copy"), FGachaRevealResultTestAccessor::Text(Widget, TEXT("SingleCompareGoodText")), FString(TEXT("새로 얻은 특성입니다")));
	const UWidget* TraitIcon = FGachaRevealResultTestAccessor::Named(Widget, TEXT("BestCompareGoodIconBox"));
	TestTrue(TEXT("trait NEW comparison has no equipment arrow"), TraitIcon && TraitIcon->GetVisibility() == ESlateVisibility::Collapsed);
	FGachaRevealResultTestAccessor::SetCompareTrait(Widget, false);
	TestEqual(TEXT("unknown trait ownership hides comparison"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("BestCompare")), INDEX_NONE);

	FGachaRevealResultTestAccessor::SetCompareSkin(Widget, true);
	TestEqual(TEXT("confirmed NEW skin uses good comparison"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("SingleCompare")), 0);
	TestEqual(TEXT("skin NEW copy"), FGachaRevealResultTestAccessor::Text(Widget, TEXT("BestCompareGoodText")), FString(TEXT("새로 얻은 스킨입니다")));
	const UWidget* SkinIcon = FGachaRevealResultTestAccessor::Named(Widget, TEXT("SingleCompareGoodIconBox"));
	TestTrue(TEXT("skin NEW comparison has no equipment arrow"), SkinIcon && SkinIcon->GetVisibility() == ESlateVisibility::Collapsed);
	FGachaRevealResultTestAccessor::SetCompareSkin(Widget, false);
	TestEqual(TEXT("unknown skin ownership hides comparison"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("SingleCompare")), INDEX_NONE);
	FGachaRevealResultTestAccessor::SetCompareSkinDuplicate(Widget);
	TestEqual(TEXT("duplicate skin keeps mileage comparison"), FGachaRevealResultTestAccessor::CompareIndex(Widget, TEXT("BestCompare")), 2);
	FGachaRevealResultTestAccessor::Destroy(Rig);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultMileageLimitTest,
	"CGR.Gacha.Reveal.Result.MileageLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultMileageLimitTest::RunTest(const FString& Parameters)
{
	FGachaRevealResultTestAccessor::FRig Rig;
	if (!FGachaRevealResultTestAccessor::Make(*this, Rig)) { FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
	FGachaRevealResultTestAccessor::Begin(Rig, {4});
	UGachaRevealPresentationWidget& Widget = *Rig.Widget;
	for (EGachaRevealKind Kind : {EGachaRevealKind::Trait, EGachaRevealKind::Skin})
	{
		FGachaRevealResultTestAccessor::SetMileage(Widget, Kind, 92, 0);
		TestEqual(TEXT("mileage count reaches legendary exchange tier"), FGachaRevealResultTestAccessor::Text(Widget, TEXT("PityCount")), FString(TEXT("92/150")));
		TestTrue(TEXT("mileage fill uses legendary exchange tier"), FMath::IsNearlyEqual(FGachaRevealResultTestAccessor::GaugeWidth(Widget), 420.f * 92.f / 150.f, .1f));
	}
	FGachaRevealResultTestAccessor::Destroy(Rig);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultSummaryTest,
	"CGR.Gacha.Reveal.Result.Summary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultSummaryTest::RunTest(const FString& Parameters)
{
	UGachaRevealPresentationWidget* Widget = NewObject<UGachaRevealPresentationWidget>();
	TArray<FGachaRevealEntry> Entries;
	FGachaRevealEntry& First = Entries.AddDefaulted_GetRef();
	First.Grade = 3; First.bNew = true; First.SlotKey = 0; First.bEmployeeKnown = true; First.EquippedGrade = 1;
	FGachaRevealEntry& Second = Entries.AddDefaulted_GetRef();
	Second.PullIndex = 1; Second.Grade = 1; Second.SlotKey = 1; Second.bEmployeeKnown = true; Second.EquippedGrade = 0;
	FGachaRevealEntry& RepeatedDefinition = Entries.AddDefaulted_GetRef();
	RepeatedDefinition.PullIndex = 2; RepeatedDefinition.Grade = 1; RepeatedDefinition.SlotKey = 0;
	RepeatedDefinition.bEmployeeKnown = true; RepeatedDefinition.EquippedGrade = 1;
	FGachaRevealAwardContext Award;
	// 같은 DefinitionId가 한 거래에 다시 나왔다는 계약: 첫 결과만 신규, 둘째는 결과 순서상 이미 보유.
	Award.bOwnedBefore = {false, true, true};
	FGachaRevealResultTestAccessor::Set(*Widget, EGachaRevealKind::Equipment, Entries, Award);
	TestEqual(TEXT("equipment uses grade and actual recommendation"), Widget->BuildSummaryLine().ToString(), FString(TEXT("최고 S · 새 장비 1개 · 장착 추천 2개")));

	TArray<FGachaRevealEntry> TraitEntries;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		FGachaRevealEntry& Entry = TraitEntries.AddDefaulted_GetRef();
		Entry.PullIndex = Index;
		Entry.Grade = Index == 0 ? 4 : 1;
		Entry.bNew = Index < 3;
	}
	Award.bOwnedBefore = {false, false, false, true, true, true, true, true, true, true};
	Award.MileageGained = 10;
	FGachaRevealResultTestAccessor::Set(*Widget, EGachaRevealKind::Trait, TraitEntries, Award);
	TestEqual(TEXT("trait ten-pull uses awarded mileage"), Widget->BuildSummaryLine().ToString(), FString(TEXT("최고 레전드 · 새 특성 3개 · 마일리지 +10P")));

	TArray<FGachaRevealEntry> SkinEntries;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FGachaRevealEntry& Entry = SkinEntries.AddDefaulted_GetRef();
		Entry.PullIndex = Index;
		Entry.Grade = Index == 0 ? 4 : 1;
		Entry.bNew = Index < 2;
		Entry.MileageGained = Index == 2 ? 1 : 0;
	}
	Award.MileageGained = 4;
	FGachaRevealResultTestAccessor::Set(*Widget, EGachaRevealKind::Skin, SkinEntries, Award);
	TestEqual(TEXT("skin counts duplicate and actual total"), Widget->BuildSummaryLine().ToString(), FString(TEXT("최고 레전드 · 새 스킨 2개 · 중복 1개 → 마일리지 +4P")));
	Award.MileageGained = INDEX_NONE;
	FGachaRevealResultTestAccessor::Set(*Widget, EGachaRevealKind::Skin, SkinEntries, Award);
	TestEqual(TEXT("missing authority does not invent mileage"), Widget->BuildSummaryLine().ToString(), FString(TEXT("최고 레전드")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultNoEmployeeTest,
	"CGR.Gacha.Reveal.Result.NoEmployeeHidesEquip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultNoEmployeeTest::RunTest(const FString& Parameters)
{
	UGachaRevealPresentationWidget* Widget = NewObject<UGachaRevealPresentationWidget>();
	FGachaRevealEntry Entry;
	Entry.Grade = 5;
	Entry.SlotKey = 0;
	Entry.bEmployeeKnown = false;
	TArray<FGachaRevealEntry> Entries = {Entry};
	FGachaRevealAwardContext Award;
	FGachaRevealResultTestAccessor::Set(*Widget, EGachaRevealKind::Equipment, Entries, Award);
	TestEqual(TEXT("no selected employee means no recommendation"), Widget->GetRecommendedCount(), 0);
	FGachaRevealResultTestAccessor::FRig Rig;
	if (!FGachaRevealResultTestAccessor::Make(*this, Rig)) { FGachaRevealResultTestAccessor::Destroy(Rig); return false; }
	FGachaRevealResultTestAccessor::Begin(Rig, {5});
	for (const TCHAR* Name : {TEXT("SingleEquipButton"), TEXT("SingleCompare")})
	{
		const UWidget* Item = FGachaRevealResultTestAccessor::Named(*Rig.Widget, Name);
		TestTrue(*FString::Printf(TEXT("%s hidden without employee"), Name), Item && Item->GetVisibility() == ESlateVisibility::Collapsed);
	}
	if (UButtonWidget* EquipButton = Cast<UButtonWidget>(FGachaRevealResultTestAccessor::Named(*Rig.Widget, TEXT("SingleEquipButton"))))
	{
		EquipButton->SetIsEnabled(false);
		EquipButton->SetButtonText(FText::FromString(TEXT("장착 완료")));
		FGachaRevealResultTestAccessor::RestoreDetails(*Rig.Widget);
		TestTrue(TEXT("reused result restores equip control enabled state"), EquipButton->GetIsEnabled());
		const UCommonTextBlock* Label = Cast<UCommonTextBlock>(EquipButton->GetWidgetFromName(TEXT("ButtonText")));
		TestTrue(TEXT("reused single result restores 바로 장착 label"), Label && Label->GetText().ToString() == TEXT("바로 장착"));
	}
	FGachaRevealResultTestAccessor::Destroy(Rig);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultEquipRequestTest,
	"CGR.Gacha.Reveal.Result.EquipRequest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultEquipRequestTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEmployeeEquipmentManagerSubsystem* Equipment = NewObject<UEmployeeEquipmentManagerSubsystem>(GI);
	FEmployeeEquipmentState State;
	State.Revision = 7;
	FEmployeeEquipmentInstance Granted;
	Granted.InstanceId = FGuid::NewGuid();
	Granted.DefinitionId = TEXT("HeadTest");
	State.Instances.Add(Granted.InstanceId, Granted);
	State.Loadouts.FindOrAdd(99).Slots.Add(EEmployeeEquipmentSlot::Head, Granted.InstanceId);
	Equipment->RestoreState(State);
	UGachaRevealPresentationWidget* Widget = NewObject<UGachaRevealPresentationWidget>();
	FGachaRevealResultTestAccessor::SetRecommendedInstance(*Widget, 42, Granted.InstanceId);
	const FEmployeeEquipmentRequest First = FGachaRevealResultTestAccessor::Request(*Widget, *Equipment);
	const FEmployeeEquipmentRequest Second = FGachaRevealResultTestAccessor::Request(*Widget, *Equipment);
	TestEqual(TEXT("target employee"), First.EmployeeId, 42);
	TestTrue(TEXT("slot"), First.EquipmentSlot == EEmployeeEquipmentSlot::Head);
	TestTrue(TEXT("instance"), First.InstanceId == Granted.InstanceId);
	TestEqual(TEXT("actual source owner"), First.ExpectedSourceEmployeeId, 99);
	TestEqual(TEXT("current revision"), First.ExpectedRevision, int64(7));
	TestTrue(TEXT("each slot execution makes a fresh request"), First.OperationId.IsValid() && First.OperationId != Second.OperationId);
	FEmployeeEquipmentInstance Eyewear;
	Eyewear.InstanceId = FGuid::NewGuid();
	Eyewear.DefinitionId = TEXT("EyewearTest");
	State.Instances.Add(Eyewear.InstanceId, Eyewear);
	FGachaRevealResultTestAccessor::SetRecommendedPair(*Widget, Granted.InstanceId, Eyewear.InstanceId);
	Equipment->RestoreState(State);
	TestEqual(TEXT("both recommended slots initially pending"), FGachaRevealResultTestAccessor::Pending(*Widget, *Equipment).Num(), 2);
	State.Loadouts.FindOrAdd(42).Slots.Add(EEmployeeEquipmentSlot::Head, Granted.InstanceId);
	State.Loadouts.FindOrAdd(99).Slots.Remove(EEmployeeEquipmentSlot::Head);
	Equipment->RestoreState(State);
	const TArray<int32> Retry = FGachaRevealResultTestAccessor::Pending(*Widget, *Equipment);
	TestTrue(TEXT("partial success retries only unfinished slot"), Retry.Num() == 1 && Retry[0] == 1);
	State.Loadouts.FindOrAdd(42).Slots.Add(EEmployeeEquipmentSlot::Eyewear, Eyewear.InstanceId);
	Equipment->RestoreState(State);
	TestEqual(TEXT("all actual wearer slots complete"), FGachaRevealResultTestAccessor::Pending(*Widget, *Equipment).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGachaRevealResultPityHitsTest,
	"CGR.Gacha.Reveal.Result.PityHits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGachaRevealResultPityHitsTest::RunTest(const FString& Parameters)
{
	UGachaRevealPresentationWidget* Widget = NewObject<UGachaRevealPresentationWidget>();
	TestEqual(TEXT("without SSS, positive final counter interpolates directly"),
		FGachaRevealResultTestAccessor::PityAt(*Widget, {1, 2, 3}, 2, 8, .5f), 5.f);
	TestTrue(TEXT("SSS fills even when post counter exceeds pre counter"),
		FGachaRevealResultTestAccessor::PityAt(*Widget, {1, 5, 2}, 2, 8, .49f) > 38.f);
	TestTrue(TEXT("SSS resets before final counter"),
		FGachaRevealResultTestAccessor::PityAt(*Widget, {1, 5, 2}, 2, 8, .51f) < 1.f);
	TestTrue(TEXT("two SSS rewards fill second time"),
		FGachaRevealResultTestAccessor::PityAt(*Widget, {5, 1, 5, 2}, 2, 8, .65f) > 37.f);
	TestTrue(TEXT("two SSS rewards reset second time"),
		FGachaRevealResultTestAccessor::PityAt(*Widget, {5, 1, 5, 2}, 2, 8, .68f) < 1.f);
	TestEqual(TEXT("gauge ends at authoritative after counter"),
		FGachaRevealResultTestAccessor::PityAt(*Widget, {5, 1, 5, 2}, 2, 8, 1.f), 8.f);
	return true;
}

#endif
