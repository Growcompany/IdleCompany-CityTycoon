#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Data/OfficeRosterSummary.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "UI/Element/Office/OfficeRosterRowWidget.h"
#include "UI/Element/Office/OfficeRosterSummaryWidget.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeRosterSummaryRenderValidation,
	"CGR.Office.RosterSummary.Render",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌
namespace OfficeRosterRender
{
	static const TCHAR* SummaryClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Office/UIE_OfficeRosterSummary.UIE_OfficeRosterSummary_C");
	// 상단 띠(160)+18 에서 연구소/출장 줄 위(650)−12 까지 = 460, 폭 = 줄 내용 약 354 + 판 좌우 여백 28·28 = 약 410
	static constexpr float MaxPanelHeight = 460.f;
	static constexpr float MaxPanelWidth = 540.f;
	static constexpr float IconSize = 52.f;
	static constexpr float MinIconGap = 10.f;
	// 불투명 판: 왼쪽 끝 6px 안쪽은 꽉 차 뒤가 비치지 않고, 네 모서리 2x2 칸은 둥근 반경 바깥이라 비어 있다
	static constexpr int32 PlateProbeInset = 6;
	static constexpr int32 MinOpaqueAlpha = 250;
	static constexpr int32 MaxCornerAlpha = 8;

	static FString OutDir() { return FPaths::ProjectSavedDir() / TEXT("OfficeRosterSummary/render"); }

	static TArray<FOfficeRosterLine> MakeLines(std::initializer_list<int32> Counts, std::initializer_list<int64> Powers)
	{
		TArray<FOfficeRosterLine> Lines;
		Lines.SetNum(6);
		int32 Index = 0;
		for (int32 Count : Counts) { Lines[Index++].Headcount = Count; }
		Index = 0;
		for (int64 Power : Powers) { Lines[Index++].Power = Power; }
		return Lines;
	}

	// 실제 RHI 에서만 의미가 있다 ― -NullRHI 면 지오메트리가 전부 0 이라 통과도 실패도 아니다.
	// 그리기 방식은 CodexRailRenderValidation::DrawToImage 와 같다(raw 타깃, 3패스, 감마 태그 보정).
	// 패스마다 Paint 가 위젯 NativeTick 을 먼저 돌린다(SWidget::Paint). 판 크기 주입은 직전 패스의 기하를 읽으므로
	// 크기가 바뀐 첫 패스는 옛 크기로 그려지고 둘째 패스부터 실치수로 그려진다 ― 3패스면 마지막 그림은 실치수다.
	static void DrawPasses(FWidgetRenderer& Renderer, UTextureRenderTarget2D* Target, const TSharedRef<SWidget>& Slate, const FVector2D& Size)
	{
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target, Slate, 1.f, Size, Pass == 0 ? 1.f : 0.f);
			FlushRenderingCommands();
		}
	}

	static FVector2D DrawAndMeasure(FAutomationTestBase& Test, UUserWidget* Widget, const TSharedRef<SWidget>& Slate, const FString& PngName, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);

		// 한 번도 그려지지 않은 위젯은 desired 가 0 일 수 있다 ― 넉넉한 캔버스에 먼저 그려 레이아웃을 세운다
		Widget->ForceLayoutPrepass();
		const FVector2D BeforeWarmup = Widget->GetDesiredSize();
		const FVector2D WarmupSize(1024.f, 1024.f);
		TStrongObjectPtr<UTextureRenderTarget2D> Warmup(FWidgetRenderer::CreateTargetFor(WarmupSize, TF_Bilinear, false));
		DrawPasses(Renderer, Warmup.Get(), Slate, WarmupSize);
		// 사무실에서 처음 쓰는 판 재질(SDF 칩 판)은 셰이더 준비 전이라 같은 프레임의 그림에서 통째로 빠진다
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		FlushRenderingCommands();
		Widget->ForceLayoutPrepass();
		const FVector2D Desired = Widget->GetDesiredSize();
		Test.AddInfo(FString::Printf(TEXT("desired before warmup %.1fx%.1f, after %.1fx%.1f"), BeforeWarmup.X, BeforeWarmup.Y, Desired.X, Desired.Y));

		const FVector2D Size(FMath::CeilToFloat(Desired.X), FMath::CeilToFloat(Desired.Y));
		if (Size.X < 1.f || Size.Y < 1.f)
		{
			return Size;
		}
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, false));
		DrawPasses(Renderer, Target.Get(), Slate, Size);
		FImage& Image = OutImage;
		if (FImageUtils::GetRenderTargetImage(Target.Get(), Image))
		{
			Image.GammaSpace = EGammaSpace::sRGB;
			TArray64<uint8> Bytes;
			IFileManager::Get().MakeDirectory(*OutDir(), true);
			if (FImageUtils::CompressImage(Bytes, TEXT("png"), Image))
			{
				FFileHelper::SaveArrayToFile(Bytes, *(OutDir() / PngName));
			}
		}
		return Size;
	}

	// 모서리 단언의 전제 ― 판 두 층 MID 의 Wpx/Hpx 가 그려진 판 크기와 같아야 한다.
	// 씨앗 크기(공유 MI)로 남으면 모서리가 타원이 되어 아래 픽셀 단언이 엉뚱한 이유로 갈린다.
	static void CheckPlateSizeInjected(FAutomationTestBase& Test, UUserWidget* Panel, const TCHAR* Tag)
	{
		const UImage* PlateBG = Cast<UImage>(Panel->GetWidgetFromName(TEXT("PlateBG")));
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s PlateBG bound"), Tag), PlateBG)) { return; }
		const FVector2D PlateSize = PlateBG->GetCachedGeometry().GetLocalSize();
		const TCHAR* const PlateNames[] = { TEXT("PlateBG"), TEXT("PlateLine") };
		for (const TCHAR* Name : PlateNames)
		{
			const UImage* Plate = Cast<UImage>(Panel->GetWidgetFromName(Name));
			if (!Test.TestNotNull(*FString::Printf(TEXT("%s %s bound"), Tag, Name), Plate)) { continue; }
			UMaterialInstanceDynamic* PlateMID = Cast<UMaterialInstanceDynamic>(Plate->GetBrush().GetResourceObject());
			if (!Test.TestNotNull(*FString::Printf(TEXT("%s %s brush is a dynamic material"), Tag, Name), PlateMID)) { continue; }
			const float Wpx = PlateMID->K2_GetScalarParameterValue(TEXT("Wpx"));
			const float Hpx = PlateMID->K2_GetScalarParameterValue(TEXT("Hpx"));
			Test.TestTrue(*FString::Printf(TEXT("%s %s Wpx/Hpx %.1fx%.1f == drawn plate %.1fx%.1f"), Tag, Name, Wpx, Hpx, PlateSize.X, PlateSize.Y),
				FMath::Abs(Wpx - PlateSize.X) <= 1.f && FMath::Abs(Hpx - PlateSize.Y) <= 1.f);
		}
	}

	static void CheckPlatePixels(FAutomationTestBase& Test, const FImage& Shot, const FVector2D& Size, const TCHAR* Tag)
	{
		if (!Test.TestTrue(*FString::Printf(TEXT("%s panel image read back"), Tag),
			Shot.SizeX == static_cast<int32>(Size.X) && Shot.SizeY == static_cast<int32>(Size.Y) && Shot.Format == ERawImageFormat::BGRA8
			&& Shot.SizeX > PlateProbeInset && Shot.SizeY > PlateProbeInset))
		{
			return;
		}
		const TArrayView64<const FColor> Pixels = Shot.AsBGRA8();
		const auto Alpha = [&Shot, &Pixels](int32 X, int32 Y) { return static_cast<int32>(Pixels[static_cast<int64>(Y) * Shot.SizeX + X].A); };

		const int32 LeftInside = Alpha(PlateProbeInset, Shot.SizeY / 2);
		Test.TestTrue(*FString::Printf(TEXT("%s plate is opaque %dpx inside the left edge (alpha %d >= %d)"), Tag, PlateProbeInset, LeftInside, MinOpaqueAlpha),
			LeftInside >= MinOpaqueAlpha);

		struct FCornerProbe { const TCHAR* Name; int32 X; int32 Y; };
		const FCornerProbe Corners[] = {
			{ TEXT("top-left"), 0, 0 },
			{ TEXT("top-right"), Shot.SizeX - 2, 0 },
			{ TEXT("bottom-left"), 0, Shot.SizeY - 2 },
			{ TEXT("bottom-right"), Shot.SizeX - 2, Shot.SizeY - 2 },
		};
		for (const FCornerProbe& Corner : Corners)
		{
			int32 CornerAlpha = 0;
			for (int32 DY = 0; DY < 2; ++DY)
			{
				for (int32 DX = 0; DX < 2; ++DX)
				{
					CornerAlpha = FMath::Max(CornerAlpha, Alpha(Corner.X + DX, Corner.Y + DY));
				}
			}
			Test.TestTrue(*FString::Printf(TEXT("%s %s corner 2x2 is outside the rounding (max alpha %d <= %d)"), Tag, Corner.Name, CornerAlpha, MaxCornerAlpha),
				CornerAlpha <= MaxCornerAlpha);
		}
	}

	// 줄마다 인원·능력치 열 끝이 맞고, 글자가 잘리지 않고, 아이콘이 52 로 떨어져 그려지는지
	static void CheckRows(FAutomationTestBase& Test, UOfficeRosterSummaryWidget* Panel, const TCHAR* Tag)
	{
		const FGeometry& PanelGeo = Panel->GetCachedGeometry();
		const auto LocalRect = [&PanelGeo](const UWidget* W)
		{
			const FGeometry& G = W->GetCachedGeometry();
			return FBox2D(PanelGeo.AbsoluteToLocal(G.GetAbsolutePosition()), PanelGeo.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize())));
		};
		float CountRight = -1.f, PowerRight = -1.f, PrevIconBottom = -1.f;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			UOfficeRosterRowWidget* Row = Panel->GetRow(Index);
			if (!Test.TestNotNull(*FString::Printf(TEXT("%s row%d bound"), Tag, Index), Row)) { return; }
			// 열 정렬: 줄마다 인원·능력치의 오른쪽 끝이 같아야 한다(±1px)
			const FBox2D CountBox = LocalRect(Row->GetCountTextForTest());
			const FBox2D PowerBox = LocalRect(Row->GetPowerTextForTest());
			Test.TestTrue(*FString::Printf(TEXT("%s row%d count/power laid out (%.1f, %.1f wide)"), Tag, Index, CountBox.GetSize().X, PowerBox.GetSize().X),
				CountBox.GetSize().X > 1.f && PowerBox.GetSize().X > 1.f);
			const float C = CountBox.Max.X;
			const float P = PowerBox.Max.X;
			if (Index == 0) { CountRight = C; PowerRight = P; }
			Test.TestTrue(*FString::Printf(TEXT("%s row%d count column aligned (%.1f vs %.1f)"), Tag, Index, C, CountRight), FMath::Abs(C - CountRight) <= 1.f);
			Test.TestTrue(*FString::Printf(TEXT("%s row%d power column aligned (%.1f vs %.1f)"), Tag, Index, P, PowerRight), FMath::Abs(P - PowerRight) <= 1.f);

			// 아이콘이 유일한 표시다: 52 로 그려지고 위아래 아이콘이 붙지 않는다
			const FBox2D Icon = LocalRect(Row->GetIconForTest());
			Test.TestTrue(*FString::Printf(TEXT("%s row%d icon %.1fx%.1f == %.0f"), Tag, Index, Icon.GetSize().X, Icon.GetSize().Y, IconSize),
				FMath::Abs(Icon.GetSize().X - IconSize) <= 0.5f && FMath::Abs(Icon.GetSize().Y - IconSize) <= 0.5f);
			if (Index > 0)
			{
				Test.TestTrue(*FString::Printf(TEXT("%s row%d icon gap %.1f >= %.0f"), Tag, Index, Icon.Min.Y - PrevIconBottom, MinIconGap), Icon.Min.Y - PrevIconBottom >= MinIconGap);
			}
			PrevIconBottom = Icon.Max.Y;
		}
		for (int32 Index = 0; Index < 6; ++Index)
		{
			for (const UCommonTextBlock* Text : { Panel->GetRow(Index)->GetCountTextForTest(), Panel->GetRow(Index)->GetPowerTextForTest() })
			{
				Test.TestTrue(*FString::Printf(TEXT("%s row%d '%s' not clipped"), Tag, Index, *Text->GetText().ToString()),
					Text->GetCachedGeometry().GetLocalSize().X + 0.5f >= Text->GetDesiredSize().X);
			}
		}
	}
}

bool FOfficeRosterSummaryRenderValidation::RunTest(const FString& Parameters)
{
	using namespace OfficeRosterRender;
	if (!FApp::CanEverRender())
	{
		AddError(TEXT("실제 RHI 로 실행해야 한다(-NullRHI 금지)."));
		return false;
	}
	// 위젯 소유자로 게임 인스턴스가 필요하다 ― PIE 우선, 없으면 -game 스탠드얼론 월드(CodexRailRenderValidation 과 같은 2패스)
	UWorld* PlayWorld = nullptr;
	for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == Wanted && Context.World()) { PlayWorld = Context.World(); break; }
		}
		if (PlayWorld) { break; }
	}
	UGameInstance* GameInstance = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		AddError(TEXT("플레이 월드가 없다 ― -game 스탠드얼론으로 띄우거나 PIE 를 시작한 뒤 실행한다."));
		return false;
	}
	UClass* SummaryClass = LoadClass<UOfficeRosterSummaryWidget>(nullptr, SummaryClassPath);
	if (!TestNotNull(TEXT("UIE_OfficeRosterSummary class"), SummaryClass)) { return false; }
	TStrongObjectPtr<UOfficeRosterSummaryWidget> Panel(CreateWidget<UOfficeRosterSummaryWidget>(GameInstance, SummaryClass));
	if (!TestNotNull(TEXT("panel instance"), Panel.Get())) { return false; }
	// TakeWidget 이 NativeConstruct 를 돌려 실제 로스터로 한 번 갱신한다 ― 먼저 소진해야 아래 픽스처가 덮이지 않는다.
	// UMG 는 Slate 위젯을 약하게만 쥐므로 반환값을 붙잡아 두지 않으면 곧바로 파괴되고, 다음 TakeWidget 에서 다시 만들어져 갱신이 또 돈다.
	const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();

	// ① 소속 0명 = 판이 접힌다
	Panel->ApplyLines(MakeLines({0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), 0);
	TestEqual(TEXT("empty office collapses the panel"), Panel->GetVisibility(), ESlateVisibility::Collapsed);

	// ② 0명이지만 능력치가 있는 줄 + 큰 수(9.99만 · 1.23억)
	Panel->ApplyLines(MakeLines({3, 12, 0, 2, 4, 1}, {18500, 123456789, 340, 99990, 24200, 9700}), 22);
	TestEqual(TEXT("staffed office shows the panel"), Panel->GetVisibility(), ESlateVisibility::HitTestInvisible);
	FImage Shot;
	const FVector2D Size = DrawAndMeasure(*this, Panel.Get(), PanelSlate, TEXT("stress.png"), Shot);
	// 크기 0 이면 아래 정렬 단언이 전부 0==0 으로 거짓 통과한다 ― 먼저 막는다
	if (!TestTrue(*FString::Printf(TEXT("panel drew with a real size (%.0fx%.0f)"), Size.X, Size.Y), Size.X >= 1.f && Size.Y >= 1.f)) { return false; }
	TestTrue(*FString::Printf(TEXT("width %.0f <= %.0f"), Size.X, MaxPanelWidth), Size.X <= MaxPanelWidth);
	TestTrue(*FString::Printf(TEXT("height %.0f <= %.0f"), Size.Y, MaxPanelHeight), Size.Y <= MaxPanelHeight);

	// 떠 있는 불투명 판: 뒤의 벽·창이 비치지 않고 네 모서리가 둥글다
	CheckPlateSizeInjected(*this, Panel.Get(), TEXT("stress"));
	CheckPlatePixels(*this, Shot, Size, TEXT("stress"));

	CheckRows(*this, Panel.Get(), TEXT("stress"));

	// ②-1 가장 넓은 문자열 ― 세 자리 인원, AbbreviateNumber 의 정수부 4자리(9990만·9990억)
	Panel->ApplyLines(MakeLines({100, 88, 0, 7, 12, 1}, {99990000, 999900000000LL, 5, 9999, 123, 10000}), 208);
	FImage WidestShot;
	const FVector2D WidestSize = DrawAndMeasure(*this, Panel.Get(), PanelSlate, TEXT("widest.png"), WidestShot);
	TestTrue(*FString::Printf(TEXT("widest width %.0f <= %.0f"), WidestSize.X, MaxPanelWidth), WidestSize.X <= MaxPanelWidth);
	TestEqual(TEXT("widest power text"), Panel->GetRow(0)->GetPowerTextForTest()->GetText().ToString(), FString(TEXT("9990만")));
	TestEqual(TEXT("widest power text eok"), Panel->GetRow(1)->GetPowerTextForTest()->GetText().ToString(), FString(TEXT("9990억")));
	CheckRows(*this, Panel.Get(), TEXT("widest"));
	// 판이 가장 넓게 늘어나도 실치수가 들어가 모서리가 둥글게 남는다
	CheckPlateSizeInjected(*this, Panel.Get(), TEXT("widest"));
	CheckPlatePixels(*this, WidestShot, WidestSize, TEXT("widest"));

	// 판을 그린 뒤 0명이 돼도 판째 접힌다 ― 빈 판이 남지 않는다
	Panel->ApplyLines(MakeLines({0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}), 0);
	TestEqual(TEXT("widest then empty office collapses the whole plate"), Panel->GetVisibility(), ESlateVisibility::Collapsed);

	// 아래 단언은 stress 픽스처 기준이라 다시 넣고 그린다
	Panel->ApplyLines(MakeLines({3, 12, 0, 2, 4, 1}, {18500, 123456789, 340, 99990, 24200, 9700}), 22);
	DrawAndMeasure(*this, Panel.Get(), PanelSlate, TEXT("stress.png"), Shot);

	// ③ 0명 줄: 경고색 없이 줄 전체가 흐려지고, 능력치 숫자는 그대로 읽힌다(그래픽 = 슬롯 2, 능력치 340)
	UOfficeRosterRowWidget* ZeroRow = Panel->GetRow(2);
	UOfficeRosterRowWidget* StaffedRow = Panel->GetRow(1);
	const FLinearColor ZeroInk = ZeroRow->GetCountTextForTest()->GetColorAndOpacity().GetSpecifiedColor();
	const FLinearColor StaffedInk = StaffedRow->GetCountTextForTest()->GetColorAndOpacity().GetSpecifiedColor();
	TestTrue(*FString::Printf(TEXT("zero row count ink is dimmer (%.3f < %.3f)"), ZeroInk.GetLuminance(), StaffedInk.GetLuminance()), ZeroInk.GetLuminance() < StaffedInk.GetLuminance());
	TestTrue(*FString::Printf(TEXT("zero row count ink is not a warning red (R %.3f <= B %.3f)"), ZeroInk.R, ZeroInk.B), ZeroInk.R <= ZeroInk.B);
	TestEqual(TEXT("zero row unit shares the count ink"), ZeroRow->GetUnitTextForTest()->GetColorAndOpacity().GetSpecifiedColor(), ZeroInk);
	TestTrue(TEXT("zero row icon is dimmed"), ZeroRow->GetIconForTest()->GetRenderOpacity() < 1.f);
	TestTrue(TEXT("zero row power is dimmed"), ZeroRow->GetPowerTextForTest()->GetRenderOpacity() < 1.f);
	TestEqual(TEXT("staffed row power stays opaque"), StaffedRow->GetPowerTextForTest()->GetRenderOpacity(), 1.f);
	TestEqual(TEXT("zero row power label"), ZeroRow->GetPowerTextForTest()->GetText().ToString(), FString(TEXT("340")));
	TestEqual(TEXT("big power uses Korean units"), Panel->GetRow(1)->GetPowerTextForTest()->GetText().ToString(), FString(TEXT("1.23억")));
	TestEqual(TEXT("under 10k stays full"), Panel->GetRow(5)->GetPowerTextForTest()->GetText().ToString(), FString(TEXT("9,700")));
	return true;
}
#endif
