#include "Misc/AutomationTest.h"
#include "ICommonInputModule.h"
#include "CommonInputSettings.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Element/Buttons/MenuTileWidget.h"
#include "UI/Element/Buttons/MenuSheetTileWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Tests/LiveOpsRenderTestUtils.h"

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌
namespace LiveOpsMenuTileRender
{
	using LiveOpsRenderTestUtils::PixelAt;
	using LiveOpsRenderTestUtils::Luma;
	using LiveOpsRenderTestUtils::SaveTilePNG;
	using LiveOpsRenderTestUtils::DrawToImage;
	using LiveOpsRenderTestUtils::MeasureWidget;
	using LiveOpsRenderTestUtils::SettleDraw;
	using LiveOpsRenderTestUtils::IsBadgeRed;
	using FBox = LiveOpsRenderTestUtils::FWidgetBox;

	// 메뉴 시트 ― 11 타일 인스턴스 · 묶음 홈판 · 설정 · 세로 예산 계약을 단언한다 (아래 「시트 계약」)
	static const TCHAR* MenuPanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_MenuPanel.UI_MenuPanel_C");
	// 모달 셸 참고 렌더 (단언 없음) — 셸에 상한이 걸려 목록이 화면 밖으로 안 자라는지 눈으로 본다
	static const TCHAR* QuestPanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_QuestPanel.UI_QuestPanel_C");
	static constexpr int32 ModalWidth = 2560, ModalHeight = 1440;

	// 시트 계약 (메뉴 시트 개선 2026-10-21, 저작 = Tools/LiveOps/menu_sheet_panel.py)
	static constexpr int32 SheetWidth = 728;
	static constexpr int32 SheetHeight = 1440;
	static constexpr double SheetSizeTolerance = 1.0;
	// 왼쪽 가장자리 = 검은 테 #0A0D12 6 + 밝은 선 #2F3746 2 (S0 셸 문법)
	static constexpr double SheetEdgeRimWidth = 6.0;
	static constexpr double SheetEdgeLightWidth = 2.0;
	static constexpr double WellColumnGap = 20.0;
	static constexpr double WellColumnGapTolerance = 2.0;
	static constexpr double WellSymmetryTolerance = 2.0;
	static constexpr double WideWidthDiffMax = 2.0;
	static constexpr double SettingsWidth = 360.0;
	static constexpr double SettingsWidthTolerance = 2.0;
	static constexpr double SettingsHeight = 100.0;
	static constexpr double SettingsHeightTolerance = 4.0;
	// 누르는 자리는 G1 터치 하한 141 ― 보이는 판 100 은 그대로 두고 슬롯 음수 여백으로 위아래 20.5 씩 넓힌다(사용자 결정 2026-10-21)
	static constexpr double SettingsHitMinHeight = 141.0;
	static constexpr double SettingsCenterTolerance = 2.0;
	static constexpr double SheetBottomMargin = 28.0;
	// 알림 점은 타일 오른쪽 위 모서리 밖으로 5 걸친다(menu_sheet_tiles.BADGE_OVERHANG). 찾는 창은 그 모서리 근처만 ―
	// 안쪽 40 이면 정사각 받침(가운데 ±55)도, 옆 타일(간격 20 너머)도 창에 안 들어온다
	static constexpr int32 BadgeSearchInside = 40;
	static constexpr int32 BadgeSearchOutside = 16;
	static constexpr double BadgeMinOverhang = 2.0;
	static const TCHAR* SquareTileClassName = TEXT("UIE_MenuSheetTile_C");
	static const TCHAR* WideTileClassName = TEXT("UIE_MenuSheetTile_Wide_C");
	// 「도전」 두 칸(스카이라인 · 산업 융합)만 가로 타일 ― 「스카이라인」 은 28pt 그대로 정사각에 안 들어간다
	static constexpr int32 WideTileFirst = 9;
	// 가장자리 띠 색 (sRGB) ― 렌더 픽셀은 이 값 ± 4, 브러시 틴트는 linear 로 ± 0.001
	static const FColor SheetEdgeRimInk(0x0A, 0x0D, 0x12);
	static const FColor SheetEdgeLightInk(0x2F, 0x37, 0x46);
	static constexpr double SheetEdgeInkTolerance = 4.0;
	static constexpr float SheetEdgeTintTolerance = 0.001f;
	// 시트 크기는 드로어보다 넓고 높은 캔버스에서 잰다 ― 728x1440 캔버스는 오른쪽 정렬 슬롯이 자식을 캔버스 크기로 잘라
	// 시트가 880 으로 넓어진 회귀도 728 로 읽힌다. 높이는 Fill 이라 화면 높이를 따르므로 계약은 desired 높이로 본다
	static constexpr int32 FootprintCanvasWidth = 1000;
	static constexpr int32 FootprintCanvasHeight = 1600;

	// 메뉴 시트 다듬기 P1 (2026-10-21, 목업 A안 대조). 목업 값 = Saved/MenuSheetRedesign/capture/mock_A_sheet_728x1440.png 실측
	// 본문 여백 왼쪽 36 · 오른쪽 28 ― 가장자리 띠(8) 를 뺀 보이는 여백이 양쪽 28 로 같다
	static constexpr double VisibleContentGap = 28.0;
	// 「매일」 머리줄 위 = 목업 y 118 (헤더 84 + 시트 위 32 + 2)
	static constexpr double DailyHeadTop = 118.0;
	static constexpr double DailyHeadTopTolerance = 2.0;
	// 머리줄 마름모 배치 상자 16 (목업 .gem 16). 그리기는 렌더 배율로 색 16 + 검은 테 3 이 된다
	static constexpr double GroupGemBox = 16.0;
	// 묶음 이름 잉크 #DDE2EA (목업 .gh .nm). 렌더 글리프 가장 밝은 픽셀이 이 값 ± 6
	static const FColor GroupHeadInk(0xDD, 0xE2, 0xEA);
	static constexpr double GroupHeadInkTolerance = 6.0;
	// 타일 면 ― 면 왼쪽 안(테 4 + 8) 의 위·가운데 표본. 목업 같은 자리 (68,82,107) · (55,67,91), ± 4.
	// 렌더에 재질 면이 그려진 문맥에서만 잰다(헤드리스는 면이 없다)
	static constexpr int32 TileFaceSampleX = 12;
	static constexpr int32 TileFaceSampleTopY = 25;
	static constexpr int32 TileFaceSampleMidY = 91;
	static const FColor TileFaceTopInk(68, 82, 107);
	static const FColor TileFaceMidInk(55, 67, 91);
	static constexpr double TileFaceTolerance = 4.0;
	// 바탕 결 ― 설정 왼쪽 빈 바탕(x 40..170 · y 1300..1400) 휘도 표준편차와 사선 주기 진폭. 목업 1.99 · 4.37
	static constexpr int32 GrainPatchLeft = 40, GrainPatchTop = 1300, GrainPatchRight = 170, GrainPatchBottom = 1400;
	static constexpr double GrainStdMin = 1.5, GrainStdMax = 2.5;
	static constexpr double StripeAmpMin = 3.3, StripeAmpMax = 5.5;
	// 잠금 아이콘 ― 열린 렌더의 흰 글리프 속(휘도 ≥ 230) 자리를 잠금 렌더에서 다시 읽는다. 목업 grayscale + brightness .6
	static constexpr double LockedIconCoreLuma = 230.0;
	static constexpr int32 LockedIconMinCore = 50;
	static constexpr double LockedIconLumaRatioMin = 0.40, LockedIconLumaRatioMax = 0.75;
	static constexpr double LockedIconMaxSaturation = 0.08;
	// 회색 칸 수 ≤ 글리프 속 × 1.6 ― 텍스처가 안 옮겨져 틴트만 남으면 72x72 회색 사각이 되어 여기서 잡힌다
	static constexpr double LockedIconMaxGreyToCore = 1.6;
	static constexpr double LockedGreyLumaMin = 110.0, LockedGreyLumaMax = 200.0;
	// 감지력 자기검사용 ― 2026-10-07 이전 드로어 폭
	static constexpr float WidenedSheetWidth = 880.f;

	static const TCHAR* SheetTileNames[] = {
		TEXT("AttendanceTile"), TEXT("QuestTile"), TEXT("EventTile"),
		TEXT("MailboxTile"), TEXT("AchievementTile"), TEXT("CodexTile"),
		TEXT("NoticeTile"), TEXT("RankingTile"), TEXT("ProfileTile"),
		TEXT("SkylineTile"), TEXT("FusionTile") };

	// 묶음 = 홈판 하나 + 받침 MI 하나. First/Count 는 SheetTileNames 의 구간이다
	struct FSheetGroup
	{
		const TCHAR* Well;
		const TCHAR* Medallion;
		int32 First;
		int32 Count;
	};
	static const FSheetGroup SheetGroups[] = {
		{ TEXT("WellDaily"), TEXT("MI_MenuSheet_Medallion_Daily"), 0, 3 },
		{ TEXT("WellKeep"), TEXT("MI_MenuSheet_Medallion_Keep"), 3, 3 },
		{ TEXT("WellNews"), TEXT("MI_MenuSheet_Medallion_News"), 6, 3 },
		{ TEXT("WellChallenge"), TEXT("MI_MenuSheet_Medallion_Challenge"), 9, 2 } };

	static int32 GroupOfTile(int32 TileIndex)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SheetGroups); ++Index)
		{
			if (TileIndex < SheetGroups[Index].First + SheetGroups[Index].Count)
			{
				return Index;
			}
		}
		return UE_ARRAY_COUNT(SheetGroups) - 1;
	}

	// 결 측정 ― 휘도 표준편차 + (x±y) 주기 진폭(주기 8..45, 12 칸 평균의 최대-최소). 사선 줄이 보이는지를 숫자로 본다
	struct FGrainStats
	{
		bool bOpaque = false;
		double Std = 0.0;
		double StripeAmp = 0.0;
		double StripePeriod = 0.0;
	};

	static FGrainStats MeasureGrain(const FImage& Image, int32 Left, int32 Top, int32 Right, int32 Bottom)
	{
		FGrainStats Out;
		TArray<FIntPoint> Points;
		TArray<double> Values;
		double Sum = 0.0, MinAlpha = 255.0;
		for (int32 Y = Top; Y < Bottom; ++Y)
		{
			for (int32 X = Left; X < Right; ++X)
			{
				if (const FColor* Pixel = PixelAt(Image, X, Y))
				{
					const double Value = Luma(Pixel->R, Pixel->G, Pixel->B);
					Points.Add(FIntPoint(X, Y));
					Values.Add(Value);
					Sum += Value;
					MinAlpha = FMath::Min(MinAlpha, static_cast<double>(Pixel->A));
				}
			}
		}
		if (Values.Num() == 0)
		{
			return Out;
		}
		Out.bOpaque = MinAlpha >= 250.0;
		const double Mean = Sum / Values.Num();
		double Var = 0.0;
		for (const double Value : Values)
		{
			Var += FMath::Square(Value - Mean);
		}
		Out.Std = FMath::Sqrt(Var / Values.Num());
		constexpr int32 Bins = 12;
		for (int32 HalfStep = 16; HalfStep < 90; ++HalfStep)
		{
			const double Period = HalfStep * 0.5;
			for (const int32 Sign : { 1, -1 })
			{
				double BinSum[Bins] = {};
				int32 BinCount[Bins] = {};
				for (int32 Index = 0; Index < Points.Num(); ++Index)
				{
					const double Phase = FMath::Fmod(static_cast<double>(Points[Index].X + Sign * Points[Index].Y), Period);
					const int32 Bin = FMath::Clamp(static_cast<int32>(((Phase < 0.0 ? Phase + Period : Phase) / Period) * Bins), 0, Bins - 1);
					BinSum[Bin] += Values[Index];
					++BinCount[Bin];
				}
				double High = TNumericLimits<double>::Lowest(), Low = TNumericLimits<double>::Max();
				for (int32 Bin = 0; Bin < Bins; ++Bin)
				{
					if (BinCount[Bin] > 0)
					{
						const double BinMean = BinSum[Bin] / BinCount[Bin];
						High = FMath::Max(High, BinMean);
						Low = FMath::Min(Low, BinMean);
					}
				}
				if (High - Low > Out.StripeAmp)
				{
					Out.StripeAmp = High - Low;
					Out.StripePeriod = Period;
				}
			}
		}
		return Out;
	}

	// 렌더가 가능한 세션인데 재질이 안 그려졌다면 셰이더 컴파일 실패 같은 회귀다 ― 렌더 불가 세션(-nullrhi)만 경고로 둔다
	static void ReportUnrasterized(FAutomationTestBase& Test, const TCHAR* What)
	{
		if (FApp::CanEverRender())
		{
			Test.AddError(What);
		}
		else
		{
			Test.AddWarning(FString::Printf(TEXT("%s ― 렌더 불가 세션이라 판정하지 않는다"), What));
		}
	}

	static bool InkWithin(const FColor& Pixel, const FColor& Expected, double Tolerance)
	{
		return FMath::Abs(Pixel.R - Expected.R) <= Tolerance && FMath::Abs(Pixel.G - Expected.G) <= Tolerance
			&& FMath::Abs(Pixel.B - Expected.B) <= Tolerance;
	}

	struct FSheetFootprint
	{
		bool bMeasured = false;
		double DrawnWidth = 0.0, DrawnHeight = 0.0;
		double RimHeight = 0.0, LightHeight = 0.0;
		FVector2D Desired = FVector2D::ZeroVector;
	};

	static FSheetFootprint MeasureSheetFootprint(UUserWidget& Panel)
	{
		FSheetFootprint Out;
		const TSharedRef<SWidget> Slate = Panel.TakeWidget();
		FImage Ignored;
		UWidget* Sheet = Panel.GetWidgetFromName(TEXT("AppearSlideTarget"));
		if (Sheet == nullptr
			|| !DrawToImage(Slate, FIntPoint(FootprintCanvasWidth, FootprintCanvasHeight), 1.f, Ignored))
		{
			return Out;
		}
		const FVector2D Origin = Panel.GetRootWidget()
			? FVector2D(Panel.GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
		const LiveOpsRenderTestUtils::FWidgetBox Drawn = MeasureWidget(Panel, TEXT("AppearSlideTarget"), Origin);
		Out.DrawnWidth = Drawn.Width;
		Out.DrawnHeight = Drawn.Height;
		Out.RimHeight = MeasureWidget(Panel, TEXT("SheetEdgeRim"), Origin).Height;
		Out.LightHeight = MeasureWidget(Panel, TEXT("SheetEdgeLight"), Origin).Height;
		Out.Desired = Sheet->GetDesiredSize();
		Out.bMeasured = Drawn.bFound;
		return Out;
	}

	static bool FootprintMatchesContract(const FSheetFootprint& Footprint, double Tolerance)
	{
		return Footprint.bMeasured
			&& FMath::Abs(Footprint.DrawnWidth - SheetWidth) <= Tolerance
			&& FMath::Abs(Footprint.Desired.X - SheetWidth) <= Tolerance
			&& FMath::Abs(Footprint.Desired.Y - SheetHeight) <= Tolerance;
	}

	// IconColor 는 게터가 없는 protected UPROPERTY — 테스트가 헤더를 건드리지 않으려고 리플렉션으로 읽는다
	static FLinearColor ReadIconColor(const UObject* Object)
	{
		if (Object == nullptr)
		{
			return FLinearColor::Transparent;
		}
		if (const FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), TEXT("IconColor")))
		{
			return *Property->ContainerPtrToValuePtr<FLinearColor>(Object);
		}
		return FLinearColor::Transparent;
	}

	static FLinearColor BrushTint(const UUserWidget& Panel, const TCHAR* ImageName)
	{
		const UImage* Image = Cast<UImage>(Panel.GetWidgetFromName(ImageName));
		return Image ? Image->GetBrush().TintColor.GetSpecifiedColor() : FLinearColor::Transparent;
	}

	struct FSample
	{
		double R = 0.0, G = 0.0, B = 0.0, A = 0.0;
	};

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("LiveOpsUI/render"));
	}

	static FSample SampleBlock(const FImage& Image, int32 OriginX, int32 OriginY, int32 Size)
	{
		FSample Out;
		int32 Count = 0;
		for (int32 Y = OriginY; Y < OriginY + Size; ++Y)
		{
			for (int32 X = OriginX; X < OriginX + Size; ++X)
			{
				if (const FColor* Pixel = PixelAt(Image, X, Y))
				{
					Out.R += Pixel->R;
					Out.G += Pixel->G;
					Out.B += Pixel->B;
					Out.A += Pixel->A;
					++Count;
				}
			}
		}
		if (Count > 0)
		{
			const double Divisor = Count;
			Out.R /= Divisor; Out.G /= Divisor; Out.B /= Divisor; Out.A /= Divisor;
		}
		return Out;
	}

	static TSharedPtr<FJsonObject> SampleJson(const FSample& Sample)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetNumberField(TEXT("r"), Sample.R);
		Out->SetNumberField(TEXT("g"), Sample.G);
		Out->SetNumberField(TEXT("b"), Sample.B);
		Out->SetNumberField(TEXT("a"), Sample.A);
		return Out;
	}

	static TSharedPtr<FJsonObject> BoxJson(const FBox& Box)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("found"), Box.bFound);
		Out->SetNumberField(TEXT("left"), Box.Left);
		Out->SetNumberField(TEXT("top"), Box.Top);
		Out->SetNumberField(TEXT("width"), Box.Width);
		Out->SetNumberField(TEXT("height"), Box.Height);
		Out->SetNumberField(TEXT("bottom"), Box.Bottom());
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsMenuTileRenderTest, "CGR.LiveOps.MenuTileRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsMenuTileRenderTest::RunTest(const FString& Parameters)
{
	// 참고 렌더의 미션 창·시트는 뒤로가기 처리 대상이라 생성 때 CommonInput 뒤로가기 액션을 찾는다 ―
	// 게임(로컬 플레이어) 없이 만들면 입력 데이터가 안 읽혀 세션 첫 실행에서 ensure 가 난다
	ICommonInputModule::GetSettings().LoadData();
	using namespace LiveOpsMenuTileRender;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World == nullptr)
	{
		AddError(TEXT("에디터 월드가 없다 — 렌더 하네스를 세울 수 없다"));
		return false;
	}

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);

	// 머티리얼 브러시는 직전 씬 렌더의 씬 텍스처 추출물이 있어야 그려진다 — 백그라운드 에디터는 씬을 안 그리므로 레벨 뷰포트를 한 번 강제로 그린다
	if (GEditor)
	{
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient && ViewportClient->Viewport)
			{
				ViewportClient->Viewport->Draw(true);
			}
		}
		FlushRenderingCommands();
	}

	// 모달 셸 참고 캡처 (단언 없음) — AppearSlideTarget 에 MaxDesiredHeight 를 건 뒤 목록이 셸 안에서 스크롤되는지 본다
	if (UClass* QuestClass = LoadClass<UUserWidget>(nullptr, QuestPanelPath))
	{
		if (UUserWidget* QuestPanel = CreateWidget<UUserWidget>(World, QuestClass))
		{
			TStrongObjectPtr<UUserWidget> QuestGuard(QuestPanel);
			const TSharedRef<SWidget> QuestSlate = QuestPanel->TakeWidget();
			FImage QuestImage;
			if (DrawToImage(QuestSlate, FIntPoint(ModalWidth, ModalHeight), 1.f, QuestImage))
			{
				SaveTilePNG(QuestImage, Directory / TEXT("panel_quest_reference.png"));
			}
			const FVector2D QuestOrigin = QuestPanel->GetRootWidget()
				? FVector2D(QuestPanel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
			const LiveOpsMenuTileRender::FBox ShellBox = MeasureWidget(*QuestPanel, TEXT("AppearSlideTarget"), QuestOrigin);
			const LiveOpsMenuTileRender::FBox QuestListBox = MeasureWidget(*QuestPanel, TEXT("QuestList"), QuestOrigin);
			UE_LOG(LogTemp, Display, TEXT("MenuTileRender quest shell=(%.1f,%.1f %.0fx%.0f) list=(%.1f,%.1f %.0fx%.0f) viewport=%dx%d"),
				ShellBox.Left, ShellBox.Top, ShellBox.Width, ShellBox.Height,
				QuestListBox.Left, QuestListBox.Top, QuestListBox.Width, QuestListBox.Height,
				ModalWidth, ModalHeight);
		}
	}

	// 시트 (메뉴 시트 개선 2026-10-21) ― 묶음 4 홈판 · 묶음색 받침 · 「도전」 가로 2칸 · 볼륨 설정.
	// 타일 인스턴스는 생성 시점 CDO 값을 들고 있어 CDO 를 고쳐도 안 따라온다 → 시트에 놓인 인스턴스를 직접 잰다.
	TSharedPtr<FJsonObject> SheetJson = MakeShared<FJsonObject>();
	using FSheetBox = LiveOpsRenderTestUtils::FWidgetBox;
	if (UClass* PanelClass = LoadClass<UUserWidget>(nullptr, MenuPanelPath))
	{
		if (UUserWidget* Panel = CreateWidget<UUserWidget>(World, PanelClass))
		{
			TStrongObjectPtr<UUserWidget> PanelGuard(Panel);
			const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();

			// ---- SheetTilesAreMenuSheetTiles · GroupMedallions ----
			TArray<UMenuSheetTileWidget*> SheetTiles;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(SheetTileNames); ++Index)
			{
				const TCHAR* TileName = SheetTileNames[Index];
				UWidget* Found = Panel->GetWidgetFromName(FName(TileName));
				UMenuSheetTileWidget* SheetTile = Cast<UMenuSheetTileWidget>(Found);
				SheetTiles.Add(SheetTile);
				const bool bWide = Index >= WideTileFirst;
				const FString ClassName = Found ? Found->GetClass()->GetName() : FString(TEXT("none"));
				SheetJson->SetStringField(FString::Printf(TEXT("%s_class"), TileName), ClassName);
				TestTrue(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s 가 시트 타일이다 (%s, 기대 %s)"), TileName,
					*ClassName, bWide ? WideTileClassName : SquareTileClassName),
					SheetTile != nullptr && ClassName == (bWide ? WideTileClassName : SquareTileClassName));
				// 본사 레벨업 카드(HQManagePanelWidget)와 튜토리얼 앵커가 이 캐스트로 타일을 찾는다
				TestTrue(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s → UMenuTileWidget 캐스트"), TileName),
					Cast<UMenuTileWidget>(Found) != nullptr);
				if (SheetTile == nullptr)
				{
					continue;
				}
				TestTrue(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s 아이콘·라벨이 저작돼 있다"), TileName),
					SheetTile->GetTileIcon() != nullptr && !SheetTile->GetTileLabel().IsEmpty());
				// 라벨 아웃라인 전면 폐지(2026-09-26 사용자 지시) ― 옛 타일에서 보던 단언을 시트 타일로 옮겼다
				TestFalse(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s 라벨 아웃라인 비활성"), TileName),
					SheetTile->GetTextOutlineEnabled());
				const UCommonButtonStyle* InstanceStyle = SheetTile->GetStyle();
				const UCommonButtonStyle* ClassStyle = GetDefault<UMenuSheetTileWidget>(SheetTile->GetClass())->GetStyle();
				TestTrue(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s 스타일 %s == 자기 WBP CDO %s"), TileName,
					InstanceStyle ? *InstanceStyle->GetClass()->GetName() : TEXT("none"),
					ClassStyle ? *ClassStyle->GetClass()->GetName() : TEXT("none")),
					InstanceStyle != nullptr && ClassStyle != nullptr && InstanceStyle->GetClass() == ClassStyle->GetClass());
				const UMenuSheetTileWidget* ClassDefault = GetDefault<UMenuSheetTileWidget>(SheetTile->GetClass());
				const FLinearColor InstanceIcon = ReadIconColor(SheetTile);
				const FLinearColor DefaultIcon = ReadIconColor(ClassDefault);
				const FLinearColor InstanceText = SheetTile->GetTextColor().GetSpecifiedColor();
				const FLinearColor DefaultText = ClassDefault->GetTextColor().GetSpecifiedColor();
				TestTrue(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s IconColor %.4f,%.4f,%.4f == CDO %.4f,%.4f,%.4f"), TileName,
					InstanceIcon.R, InstanceIcon.G, InstanceIcon.B, DefaultIcon.R, DefaultIcon.G, DefaultIcon.B),
					DefaultIcon.A > 0.f && InstanceIcon.Equals(DefaultIcon, 0.002f));
				TestTrue(FString::Printf(TEXT("[SheetTilesAreMenuSheetTiles] %s TextColor %.4f,%.4f,%.4f == CDO %.4f,%.4f,%.4f"), TileName,
					InstanceText.R, InstanceText.G, InstanceText.B, DefaultText.R, DefaultText.G, DefaultText.B),
					DefaultText.A > 0.f && InstanceText.Equals(DefaultText, 0.002f));

				const FSheetGroup& Group = SheetGroups[GroupOfTile(Index)];
				const UMaterialInterface* Medallion = SheetTile->GetMedallionMaterial();
				const FString MedallionName = Medallion ? Medallion->GetName() : FString(TEXT("none"));
				SheetJson->SetStringField(FString::Printf(TEXT("%s_medallion"), TileName), MedallionName);
				TestEqual(FString::Printf(TEXT("[GroupMedallions] %s 받침"), TileName), MedallionName, FString(Group.Medallion));
			}

			// ---- NoSeparators ― 묶음 구분은 홈판이 맡는다(구분선 3 + 설정 위 1 제거) ----
			int32 SeparatorCount = 0;
			if (UWidgetTree* PanelTree = Panel->WidgetTree)
			{
				PanelTree->ForEachWidget([&SeparatorCount](UWidget* Widget)
				{
					if (Widget && (Widget->GetName().Contains(TEXT("Separator"))
						|| Widget->GetClass()->GetName().Contains(TEXT("Separator"))))
					{
						++SeparatorCount;
					}
				});
			}
			SheetJson->SetNumberField(TEXT("separator_count"), SeparatorCount);
			TestEqual(TEXT("[NoSeparators] 시트 트리의 구분선 위젯 수"), SeparatorCount, 0);

			// 알림 점을 전부 켠다 ― 점이 홈판·행 클리핑에 잘리는지는 실제로 그려 봐야 안다
			for (UMenuSheetTileWidget* SheetTile : SheetTiles)
			{
				if (SheetTile)
				{
					SheetTile->ShowBadge();
				}
			}
			const FIntPoint SheetSize(SheetWidth, SheetHeight);
			FImage SheetImage;
			// 처음 쓰는 재질(결 층 등)은 예열 그리기가 컴파일을 요청한다 ― 끝날 때까지 기다린 뒤 본 그림을 그린다(안 그러면 그 층만 빠진다)
			const bool bWarmed = SettleDraw(PanelSlate, SheetSize, 1.f);
			FAssetCompilingManager::Get().FinishAllCompilation();
			if (GShaderCompilingManager != nullptr)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			// 결 텍스처는 스트리밍이 낮은 밉(평균 회색)으로 시작한다 ― 헤드리스에서 사선이 통째로 뭉개졌다. 전 밉을 올린 뒤 다시 예열한다
			for (const TCHAR* LayerName : { TEXT("SheetDiagonal"), TEXT("SheetGrain") })
			{
				const UImage* Layer = Cast<UImage>(Panel->GetWidgetFromName(LayerName));
				const UMaterialInterface* LayerMaterial = Layer ? Cast<UMaterialInterface>(Layer->GetBrush().GetResourceObject()) : nullptr;
				TArray<UTexture*> Used;
				if (LayerMaterial)
				{
					LayerMaterial->GetUsedTextures(Used, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
				}
				for (UTexture* Texture : Used)
				{
					if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
					{
						Texture2D->SetForceMipLevelsToBeResident(30.f);
						Texture2D->WaitForStreaming();
					}
				}
			}
			IStreamingManager::Get().StreamAllResources(5.0f);
			FlushRenderingCommands();
			SettleDraw(PanelSlate, SheetSize, 1.f);
			const bool bSheetOk = bWarmed && DrawToImage(PanelSlate, SheetSize, 1.f, SheetImage)
				&& SaveTilePNG(SheetImage, Directory / TEXT("menu_sheet_reference.png"));
			SheetJson->SetBoolField(TEXT("captured"), bSheetOk);
			TestTrue(TEXT("시트 캡처 저장 (728x1440)"), bSheetOk);

			const FVector2D SheetOrigin = Panel->GetRootWidget()
				? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
			auto Measure = [Panel, &SheetOrigin](const TCHAR* Name) { return MeasureWidget(*Panel, Name, SheetOrigin); };
			auto HasArea = [](const FSheetBox& Box) { return Box.bFound && Box.Width > 1.0 && Box.Height > 1.0; };

			// ---- 왼쪽 가장자리 테(검은 6 + 밝은 선 2) ― 자리·폭·색. 시트 폭과 띠 높이는 맨 끝 넓은 캔버스(SheetWidth)에서 잰다 ----
			const FSheetBox SheetBox = Measure(TEXT("AppearSlideTarget"));
			SheetJson->SetObjectField(TEXT("sheet"), BoxJson(SheetBox));
			const FSheetBox EdgeRim = Measure(TEXT("SheetEdgeRim"));
			const FSheetBox EdgeLight = Measure(TEXT("SheetEdgeLight"));
			SheetJson->SetObjectField(TEXT("edge_rim"), BoxJson(EdgeRim));
			SheetJson->SetObjectField(TEXT("edge_light"), BoxJson(EdgeLight));
			TestTrue(FString::Printf(TEXT("[SheetEdge] 검은 테 x%.1f 폭%.1f · 밝은 선 x%.1f 폭%.1f (기대 0/%.0f · %.0f/%.0f, 시트 높이 전체)"),
				EdgeRim.Left - SheetBox.Left, EdgeRim.Width, EdgeLight.Left - SheetBox.Left, EdgeLight.Width,
				SheetEdgeRimWidth, SheetEdgeRimWidth, SheetEdgeLightWidth),
				HasArea(EdgeRim) && HasArea(EdgeLight)
				&& FMath::Abs(EdgeRim.Left - SheetBox.Left) <= SheetSizeTolerance
				&& FMath::Abs(EdgeRim.Width - SheetEdgeRimWidth) <= SheetSizeTolerance
				&& FMath::Abs(EdgeLight.Left - EdgeRim.Right()) <= SheetSizeTolerance
				&& FMath::Abs(EdgeLight.Width - SheetEdgeLightWidth) <= SheetSizeTolerance);
			// 색 ― 렌더 픽셀(sRGB)과 브러시 틴트(linear) 둘 다. 띠가 다른 층에 덮이거나 틴트가 sRGB 값 그대로 들어가면 잡힌다
			const int32 EdgeSampleY = SheetHeight / 2;
			const FSample RimInk = bSheetOk ? SampleBlock(SheetImage,
				FMath::RoundToInt(static_cast<float>(EdgeRim.Left)) + 1, EdgeSampleY, 4) : FSample();
			const FSample LightInk = bSheetOk ? SampleBlock(SheetImage,
				FMath::RoundToInt(static_cast<float>(EdgeLight.Left)), EdgeSampleY, 2) : FSample();
			auto InkNear = [](const FSample& Sample, const FColor& Expected)
			{
				return Sample.A >= 250.0 && FMath::Abs(Sample.R - Expected.R) <= SheetEdgeInkTolerance
					&& FMath::Abs(Sample.G - Expected.G) <= SheetEdgeInkTolerance
					&& FMath::Abs(Sample.B - Expected.B) <= SheetEdgeInkTolerance;
			};
			TestTrue(FString::Printf(TEXT("[SheetEdge] 렌더 색 검은 테 (%.0f,%.0f,%.0f) · 밝은 선 (%.0f,%.0f,%.0f) (기대 #0A0D12 · #2F3746 ±%.0f)"),
				RimInk.R, RimInk.G, RimInk.B, LightInk.R, LightInk.G, LightInk.B, SheetEdgeInkTolerance),
				bSheetOk && InkNear(RimInk, SheetEdgeRimInk) && InkNear(LightInk, SheetEdgeLightInk));
			const FLinearColor RimTint = BrushTint(*Panel, TEXT("SheetEdgeRim"));
			const FLinearColor LightTint = BrushTint(*Panel, TEXT("SheetEdgeLight"));
			TestTrue(FString::Printf(TEXT("[SheetEdge] 틴트 linear 검은 테 %.4f,%.4f,%.4f · 밝은 선 %.4f,%.4f,%.4f"),
				RimTint.R, RimTint.G, RimTint.B, LightTint.R, LightTint.G, LightTint.B),
				RimTint.Equals(FLinearColor::FromSRGBColor(SheetEdgeRimInk), SheetEdgeTintTolerance)
				&& LightTint.Equals(FLinearColor::FromSRGBColor(SheetEdgeLightInk), SheetEdgeTintTolerance));

			// ---- WellsAndRows · BadgeInsideWell ----
			int32 WellCount = 0;
			FSheetBox PreviousWell;
			FSheetBox LastWell;
			for (const FSheetGroup& Group : SheetGroups)
			{
				const FSheetBox Well = Measure(Group.Well);
				SheetJson->SetObjectField(Group.Well, BoxJson(Well));
				if (!HasArea(Well))
				{
					AddError(FString::Printf(TEXT("[WellsAndRows] 홈판이 없다: %s"), Group.Well));
					continue;
				}
				++WellCount;
				LastWell = Well;
				if (PreviousWell.bFound)
				{
					TestTrue(FString::Printf(TEXT("[WellsAndRows] %s 위 %.1f 가 앞 홈판 바닥 %.1f 아래"), Group.Well, Well.Top,
						PreviousWell.Bottom()), Well.Top > PreviousWell.Bottom());
				}
				PreviousWell = Well;

				TArray<FSheetBox> Row;
				for (int32 Offset = 0; Offset < Group.Count; ++Offset)
				{
					Row.Add(Measure(SheetTileNames[Group.First + Offset]));
				}
				bool bRowMeasured = true;
				for (int32 Offset = 0; Offset < Row.Num(); ++Offset)
				{
					const FSheetBox& TileBox = Row[Offset];
					const TCHAR* TileName = SheetTileNames[Group.First + Offset];
					SheetJson->SetObjectField(TileName, BoxJson(TileBox));
					bRowMeasured &= HasArea(TileBox);
					TestTrue(FString::Printf(TEXT("[WellsAndRows] %s (%.1f,%.1f %.1fx%.1f) 가 %s 안"), TileName,
						TileBox.Left, TileBox.Top, TileBox.Width, TileBox.Height, Group.Well),
						HasArea(TileBox) && TileBox.Left >= Well.Left && TileBox.Right() <= Well.Right()
						&& TileBox.Top >= Well.Top && TileBox.Bottom() <= Well.Bottom());
				}
				if (!bRowMeasured)
				{
					AddError(FString::Printf(TEXT("[WellsAndRows] %s 행 타일 지오메트리를 못 쟀다"), Group.Well));
					continue;
				}
				for (int32 Offset = 1; Offset < Row.Num(); ++Offset)
				{
					const double Gap = Row[Offset].Left - Row[Offset - 1].Right();
					TestTrue(FString::Printf(TEXT("[WellsAndRows] %s 열 간격 %d-%d = %.1f (기대 %.0f±%.0f)"), Group.Well,
						Offset - 1, Offset, Gap, WellColumnGap, WellColumnGapTolerance),
						FMath::Abs(Gap - WellColumnGap) <= WellColumnGapTolerance);
				}
				const double LeftMargin = Row[0].Left - Well.Left;
				const double RightMargin = Well.Right() - Row.Last().Right();
				TestTrue(FString::Printf(TEXT("[WellsAndRows] %s 좌우 대칭 %.1f / %.1f (차 ≤ %.0f)"), Group.Well,
					LeftMargin, RightMargin, WellSymmetryTolerance),
					FMath::Abs(LeftMargin - RightMargin) <= WellSymmetryTolerance);
				if (Group.Count == 2)
				{
					TestTrue(FString::Printf(TEXT("[WellsAndRows] %s 가로 두 칸 폭 %.1f / %.1f (차 ≤ %.0f)"), Group.Well,
						Row[0].Width, Row[1].Width, WideWidthDiffMax), FMath::Abs(Row[0].Width - Row[1].Width) <= WideWidthDiffMax);
				}

				for (int32 Offset = 0; Offset < Row.Num(); ++Offset)
				{
					const int32 TileIndex = Group.First + Offset;
					const TCHAR* TileName = SheetTileNames[TileIndex];
					// 타일 → 홈판 사이에 자르는 조상이 있으면 모서리 밖 5px 가 사라진다
					FString Clipper;
					bool bReachedWell = false;
					for (UWidget* Node = SheetTiles[TileIndex]; Node != nullptr; Node = Node->GetParent())
					{
						if (Node->GetClipping() != EWidgetClipping::Inherit || Node->IsA<UScrollBox>())
						{
							Clipper = Node->GetName();
							break;
						}
						if (Node->GetFName() == FName(Group.Well))
						{
							bReachedWell = true;
							break;
						}
					}
					// 자르는 조상에서 멈췄으면 아래 단언이 실패를 맡는다 ― 아니면 반드시 자기 홈판에서 멈췄어야 한다
					TestTrue(FString::Printf(TEXT("[BadgeInsideWell] %s 의 조상 사슬에 %s 가 있다"), TileName, Group.Well),
						bReachedWell || !Clipper.IsEmpty());
					TestTrue(FString::Printf(TEXT("[BadgeInsideWell] %s → %s 사이에 자르는 조상 없음 (%s)"), TileName, Group.Well,
						Clipper.IsEmpty() ? TEXT("없음") : *Clipper), Clipper.IsEmpty());

					// 실제 그림의 빨간 점 ― 타일 오른쪽 위 모서리 근처 창에서만 찾는다(받침 테 색이 섞이지 않게)
					const FSheetBox& TileBox = Row[Offset];
					const int32 TileRight = FMath::RoundToInt(static_cast<float>(TileBox.Right()));
					const int32 TileTop = FMath::RoundToInt(static_cast<float>(TileBox.Top));
					int32 MinX = TNumericLimits<int32>::Max(), MaxX = TNumericLimits<int32>::Lowest();
					int32 MinY = TNumericLimits<int32>::Max(), MaxY = TNumericLimits<int32>::Lowest(), RedCount = 0;
					for (int32 Y = TileTop - BadgeSearchOutside; bSheetOk && Y < TileTop + BadgeSearchInside; ++Y)
					{
						for (int32 X = TileRight - BadgeSearchInside; X < TileRight + BadgeSearchOutside; ++X)
						{
							const FColor* Pixel = PixelAt(SheetImage, X, Y);
							if (Pixel && IsBadgeRed(*Pixel))
							{
								++RedCount;
								MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
								MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y);
							}
						}
					}
					const double RightOverhang = (MaxX + 1) - TileBox.Right();
					const double TopOverhang = TileBox.Top - MinY;
					UE_LOG(LogTemp, Display, TEXT("MenuTileRender badge %s red=%d glyph x%d..%d y%d..%d overhang r%.1f t%.1f well=(%.1f,%.1f %.1fx%.1f)"),
						TileName, RedCount, MinX, MaxX, MinY, MaxY, RightOverhang, TopOverhang, Well.Left, Well.Top, Well.Width, Well.Height);
					TestTrue(FString::Printf(TEXT("[BadgeInsideWell] %s 빨간 점 픽셀 %d"), TileName, RedCount), RedCount > 0);
					TestTrue(FString::Printf(TEXT("[BadgeInsideWell] %s 점이 모서리 밖으로 걸쳐 그려졌다 (오른쪽 %.1f · 위 %.1f ≥ %.0f)"),
						TileName, RightOverhang, TopOverhang, BadgeMinOverhang),
						RedCount > 0 && RightOverhang >= BadgeMinOverhang && TopOverhang >= BadgeMinOverhang);
					TestTrue(FString::Printf(TEXT("[BadgeInsideWell] %s 점 x%d..%d y%d..%d 이 %s (%.1f..%.1f, %.1f..%.1f) 안"),
						TileName, MinX, MaxX, MinY, MaxY, Group.Well, Well.Left, Well.Right(), Well.Top, Well.Bottom()),
						RedCount > 0 && MinX >= Well.Left && MaxX + 1 <= Well.Right() && MinY >= Well.Top && MaxY + 1 <= Well.Bottom());
				}
			}
			TestEqual(TEXT("[WellsAndRows] 홈판 수"), WellCount, static_cast<int32>(UE_ARRAY_COUNT(SheetGroups)));

			// 일일 리셋 시계는 「매일」 머리줄 오른쪽에 산다
			const FSheetBox DailyHead = Measure(TEXT("GroupHeadDaily"));
			const FSheetBox Countdown = Measure(TEXT("CountdownText"));
			SheetJson->SetObjectField(TEXT("GroupHeadDaily"), BoxJson(DailyHead));
			SheetJson->SetObjectField(TEXT("CountdownText"), BoxJson(Countdown));
			TestTrue(FString::Printf(TEXT("CountdownText (%.1f,%.1f %.1fx%.1f) 가 매일 머리줄 (%.1f,%.1f %.1fx%.1f) 안"),
				Countdown.Left, Countdown.Top, Countdown.Width, Countdown.Height,
				DailyHead.Left, DailyHead.Top, DailyHead.Width, DailyHead.Height),
				HasArea(DailyHead) && HasArea(Countdown) && Countdown.Left >= DailyHead.Left - 1.0
				&& Countdown.Right() <= DailyHead.Right() + 1.0 && Countdown.Top >= DailyHead.Top - 1.0
				&& Countdown.Bottom() <= DailyHead.Bottom() + 1.0);

			// ---- SettingsButton · VerticalBudget ― 보이는 판(SettingsFace) 360x100, 누르는 자리(SettingsBtn) 는 141 이상 ----
			const FSheetBox Settings = Measure(TEXT("SettingsFace"));
			const FSheetBox SettingsHit = Measure(TEXT("SettingsBtn"));
			SheetJson->SetObjectField(TEXT("SettingsFace"), BoxJson(Settings));
			SheetJson->SetObjectField(TEXT("SettingsBtn"), BoxJson(SettingsHit));
			TestTrue(FString::Printf(TEXT("[SettingsButton] 히트 %.1fx%.1f (높이 >= %.0f, 폭 = 판) 가 보이는 판 (%.1f,%.1f %.1fx%.1f) 을 감싼다"),
				SettingsHit.Width, SettingsHit.Height, SettingsHitMinHeight, Settings.Left, Settings.Top, Settings.Width, Settings.Height),
				HasArea(SettingsHit) && HasArea(Settings) && SettingsHit.Height >= SettingsHitMinHeight
				&& FMath::Abs(SettingsHit.Width - Settings.Width) <= SettingsWidthTolerance
				&& SettingsHit.Top <= Settings.Top && SettingsHit.Bottom() >= Settings.Bottom()
				&& SettingsHit.Bottom() <= SheetHeight);
			// 보이는 영역(가장자리 띠 오른쪽 ~ 시트 오른쪽) 의 가운데 ― 목업 .foot 도 본문(36/28) 가운데에 선다
			const double SheetCenterX = 0.5 * (EdgeLight.Right() + SheetBox.Right());
			const double SettingsCenterX = Settings.Left + 0.5 * Settings.Width;
			TestTrue(FString::Printf(TEXT("[SettingsButton] %.1fx%.1f (기대 %.0f±%.0f x %.0f±%.0f)"), Settings.Width, Settings.Height,
				SettingsWidth, SettingsWidthTolerance, SettingsHeight, SettingsHeightTolerance),
				HasArea(Settings) && FMath::Abs(Settings.Width - SettingsWidth) <= SettingsWidthTolerance
				&& FMath::Abs(Settings.Height - SettingsHeight) <= SettingsHeightTolerance);
			TestTrue(FString::Printf(TEXT("[SettingsButton] 중심 x %.1f == 보이는 시트 중심 %.1f (±%.0f)"), SettingsCenterX, SheetCenterX,
				SettingsCenterTolerance), HasArea(Settings) && HasArea(SheetBox)
				&& FMath::Abs(SettingsCenterX - SheetCenterX) <= SettingsCenterTolerance);
			TestTrue(FString::Printf(TEXT("[VerticalBudget] 설정 바닥 %.1f <= %d - %.0f"), Settings.Bottom(), SheetHeight,
				SheetBottomMargin), HasArea(Settings) && Settings.Bottom() <= SheetHeight - SheetBottomMargin);
			TestTrue(FString::Printf(TEXT("[VerticalBudget] 마지막 홈판 바닥 %.1f < 설정 위 %.1f"), LastWell.Bottom(), Settings.Top),
				LastWell.bFound && HasArea(Settings) && LastWell.Bottom() < Settings.Top);

			// ---- [ContentGaps] 보이는 좌우 여백이 같다 (가장자리 띠 오른쪽 → 홈판 왼쪽 == 홈판 오른쪽 → 시트 오른쪽) ----
			const FSheetBox FirstWell = Measure(SheetGroups[0].Well);
			const double LeftGap = FirstWell.Left - EdgeLight.Right();
			const double RightGap = SheetBox.Right() - FirstWell.Right();
			SheetJson->SetNumberField(TEXT("visible_gap_left"), LeftGap);
			SheetJson->SetNumberField(TEXT("visible_gap_right"), RightGap);
			TestTrue(FString::Printf(TEXT("[ContentGaps] 보이는 여백 왼쪽 %.1f · 오른쪽 %.1f (기대 둘 다 %.0f±1)"), LeftGap, RightGap,
				VisibleContentGap), HasArea(FirstWell) && FMath::Abs(LeftGap - VisibleContentGap) <= SheetSizeTolerance
				&& FMath::Abs(RightGap - VisibleContentGap) <= SheetSizeTolerance);

			// ---- [HeaderGap] 「매일」 머리줄이 목업 자리(118)에서 시작하고 헤더와 겹치지 않는다 ----
			const FSheetBox HeaderBox = Measure(TEXT("HeaderSize"));
			SheetJson->SetObjectField(TEXT("HeaderSize"), BoxJson(HeaderBox));
			TestTrue(FString::Printf(TEXT("[HeaderGap] 매일 머리줄 위 %.1f (기대 %.0f±%.0f) · 헤더 바닥 %.1f"), DailyHead.Top,
				DailyHeadTop, DailyHeadTopTolerance, HeaderBox.Bottom()),
				HasArea(DailyHead) && HasArea(HeaderBox) && FMath::Abs(DailyHead.Top - DailyHeadTop) <= DailyHeadTopTolerance
				&& HeaderBox.Bottom() <= DailyHead.Top + 0.5);

			// ---- [GroupGem] 마름모 배치 상자 16 · [GroupHeadInk] 묶음 이름 잉크 #DDE2EA ----
			static const TCHAR* GroupKeys[] = { TEXT("Daily"), TEXT("Keep"), TEXT("News"), TEXT("Challenge") };
			const FLinearColor HeadInkLinear = FLinearColor::FromSRGBColor(GroupHeadInk);
			for (int32 GroupIndex = 0; GroupIndex < UE_ARRAY_COUNT(GroupKeys); ++GroupIndex)
			{
				const FString GemName = FString::Printf(TEXT("GroupGem%s"), GroupKeys[GroupIndex]);
				const FSheetBox Gem = Measure(*GemName);
				TestTrue(FString::Printf(TEXT("[GroupGem] %s 상자 %.1fx%.1f (기대 %.0f)"), *GemName, Gem.Width, Gem.Height, GroupGemBox),
					HasArea(Gem) && FMath::Abs(Gem.Width - GroupGemBox) <= SheetSizeTolerance
					&& FMath::Abs(Gem.Height - GroupGemBox) <= SheetSizeTolerance);

				const FString LabelName = FString::Printf(TEXT("GroupLabel%d"), GroupIndex);
				const UTextBlock* Label = Cast<UTextBlock>(Panel->GetWidgetFromName(FName(*LabelName)));
				const FLinearColor LabelInk = Label ? Label->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::Transparent;
				TestTrue(FString::Printf(TEXT("[GroupHeadInk] %s 잉크 linear %.4f,%.4f,%.4f (기대 #DDE2EA)"), *LabelName,
					LabelInk.R, LabelInk.G, LabelInk.B), Label != nullptr && LabelInk.Equals(HeadInkLinear, 0.002f));
				const FSheetBox LabelBox = Measure(*LabelName);
				FColor Brightest(0, 0, 0, 0);
				for (int32 Y = FMath::FloorToInt(LabelBox.Top); bSheetOk && Y < FMath::CeilToInt(LabelBox.Bottom()); ++Y)
				{
					for (int32 X = FMath::FloorToInt(LabelBox.Left); X < FMath::CeilToInt(LabelBox.Right()); ++X)
					{
						const FColor* Pixel = PixelAt(SheetImage, X, Y);
						if (Pixel && Luma(Pixel->R, Pixel->G, Pixel->B) > Luma(Brightest.R, Brightest.G, Brightest.B))
						{
							Brightest = *Pixel;
						}
					}
				}
				TestTrue(FString::Printf(TEXT("[GroupHeadInk] %s 렌더 가장 밝은 잉크 (%d,%d,%d) (기대 221,226,234 ±%.0f)"), *LabelName,
					Brightest.R, Brightest.G, Brightest.B, GroupHeadInkTolerance),
					bSheetOk && InkWithin(Brightest, GroupHeadInk, GroupHeadInkTolerance));
			}

			// ---- [WellShort] 도전 홈판(164) 은 짧은 홈판 전용 MI ― 모서리 반경이 다른 홈판과 같은 28 이 된다 ----
			for (const FSheetGroup& Group : SheetGroups)
			{
				const FString FaceName = FString(TEXT("WellFace")) + (FString(Group.Well).RightChop(4));
				const UImage* Face = Cast<UImage>(Panel->GetWidgetFromName(FName(*FaceName)));
				const UObject* Resource = Face ? Face->GetBrush().GetResourceObject() : nullptr;
				const FString Expected = Group.Count == 2 ? TEXT("MI_MenuSheet_WellShort") : TEXT("MI_MenuSheet_Well");
				TestEqual(FString::Printf(TEXT("[WellShort] %s 재질"), *FaceName),
					Resource ? Resource->GetName() : FString(TEXT("none")), Expected);
			}

			// ---- [TileFace] 타일 면 밝기가 목업과 같다 (재질 면이 그려진 문맥에서만) ----
			{
				const FSheetBox FaceTile = Measure(SheetTileNames[0]);
				const int32 FaceX = FMath::RoundToInt(static_cast<float>(FaceTile.Left)) + TileFaceSampleX;
				const int32 FaceTop = FMath::RoundToInt(static_cast<float>(FaceTile.Top));
				const FSample FaceHigh = bSheetOk ? SampleBlock(SheetImage, FaceX, FaceTop + TileFaceSampleTopY, 3) : FSample();
				const FSample FaceMid = bSheetOk ? SampleBlock(SheetImage, FaceX, FaceTop + TileFaceSampleMidY, 3) : FSample();
				const bool bFaceDrawn = FaceHigh.A >= 250.0 && FaceMid.A >= 250.0;
				SheetJson->SetObjectField(TEXT("tile_face_top"), SampleJson(FaceHigh));
				SheetJson->SetObjectField(TEXT("tile_face_mid"), SampleJson(FaceMid));
				SheetJson->SetBoolField(TEXT("tile_face_rasterized"), bFaceDrawn);
				UE_LOG(LogTemp, Display, TEXT("MenuTileRender tile face top=(%.0f,%.0f,%.0f,a%.0f) mid=(%.0f,%.0f,%.0f,a%.0f) rasterized=%d"),
					FaceHigh.R, FaceHigh.G, FaceHigh.B, FaceHigh.A, FaceMid.R, FaceMid.G, FaceMid.B, FaceMid.A, bFaceDrawn ? 1 : 0);
				if (bFaceDrawn)
				{
					auto Near = [](const FSample& Sample, const FColor& Expected)
					{
						return FMath::Abs(Sample.R - Expected.R) <= TileFaceTolerance && FMath::Abs(Sample.G - Expected.G) <= TileFaceTolerance
							&& FMath::Abs(Sample.B - Expected.B) <= TileFaceTolerance;
					};
					TestTrue(FString::Printf(TEXT("[TileFace] 면 위 (%.0f,%.0f,%.0f) · 가운데 (%.0f,%.0f,%.0f) (목업 68,82,107 · 55,67,91 ±%.0f)"),
						FaceHigh.R, FaceHigh.G, FaceHigh.B, FaceMid.R, FaceMid.G, FaceMid.B, TileFaceTolerance),
						Near(FaceHigh, TileFaceTopInk) && Near(FaceMid, TileFaceMidInk));
				}
				else
				{
					ReportUnrasterized(*this, TEXT("[TileFace] 타일 면 재질이 그려지지 않았다(알파 < 250)"));
				}
			}

			// ---- [SheetGrain] 사선·노이즈 결이 목업처럼 보인다 (바탕 재질이 그려진 문맥에서만) ----
			{
				const FGrainStats Grain = bSheetOk
					? MeasureGrain(SheetImage, GrainPatchLeft, GrainPatchTop, GrainPatchRight, GrainPatchBottom) : FGrainStats();
				SheetJson->SetNumberField(TEXT("grain_std"), Grain.Std);
				SheetJson->SetNumberField(TEXT("grain_stripe_amp"), Grain.StripeAmp);
				SheetJson->SetNumberField(TEXT("grain_stripe_period"), Grain.StripePeriod);
				UE_LOG(LogTemp, Display, TEXT("MenuTileRender grain std=%.3f stripe_amp=%.3f period=%.1f opaque=%d (목업 1.99 · 4.37 · 31)"),
					Grain.Std, Grain.StripeAmp, Grain.StripePeriod, Grain.bOpaque ? 1 : 0);
				if (Grain.bOpaque)
				{
					TestTrue(FString::Printf(TEXT("[SheetGrain] 바탕 휘도 표준편차 %.2f (%.1f~%.1f) · 사선 진폭 %.2f (%.1f~%.1f)"),
						Grain.Std, GrainStdMin, GrainStdMax, Grain.StripeAmp, StripeAmpMin, StripeAmpMax),
						Grain.Std >= GrainStdMin && Grain.Std <= GrainStdMax
						&& Grain.StripeAmp >= StripeAmpMin && Grain.StripeAmp <= StripeAmpMax);
				}
				else
				{
					ReportUnrasterized(*this, TEXT("[SheetGrain] 시트 바탕 재질이 그려지지 않았다(바탕 칸이 불투명하지 않다)"));
				}
			}

			// ---- [LockedIconGrey] 잠금 칸 아이콘 = 회색·어둡게. 정사각(도감)·가로(스카이라인) 를 잠가 다시 그린다 ----
			{
				static const TCHAR* LockedNames[] = { TEXT("CodexTile"), TEXT("SkylineTile") };
				TArray<TArray<FIntPoint>> Cores;
				TArray<FSheetBox> IconBoxes;
				for (const TCHAR* TileName : LockedNames)
				{
					TArray<FIntPoint>& Core = Cores.AddDefaulted_GetRef();
					UUserWidget* TileWidget = Cast<UUserWidget>(Panel->GetWidgetFromName(FName(TileName)));
					const FSheetBox IconBox = TileWidget ? MeasureWidget(*TileWidget, TEXT("IconImage"), SheetOrigin) : FSheetBox();
					IconBoxes.Add(IconBox);
					for (int32 Y = FMath::FloorToInt(IconBox.Top); bSheetOk && IconBox.bFound && Y < FMath::CeilToInt(IconBox.Bottom()); ++Y)
					{
						for (int32 X = FMath::FloorToInt(IconBox.Left); X < FMath::CeilToInt(IconBox.Right()); ++X)
						{
							const FColor* Pixel = PixelAt(SheetImage, X, Y);
							if (Pixel && Pixel->A >= 250 && Luma(Pixel->R, Pixel->G, Pixel->B) >= LockedIconCoreLuma)
							{
								Core.Add(FIntPoint(X, Y));
							}
						}
					}
					if (UMenuTileWidget* MenuTile = Cast<UMenuTileWidget>(TileWidget))
					{
						MenuTile->SetLocked(true, FText::FromString(TEXT("본사 Lv.2")));
					}
				}
				FImage LockedSheet;
				const bool bLockedSheetOk = SettleDraw(PanelSlate, SheetSize, 1.f) && DrawToImage(PanelSlate, SheetSize, 1.f, LockedSheet)
					&& SaveTilePNG(LockedSheet, Directory / TEXT("menu_sheet_locked.png"));
				for (int32 Index = 0; Index < UE_ARRAY_COUNT(LockedNames); ++Index)
				{
					double OpenLuma = 0.0, LockedLuma = 0.0, Saturation = 0.0;
					for (const FIntPoint& Point : Cores[Index])
					{
						const FColor* Open = PixelAt(SheetImage, Point.X, Point.Y);
						const FColor* Locked = PixelAt(LockedSheet, Point.X, Point.Y);
						if (Open && Locked)
						{
							OpenLuma += Luma(Open->R, Open->G, Open->B);
							LockedLuma += Luma(Locked->R, Locked->G, Locked->B);
							const double High = FMath::Max3(Locked->R, Locked->G, Locked->B);
							const double Low = FMath::Min3(Locked->R, Locked->G, Locked->B);
							Saturation += High > 0.0 ? (High - Low) / High : 0.0;
						}
					}
					const int32 Count = Cores[Index].Num();
					const double Ratio = Count > 0 && OpenLuma > 0.0 ? LockedLuma / OpenLuma : 0.0;
					const double MeanSaturation = Count > 0 ? Saturation / Count : 1.0;
					int32 GreyCount = 0;
					const FSheetBox& IconBox = IconBoxes[Index];
					for (int32 Y = FMath::FloorToInt(IconBox.Top); bLockedSheetOk && Y < FMath::CeilToInt(IconBox.Bottom()); ++Y)
					{
						for (int32 X = FMath::FloorToInt(IconBox.Left); X < FMath::CeilToInt(IconBox.Right()); ++X)
						{
							const FColor* Pixel = PixelAt(LockedSheet, X, Y);
							const double PixelLuma = Pixel ? Luma(Pixel->R, Pixel->G, Pixel->B) : 0.0;
							if (Pixel && Pixel->A >= 250 && PixelLuma >= LockedGreyLumaMin && PixelLuma <= LockedGreyLumaMax
								&& FMath::Max3(Pixel->R, Pixel->G, Pixel->B) - FMath::Min3(Pixel->R, Pixel->G, Pixel->B) <= 0.08 * 255.0)
							{
								++GreyCount;
							}
						}
					}
					TestTrue(FString::Printf(TEXT("[LockedIconGrey] %s 회색 칸 %d ≤ 글리프 속 %d × %.1f (글리프 모양을 따른다)"),
						LockedNames[Index], GreyCount, Count, LockedIconMaxGreyToCore),
						Count > 0 && GreyCount > 0 && GreyCount <= LockedIconMaxGreyToCore * Count);
					UE_LOG(LogTemp, Display, TEXT("MenuTileRender locked icon %s core=%d open_luma=%.1f locked_luma=%.1f ratio=%.3f sat=%.3f"),
						LockedNames[Index], Count, Count ? OpenLuma / Count : 0.0, Count ? LockedLuma / Count : 0.0, Ratio, MeanSaturation);
					TestTrue(FString::Printf(TEXT("[LockedIconGrey] %s 흰 글리프 속 %d px 가 잠금 시 휘도 비 %.2f (%.2f~%.2f) · 채도 %.3f (≤ %.2f)"),
						LockedNames[Index], Count, Ratio, LockedIconLumaRatioMin, LockedIconLumaRatioMax, MeanSaturation,
						LockedIconMaxSaturation), bLockedSheetOk && Count >= LockedIconMinCore
						&& Ratio >= LockedIconLumaRatioMin && Ratio <= LockedIconLumaRatioMax
						&& MeanSaturation <= LockedIconMaxSaturation);
				}
			}

			// ---- SheetWidth ― 넓은 캔버스(1000x1600)의 실제 폭 + desired 크기. 이 그리기가 캐시 지오메트리를 바꾸므로 시트 측정의 맨 끝 ----
			const FSheetFootprint Footprint = MeasureSheetFootprint(*Panel);
			SheetJson->SetNumberField(TEXT("footprint_drawn_width"), Footprint.DrawnWidth);
			SheetJson->SetNumberField(TEXT("footprint_desired_width"), Footprint.Desired.X);
			SheetJson->SetNumberField(TEXT("footprint_desired_height"), Footprint.Desired.Y);
			UE_LOG(LogTemp, Display, TEXT("MenuTileRender footprint canvas %dx%d drawn=%.1fx%.1f desired=%.1fx%.1f rim_h=%.1f light_h=%.1f"),
				FootprintCanvasWidth, FootprintCanvasHeight, Footprint.DrawnWidth, Footprint.DrawnHeight,
				Footprint.Desired.X, Footprint.Desired.Y, Footprint.RimHeight, Footprint.LightHeight);
			TestTrue(FString::Printf(TEXT("[SheetWidth] %dx%d 캔버스에서 시트 폭 %.1f · desired %.1fx%.1f (기대 %dx%d)"),
				FootprintCanvasWidth, FootprintCanvasHeight, Footprint.DrawnWidth, Footprint.Desired.X, Footprint.Desired.Y,
				SheetWidth, SheetHeight), FootprintMatchesContract(Footprint, SheetSizeTolerance));
			// 띠는 시트 높이를 따라간다(Fill) ― 1600 캔버스에서 시트가 1600 이면 띠도 1600 이어야 한다
			TestTrue(FString::Printf(TEXT("[SheetEdge] 띠 높이 %.1f / %.1f == 그려진 시트 높이 %.1f"),
				Footprint.RimHeight, Footprint.LightHeight, Footprint.DrawnHeight),
				Footprint.bMeasured && Footprint.DrawnHeight > SheetHeight
				&& FMath::Abs(Footprint.RimHeight - Footprint.DrawnHeight) <= SheetSizeTolerance
				&& FMath::Abs(Footprint.LightHeight - Footprint.DrawnHeight) <= SheetSizeTolerance);

			// 감지력 자기검사 ― 별도 인스턴스의 시트만 옛 폭 880 으로 넓혀 같은 측정을 돌린다. 계약 판정이 이걸 거부해야 위 단언이 의미가 있다
			UUserWidget* Widened = CreateWidget<UUserWidget>(World, PanelClass);
			if (TestNotNull(TEXT("[SheetWidth] 감지력 자기검사용 두 번째 시트 인스턴스"), Widened))
			{
				TStrongObjectPtr<UUserWidget> WidenedGuard(Widened);
				if (USizeBox* WidenedSheet = Cast<USizeBox>(Widened->GetWidgetFromName(TEXT("AppearSlideTarget"))))
				{
					WidenedSheet->SetMinDesiredWidth(WidenedSheetWidth);
					WidenedSheet->SetMaxDesiredWidth(WidenedSheetWidth);
				}
				// 옛 방식(728 캔버스) 으로 재면 몇이 나오는지 증거로만 남긴다 ― 캔버스가 잘라 728 로 읽힌다
				double NarrowCanvasWidth = 0.0;
				{
					const TSharedRef<SWidget> WidenedSlate = Widened->TakeWidget();
					FImage Narrow;
					if (DrawToImage(WidenedSlate, FIntPoint(SheetWidth, SheetHeight), 1.f, Narrow) && Widened->GetRootWidget())
					{
						NarrowCanvasWidth = MeasureWidget(*Widened, TEXT("AppearSlideTarget"),
							FVector2D(Widened->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition())).Width;
					}
				}
				const FSheetFootprint WidenedFootprint = MeasureSheetFootprint(*Widened);
				const bool bWidenedAccepted = FootprintMatchesContract(WidenedFootprint, SheetSizeTolerance);
				UE_LOG(LogTemp, Display, TEXT("MenuTileRender widened probe old_728_canvas=%.1f drawn=%.1f desired=%.1fx%.1f contract=%s"),
					NarrowCanvasWidth, WidenedFootprint.DrawnWidth, WidenedFootprint.Desired.X, WidenedFootprint.Desired.Y,
					bWidenedAccepted ? TEXT("PASS(missed)") : TEXT("FAIL(detected)"));
				TestTrue(FString::Printf(TEXT("[SheetWidth] 감지력: 880 으로 넓힌 시트를 %.1f 로 재고 계약이 거부한다"),
					WidenedFootprint.DrawnWidth), WidenedFootprint.bMeasured
					&& FMath::Abs(WidenedFootprint.DrawnWidth - WidenedSheetWidth) <= SheetSizeTolerance && !bWidenedAccepted);
			}
		}
		else
		{
			AddError(TEXT("UI_MenuPanel 인스턴스 생성 실패 ― 시트 구간을 하나도 재지 못했다"));
		}
	}
	else
	{
		AddError(FString::Printf(TEXT("UI_MenuPanel 클래스를 못 읽었다 ― 시트 구간을 하나도 재지 못했다: %s"), MenuPanelPath));
	}

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetObjectField(TEXT("sheet"), SheetJson);

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Serialized, *(Directory / TEXT("menu_tile_render.json")));

	return true;
}

#endif