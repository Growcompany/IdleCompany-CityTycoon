#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SafeZoneSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Element/Common/DiamondProgressWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeHudCleanupRosterPlateTest,
	"CGR.Office.HudCleanup.RosterPlate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeHudCleanupFeatureBackingTest,
	"CGR.Office.HudCleanup.FeatureBacking",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeHudCleanupTopBandStructureTest,
	"CGR.Office.HudCleanup.TopBandStructure",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeHudCleanupMissionClearanceTest,
	"CGR.Office.HudCleanup.MissionClearance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeHudCleanupActionDockTest,
	"CGR.Office.HudCleanup.ActionDock",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeHudCleanupOpCardTest,
	"CGR.Office.HudCleanup.OpCard",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌
namespace OfficeHudCleanupTests
{
	static const TCHAR* RosterSummaryClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Office/UIE_OfficeRosterSummary.UIE_OfficeRosterSummary_C");
	static const TCHAR* ResearchButtonClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Buttons/UIE_OfficeResearchButton.UIE_OfficeResearchButton_C");
	static const TCHAR* TripButtonClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Buttons/UIE_OfficeTripButton.UIE_OfficeTripButton_C");
	static const TCHAR* OfficeLayerClassPath = TEXT("/Game/CompanyGrowth/UI/Panels/OfficeMap/UI_OfficeLayer.UI_OfficeLayer_C");
	static const TCHAR* OfficeMapMainClassPath = TEXT("/Game/CompanyGrowth/UI/Panels/OfficeMap/UI_OfficeMapMain.UI_OfficeMapMain_C");
	static const TCHAR* InGameLayerClassPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_InGameLayer.UI_InGameLayer_C");
	// Overlay 로 감싼 뒤에도 버튼 자체는 그대로여야 뒤로 키 이름 조회와 튜토리얼 링이 같은 대상을 잡는다
	static const TCHAR* BackButtonClassName = TEXT("UI_Element_IconWithButton_Vertical_C");

	// 월드 없이 컴파일된 클래스의 저작 트리만 본다
	static UWidgetTree* LoadTree(FAutomationTestBase& Test, const TCHAR* ClassPath)
	{
		UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UUserWidget>(nullptr, ClassPath));
		if (!Test.TestNotNull(*FString::Printf(TEXT("WBP class %s"), ClassPath), WidgetClass)) { return nullptr; }
		UWidgetTree* Tree = WidgetClass->GetWidgetTreeArchetype();
		Test.TestNotNull(*FString::Printf(TEXT("widget tree of %s"), ClassPath), Tree);
		return Tree;
	}

	static FString BrushResourceName(const UImage* Image)
	{
		const UObject* Resource = Image ? Image->GetBrush().GetResourceObject() : nullptr;
		return Resource ? Resource->GetName() : FString(TEXT("<none>"));
	}

	static UPanelSlot* SlotOf(const UWidgetTree* Tree, const TCHAR* Name)
	{
		const UWidget* Widget = Tree ? Tree->FindWidget(Name) : nullptr;
		return Widget ? Widget->Slot : nullptr;
	}

	// 직계 자식이면 그 인덱스, 아니면 INDEX_NONE
	static int32 ChildIndexIn(const UPanelWidget* Parent, const UWidget* Child)
	{
		return (Parent && Child && Child->GetParent() == Parent) ? Parent->GetChildIndex(Child) : INDEX_NONE;
	}
}

bool FOfficeHudCleanupRosterPlateTest::RunTest(const FString& Parameters)
{
	using namespace OfficeHudCleanupTests;
	UWidgetTree* Tree = LoadTree(*this, RosterSummaryClassPath);
	if (!Tree) { return false; }

	const UImage* PlateBG = Tree->FindWidget<UImage>(TEXT("PlateBG"));
	const UImage* PlateLine = Tree->FindWidget<UImage>(TEXT("PlateLine"));
	if (!TestNotNull(TEXT("PlateBG is an Image"), PlateBG) || !TestNotNull(TEXT("PlateLine is an Image"), PlateLine)) { return false; }

	TestEqual(TEXT("PlateBG brush"), BrushResourceName(PlateBG), FString(TEXT("MI_UI_ChipBG_OfficeFloat")));
	TestEqual(TEXT("PlateLine brush"), BrushResourceName(PlateLine), FString(TEXT("MI_UI_ChipLine_OfficeFloat")));

	// 테 층은 바탕 바로 위 형제라야 같은 칸을 덮고 줄(Rows)보다 뒤에 그려진다
	const UPanelWidget* PlateParent = PlateBG->GetParent();
	if (!TestNotNull(TEXT("PlateBG has a parent panel"), PlateParent)) { return false; }
	const int32 BGIndex = ChildIndexIn(PlateParent, PlateBG);
	const int32 LineIndex = ChildIndexIn(PlateParent, PlateLine);
	TestTrue(*FString::Printf(TEXT("PlateLine shares PlateBG's parent (%s)"), *PlateParent->GetName()), LineIndex != INDEX_NONE);
	TestEqual(*FString::Printf(TEXT("PlateLine index %d == PlateBG index %d + 1"), LineIndex, BGIndex), LineIndex, BGIndex + 1);
	return true;
}

bool FOfficeHudCleanupFeatureBackingTest::RunTest(const FString& Parameters)
{
	using namespace OfficeHudCleanupTests;
	// 받침은 필수 바인딩이라 지우지 않고 숨긴다 ― Hidden 은 자리를 지켜 아이콘·라벨 배치가 안 바뀐다
	const TCHAR* const ButtonClassPaths[] = { ResearchButtonClassPath, TripButtonClassPath };
	for (const TCHAR* ClassPath : ButtonClassPaths)
	{
		UWidgetTree* Tree = LoadTree(*this, ClassPath);
		if (!Tree) { continue; }
		const UImage* Backing = Tree->FindWidget<UImage>(TEXT("BackingImage"));
		if (!TestNotNull(*FString::Printf(TEXT("%s BackingImage is an Image"), ClassPath), Backing)) { continue; }
		const ESlateVisibility Visibility = Backing->GetVisibility();
		TestTrue(*FString::Printf(TEXT("%s BackingImage visibility %s == Hidden"), ClassPath, *UEnum::GetValueAsString(Visibility)),
			Visibility == ESlateVisibility::Hidden);
	}
	return true;
}

bool FOfficeHudCleanupTopBandStructureTest::RunTest(const FString& Parameters)
{
	using namespace OfficeHudCleanupTests;
	UWidgetTree* Tree = LoadTree(*this, OfficeLayerClassPath);
	UWidgetTree* City = LoadTree(*this, InGameLayerClassPath);
	if (!Tree || !City) { return false; }

	// 띠 바탕(10-21)과 옛 회사 배너(이름 두 줄·칩 밑 페이드·업종 색 판, 10-22 도시 짜임으로 교체)
	const TCHAR* const RemovedWidgets[] = {
		TEXT("TopBandBG"), TEXT("TopBandSheet"), TEXT("TopBandLine"), TEXT("BandEdgeShadow"),
		TEXT("BannerRow"), TEXT("PlateInner"), TEXT("LevelBadgeHost"), TEXT("LevelBadgeStack"), TEXT("CompanyPlate"), TEXT("FrameGlint"),
		TEXT("NamePlateFade"), TEXT("NamePlateSolid"), TEXT("BannerNameCol"), TEXT("CompanyIndustryText"),
		TEXT("ResourceRowLift"), TEXT("ResourceRowLift2") };
	for (const TCHAR* Name : RemovedWidgets)
	{
		TestNull(*FString::Printf(TEXT("%s is removed"), Name), Tree->FindWidget(Name));
	}

	// 회사 블록 = 도시 화면 TopBlock 과 같은 짜임·같은 값 [사용자 결정 2026-10-22]
	const UPanelWidget* BandRow = Tree->FindWidget<UPanelWidget>(TEXT("BandRow"));
	const UOverlay* TopBlock = Tree->FindWidget<UOverlay>(TEXT("TopBlock"));
	const UButton* PlateButton = Tree->FindWidget<UButton>(TEXT("CompanyPlateButton"));
	const UOverlay* PlateHitRoot = Tree->FindWidget<UOverlay>(TEXT("PlateHitRoot"));
	const UWidget* ResourceRow = Tree->FindWidget(TEXT("ResourceRow"));
	const USizeBox* NamePlateBox = Tree->FindWidget<USizeBox>(TEXT("NamePlateBox"));
	const USizeBox* ProfileFrameBox = Tree->FindWidget<USizeBox>(TEXT("ProfileFrameBox"));
	if (!TestNotNull(TEXT("BandRow is a panel"), BandRow) || !TestNotNull(TEXT("TopBlock is an Overlay"), TopBlock)
		|| !TestNotNull(TEXT("CompanyPlateButton is a Button"), PlateButton) || !TestNotNull(TEXT("PlateHitRoot is an Overlay"), PlateHitRoot)
		|| !TestNotNull(TEXT("ResourceRow exists"), ResourceRow) || !TestNotNull(TEXT("NamePlateBox is a SizeBox"), NamePlateBox)
		|| !TestNotNull(TEXT("ProfileFrameBox is a SizeBox"), ProfileFrameBox)) { return false; }

	TestEqual(TEXT("TopBlock is BandRow's first child"), ChildIndexIn(BandRow, TopBlock), 0);
	TestEqual(TEXT("TopBlock children = [CompanyPlateButton, ResourceRow]"),
		FString::Printf(TEXT("%d,%d"), ChildIndexIn(TopBlock, PlateButton), ChildIndexIn(TopBlock, ResourceRow)), FString(TEXT("0,1")));
	TestTrue(TEXT("CompanyPlateButton content is PlateHitRoot"), PlateButton->GetContent() == PlateHitRoot);
	if (const UButtonSlot* ButtonSlot = Cast<UButtonSlot>(PlateButton->GetContentSlot()); TestNotNull(TEXT("button content slot"), ButtonSlot))
	{
		TestTrue(TEXT("button content slot padding 0, Fill/Fill"), ButtonSlot->GetPadding() == FMargin(0.f)
			&& ButtonSlot->GetHorizontalAlignment() == HAlign_Fill && ButtonSlot->GetVerticalAlignment() == VAlign_Fill);
	}
	// 판이 앞 칸이라 액자가 판 위에 그려지고, 판 왼쪽 20 이 액자 뒤로 들어간다
	TestEqual(TEXT("PlateHitRoot children = [NamePlateBox, ProfileFrameBox]"),
		FString::Printf(TEXT("%d,%d"), ChildIndexIn(PlateHitRoot, NamePlateBox), ChildIndexIn(PlateHitRoot, ProfileFrameBox)), FString(TEXT("0,1")));

	// 위치: 도시 SafeZone 여백 = 블록 왼쪽·위, TopBlock 칸 여백 = 판·칩 자리
	const UOverlaySlot* CitySafe = Cast<UOverlaySlot>(SlotOf(City, TEXT("SafeZone")));
	const USafeZoneSlot* BandSafe = Cast<USafeZoneSlot>(SlotOf(Tree, TEXT("BandRow")));
	const UHorizontalBoxSlot* BlockSlot = Cast<UHorizontalBoxSlot>(TopBlock->Slot);
	if (TestNotNull(TEXT("city SafeZone slot"), CitySafe) && TestNotNull(TEXT("BandRow SafeZone slot"), BandSafe) && TestNotNull(TEXT("TopBlock slot"), BlockSlot))
	{
		TestEqual(TEXT("block left = city SafeZone left"), BandSafe->GetPadding().Left, CitySafe->GetPadding().Left);
		TestEqual(TEXT("block top = city SafeZone top"), BlockSlot->GetPadding().Top, CitySafe->GetPadding().Top);
		TestTrue(TEXT("TopBlock VAlign Top"), BlockSlot->GetVerticalAlignment() == VAlign_Top);
	}
	struct FSlotPair { const TCHAR* Office; const TCHAR* CityName; };
	const FSlotPair BlockSlots[] = { { TEXT("NamePlateBox"), TEXT("NamePlateBox") }, { TEXT("ResourceRow"), TEXT("ResourceBox") }, { TEXT("ProfileFrameBox"), TEXT("ProfileFrameBox") } };
	for (const FSlotPair& Pair : BlockSlots)
	{
		const UOverlaySlot* Mine = Cast<UOverlaySlot>(SlotOf(Tree, Pair.Office));
		const UOverlaySlot* Theirs = Cast<UOverlaySlot>(SlotOf(City, Pair.CityName));
		if (TestNotNull(*FString::Printf(TEXT("%s overlay slot"), Pair.Office), Mine) && TestNotNull(*FString::Printf(TEXT("city %s overlay slot"), Pair.CityName), Theirs))
		{
			TestTrue(*FString::Printf(TEXT("%s slot = city %s slot"), Pair.Office, Pair.CityName), Mine->GetPadding() == Theirs->GetPadding()
				&& Mine->GetHorizontalAlignment() == Theirs->GetHorizontalAlignment() && Mine->GetVerticalAlignment() == Theirs->GetVerticalAlignment());
		}
	}

	// 크기·브러시·글꼴은 도시 위젯 값 그대로
	const TCHAR* const SameSizeBoxes[] = { TEXT("NamePlateBox"), TEXT("ProfileFrameBox"), TEXT("LevelBadgeBox"), TEXT("LvTextBox") };
	for (const TCHAR* Name : SameSizeBoxes)
	{
		const USizeBox* Mine = Tree->FindWidget<USizeBox>(Name);
		const USizeBox* Theirs = City->FindWidget<USizeBox>(Name);
		if (TestNotNull(*FString::Printf(TEXT("%s is a SizeBox"), Name), Mine) && TestNotNull(*FString::Printf(TEXT("city %s"), Name), Theirs))
		{
			TestTrue(*FString::Printf(TEXT("%s size %.0fx%.0f = city %.0fx%.0f"), Name, Mine->GetWidthOverride(), Mine->GetHeightOverride(), Theirs->GetWidthOverride(), Theirs->GetHeightOverride()),
				Mine->GetWidthOverride() == Theirs->GetWidthOverride() && Mine->GetHeightOverride() == Theirs->GetHeightOverride());
		}
	}
	const FSlotPair SameImages[] = { { TEXT("Shadow_Img"), TEXT("Shadow_Img") }, { TEXT("FrameBG"), TEXT("FrameBG") }, { TEXT("InnerPlate"), TEXT("InnerPlate") },
		{ TEXT("ThumbGlow"), TEXT("ThumbGlow") }, { TEXT("LevelBadgeBG"), TEXT("LevelBadgeBG") }, { TEXT("PlateSolid"), TEXT("PlateSolid") }, { TEXT("PlateFade"), TEXT("PlateFade") } };
	for (const FSlotPair& Pair : SameImages)
	{
		const UImage* Mine = Tree->FindWidget<UImage>(Pair.Office);
		const UImage* Theirs = City->FindWidget<UImage>(Pair.CityName);
		if (TestNotNull(*FString::Printf(TEXT("%s is an Image"), Pair.Office), Mine) && TestNotNull(*FString::Printf(TEXT("city %s"), Pair.CityName), Theirs))
		{
			TestTrue(*FString::Printf(TEXT("%s brush = city"), Pair.Office), Mine->GetBrush() == Theirs->GetBrush());
			TestTrue(*FString::Printf(TEXT("%s colour/opacity = city"), Pair.Office), Mine->GetColorAndOpacity().Equals(Theirs->GetColorAndOpacity()));
			TestTrue(*FString::Printf(TEXT("%s render transform = city"), Pair.Office), Mine->GetRenderTransform().Translation.Equals(Theirs->GetRenderTransform().Translation)
				&& Mine->GetRenderTransform().Angle == Theirs->GetRenderTransform().Angle && Mine->GetRenderTransformPivot().Equals(Theirs->GetRenderTransformPivot()));
		}
	}
	const UWidget* OfficeBadge = Tree->FindWidget(TEXT("LevelBadgeBox"));
	const UWidget* CityBadge = City->FindWidget(TEXT("LevelBadgeBox"));
	if (OfficeBadge && CityBadge)
	{
		TestTrue(TEXT("LevelBadgeBox docks on the frame corner like the city (render translation)"),
			OfficeBadge->GetRenderTransform().Translation.Equals(CityBadge->GetRenderTransform().Translation));
	}
	const FSlotPair SameTexts[] = { { TEXT("CompanyNameText"), TEXT("PlayerNameText") }, { TEXT("CompanyLevelText"), TEXT("PlayerLevelText") } };
	for (const FSlotPair& Pair : SameTexts)
	{
		const UTextBlock* Mine = Tree->FindWidget<UTextBlock>(Pair.Office);
		const UTextBlock* Theirs = City->FindWidget<UTextBlock>(Pair.CityName);
		if (TestNotNull(*FString::Printf(TEXT("%s is a text block"), Pair.Office), Mine) && TestNotNull(*FString::Printf(TEXT("city %s"), Pair.CityName), Theirs))
		{
			TestTrue(*FString::Printf(TEXT("%s font = city %s font"), Pair.Office, Pair.CityName), Mine->GetFont() == Theirs->GetFont());
			TestTrue(*FString::Printf(TEXT("%s colour = city"), Pair.Office), Mine->GetColorAndOpacity() == Theirs->GetColorAndOpacity());
		}
	}

	// 액자 안 칸 순서·여백: 도시의 ProfileImage 자리에 건물 아이콘, 도시의 투명 버튼 자리는 없다(바깥 CompanyPlateButton 이 받는다)
	const FSlotPair FrameSlots[] = { { TEXT("Shadow_Img"), TEXT("Shadow_Img") }, { TEXT("FrameBG"), TEXT("FrameBG") }, { TEXT("InnerPlate"), TEXT("InnerPlate") },
		{ TEXT("ThumbGlow"), TEXT("ThumbGlow") }, { TEXT("CompanyProfileIcon"), TEXT("ProfileImage") }, { TEXT("LevelBadgeBox"), TEXT("LevelBadgeBox") } };
	const UOverlay* FrameOverlay = Tree->FindWidget<UOverlay>(TEXT("ProfileFrameOverlay"));
	if (TestNotNull(TEXT("ProfileFrameOverlay is an Overlay"), FrameOverlay))
	{
		TestTrue(TEXT("ProfileFrameOverlay is ProfileFrameBox content"), ProfileFrameBox->GetContent() == FrameOverlay);
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FrameSlots)); ++Index)
		{
			const UWidget* Mine = Tree->FindWidget(FrameSlots[Index].Office);
			const UWidget* Theirs = City->FindWidget(FrameSlots[Index].CityName);
			TestEqual(*FString::Printf(TEXT("%s index in ProfileFrameOverlay"), FrameSlots[Index].Office), ChildIndexIn(FrameOverlay, Mine), Index);
			const UOverlaySlot* MineSlot = Mine ? Cast<UOverlaySlot>(Mine->Slot) : nullptr;
			const UOverlaySlot* TheirSlot = Theirs ? Cast<UOverlaySlot>(Theirs->Slot) : nullptr;
			if (MineSlot && TheirSlot)
			{
				TestTrue(*FString::Printf(TEXT("%s slot = city %s slot"), FrameSlots[Index].Office, FrameSlots[Index].CityName), MineSlot->GetPadding() == TheirSlot->GetPadding()
					&& MineSlot->GetHorizontalAlignment() == TheirSlot->GetHorizontalAlignment() && MineSlot->GetVerticalAlignment() == TheirSlot->GetVerticalAlignment());
			}
		}
	}
	const FSlotPair PlateSlots[] = { { TEXT("PlateSolid"), TEXT("PlateSolid") }, { TEXT("PlateFade"), TEXT("PlateFade") }, { TEXT("NameClampBox"), TEXT("PlayerNameText") } };
	const UOverlay* PlateRoot = Tree->FindWidget<UOverlay>(TEXT("PlateRoot"));
	if (TestNotNull(TEXT("PlateRoot is an Overlay"), PlateRoot))
	{
		TestTrue(TEXT("PlateRoot is NamePlateBox content"), NamePlateBox->GetContent() == PlateRoot);
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(PlateSlots)); ++Index)
		{
			const UWidget* Mine = Tree->FindWidget(PlateSlots[Index].Office);
			const UWidget* Theirs = City->FindWidget(PlateSlots[Index].CityName);
			TestEqual(*FString::Printf(TEXT("%s index in PlateRoot"), PlateSlots[Index].Office), ChildIndexIn(PlateRoot, Mine), Index);
			const UOverlaySlot* MineSlot = Mine ? Cast<UOverlaySlot>(Mine->Slot) : nullptr;
			const UOverlaySlot* TheirSlot = Theirs ? Cast<UOverlaySlot>(Theirs->Slot) : nullptr;
			if (MineSlot && TheirSlot)
			{
				TestTrue(*FString::Printf(TEXT("%s slot = city %s slot"), PlateSlots[Index].Office, PlateSlots[Index].CityName), MineSlot->GetPadding() == TheirSlot->GetPadding()
					&& MineSlot->GetHorizontalAlignment() == TheirSlot->GetHorizontalAlignment() && MineSlot->GetVerticalAlignment() == TheirSlot->GetVerticalAlignment());
			}
		}
	}

	// 진행 게이지는 뱃지 테두리 위에 겹친다: 바탕 다음·숫자 앞, 바깥으로 12 넓혀 선이 테두리 위에 오고, 트랙은 투명이라 0% 면 도시 뱃지와 같다
	const UOverlay* BadgeOverlay = Tree->FindWidget<UOverlay>(TEXT("LevelBadgeOverlay"));
	const UDiamondProgressWidget* Gauge = Tree->FindWidget<UDiamondProgressWidget>(TEXT("CompanyLevelProgress"));
	if (TestNotNull(TEXT("LevelBadgeOverlay is an Overlay"), BadgeOverlay) && TestNotNull(TEXT("CompanyLevelProgress is a DiamondProgressWidget"), Gauge))
	{
		TestEqual(TEXT("LevelBadgeOverlay children = [LevelBadgeBG, CompanyLevelProgress, LvTextBox]"),
			FString::Printf(TEXT("%d,%d,%d"), ChildIndexIn(BadgeOverlay, Tree->FindWidget(TEXT("LevelBadgeBG"))), ChildIndexIn(BadgeOverlay, Gauge),
				ChildIndexIn(BadgeOverlay, Tree->FindWidget(TEXT("LvTextBox")))), FString(TEXT("0,1,2")));
		if (const UOverlaySlot* GaugeSlot = Cast<UOverlaySlot>(Gauge->Slot); TestNotNull(TEXT("gauge overlay slot"), GaugeSlot))
		{
			TestTrue(TEXT("gauge slot padding -12, Fill/Fill"), GaugeSlot->GetPadding() == FMargin(-12.f)
				&& GaugeSlot->GetHorizontalAlignment() == HAlign_Fill && GaugeSlot->GetVerticalAlignment() == VAlign_Fill);
		}
		TestEqual(TEXT("gauge track alpha"), Gauge->TrackColor.A, 0.f);
		TestEqual(TEXT("gauge thickness"), Gauge->Thickness, 3.f);
	}

	// 띠 높이는 블록 위 여백 + 액자 160 을 담고, 나가기는 옛 띠(160) 가운데였던 자리(위 28)에 그대로 있다
	if (const UCanvasPanelSlot* BandSlot = Cast<UCanvasPanelSlot>(SlotOf(Tree, TEXT("OfficeTopBand"))); TestNotNull(TEXT("OfficeTopBand canvas slot"), BandSlot))
	{
		TestEqual(TEXT("OfficeTopBand height"), BandSlot->GetOffsets().Bottom, 210.f);
	}
	if (const UHorizontalBoxSlot* BackSlot = Cast<UHorizontalBoxSlot>(SlotOf(Tree, TEXT("BackButtonBox"))); TestNotNull(TEXT("BackButtonBox slot"), BackSlot))
	{
		TestTrue(TEXT("BackButtonBox VAlign Top, top 28"), BackSlot->GetVerticalAlignment() == VAlign_Top && BackSlot->GetPadding().Top == 28.f);
	}

	// 나가기 판: 판(SizeBox)이 버튼보다 먼저라 버튼 뒤에 깔린다
	const UWidget* BackButtonWidget = Tree->FindWidget(TEXT("BackButton"));
	const UOverlay* BackPlateBox = Tree->FindWidget<UOverlay>(TEXT("BackPlateBox"));
	if (TestNotNull(TEXT("BackPlateBox is an Overlay"), BackPlateBox))
	{
		const int32 PlateSizeIndex = ChildIndexIn(BackPlateBox, Tree->FindWidget(TEXT("BackPlateSize")));
		const int32 ButtonIndex = ChildIndexIn(BackPlateBox, BackButtonWidget);
		TestTrue(*FString::Printf(TEXT("BackPlateSize/BackButton are BackPlateBox children (%d, %d)"), PlateSizeIndex, ButtonIndex),
			PlateSizeIndex != INDEX_NONE && ButtonIndex != INDEX_NONE);
		TestTrue(*FString::Printf(TEXT("back plate order BackPlateSize < BackButton (%d, %d)"), PlateSizeIndex, ButtonIndex),
			PlateSizeIndex != INDEX_NONE && PlateSizeIndex < ButtonIndex);
	}
	if (TestNotNull(TEXT("BackButton exists"), BackButtonWidget))
	{
		TestEqual(TEXT("BackButton keeps its authored class"), BackButtonWidget->GetClass()->GetName(), FString(BackButtonClassName));
	}

	TestNotNull(TEXT("CompanyPlateButton exists"), Tree->FindWidget(TEXT("CompanyPlateButton")));
	TestNotNull(TEXT("InGameCanvas exists"), Tree->FindWidget(TEXT("InGameCanvas")));
	return true;
}

bool FOfficeHudCleanupMissionClearanceTest::RunTest(const FString& Parameters)
{
	using namespace OfficeHudCleanupTests;
	UWidgetTree* Tree = LoadTree(*this, OfficeMapMainClassPath);
	if (!Tree) { return false; }

	// 성장 퀘스트 판은 회사 블록(위 50 + 액자 160, 모서리 마름모가 그 아래로 걸침) 밑에서 시작한다 ― 옛 띠 아래 166 에서 70 내렸다
	const UOverlaySlot* AnchorSlot = Cast<UOverlaySlot>(SlotOf(Tree, TEXT("MissionAnchorBox")));
	if (TestNotNull(TEXT("MissionAnchorBox overlay slot"), AnchorSlot))
	{
		TestEqual(TEXT("MissionAnchorBox top"), AnchorSlot->GetPadding().Top, 236.f);
		TestEqual(TEXT("MissionAnchorBox left unchanged"), AnchorSlot->GetPadding().Left, 38.f);
	}
	// 저작 위치 264 = 블록 밑(제조 연구 중 자리). 개발 중에는 OfficeMainWidget 이 맨 위로 올리고 블록을 숨기며, 운영 중에는 줄을 접는다
	const UOverlaySlot* StripSlot = Cast<UOverlaySlot>(SlotOf(Tree, TEXT("StripRailColumn")));
	if (TestNotNull(TEXT("StripRailColumn overlay slot"), StripSlot))
	{
		TestEqual(TEXT("StripRailColumn top"), StripSlot->GetPadding().Top, 264.f);
	}
	return true;
}

bool FOfficeHudCleanupOpCardTest::RunTest(const FString& Parameters)
{
	using namespace OfficeHudCleanupTests;
	UWidgetTree* Tree = LoadTree(*this, OfficeMapMainClassPath);
	if (!Tree) { return false; }

	// 운영 중 내용 = 오른쪽 열 카드 한 장(시안 docs/05_UI/mockups/OfficeOpCard_2026-10-22, 가운데 자리). 진행 줄의 운영 페이지는 없다
	const USizeBox* Card = Cast<USizeBox>(Tree->FindWidget(TEXT("OpCard")));
	if (!TestNotNull(TEXT("OpCard size box"), Card)) { return false; }
	// 오른쪽 열(OfficeContentRail)은 대기 중에만 보이므로(RefreshPlacementEditControls) 카드는 그 안이 아니라 같은 겹판의 형제다
	const UWidget* Rail = Tree->FindWidget(TEXT("OfficeContentRail"));
	const UWidget* ContentRoot = Tree->FindWidget(TEXT("OfficeContentRoot"));
	TestTrue(TEXT("OpCard is the rail's sibling under OfficeContentRoot"),
		ContentRoot != nullptr && Rail != nullptr && Card->GetParent() == ContentRoot && Rail->GetParent() == ContentRoot);
	TestEqual(TEXT("OpCard width"), Card->GetWidthOverride(), 409.f);
	TestTrue(TEXT("OpCard collapsed until operating"), Card->GetVisibility() == ESlateVisibility::Collapsed);
	const UOverlaySlot* CardSlot = Cast<UOverlaySlot>(Card->Slot);
	const UOverlaySlot* RosterSlot = Cast<UOverlaySlot>(SlotOf(Tree, TEXT("OfficeRosterSummary")));
	if (TestNotNull(TEXT("OpCard overlay slot"), CardSlot) && TestNotNull(TEXT("OfficeRosterSummary overlay slot"), RosterSlot))
	{
		// 윗선 = 왼쪽 성장 퀘스트 카드 윗선, 오른쪽 끝 = 오른쪽 열 공통 끝선
		TestEqual(TEXT("OpCard top"), CardSlot->GetPadding().Top, 322.f);
		TestEqual(TEXT("OpCard right edge on the rail line"), CardSlot->GetPadding().Right, RosterSlot->GetPadding().Right);
		TestTrue(TEXT("OpCard anchored top-right"),
			CardSlot->GetHorizontalAlignment() == HAlign_Right && CardSlot->GetVerticalAlignment() == VAlign_Top);
	}
	// C++ 갱신 경로는 이름으로 묶여 있어 카드 안으로 옮긴 뒤에도 같은 위젯이어야 한다
	for (const TCHAR* Name : { TEXT("OpCardStatusRow"), TEXT("OpThumb"), TEXT("OpThumbInner"), TEXT("OpThumbQ"),
		TEXT("OpGradeBadgeBox"), TEXT("OpGradeBadge"), TEXT("OpProjectNameBox"), TEXT("OpProjectName"), TEXT("RevPerSecText"),
		TEXT("SalesCurve"), TEXT("TotalRevText"), TEXT("OpTimeLabel"), TEXT("OpModeBadgeText"), TEXT("StripStopButton") })
	{
		bool bInCard = false;
		for (const UWidget* Up = Tree->FindWidget(Name); Up; Up = Up->GetParent()) { bInCard |= Up == Card; }
		TestTrue(*FString::Printf(TEXT("%s is inside OpCard"), Name), bInCard);
	}
	if (const USizeBox* TitleBox = Cast<USizeBox>(Tree->FindWidget(TEXT("OpProjectNameBox"))); TestNotNull(TEXT("OpProjectNameBox"), TitleBox))
	{
		// 제목 줄 나눔(OfficeMainWidget FitOpCardTitle)의 기준 폭 = 이 칸 폭
		TestEqual(TEXT("title column width"), TitleBox->GetWidthOverride(), 169.f);
	}
	if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("ProjectWidgetSwitcher"))); TestNotNull(TEXT("ProjectWidgetSwitcher"), Switcher))
	{
		TestEqual(TEXT("strip pages = idle + dev"), Switcher->GetChildrenCount(), 2);
	}
	TestNull(TEXT("old operating page gone"), Tree->FindWidget(TEXT("OpPage")));
	// 운영 종료 = 카드 전용 볼륨 스타일(공용 회색 버튼은 카드 위에서 납작했다, 2026-10-23) + 아래 두께 판
	if (const UCommonButtonBase* Stop = Cast<UCommonButtonBase>(Tree->FindWidget(TEXT("StripStopButton"))); TestNotNull(TEXT("StripStopButton"), Stop))
	{
		const UCommonButtonStyle* StopStyle = Stop->GetStyle();
		TestTrue(TEXT("stop button uses CUI_Style2_Btn_OpCardStop"),
			StopStyle != nullptr && StopStyle->GetClass()->GetName().Equals(TEXT("CUI_Style2_Btn_OpCardStop_C"), ESearchCase::CaseSensitive));
		const UWidget* Slab = Tree->FindWidget(TEXT("OpStopButtonBase"));
		TestTrue(TEXT("slab sits beside the button under OpStopButtonRoot"),
			Slab != nullptr && Slab->GetParent() != nullptr && Slab->GetParent() == Stop->GetParent());
	}
	return true;
}

bool FOfficeHudCleanupActionDockTest::RunTest(const FString& Parameters)
{
	using namespace OfficeHudCleanupTests;
	UWidgetTree* Tree = LoadTree(*this, OfficeMapMainClassPath);
	if (!Tree) { return false; }

	// 사용자 결정 7-①(2026-10-21): 두 칸 사이에 틈, 그 가운데 구분선, 판은 출장 아래로 12 더 내려간다
	const float PlateBelowTrip = 12.f;
	const float DividerInset = 16.f;

	const UGridPanel* ActionGrid = Tree->FindWidget<UGridPanel>(TEXT("OfficeActionGrid"));
	const UWidget* TrainingButton = Tree->FindWidget(TEXT("TrainingCenterOpenButton"));
	const UWidget* TripButton = Tree->FindWidget(TEXT("BusinessTripOpenButton"));
	if (!TestNotNull(TEXT("OfficeActionGrid is a GridPanel"), ActionGrid) || !TestNotNull(TEXT("TrainingCenterOpenButton exists"), TrainingButton)
		|| !TestNotNull(TEXT("BusinessTripOpenButton exists"), TripButton)) { return false; }
	const UGridSlot* TrainingSlot = Cast<UGridSlot>(TrainingButton->Slot);
	const UGridSlot* TripSlot = Cast<UGridSlot>(TripButton->Slot);
	const int32 TrainingIndex = ChildIndexIn(ActionGrid, TrainingButton);
	if (!TestTrue(TEXT("TrainingCenterOpenButton and BusinessTripOpenButton are OfficeActionGrid children"),
		TrainingSlot != nullptr && TripSlot != nullptr && TrainingIndex != INDEX_NONE && ChildIndexIn(ActionGrid, TripButton) != INDEX_NONE)) { return false; }

	// Center 는 위·아래 패딩 차이를 통째로 이동량에 쓴다(LayoutUtils.h AlignChild) ― Top 이어야 패딩이 곧 화면 간격이다
	TestTrue(TEXT("TrainingCenterOpenButton VAlign Top"), TrainingSlot->GetVerticalAlignment() == VAlign_Top);
	TestTrue(TEXT("BusinessTripOpenButton VAlign Top"), TripSlot->GetVerticalAlignment() == VAlign_Top);
	TestEqual(TEXT("BusinessTripOpenButton row == TrainingCenterOpenButton row + 1"), TripSlot->GetRow(), TrainingSlot->GetRow() + 1);
	const FMargin TrainingPad = TrainingSlot->GetPadding();
	const FMargin TripPad = TripSlot->GetPadding();
	const float CellGap = TrainingPad.Bottom + TripPad.Top;
	TestTrue(*FString::Printf(TEXT("gap between the two cells %.1f > 0"), CellGap), CellGap > 0.f);

	// 판 한 장이 두 칸을 덮는다: 연구소 행부터 두 칸·같은 열·버튼보다 앞 인덱스(= 뒤에 그려짐), 위 = 연구소 위, 아래 = 출장 아래 + 12
	float PlateWidth = 0.f;
	float PlateRight = 0.f;
	const TCHAR* const DockPlateNames[] = { TEXT("ActionDockPlateBG"), TEXT("ActionDockPlateLine") };
	for (const TCHAR* Name : DockPlateNames)
	{
		const UImage* DockPlate = Tree->FindWidget<UImage>(Name);
		if (!TestNotNull(*FString::Printf(TEXT("%s is an Image"), Name), DockPlate)) { continue; }
		const UGridSlot* PlateSlot = Cast<UGridSlot>(DockPlate->Slot);
		const int32 PlateIndex = ChildIndexIn(ActionGrid, DockPlate);
		if (!TestTrue(*FString::Printf(TEXT("%s is an OfficeActionGrid child"), Name), PlateSlot != nullptr && PlateIndex != INDEX_NONE)) { continue; }
		TestEqual(*FString::Printf(TEXT("%s row == TrainingCenterOpenButton row"), Name), PlateSlot->GetRow(), TrainingSlot->GetRow());
		TestEqual(*FString::Printf(TEXT("%s row span"), Name), PlateSlot->GetRowSpan(), 2);
		TestEqual(*FString::Printf(TEXT("%s column == TrainingCenterOpenButton column"), Name), PlateSlot->GetColumn(), TrainingSlot->GetColumn());
		TestTrue(*FString::Printf(TEXT("%s index %d < TrainingCenterOpenButton index %d"), Name, PlateIndex, TrainingIndex), PlateIndex < TrainingIndex);
		TestTrue(*FString::Printf(TEXT("%s VAlign Fill"), Name), PlateSlot->GetVerticalAlignment() == VAlign_Fill);
		const FMargin PlatePad = PlateSlot->GetPadding();
		TestEqual(*FString::Printf(TEXT("%s top padding == TrainingCenterOpenButton top padding"), Name), PlatePad.Top, TrainingPad.Top);
		TestEqual(*FString::Printf(TEXT("%s reaches below BusinessTripOpenButton by (Trip bottom pad %.1f - plate bottom pad %.1f)"), Name, TripPad.Bottom, PlatePad.Bottom),
			TripPad.Bottom - PlatePad.Bottom, PlateBelowTrip);
		TestEqual(*FString::Printf(TEXT("%s right padding == button right padding"), Name), PlatePad.Right, TrainingPad.Right);
		// 두 행을 걸친 판의 원하는 높이는 반씩 나뉘어 각 행 높이의 하한이 된다(SGridPanel.cpp:446-450) ― 브러시가 높으면 행이 늘어나 버튼과 구분선이 칸 틈에서 밀린다
		const float PlateBrushHeight = DockPlate->GetBrush().GetImageSize().Y;
		TestTrue(*FString::Printf(TEXT("%s brush ImageSize.Y %.1f <= 1 (plate must not set the row heights)"), Name, PlateBrushHeight), PlateBrushHeight <= 1.f);
		PlateWidth = DockPlate->GetBrush().GetImageSize().X;
		PlateRight = PlatePad.Right;
	}

	// 구분선: 연구소 행 아래 끝(= 칸 사이 틈)에 붙어 두께 중심이 틈 가운데, 판 안쪽 좌우 16
	const UImage* Divider = Tree->FindWidget<UImage>(TEXT("ActionDockDivider"));
	if (TestNotNull(TEXT("ActionDockDivider is an Image"), Divider))
	{
		const UGridSlot* DividerSlot = Cast<UGridSlot>(Divider->Slot);
		const int32 DividerIndex = ChildIndexIn(ActionGrid, Divider);
		if (TestTrue(TEXT("ActionDockDivider is an OfficeActionGrid child"), DividerSlot != nullptr && DividerIndex != INDEX_NONE))
		{
			TestTrue(*FString::Printf(TEXT("ActionDockDivider index %d < TrainingCenterOpenButton index %d"), DividerIndex, TrainingIndex), DividerIndex < TrainingIndex);
			TestEqual(TEXT("ActionDockDivider row == TrainingCenterOpenButton row"), DividerSlot->GetRow(), TrainingSlot->GetRow());
			TestEqual(TEXT("ActionDockDivider row span"), FMath::Max(1, DividerSlot->GetRowSpan()), 1);
			TestTrue(TEXT("ActionDockDivider VAlign Bottom"), DividerSlot->GetVerticalAlignment() == VAlign_Bottom);
			TestTrue(TEXT("ActionDockDivider HAlign Right"), DividerSlot->GetHorizontalAlignment() == HAlign_Right);
			const FVector2f DividerSize = Divider->GetBrush().GetImageSize();
			const FMargin DividerPad = DividerSlot->GetPadding();
			// 행 아래 끝에서 잰 거리: 구분선 중심 = 아래 패딩 + 두께/2, 틈 중심 = (연구소 아래 패딩 - 출장 위 패딩)/2
			const float DividerCentre = DividerPad.Bottom + DividerSize.Y * 0.5f;
			const float GapCentre = (TrainingPad.Bottom - TripPad.Top) * 0.5f;
			TestTrue(*FString::Printf(TEXT("ActionDockDivider centre %.2f == cell gap centre %.2f (from row 0 bottom)"), DividerCentre, GapCentre),
				FMath::IsNearlyEqual(DividerCentre, GapCentre, 0.01f));
			const float InsetRight = DividerPad.Right - PlateRight;
			const float InsetLeft = PlateWidth - DividerSize.X - InsetRight;
			TestTrue(*FString::Printf(TEXT("ActionDockDivider insets left %.1f / right %.1f == %.1f"), InsetLeft, InsetRight, DividerInset),
				FMath::IsNearlyEqual(InsetLeft, DividerInset, 0.01f) && FMath::IsNearlyEqual(InsetRight, DividerInset, 0.01f));
		}
	}

	// 진행 줄을 띠에 붙이려고 건 잘림이 풀려야 둥근 위 모서리가 보인다
	const UWidget* StripClip = Tree->FindWidget(TEXT("StripClip"));
	if (TestNotNull(TEXT("StripClip exists"), StripClip))
	{
		const EWidgetClipping Clipping = StripClip->GetClipping();
		TestTrue(*FString::Printf(TEXT("StripClip clipping %d != ClipToBounds"), static_cast<int32>(Clipping)), Clipping != EWidgetClipping::ClipToBounds);
	}
	return true;
}

#endif
