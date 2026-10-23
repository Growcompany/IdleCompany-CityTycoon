#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/GachaRevealTestTables.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Gacha/GachaRevealBoxWidget.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "UI/UISoundTags.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

// 공개 위젯의 friend(이름 고정) ― 게임 인스턴스 없는 시험 월드에서 표·연출 감소를 넣고 내부 상태를 읽는다
struct FGachaRevealWidgetTestAccessor
{
	using FWidget = UGachaRevealPresentationWidget;

	static void Prepare(FWidget& W, const UTableManagerSubsystem& Tables, bool bReduceMotion)
	{
		W.TablesOverride = &Tables;
		W.ReduceMotionOverride = bReduceMotion;
		W.SetDisableExitFade(true);
	}

	static void Begin(FWidget& W, const TArray<int32>& Grades)
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
			Card.Grade = Grades[Index];
			Card.Name = FText::FromString(FString::Printf(TEXT("시험 장비 %d"), Index));
			Meta.Add(FText::FromString(TEXT("머리 · 장비")));
		}
		W.BeginReveal(EGachaRevealKind::Equipment, EGachaBoxType::EquipmentAdvanced, FText::FromString(TEXT("고급 장비 뽑기 · 시험")), Entries, Cards, Meta);
	}

	static bool Back(FWidget& W) { return W.HandleBackRequest(); }
	static UButtonWidget* Skip(const FWidget& W) { return W.SkipButton; }
	static void ClickConfirm(FWidget& W) { W.OnConfirmClicked(); }
	static int32 EntryCount(const FWidget& W) { return W.Entries.Num(); }
	static int32 CardCount(const FWidget& W) { return W.DealtCards.Num(); }
	static bool ResultEntered(const FWidget& W) { return W.ResultClockStartMs >= 0.f; }
	static float Width(const FWidget& W) { return W.StageWidth(); }
	static void SetWidth(FWidget& W, float InWidth) { W.LastGeometryWidth = InWidth; }
	static UButtonWidget* Confirm(const FWidget& W) { return W.ConfirmButton; }
	static UWidget* SpotlightRoot(const FWidget& W) { return W.SpotlightRoot; }
	static UGachaRevealCardWidget* SpotlightCard(const FWidget& W) { return W.SpotlightCard; }
	static UGachaRevealCardWidget* DealtCard(const FWidget& W, int32 Index) { return W.DealtCards.IsValidIndex(Index) ? W.DealtCards[Index] : nullptr; }
	static UWidget* Box(const FWidget& W) { return W.BoxWidget.Get(); }
	static float Speed(const FWidget& W) { return W.Runner.GetSpeed(); }
	// 실제 틱 한 번(NativeTick 과 같은 길) ― DebugAdvanceTime 은 1/60 로 나눠 한 틱에 여러 박자가 안 생긴다
	static void Step(FWidget& W, float Seconds) { W.StepTick(Seconds); }
	// Slate 해제 = SObjectWidget::ResetWidget → NativeDestruct(엔진 경로). 시험 월드엔 Slate 가 없어 직접 부른다
	static void SlateReleased(FWidget& W) { W.NativeDestruct(); }
	static const TArray<int32>& Tally(const FWidget& W) { return W.TallyCounts; }
	static void Bump(FWidget& W, int32 Grade) { W.BumpTally(Grade, 0.f); }
	static UWidget* Named(const FWidget& W, const TCHAR* Name) { return W.WidgetTree ? W.WidgetTree->FindWidget(FName(Name)) : nullptr; }
	static const TMap<FName, int32>& Events(const FWidget& W) { return W.TestEventCounts; }
	static int32 Event(const FWidget& W, FName Key) { const int32* Found = W.TestEventCounts.Find(Key); return Found ? *Found : 0; }
	static bool CardAt(const UGachaRevealCardWidget* Card, float X, float Y, float Width, float Height)
	{
		const UCanvasPanelSlot* CanvasSlot = Card ? Cast<UCanvasPanelSlot>(Card->Slot) : nullptr;
		return CanvasSlot && CanvasSlot->GetAnchors().Minimum.Equals(FVector2D::ZeroVector, .01f)
			&& CanvasSlot->GetAnchors().Maximum.Equals(FVector2D::ZeroVector, .01f)
			&& CanvasSlot->GetAlignment().Equals(FVector2D::ZeroVector, .01f)
			&& !CanvasSlot->GetAutoSize()
			&& CanvasSlot->GetPosition().Equals(FVector2D(X, Y), .01f)
			&& CanvasSlot->GetSize().Equals(FVector2D(Width, Height), .01f);
	}

	static int32 FaceUpVisibleCount(const FWidget& W)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < W.DealtCards.Num(); ++Index)
		{
			const UGachaRevealCardWidget* Card = W.DealtCards[Index];
			Count += Card && W.CardMotion.IsValidIndex(Index) && W.CardMotion[Index].bFaceUp && !W.CardMotion[Index].bHidden
				&& Card->GetRenderOpacity() > .99f ? 1 : 0;
		}
		return Count;
	}

	// 결과 칸 = 정렬 순서(등급순)의 ComputeSlots(n, 폭, true)
	static bool CardsOnSettledSlots(const FWidget& W)
	{
		const TArray<int32> Order = GachaRevealModel::SortedOrder(W.Entries, EGachaRevealSort::Grade, W.Recommended);
		const TArray<FGachaRevealRect> Slots = GachaRevealModel::ComputeSlots(W.Entries.Num(), FMath::RoundToFloat(W.StageWidth()), true);
		bool bAll = Order.Num() == W.DealtCards.Num();
		for (int32 Rank = 0; bAll && Rank < Order.Num(); ++Rank)
		{
			const UGachaRevealCardWidget* Card = W.DealtCards[Order[Rank]];
			const UCanvasPanelSlot* CanvasSlot = Card ? Cast<UCanvasPanelSlot>(Card->Slot) : nullptr;
			bAll = CanvasSlot && Slots.IsValidIndex(Rank)
				&& CanvasSlot->GetPosition().Equals(FVector2D(Slots[Rank].X, Slots[Rank].Y), .01f)
				&& CanvasSlot->GetSize().Equals(FVector2D(Slots[Rank].W, Slots[Rank].H), .01f);
		}
		return bAll;
	}
};

namespace GachaRevealWidgetTestUtil
{
	using FAccess = FGachaRevealWidgetTestAccessor;

	struct FRig
	{
		UWorld* World = nullptr;
		TStrongObjectPtr<UTableManagerSubsystem> Tables;
		TStrongObjectPtr<UGachaRevealPresentationWidget> Widget;
		int32 ClosedCount = 0;
	};

	// 게임 인스턴스 없는 EditorPreview 월드 ― Game 월드는 남의 월드 하위 시스템을 깨워 경고를 끌어온다(Task 7 판정)
	bool Make(FAutomationTestBase& Test, FRig& Rig, bool bReduceMotion)
	{
		const TSubclassOf<UUserWidget> Class = GachaRevealTestTables::RegisteredWidgetClass(EWidgetType::GachaRevealPresentation);
		if (!Test.TestNotNull(TEXT("DT_WidgetClass 에 GachaRevealPresentation 행"), Class.Get())
			|| !Test.TestTrue(TEXT("행 클래스가 UGachaRevealPresentationWidget"), Class->IsChildOf(UGachaRevealPresentationWidget::StaticClass())))
		{
			return false;
		}
		Rig.World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (GEngine)
		{
			GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(Rig.World);
		}
		Rig.Tables = GachaRevealTestTables::MakeWidgetTables();
		Rig.Widget.Reset(CreateWidget<UGachaRevealPresentationWidget>(Rig.World, Class));
		if (!Test.TestNotNull(TEXT("공개 위젯 생성"), Rig.Widget.Get()))
		{
			return false;
		}
		FAccess::Prepare(*Rig.Widget, *Rig.Tables, bReduceMotion);
		int32* Closed = &Rig.ClosedCount;
		Rig.Widget->OnPresentationClosed.BindLambda([Closed]() { ++*Closed; });
		// 상자 위젯은 자기 게임 인스턴스의 표만 본다 ― 시험 월드에는 없어 그림 없이 진행하며 그 경고는 정상
		Test.AddExpectedMessagePlain(TEXT("[GachaRevealBox] TableManager 없음"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0);
		return true;
	}

	void Destroy(FRig& Rig)
	{
		Rig.Widget.Reset();
		if (Rig.World)
		{
			if (GEngine)
			{
				GEngine->DestroyWorldContext(Rig.World);
			}
			Rig.World->DestroyWorld(false);
			Rig.World = nullptr;
		}
	}

	// 실행기는 한 번에 0.1초까지만 쓴다 ― 위젯 DebugAdvanceTime 이 1/60 로 나눈다
	bool AdvanceUntil(UGachaRevealPresentationWidget& W, TFunctionRef<bool()> Done, float LimitSec = 20.f)
	{
		for (float Elapsed = 0.f; Elapsed < LimitSec; Elapsed += 1.f / 60.f)
		{
			if (Done())
			{
				return true;
			}
			W.DebugAdvanceTime(1.f / 60.f);
		}
		return Done();
	}

	const TArray<int32> TenGrades = { 0, 1, 2, 0, 4, 0, 1, 3, 0, 2 };

	FName TagName(const FGameplayTag& Tag) { return Tag.GetTagName(); }

	int32 StampMembers(const FGachaRevealPlan& Plan)
	{
		int32 Members = 0;
		for (const TPair<int32, TArray<int32>>& Group : Plan.StampGroups)
		{
			Members += Group.Value.Num();
		}
		return Members;
	}

	// 한 번 공개(10회, 손 긋기 끔)에서 1회성 사건이 나야 하는 횟수 ― 실행기 계획에서 센다(시안 :797-1066)
	TMap<FName, int32> ExpectedEvents(const FGachaRevealPlan& Plan, const TArray<int32>& Grades)
	{
		TMap<FName, int32> Out;
		auto Add = [&Out](FName Key, int32 Count) { if (Count > 0) { Out.FindOrAdd(Key) += Count; } };
		const int32 Steps = Plan.bUpgrade ? FMath::Max(0, Plan.UpgradeSteps.Num() - 1) : 0;
		const bool bTop = Grades.IsValidIndex(Plan.TopIndex);
		const int32 TopGrade = bTop ? Grades[Plan.TopIndex] : INDEX_NONE;
		const int32 Groups = Plan.StampGroups.Num();
		int32 LowA = 0;
		int32 LowTok = 0;
		for (int32 G : Grades)
		{
			LowA += G == 2 ? 1 : 0;
			LowTok += G < 2 ? 1 : 0;
		}
		Add(TagName(CGUISoundTags::GachaBoxLand), 1);
		Add(TagName(CGUISoundTags::GachaBoxZip), 1);
		Add(TagName(CGUISoundTags::GachaBoxZipEnd), 1);
		Add(TagName(CGUISoundTags::GachaBoxJolt1), FMath::Min(Steps, 1));
		Add(TagName(CGUISoundTags::GachaBoxJolt2), FMath::Max(0, Steps - 1));
		Add(TagName(Plan.BestGrade >= GachaRevealModel::HighGrade ? CGUISoundTags::GachaBoxLidHigh : CGUISoundTags::GachaBoxLid), 1);
		Add(TagName(CGUISoundTags::GachaBoxDeal10), 1);
		Add(TagName(CGUISoundTags::GachaBoxFlipA), LowA);
		Add(TEXT("Tok"), LowTok + (bTop ? 1 : 0));
		Add(TagName(CGUISoundTags::GachaBoxRiseStamp), Groups);
		Add(TagName(CGUISoundTags::GachaBoxStamp), Groups + (bTop ? 1 : 0));
		if (bTop)
		{
			Add(TagName(TopGrade >= 5 ? CGUISoundTags::GachaBoxRiseSSS : (TopGrade >= 4 ? CGUISoundTags::GachaBoxRiseSS : CGUISoundTags::GachaBoxRiseS)), 1);
			Add(TagName(TopGrade >= 5 ? CGUISoundTags::GachaBoxHitSSS : (TopGrade >= 4 ? CGUISoundTags::GachaBoxHitSS : CGUISoundTags::GachaBoxHitS)), 1);
		}
		Add(TagName(CGUISoundTags::GachaBoxSettle), 1);
		// 착지 · 승급 단계마다 · 뚜껑 · 도장 묶음마다 · 단독 장면 박힘(+SSS 두 번째) · 정리
		Add(TEXT("Shake"), 1 + Steps + 1 + Groups + (bTop ? 1 : 0) + (TopGrade >= 5 ? 1 : 0) + 1);
		Add(TEXT("Stamp"), StampMembers(Plan) + (bTop ? 1 : 0));
		Add(TEXT("Flip"), Plan.LowCount + StampMembers(Plan));
		return Out;
	}

	// 사건 이름마다 정확히 기대만큼 ― 톡 3변형(무작위)은 합이 Tok 과 같아야 한다
	void ExpectEventsExactly(FAutomationTestBase& Test, const FString& What, const TMap<FName, int32>& Actual, const TMap<FName, int32>& Expected)
	{
		TMap<FName, int32> Rest = Actual;
		int32 TokVariants = 0;
		for (const FGameplayTag& Variant : { CGUISoundTags::GachaBoxTok.GetTag(), CGUISoundTags::GachaBoxTok2.GetTag(), CGUISoundTags::GachaBoxTok3.GetTag() })
		{
			int32 Count = 0;
			if (Rest.RemoveAndCopyValue(Variant.GetTagName(), Count))
			{
				TokVariants += Count;
			}
		}
		Test.TestEqual(*(What + TEXT(": 톡 3변형 합 = Tok")), TokVariants, Expected.FindRef(TEXT("Tok")));
		for (const TPair<FName, int32>& Pair : Expected)
		{
			Test.TestEqual(*(What + TEXT(": ") + Pair.Key.ToString() + TEXT(" 횟수")), Rest.FindRef(Pair.Key), Pair.Value);
		}
		for (const TPair<FName, int32>& Pair : Rest)
		{
			Test.TestTrue(*(What + TEXT(": 기대 밖 사건 없음 ") + Pair.Key.ToString()), Expected.Contains(Pair.Key));
		}
	}

	struct FCatchUpRun
	{
		TMap<FName, int32> Events;
		TMap<FName, int32> Expected;
		TArray<int32> TallyAtSettle;
		int32 StampsAtSettle = 0;
		int32 StampMemberCount = 0;
		bool bSettle = false;
		bool bResult = false;
		bool bLidInOneTick = false;
		int32 MultiBeatTicks = 0;
	};

	// 첫 틱 뒤 탭(4배), 그 뒤 StepSec 틱마다 탭 ― 도장·단독 장면이 배속을 풀기 때문(단독 장면 대기 중 탭 = 그 대기만 끝)
	FCatchUpRun RunCatchUp(UGachaRevealPresentationWidget& W, float StepSec, bool bAlignLid)
	{
		FCatchUpRun Run;
		FAccess::Begin(W, TenGrades);
		FAccess::Step(W, 1.f / 60.f);
		W.DebugTap();
		const FGachaRevealPlan& Plan = W.GetRunner().GetPlan();
		Run.Expected = ExpectedEvents(Plan, TenGrades);
		Run.StampMemberCount = StampMembers(Plan);
		const int32 LidIndex = Plan.Beats.IndexOfByPredicate([](const FGachaRevealBeatSpec& Spec) { return Spec.Beat == EGachaRevealBeat::Lid; });
		if (bAlignLid && Plan.Beats.IsValidIndex(LidIndex))
		{
			// 뚜껑 전엔 대기가 없어 시계 = 명목 시각. 한 틱(배속 × StepSec)이 뚜껑 50ms 앞에서 시작하면 뚜껑 240 이 그 틱 안에서 시작하고 끝난다
			const float Span = StepSec * 1000.f * GachaRevealTiming::SkipTimeScale;
			const float Lead = FMath::Fmod(Plan.Beats[LidIndex].StartMs - 50.f - W.GetRunner().GetClockMs(), Span);
			if (Lead > 0.f)
			{
				FAccess::Step(W, Lead / GachaRevealTiming::SkipTimeScale / 1000.f);
			}
		}
		for (int32 Tick = 0; Tick < 3000 && W.IsPresentationOpen() && !W.IsOnBeat(EGachaRevealBeat::Settle); ++Tick)
		{
			const int32 Before = W.GetRunner().GetBeatIndex();
			W.DebugTap();
			FAccess::Step(W, StepSec);
			const int32 After = W.GetRunner().GetBeatIndex();
			Run.MultiBeatTicks += After - Before >= 2 ? 1 : 0;
			Run.bLidInOneTick = Run.bLidInOneTick || (Before < LidIndex && After > LidIndex);
		}
		Run.bSettle = W.IsOnBeat(EGachaRevealBeat::Settle);
		Run.TallyAtSettle = FAccess::Tally(W);
		Run.StampsAtSettle = FAccess::Event(W, TEXT("Stamp"));
		for (int32 Tick = 0; Tick < 600 && W.IsPresentationOpen() && !W.IsOnBeat(EGachaRevealBeat::Result); ++Tick)
		{
			FAccess::Step(W, StepSec);
		}
		Run.bResult = W.IsOnBeat(EGachaRevealBeat::Result);
		// 결과 화면에서 더 흘러도 다시 쏘지 않는지
		for (float Elapsed = 0.f; Elapsed < .6f; Elapsed += StepSec)
		{
			FAccess::Step(W, StepSec);
		}
		Run.Events = FAccess::Events(W);
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealWidgetSkipImmediatelyTest,
	"CGR.Gacha.Reveal.Widget.SkipImmediately",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetSkipImmediatelyTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	for (const EGachaRevealBeat Target : { EGachaRevealBeat::Throw, EGachaRevealBeat::Deal, EGachaRevealBeat::Stamp, EGachaRevealBeat::Spotlight })
	{
		FRig Rig;
		ON_SCOPE_EXIT { Destroy(Rig); };
		if (!Make(*this, Rig, false)) { return false; }
		UGachaRevealPresentationWidget& W = *Rig.Widget;
		const TSharedRef<SWidget> SlateWidget = W.TakeWidget();
		FAccess::Begin(W, TenGrades);
		TestTrue(TEXT("건너뛰기 대상 장면 도달"), AdvanceUntil(W, [&W, Target]() { return W.IsOnBeat(Target); }));
		UButtonWidget* SkipControl = FAccess::Skip(W);
		if (!TestNotNull(TEXT("실제 WBP 건너뛰기 버튼"), SkipControl)) { return false; }
		SkipControl->OnClicked().Broadcast();
		TestTrue(TEXT("추가 틱 없이 즉시 Result"), W.IsOnBeat(EGachaRevealBeat::Result));
		TestTrue(TEXT("결과 레이아웃 초기화"), FAccess::ResultEntered(W));
		TestEqual(TEXT("모든 카드 공개"), FAccess::FaceUpVisibleCount(W), 10);
		TestTrue(TEXT("카드를 결과 위치로 정렬"), FAccess::CardsOnSettledSlots(W));
		TestFalse(TEXT("연속 클릭 방지 확인 대기"), W.GetRunner().IsConfirmEnabled());
		SkipControl->OnClicked().Broadcast();
		TestEqual(TEXT("중복 건너뛰기는 닫지 않음"), Rig.ClosedCount, 0);
		TestEqual(TEXT("중복 건너뛰기는 항목을 바꾸지 않음"), FAccess::EntryCount(W), 10);
		W.DebugAdvanceTime(.51f);
		TestTrue(TEXT("대기 후 확인 가능"), W.GetRunner().IsConfirmEnabled());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealFlowNoDoubleOpenTest,
	"CGR.Gacha.Reveal.Flow.NoDoubleOpen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealFlowNoDoubleOpenTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	FRig Rig;
	ON_SCOPE_EXIT { Destroy(Rig); };
	if (!Make(*this, Rig, false))
	{
		return false;
	}
	UGachaRevealPresentationWidget& W = *Rig.Widget;
	// 열린 동안 두 번 = 연출 중 1번 + 결과 화면 1번(검토 초점 2)
	AddExpectedMessagePlain(TEXT("[GachaReveal] 공개 화면이 이미 열려 있어"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);

	FAccess::Begin(W, TenGrades);
	W.DebugAdvanceTime(1.f / 60.f);
	TestTrue(TEXT("첫 틱에 Throw"), W.IsOnBeat(EGachaRevealBeat::Throw));
	TestTrue(TEXT("열림"), W.IsPresentationOpen());

	TArray<FBuildingTraitGachaResult> Second;
	Second.AddDefaulted();
	W.SetupPullTrait(Second, false);
	TestEqual(TEXT("연출 중 두 번째 Setup = 항목 그대로 10"), FAccess::EntryCount(W), 10);
	TestEqual(TEXT("카드 그대로 10"), FAccess::CardCount(W), 10);
	TestTrue(TEXT("박자 그대로 Throw"), W.IsOnBeat(EGachaRevealBeat::Throw));

	W.DebugSkipToResult();
	TestTrue(TEXT("결과 화면"), W.IsOnBeat(EGachaRevealBeat::Result));
	W.SetupPullTrait(Second, false);
	TestEqual(TEXT("결과 화면에서 두 번째 Setup = 그대로"), FAccess::EntryCount(W), 10);
	TestEqual(TEXT("닫힘 통지 아직 0"), Rig.ClosedCount, 0);

	// 진행 중 뒤로가기 = 결과까지 건너뛴 뒤 닫힘(같은 객체를 다시 열어 확인)
	TestTrue(TEXT("뒤로가기 받음"), FAccess::Back(W));
	TestEqual(TEXT("닫힘 통지 1번"), Rig.ClosedCount, 1);
	TestFalse(TEXT("닫힌 뒤 열림 아님"), W.IsPresentationOpen());

	FAccess::Begin(W, TenGrades);
	TestTrue(TEXT("닫힌 뒤 같은 객체 다시 열림"), W.IsPresentationOpen());
	AdvanceUntil(W, [&W]() { return W.IsOnBeat(EGachaRevealBeat::Deal); });
	TestTrue(TEXT("뿌리기까지 진행"), W.IsOnBeat(EGachaRevealBeat::Deal));
	TestTrue(TEXT("뿌리기 중 뒤로가기 받음"), FAccess::Back(W));
	TestTrue(TEXT("뒤로가기가 결과 진입 경로를 거침(SkipToResult)"), FAccess::ResultEntered(W));
	TestEqual(TEXT("닫힘 통지 누적 2번"), Rig.ClosedCount, 2);
	TestFalse(TEXT("두 번째도 닫힘"), W.IsPresentationOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealWidgetTapAdvancesOneBeatTest,
	"CGR.Gacha.Reveal.Widget.TapAdvancesOneBeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetTapAdvancesOneBeatTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	FRig Rig;
	ON_SCOPE_EXIT { Destroy(Rig); };
	if (!Make(*this, Rig, false))
	{
		return false;
	}
	UGachaRevealPresentationWidget& W = *Rig.Widget;
	FAccess::Begin(W, TenGrades);
	const bool bWaiting = AdvanceUntil(W, [&W]() { return W.IsOnBeat(EGachaRevealBeat::Spotlight) && W.GetRunner().IsWaiting(); });
	if (!TestTrue(TEXT("단독 장면 대기에 닿음"), bWaiting))
	{
		return false;
	}
	TestEqual(TEXT("대기 동안 SpotlightRoot 보임"), FAccess::SpotlightRoot(W)->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	// R11: 1번째 = 대기만 끝, 2·3번째 = 배속(아직 Spotlight)
	W.DebugTap();
	TestFalse(TEXT("1번째 탭: 대기 끝"), W.GetRunner().IsWaiting());
	TestTrue(TEXT("1번째 탭: 아직 Spotlight"), W.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestEqual(TEXT("1번째 탭: 배속 1"), FAccess::Speed(W), 1.f);
	W.DebugTap();
	W.DebugTap();
	TestTrue(TEXT("2·3번째 탭 뒤에도 Spotlight"), W.IsOnBeat(EGachaRevealBeat::Spotlight));
	TestEqual(TEXT("2·3번째 탭 = 배속 4"), FAccess::Speed(W), GachaRevealTiming::SkipTimeScale);

	W.DebugAdvanceTime(.2f);
	TestTrue(TEXT("0.2초 뒤 Settle"), W.IsOnBeat(EGachaRevealBeat::Settle));
	TestEqual(TEXT("단독 장면 끝나면 SpotlightRoot 접힘(카드 탭을 막지 않게)"), FAccess::SpotlightRoot(W)->GetVisibility(), ESlateVisibility::Collapsed);
	TestNotEqual(TEXT("Settle 동안 [확인] 숨김"), FAccess::Confirm(W)->GetVisibility(), ESlateVisibility::Visible);

	AdvanceUntil(W, [&W]() { return W.IsOnBeat(EGachaRevealBeat::Result); });
	TestTrue(TEXT("결과 화면"), W.IsOnBeat(EGachaRevealBeat::Result));
	W.DebugAdvanceTime(.4f);
	TestNotEqual(TEXT("Result+0.4초 [확인] 숨김"), FAccess::Confirm(W)->GetVisibility(), ESlateVisibility::Visible);
	FAccess::ClickConfirm(W);
	TestTrue(TEXT("켜지기 전 [확인] 누름 무시"), W.IsPresentationOpen());
	TestEqual(TEXT("켜지기 전 누름은 닫힘 통지 없음"), Rig.ClosedCount, 0);

	W.DebugAdvanceTime(.15f);
	TestEqual(TEXT("Result+0.55초 [확인] 보임"), FAccess::Confirm(W)->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("결과 화면 카드 10장 앞면"), FAccess::FaceUpVisibleCount(W), 10);
	TestTrue(TEXT("결과 칸 = 등급순 정리 칸"), FAccess::CardsOnSettledSlots(W));
	FAccess::ClickConfirm(W);
	TestFalse(TEXT("[확인] = 닫힘"), W.IsPresentationOpen());
	TestEqual(TEXT("닫힘 통지 1번"), Rig.ClosedCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealWidgetSingleCardSizeTest,
	"CGR.Gacha.Reveal.Widget.SingleCardSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetSingleCardSizeTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	FRig Rig;
	ON_SCOPE_EXIT { Destroy(Rig); };
	if (!Make(*this, Rig, false)) { return false; }
	UGachaRevealPresentationWidget& W = *Rig.Widget;
	const TSharedRef<SWidget> SlateWidget = W.TakeWidget();
	FAccess::Begin(W, {4});
	const float Width = FAccess::Width(W);
	TestTrue(TEXT("1회 배분 카드 200"), FAccess::CardAt(FAccess::DealtCard(W, 0), Width / 2.f - 100.f, 460.f, 200.f, 200.f));
	if (!TestTrue(TEXT("1회 고등급 단독 장면 도달"), AdvanceUntil(W, [&W]()
		{ return W.IsOnBeat(EGachaRevealBeat::Spotlight) && FAccess::SpotlightCard(W)->GetRenderOpacity() > .99f; }))) { return false; }
	TestTrue(TEXT("1회 단독 장면 카드 220"), FAccess::CardAt(FAccess::SpotlightCard(W), Width / 2.f - 360.f, 450.f, 220.f, 220.f));
	TestTrue(TEXT("1회 단독 장면도 Single 묶음"), FAccess::SpotlightCard(W)->GetCardSize() == EGachaCardSize::Single);
	W.DebugSkipToResult();
	TestTrue(TEXT("고등급 건너뛰기 결과 카드 200"), FAccess::CardAt(FAccess::DealtCard(W, 0), Width / 2.f - 340.f, 460.f, 200.f, 200.f));
	FAccess::Back(W);

	FAccess::SetWidth(W, 3120.f);
	FAccess::Begin(W, TenGrades);
	const float WideWidth = FAccess::Width(W);
	if (!TestTrue(TEXT("재사용 후 다회 단독 장면 도달"), AdvanceUntil(W, [&W]()
		{ return W.IsOnBeat(EGachaRevealBeat::Spotlight) && FAccess::SpotlightCard(W)->GetRenderOpacity() > .99f; }))) { return false; }
	TestTrue(TEXT("다회 단독 장면 슬롯 복원"), FAccess::CardAt(FAccess::SpotlightCard(W), WideWidth / 2.f - 620.f, 330.f, 560.f, 560.f));
	TestTrue(TEXT("다회 단독 장면 Spot 묶음"), FAccess::SpotlightCard(W)->GetCardSize() == EGachaCardSize::Spot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealWidgetReduceMotionTest,
	"CGR.Gacha.Reveal.Widget.ReduceMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetReduceMotionTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	FRig Rig;
	ON_SCOPE_EXIT { Destroy(Rig); };
	if (!Make(*this, Rig, true))
	{
		return false;
	}
	UGachaRevealPresentationWidget& W = *Rig.Widget;
	const TArray<int32> Thirty = { 5, 0, 0, 1, 2, 0, 0, 3, 0, 1, 0, 0, 2, 0, 4, 0, 0, 1, 0, 0, 3, 0, 0, 2, 0, 1, 0, 0, 0, 0 };
	FAccess::Begin(W, Thirty);
	W.DebugAdvanceTime(1.f / 60.f);
	TestTrue(TEXT("연출 감소: 첫 틱에 Result"), W.IsOnBeat(EGachaRevealBeat::Result));
	TestEqual(TEXT("30장 전부 앞면·보임"), FAccess::FaceUpVisibleCount(W), 30);
	TestTrue(TEXT("결과 칸 = 등급순 정리 칸"), FAccess::CardsOnSettledSlots(W));
	TestEqual(TEXT("상자 접힘"), FAccess::Box(W)->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("단독 장면 접힘"), FAccess::SpotlightRoot(W)->GetVisibility(), ESlateVisibility::Collapsed);
	TestNotEqual(TEXT("[확인]은 바로 안 나옴"), FAccess::Confirm(W)->GetVisibility(), ESlateVisibility::Visible);
	W.DebugAdvanceTime(.55f);
	TestEqual(TEXT("+0.5초 뒤 [확인]"), FAccess::Confirm(W)->GetVisibility(), ESlateVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealWidgetCatchUpFiresOnceTest,
	"CGR.Gacha.Reveal.Widget.CatchUpFiresOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetCatchUpFiresOnceTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	FRig Rig;
	ON_SCOPE_EXIT { Destroy(Rig); };
	if (!Make(*this, Rig, false))
	{
		return false;
	}
	UGachaRevealPresentationWidget& W = *Rig.Widget;
	TArray<int32> GradeCounts;
	GradeCounts.Init(0, GachaRevealModel::MaxGrade + 1);
	for (int32 G : TenGrades)
	{
		++GradeCounts[G];
	}

	// R5: 10fps 복귀(4배 × 0.1초 = 박자 400ms 한 틱)와 보통 프레임(1/60)이 같은 사건 수를 낸다 ― 둘 다 같은 기대 표와 대조
	const FCatchUpRun Slow = RunCatchUp(W, .1f, true);
	FAccess::ClickConfirm(W);
	const FCatchUpRun Frame = RunCatchUp(W, 1.f / 60.f, false);
	FAccess::ClickConfirm(W);
	TestEqual(TEXT("두 번 다 [확인]으로 닫힘"), Rig.ClosedCount, 2);
	TestTrue(TEXT("0.1초 틱: 뚜껑이 한 틱 안에서 시작하고 끝남"), Slow.bLidInOneTick);
	TestTrue(TEXT("0.1초 틱: 한 틱에 박자를 2개 이상 지난 틱이 있음"), Slow.MultiBeatTicks > 0);
	for (const FCatchUpRun* Run : { &Slow, &Frame })
	{
		const FString What = Run == &Slow ? TEXT("0.1초 틱") : TEXT("1/60 틱");
		TestTrue(*(What + TEXT(": Settle 닿음")), Run->bSettle);
		TestTrue(*(What + TEXT(": Result 닿음")), Run->bResult);
		// 결과 화면은 띠를 다시 세므로 Settle 진입 때 본다 ― 뒤집기·도장·단독 장면 모두 카드마다 정확히 +1
		for (int32 G = 0; G < GradeCounts.Num(); ++G)
		{
			TestEqual(*FString::Printf(TEXT("%s: 등급 %d 띠 = 결과 장수"), *What, G), Run->TallyAtSettle.IsValidIndex(G) ? Run->TallyAtSettle[G] : -1, GradeCounts[G]);
		}
		TestEqual(*(What + TEXT(": 도장 = 도장 묶음 장수 + 단독 장면 1")), Run->StampsAtSettle, Run->StampMemberCount + 1);
		ExpectEventsExactly(*this, What, Run->Events, Run->Expected);
	}

	// M-6: 정리 중 뒤로가기 = Result 로 뛸 뿐 정리 착지음·흔들림을 다시 내지 않는다
	FAccess::Begin(W, TenGrades);
	for (int32 Tick = 0; Tick < 3000 && !W.IsOnBeat(EGachaRevealBeat::Settle); ++Tick)
	{
		W.DebugTap();
		FAccess::Step(W, 1.f / 60.f);
	}
	if (!TestTrue(TEXT("세 번째: Settle 닿음"), W.IsOnBeat(EGachaRevealBeat::Settle)))
	{
		return false;
	}
	const int32 ShakesBefore = FAccess::Event(W, TEXT("Shake"));
	TestEqual(TEXT("정리 시작 직후 착지음 아직 0"), FAccess::Event(W, TagName(CGUISoundTags::GachaBoxSettle)), 0);
	TestTrue(TEXT("정리 중 뒤로가기 받음"), FAccess::Back(W));
	TestTrue(TEXT("뒤로가기가 결과 진입 경로를 거침"), FAccess::ResultEntered(W));
	TestEqual(TEXT("뒤로가기로 정리 착지음 안 남"), FAccess::Event(W, TagName(CGUISoundTags::GachaBoxSettle)), 0);
	TestEqual(TEXT("뒤로가기로 흔들림 안 늘어남"), FAccess::Event(W, TEXT("Shake")), ShakesBefore);
	TestEqual(TEXT("닫힘 통지 3번"), Rig.ClosedCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCGRGachaRevealWidgetTallyFreshSessionTest,
    "CGR.Gacha.Reveal.Widget.TallyFreshSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetTallyFreshSessionTest::RunTest(const FString& Parameters)
{
    using namespace GachaRevealWidgetTestUtil;
    FRig Rig;
    ON_SCOPE_EXIT { Destroy(Rig); };
    if (!Make(*this, Rig, true)) { return false; }
    UGachaRevealPresentationWidget& W = *Rig.Widget;
    auto Label = [&W](const TCHAR* Name) { return Cast<UTextBlock>(FAccess::Named(W, Name)); };
    UWidget* Summary = FAccess::Named(W, TEXT("TallySummary"));
    UTextBlock* Grade = Label(TEXT("TallyGrade2"));
    UTextBlock* Korean = Label(TEXT("TallyGradeKor2"));
    UTextBlock* Count = Label(TEXT("TallyCount2"));
    UTextBlock* ZeroGrade = Label(TEXT("TallyGrade3"));
    UTextBlock* ZeroCount = Label(TEXT("TallyCount3"));
    UTextBlock* BestGradeText = Label(TEXT("SummaryBestGrade"));
    if (!TestNotNull(TEXT("summary"), Summary) || !TestNotNull(TEXT("grade"), Grade)
        || !TestNotNull(TEXT("Korean grade"), Korean) || !TestNotNull(TEXT("count"), Count)
        || !TestNotNull(TEXT("zero grade"), ZeroGrade) || !TestNotNull(TEXT("zero count"), ZeroCount)
        || !TestNotNull(TEXT("best summary"), BestGradeText)) { return false; }

    FAccess::Begin(W, {2, 2, 4});
    TestEqual(TEXT("first setup hides summary"), Summary->GetVisibility(), ESlateVisibility::Collapsed);
    TestTrue(TEXT("first setup clears summary"), BestGradeText->GetText().IsEmpty());
    TestEqual(TEXT("first setup zero text"), Count->GetText().ToString(), FString(TEXT("0")));
    TestTrue(TEXT("first setup grade opacity"), FMath::IsNearlyEqual(Grade->GetRenderOpacity(), .4f));
    TestTrue(TEXT("first setup Korean grade opacity"), FMath::IsNearlyEqual(Korean->GetRenderOpacity(), .4f));
    TestTrue(TEXT("first setup count opacity"), FMath::IsNearlyEqual(Count->GetRenderOpacity(), .28f));
    FAccess::Bump(W, 2);
    TestEqual(TEXT("bump count"), Count->GetText().ToString(), FString(TEXT("1")));
    TestTrue(TEXT("bump grade opacity"), FMath::IsNearlyEqual(Grade->GetRenderOpacity(), 1.f));
    TestTrue(TEXT("bump Korean grade opacity"), FMath::IsNearlyEqual(Korean->GetRenderOpacity(), 1.f));
    TestTrue(TEXT("bump count opacity"), FMath::IsNearlyEqual(Count->GetRenderOpacity(), 1.f));
    W.DebugSkipToResult();
    TestEqual(TEXT("result shows summary"), Summary->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
    TestFalse(TEXT("result fills summary"), BestGradeText->GetText().IsEmpty());
    TestEqual(TEXT("result recomputes positive count"), Count->GetText().ToString(), FString(TEXT("2")));
    TestTrue(TEXT("result zero grade opacity"), FMath::IsNearlyEqual(ZeroGrade->GetRenderOpacity(), .4f));
    TestTrue(TEXT("result zero count opacity"), FMath::IsNearlyEqual(ZeroCount->GetRenderOpacity(), .28f));
    W.DebugAdvanceTime(.55f);
    FAccess::ClickConfirm(W);
    TestEqual(TEXT("first session closed"), Rig.ClosedCount, 1);

    FAccess::Begin(W, {1, 1});
    TestEqual(TEXT("reuse hides stale summary"), Summary->GetVisibility(), ESlateVisibility::Collapsed);
    for (const TCHAR* Name : { TEXT("SummaryBestGrade"), TEXT("SummaryNew"), TEXT("SummaryRecommend"), TEXT("SummaryMileage") })
    {
        UTextBlock* Text = Label(Name);
        if (!TestNotNull(Name, Text)) { return false; }
        TestTrue(*FString::Printf(TEXT("reuse clears %s"), Name), Text->GetText().IsEmpty());
    }
    TestEqual(TEXT("reuse resets count"), Count->GetText().ToString(), FString(TEXT("0")));
    TestTrue(TEXT("reuse resets grade opacity"), FMath::IsNearlyEqual(Grade->GetRenderOpacity(), .4f));
    TestTrue(TEXT("reuse resets count opacity"), FMath::IsNearlyEqual(Count->GetRenderOpacity(), .28f));
    FAccess::Back(W);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealWidgetReuseAfterDestroyTest,
	"CGR.Gacha.Reveal.Widget.ReuseAfterDestroy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealWidgetReuseAfterDestroyTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealWidgetTestUtil;
	FRig Rig;
	ON_SCOPE_EXIT { Destroy(Rig); };
	if (!Make(*this, Rig, false))
	{
		return false;
	}
	UGachaRevealPresentationWidget& W = *Rig.Widget;

	// 연출 중 파괴 → 닫힘 통지 1번 → 같은 객체로 다시 열고 [확인]으로 닫힘
	FAccess::Begin(W, TenGrades);
	AdvanceUntil(W, [&W]() { return W.IsOnBeat(EGachaRevealBeat::Deal); });
	FAccess::SlateReleased(W);
	TestEqual(TEXT("연출 중 파괴 = 닫힘 통지 1번"), Rig.ClosedCount, 1);
	TestFalse(TEXT("연출 중 파괴 뒤 열림 아님"), W.IsPresentationOpen());
	FAccess::Begin(W, TenGrades);
	TestTrue(TEXT("파괴 뒤 같은 객체 다시 열림"), W.IsPresentationOpen());
	W.DebugSkipToResult();
	W.DebugAdvanceTime(.55f);
	FAccess::ClickConfirm(W);
	TestEqual(TEXT("다시 연 것도 [확인]으로 닫힘"), Rig.ClosedCount, 2);
	TestFalse(TEXT("닫힌 뒤 열림 아님"), W.IsPresentationOpen());

	// 닫기 페이드 중 파괴 → 닫힘 통지 1번 → 다시 열기가 거절되지 않음(옛 코드 = bClosing 이 남아 영영 거절)
	W.SetDisableExitFade(false);
	FAccess::Begin(W, TenGrades);
	W.DebugAdvanceTime(1.f / 60.f);
	TestTrue(TEXT("뒤로가기 받음"), FAccess::Back(W));
	TestTrue(TEXT("닫기 페이드 중 = 아직 열림"), W.IsPresentationOpen());
	TestEqual(TEXT("페이드 중엔 통지 전"), Rig.ClosedCount, 2);
	FAccess::SlateReleased(W);
	TestEqual(TEXT("닫는 중 파괴 = 닫힘 통지 1번"), Rig.ClosedCount, 3);
	TestFalse(TEXT("닫는 중 파괴 뒤 열림 아님"), W.IsPresentationOpen());
	W.SetDisableExitFade(true);
	FAccess::Begin(W, TenGrades);
	TestTrue(TEXT("닫는 중 파괴 뒤 같은 객체 다시 열림"), W.IsPresentationOpen());
	W.DebugAdvanceTime(1.f / 60.f);
	TestTrue(TEXT("다시 연 것 뒤로가기 받음"), FAccess::Back(W));
	TestEqual(TEXT("닫힘 통지 4번"), Rig.ClosedCount, 4);
	TestFalse(TEXT("마지막도 닫힘"), W.IsPresentationOpen());
	return true;
}

#endif
