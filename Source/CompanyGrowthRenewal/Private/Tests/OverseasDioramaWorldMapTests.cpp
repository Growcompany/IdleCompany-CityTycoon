#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Image.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"
#include "Enum/MedallionAnchor.h"
#include "Enum/WidgetType.h"
#include "Framework/Application/SlateApplication.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Table/WidgetDataTable.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "UObject/StrongObjectPtr.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 형제 시험 파일의 익명 ns 와 이름이 겹친다.
namespace OverseasDioramaWorldMapTest
{
	static void RunFullEnter(FDioramaViewTransition& Transition)
	{
		Transition.Enter();
		Transition.Tick(FDioramaViewTransition::EnterDelay);
		Transition.Tick(FDioramaViewTransition::EnterDuration);
	}

	static UWorld* FindPlayWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* ContextWorld = Context.World();
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
				&& ContextWorld && ContextWorld->GetGameInstance())
			{
				return ContextWorld;
			}
		}
		return nullptr;
	}

	static TSubclassOf<UUserWidget> ResolveCountryNameClass()
	{
		if (const UWorld* PlayWorld = FindPlayWorld())
		{
			if (const UTableManagerSubsystem* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
			{
				return Tables->GetWidgetClass(EWidgetType::CountryName);
			}
		}
		// 헤드리스 실행에는 게임 인스턴스가 없다 ― 테이블 관리자가 읽는 같은 DT_WidgetClass 행을 EWidgetType 으로 찾는다.
		const UDataTable* WidgetTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous();
		if (!WidgetTable)
		{
			return nullptr;
		}
		TArray<FWidgetDataTable*> Rows;
		WidgetTable->GetAllRows<FWidgetDataTable>(TEXT("OverseasDioramaWorldMapTest"), Rows);
		for (const FWidgetDataTable* Row : Rows)
		{
			if (Row && Row->WidgetType == EWidgetType::CountryName)
			{
				return Row->WidgetClass;
			}
		}
		return nullptr;
	}

	enum class ESetupResult : uint8
	{
		Ready,
		Skipped,
		Failed
	};

	struct FMiniatureFixture
	{
		UWorld* PreviewWorld = nullptr;
		TStrongObjectPtr<UCountryNameWidget> Pin;
		// UWidget 은 Slate 위젯을 약하게만 잡는다 ― 여기서 붙들지 않으면 TakeWidget 직후 사라져 desired 가 0 이 된다.
		TSharedPtr<SWidget> PinSlate;
		TStrongObjectPtr<UTexture2D> Still;
		TStrongObjectPtr<UMaterialInterface> RTMaterial;

		~FMiniatureFixture()
		{
			PinSlate.Reset();
			Pin.Reset();
			if (PreviewWorld)
			{
				if (GEngine)
				{
					GEngine->DestroyWorldContext(PreviewWorld);
				}
				PreviewWorld->DestroyWorld(false);
			}
		}

		ESetupResult Setup(FAutomationTestBase& Test)
		{
			if (!FSlateApplication::IsInitialized())
			{
				Test.AddWarning(TEXT("Slate 가 초기화되지 않은 실행이라 메달리온 레이아웃을 잴 수 없어 건너뜁니다."));
				return ESetupResult::Skipped;
			}
			const TSubclassOf<UUserWidget> PinClass = ResolveCountryNameClass();
			if (!PinClass || !PinClass->IsChildOf(UCountryNameWidget::StaticClass()))
			{
				Test.AddError(TEXT("EWidgetType::CountryName 위젯 클래스를 못 찾았습니다(DT_WidgetClass 확인)."));
				return ESetupResult::Failed;
			}
			RTMaterial.Reset(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT.M_GachaRevealRT"))).LoadSynchronous());
			Still.Reset(UTexture2D::CreateTransient(4, 4));
			if (!RTMaterial.IsValid() || !Still.IsValid())
			{
				Test.AddError(TEXT("M_GachaRevealRT 또는 임시 텍스처를 준비하지 못했습니다."));
				return ESetupResult::Failed;
			}

			PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (!PreviewWorld)
			{
				Test.AddError(TEXT("메달리온을 띄울 임시 월드를 만들지 못했습니다."));
				return ESetupResult::Failed;
			}
			if (GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(PreviewWorld);
			}
			Pin.Reset(CreateWidget<UCountryNameWidget>(PreviewWorld, PinClass));
			if (!Pin.IsValid())
			{
				Test.AddError(TEXT("UIE_CountryName 인스턴스를 만들지 못했습니다."));
				return ESetupResult::Failed;
			}
			if (!Pin->GetWidgetFromName(TEXT("MiniatureImage")))
			{
				Test.AddWarning(TEXT("UIE_CountryName 에 MiniatureImage 가 아직 없습니다(Task 8 저작 전) ― 미니어처 판정을 건너뜁니다."));
				return ESetupResult::Skipped;
			}

			Pin->SetCountryType(ECountryType::Korea);
			Pin->SetCountryName(FText::FromString(TEXT("대한민국")));
			Pin->SetStatusLine(FText::FromString(TEXT("공장 1/3")));
			PinSlate = Pin->TakeWidget();
			Pin->ForceLayoutPrepass();
			return ESetupResult::Ready;
		}

		static FDioramaStillInfo FixtureInfo()
		{
			FDioramaStillInfo Info;
			Info.SizePx = FIntPoint(300, 200);
			Info.PivotPx = FVector2D(150.0, 120.0);
			return Info;
		}

		void ShowMiniature()
		{
			Pin->SetMiniature(Still.Get(), FixtureInfo(), RTMaterial.Get());
			Pin->ForceLayoutPrepass();
		}

		ESlateVisibility VisibilityOf(const TCHAR* WidgetName) const
		{
			const UWidget* Part = Pin->GetWidgetFromName(WidgetName);
			return Part ? Part->GetVisibility() : ESlateVisibility::Collapsed;
		}
	};
}

// ── Task 9: 확대 화면 진행 상태 · 크기 · 탭 ─────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapEnterCurveTest, "CGR.Overseas.WorldMap.EnterCurve",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapEnterCurveTest::RunTest(const FString& Parameters)
{
	FDioramaViewTransition Transition;
	TestEqual(TEXT("before enter map is opaque"), Transition.MapOpacity(), 1.0f);
	Transition.Enter();
	TestFalse(TEXT("delay tick does not end"), Transition.Tick(0.1f));
	TestEqual(TEXT("0.1s delay keeps K at 0"), Transition.K, 0.0f);
	TestEqual(TEXT("view starts 40px low"), Transition.OffsetY(), 40.0f);
	TestFalse(TEXT("enter tick does not end"), Transition.Tick(0.45f));
	TestEqual(TEXT("0.45s reaches K 1"), Transition.K, 1.0f);
	TestEqual(TEXT("view fully opaque"), Transition.ViewOpacity(), 1.0f);
	TestEqual(TEXT("map dims to 0.15"), Transition.MapOpacity(), 0.15f);
	TestEqual(TEXT("view risen"), Transition.OffsetY(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapExitFasterTest, "CGR.Overseas.WorldMap.ExitFaster",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapExitFasterTest::RunTest(const FString& Parameters)
{
	FDioramaViewTransition Transition;
	OverseasDioramaWorldMapTest::RunFullEnter(Transition);
	TestEqual(TEXT("entered"), Transition.K, 1.0f);
	Transition.Exit(false);
	TestTrue(TEXT("exit ends at 0.3s (0.45/1.5)"), Transition.Tick(0.3f));
	TestEqual(TEXT("K back to 0"), Transition.K, 0.0f);
	TestEqual(TEXT("map opaque again"), Transition.MapOpacity(), 1.0f);
	TestFalse(TEXT("end signal fires once"), Transition.Tick(0.016f));
	TestTrue(TEXT("idle after exit"), Transition.IsIdle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapSwitchCountryTest, "CGR.Overseas.WorldMap.SwitchCountryWhileLive",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapSwitchCountryTest::RunTest(const FString& Parameters)
{
	FDioramaViewTransition Transition;
	Transition.Enter();
	Transition.Tick(0.1f);
	Transition.Tick(0.225f);
	TestEqual(TEXT("halfway in"), Transition.K, 0.5f);

	Transition.Enter();
	TestEqual(TEXT("re-enter keeps K"), Transition.K, 0.5f);
	TestFalse(TEXT("no end signal on re-enter"), Transition.Tick(0.0f));
	TestFalse(TEXT("still no end signal while finishing"), Transition.Tick(1.0f));
	TestEqual(TEXT("finishes entering"), Transition.K, 1.0f);
	TestEqual(TEXT("map stays dimmed, not reset"), Transition.MapOpacity(), 0.15f);

	// 나가는 도중 다른 나라를 누르면 지연 없이 그 자리에서 다시 올라온다.
	Transition.Exit(false);
	Transition.Tick(0.15f);
	TestEqual(TEXT("halfway out"), Transition.K, 0.5f);
	Transition.Enter();
	TestFalse(TEXT("re-enter during exit does not end"), Transition.Tick(0.045f));
	TestTrue(TEXT("re-enter during exit resumes without delay"), Transition.K > 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapCloseWhileLiveTest, "CGR.Overseas.WorldMap.CloseWhileLive",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapCloseWhileLiveTest::RunTest(const FString& Parameters)
{
	FDioramaViewTransition Transition;
	OverseasDioramaWorldMapTest::RunFullEnter(Transition);
	Transition.Exit(true);
	TestEqual(TEXT("immediate exit zeroes K"), Transition.K, 0.0f);
	TestEqual(TEXT("immediate exit restores map opacity"), Transition.MapOpacity(), 1.0f);
	TestEqual(TEXT("immediate exit hides view"), Transition.ViewOpacity(), 0.0f);
	TestTrue(TEXT("next tick signals end once"), Transition.Tick(0.016f));
	TestFalse(TEXT("only once"), Transition.Tick(0.016f));
	TestTrue(TEXT("idle"), Transition.IsIdle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapLiveRectNarrowTest, "CGR.Overseas.WorldMap.LiveRectNarrow",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapLiveRectNarrowTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("zero geometry defers"), WorldMapDiorama::ClampLiveSize(FVector2D(0.0, 0.0)) == FIntPoint::ZeroValue);
	TestTrue(TEXT("16:10 visible area 1144 wide kept"), WorldMapDiorama::ClampLiveSize(FVector2D(1144.0, 1440.0)) == FIntPoint(1144, 1440));
	TestTrue(TEXT("oversize clamps per axis"), WorldMapDiorama::ClampLiveSize(FVector2D(2400.0, 1600.0)) == FIntPoint(1600, 1440));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapTabToModuleTest, "CGR.Overseas.WorldMap.TabToModule",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapTabToModuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("info tab -> primary"), WorldMapDiorama::ModuleForTab(0, EDioramaModule::Port) == EDioramaModule::Port);
	TestTrue(TEXT("factory tab -> factory"), WorldMapDiorama::ModuleForTab(1, EDioramaModule::Port) == EDioramaModule::Factory);
	TestTrue(TEXT("mine tab -> mine"), WorldMapDiorama::ModuleForTab(2, EDioramaModule::Factory) == EDioramaModule::Mine);
	TestTrue(TEXT("unknown tab -> primary"), WorldMapDiorama::ModuleForTab(99, EDioramaModule::Mine) == EDioramaModule::Mine);
	return true;
}

// ── Task 10: 지도 미니어처(메달리온 그림 모드) ──────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapMiniatureAlignmentTest, "CGR.Overseas.WorldMap.MiniatureAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapMiniatureAlignmentTest::RunTest(const FString& Parameters)
{
	OverseasDioramaWorldMapTest::FMiniatureFixture Fixture;
	const OverseasDioramaWorldMapTest::ESetupResult Setup = Fixture.Setup(*this);
	if (Setup != OverseasDioramaWorldMapTest::ESetupResult::Ready)
	{
		return Setup == OverseasDioramaWorldMapTest::ESetupResult::Skipped;
	}

	TestTrue(TEXT("disc alignment is centre"), Fixture.Pin->GetAnchorAlignment().Equals(FVector2D(0.5, 0.5), 1e-4));
	Fixture.ShowMiniature();
	TestTrue(TEXT("miniature mode on"), Fixture.Pin->HasMiniature());
	const FVector2D Alignment = Fixture.Pin->GetAnchorAlignment();
	TestTrue(*FString::Printf(TEXT("pivot (150,120) in 300x200 -> (0.5,0.6), got %s"), *Alignment.ToString()),
		Alignment.Equals(FVector2D(0.5, 0.6), 1e-4));
	const FVector2D Desired = Fixture.Pin->GetDesiredSize();
	TestTrue(*FString::Printf(TEXT("desired size covers the 150x100 miniature, got %s"), *Desired.ToString()),
		Desired.X >= 150.0 - 0.5 && Desired.Y >= 100.0 - 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapMiniatureHidesDiscTest, "CGR.Overseas.WorldMap.MiniatureHidesDisc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapMiniatureHidesDiscTest::RunTest(const FString& Parameters)
{
	OverseasDioramaWorldMapTest::FMiniatureFixture Fixture;
	const OverseasDioramaWorldMapTest::ESetupResult Setup = Fixture.Setup(*this);
	if (Setup != OverseasDioramaWorldMapTest::ESetupResult::Ready)
	{
		return Setup == OverseasDioramaWorldMapTest::ESetupResult::Skipped;
	}

	Fixture.ShowMiniature();
	TestTrue(TEXT("ring collapsed"), Fixture.VisibilityOf(TEXT("RingImage")) == ESlateVisibility::Collapsed);
	TestTrue(TEXT("disc face collapsed"), Fixture.VisibilityOf(TEXT("BGImage")) == ESlateVisibility::Collapsed);
	TestTrue(TEXT("country code collapsed"), Fixture.VisibilityOf(TEXT("CodeText")) == ESlateVisibility::Collapsed);
	TestTrue(TEXT("miniature Visible (not HitTestInvisible)"), Fixture.VisibilityOf(TEXT("MiniatureImage")) == ESlateVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapMiniatureClearTest, "CGR.Overseas.WorldMap.MiniatureClearRestores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapMiniatureClearTest::RunTest(const FString& Parameters)
{
	OverseasDioramaWorldMapTest::FMiniatureFixture Fixture;
	const OverseasDioramaWorldMapTest::ESetupResult Setup = Fixture.Setup(*this);
	if (Setup != OverseasDioramaWorldMapTest::ESetupResult::Ready)
	{
		return Setup == OverseasDioramaWorldMapTest::ESetupResult::Skipped;
	}

	const ESlateVisibility RingBefore = Fixture.VisibilityOf(TEXT("RingImage"));
	const ESlateVisibility FaceBefore = Fixture.VisibilityOf(TEXT("BGImage"));
	const FVector2D DesiredBefore = Fixture.Pin->GetDesiredSize();
	Fixture.ShowMiniature();
	Fixture.Pin->ClearMiniature();
	Fixture.Pin->ForceLayoutPrepass();

	TestFalse(TEXT("miniature mode off"), Fixture.Pin->HasMiniature());
	TestTrue(TEXT("ring visible again"), Fixture.VisibilityOf(TEXT("RingImage")) != ESlateVisibility::Collapsed);
	TestTrue(TEXT("ring visibility restored as authored"), Fixture.VisibilityOf(TEXT("RingImage")) == RingBefore);
	TestTrue(TEXT("disc face restored as authored"), Fixture.VisibilityOf(TEXT("BGImage")) == FaceBefore);
	TestTrue(TEXT("miniature collapsed"), Fixture.VisibilityOf(TEXT("MiniatureImage")) == ESlateVisibility::Collapsed);
	TestTrue(TEXT("alignment back to centre"), Fixture.Pin->GetAnchorAlignment().Equals(FVector2D(0.5, 0.5), 1e-4));
	TestTrue(*FString::Printf(TEXT("desired size back to the disc box %s, got %s"), *DesiredBefore.ToString(),
		*Fixture.Pin->GetDesiredSize().ToString()), Fixture.Pin->GetDesiredSize().Equals(DesiredBefore, 0.5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasWorldMapMiniatureBoundsTest, "CGR.Overseas.WorldMap.MiniatureFocusBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGROverseasWorldMapMiniatureBoundsTest::RunTest(const FString& Parameters)
{
	OverseasDioramaWorldMapTest::FMiniatureFixture Fixture;
	const OverseasDioramaWorldMapTest::ESetupResult Setup = Fixture.Setup(*this);
	if (Setup != OverseasDioramaWorldMapTest::ESetupResult::Ready)
	{
		return Setup == OverseasDioramaWorldMapTest::ESetupResult::Skipped;
	}

	// 위쪽 라벨 나라여도 그림 모드의 라벨판은 그림 아래로 간다.
	Fixture.Pin->SetAnchorSide(EMedallionAnchor::Top);
	Fixture.ShowMiniature();
	FSlateRect Bounds;
	if (!TestTrue(TEXT("focus bounds measurable"), Fixture.Pin->GetFocusVisualBounds(Bounds)))
	{
		return false;
	}
	const float BoundsWidth = Bounds.Right - Bounds.Left;
	TestTrue(*FString::Printf(TEXT("bounds width >= 150 (300 x 0.5), got %.1f"), BoundsWidth), BoundsWidth >= 150.0f - 0.01f);
	TestTrue(*FString::Printf(TEXT("nothing above the miniature, top %.1f"), Bounds.Top), Bounds.Top >= -0.5f);
	TestTrue(*FString::Printf(TEXT("label plate below the 100px miniature, bottom %.1f"), Bounds.Bottom), Bounds.Bottom > 100.0f + 1.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
